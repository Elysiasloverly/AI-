// 敌人生命与复用：一次出池对应一个新生命周期，重建属性/能力并清空旧状态。
#include "Enemies/RogueEnemy.h"
#include "Enemies/Components/RogueBossSkillComponent.h"
#include "GameplayEffectExtension.h"
#include "Enemies/Components/RogueEnemyPresentationComponent.h"
#include "Enemies/Components/RogueEnemySupportComponent.h"
#include "Enemies/Components/RogueEnemyStationaryHazardComponent.h"
#include "Enemies/Components/RogueEnemyObstacleMovementComponent.h"
#include "Combat/Visuals/RogueHitFlashComponent.h"
#include "AbilitySystem/RogueAbilitySystemComponent.h"
#include "AbilitySystem/RogueAttributeSet.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "AbilitySystem/RogueGameplayTags.h"
#include "Combat/Visuals/RogueCombatFeedback.h"
#include "Enemies/Data/RogueEnemyArchetypes.h"
#include "Framework/RogueGameMode.h"
#include "Combat/Targeting/RogueEnemyTrackerSubsystem.h"
#include "Components/CapsuleComponent.h"
#include "Components/MeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "AIController.h"

void ARogueEnemy::InitializeEnemy(const FRogueEnemyProfile &InProfile)
{
    if (BossSkills) BossSkills->ResetForPool();
	if (ObstacleMovement)
		ObstacleMovement->ResetForPool();
	if (EnemyPresentation)
		EnemyPresentation->ResetForPool();
	AttackAudioState.Reset();
	if (HitFlashComponent)
	{
		HitFlashComponent->ClearHitFlash();
	}
	EnemyType = InProfile.EnemyType;
	MaxHealth = InProfile.Health;
	CurrentHealth = MaxHealth;
	MovementSlows.Reset();
	MoveSpeed = InProfile.Speed;
	ContactDamage = InProfile.Damage;
	ExperienceReward = InProfile.ExperienceReward;
	bIsBoss = InProfile.bIsBoss;

	// 优先从 DataTable 读取原型配置，未配置则回退到硬编码
	if (EnemyArchetypeDataTable != nullptr)
	{
		CurrentArchetype = RogueEnemyArchetypes::BuildEnemyArchetypeFromDataTable(EnemyArchetypeDataTable,
																				  EnemyType, bIsBoss);
	}
	else
	{
		CurrentArchetype = RogueEnemyArchetypes::BuildEnemyArchetype(EnemyType, bIsBoss);
	}

	// 每次出池都从原始原型重建，再应用阶段倍率，避免回池复用时重复减半。
	const ARogueGameMode *Mode = GetWorld() ? GetWorld()->GetAuthGameMode<ARogueGameMode>() : nullptr;
	AttackSpeedMultiplier = Mode ? FMath::Max(1.f, Mode->GetEnemyAttackSpeedMultiplier()) : 1.f;
	CurrentArchetype.Ranged.InitialCooldownMin /= AttackSpeedMultiplier;
	CurrentArchetype.Ranged.InitialCooldownMax /= AttackSpeedMultiplier;
	CurrentArchetype.Ranged.NextShotCooldown /= AttackSpeedMultiplier;
	BurstCooldown = CurrentArchetype.Movement.Model == ERogueEnemyMovementModel::BurstCharge
						? FMath::FRandRange(0.8f, 1.8f)
						: 0.0f;
	BurstTimeRemaining = 0.0f;
	ContactTimer = 0.0f;
	RangedAttackTimer = CurrentArchetype.Ranged.bUsesRangedAttack
							? FMath::FRandRange(CurrentArchetype.Ranged.InitialCooldownMin,
												CurrentArchetype.Ranged.InitialCooldownMax)
							: 0.0f;
	LastDamageNumberTime = -100.0f;
	PendingDamageNumber = 0.0f;
	bDead = false;
	bPoolAvailable = false;
	SetActorTickInterval(0.0f);
	// 同上：位置在 ActivatePooledEnemy 里才写，这里只让首帧立刻重选目标。
	CachedPlayerCharacter = nullptr;
	TargetRefreshCooldown = 0.0f;
	GetCharacterMovement()->MaxWalkSpeed = MoveSpeed;
	ApplyEnemyStyle();
	InitializeFlight();

	// 把这一只的血量上限/当前值写进 GAS 属性。
	// 池复用时 AttributeSet 是同一个对象，必须在这里重写，
	// 否则下一只怪会带着上一只的血量上限（这也是 P2-25 类问题的同类隐患）。
	SyncHealthToAbilitySystem();
	if (bIsBoss && AttributeSet != nullptr)
	{
		UE_LOG(LogTemp, Log, TEXT("[BossLifecycle] Initialized Enemy=%s Spawn=%s Health=%.2f/%.2f"),
			   *GetName(), *SpawnCycleId.ToString(), AttributeSet->GetHealth(), AttributeSet->GetMaxHealth());
	}

	// 通知蓝图子类初始化完成
	OnEnemyInitialized(InProfile);
	ConfigureGroundedCollision();

	// 生成顺序为 ActivatePooledEnemy → InitializeEnemy。先清旧 ASC 状态，
	// 再按本次已加载的 CurrentArchetype 授予，避免使用上次池周期的原型。
	GrantAttackAbilities();
    if (BossSkills) BossSkills->InitializeRuntime();
	if (HasAuthority())
	{
		RuntimeProfile.Profile = InProfile;
		RuntimeProfile.GroundZ = FlightGroundZ;
		RuntimeProfile.Generation = HitFeedback.Generation;
		ForceNetUpdate();
	}
	RefreshUtilityComponents();
}

// 出池开始新生命周期。重置目标、攻击计时和表现代次，避免上次死亡留下的状态影响新敌人。
void ARogueEnemy::ActivatePooledEnemy(AActor *InOwner, const FVector &SpawnLocation,
									  const FRotator &SpawnRotation)
{
	if (BossSkills) BossSkills->ResetForPool();
	if (EnemyPresentation)
		EnemyPresentation->ResetForPool();
	if (EnemySupport)
		EnemySupport->ResetForPool();
	if (StationaryHazard)
		StationaryHazard->ResetForPool();
	GetWorldTimerManager().ClearTimer(PendingDeathTimer);
	bPendingDeath = false;
	PendingDeathSpawnId.Invalidate();
	if (HitFlashComponent)
	{
		HitFlashComponent->ClearHitFlash();
	}
	if (HasAuthority())
	{
		const uint32 Generation = HitFeedback.Generation + 1;
		HitFeedback = FRogueEnemyHitFeedback();
		HitFeedback.Generation = Generation;
		HitFeedback.bActive = true;
		HealingFeedback = FRogueEnemyHealingFeedback();
		HealingFeedback.Generation = Generation;
		HealingFeedback.bActive = true;
	}
    MovementSlows.Reset();
	SpawnCycleId = FGuid::NewGuid();
	NextCrowdSampleTime = 0;
	CachedCrowdSteering = FVector::ZeroVector;
	FlightGroundZ = SpawnLocation.Z;
	FlyingVolleysRemaining = 0;
	FlyingTarget.Reset();
	ConfigureGroundedCollision();
	SetOwner(InOwner);
	SetActorLocationAndRotation(SpawnLocation, SpawnRotation, false, nullptr, ETeleportType::TeleportPhysics);
	SetActorHiddenInGame(false);
	SetActorTickEnabled(true);
	SetMovementTickForPool(true);
	SetActorEnableCollision(true);
	bPoolAvailable = false;
	bDead = false;

	// 通知客户端"这只怪上场了"（复制位 → 客户端自己把 tick/碰撞/标志翻回来）
	bReplicatedActive = true;

	// 清空上一只留下的 GAS 残留：激活中的 GE（尤其是授予 State.Dead 的死亡效果）
	// 与已授予的技能。**必须在设置血量之前做** ——
	// 否则 State.Dead 还在身上，死亡效果不会被正确清理，复用出来的怪会"一出生就是死的"。
	if (AbilitySystemComponent != nullptr)
	{
		AbilitySystemComponent->ResetForPool();
	}

	// 攻击能力待 InitializeEnemy 设置本次原型后授予。

	if (UCapsuleComponent *Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

		// 这里再应用一次"敌人之间互不阻挡"：构造函数里设过，但蓝图子类的属性值
		// 是在构造之后才加载的（bIgnoreOtherPawnCollision 可能在 BP 上被改过），
		// 所以每次激活时兜一次，保证开关在运行时确实生效。
		Capsule->SetCollisionResponseToChannel(ECC_Pawn, bIgnoreOtherPawnCollision ? ECR_Ignore : ECR_Block);
	}

	if (UCharacterMovementComponent *Movement = GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
		Movement->Velocity = FVector::ZeroVector;
		Movement->SetMovementMode(MOVE_Walking);
	}

	// 确保有 AI Controller —— **只在服务端**。
	// 敌人的 AI 已经只在服务端跑（见 Tick），客户端上再给每只怪生成一个
	// AAIController（带着 PathFollowingComponent）纯属浪费：
	// 200+ 敌人时这是可观的 CPU 与内存开销，而且它们什么都不做。
	// 客户端看到的是复制代理，位置由 CharacterMovement 从服务器同步。
	if (HasAuthority() && GetController() == nullptr)
	{
		SpawnDefaultController();
	}

	// 通知蓝图子类
	OnEnemyActivated();
	ConfigureGroundedCollision();
}

void ARogueEnemy::SetActorHiddenInGame(bool bNewHidden)
{
    // 引擎先应用复制的 bHidden，再通知 bReplicatedActive。隐藏前截取法阵，
    // 避免客户端跳过收尾；组件按出池周期去重，服务器原本的回收顺序也适用。
    if (bNewHidden && !IsHidden() && StationaryHazard)
        StationaryHazard->ResetForPool();
    Super::SetActorHiddenInGame(bNewHidden);
}

void ARogueEnemy::DeactivateToPool()
{
    MovementSlows.Reset();
	if (BossSkills) BossSkills->ResetForPool();
	if (EnemyPresentation)
		EnemyPresentation->ResetForPool();
	if (EnemySupport)
		EnemySupport->ResetForPool();
	if (StationaryHazard)
		StationaryHazard->ResetForPool();
	FlyingVolleysRemaining = 0;
	FlyingVolleyTimer = 0;
	FlyingTarget.Reset();
	GetWorldTimerManager().ClearTimer(PendingDeathTimer);
	bPendingDeath = false;
	PendingDeathSpawnId.Invalidate();
	AttackAudioState.Reset();
	if (HitFlashComponent)
	{
		HitFlashComponent->ClearHitFlash();
	}
	if (HasAuthority())
	{
		HitFeedback.bActive = false;
		HealingFeedback.bActive = false;
	}
	if (!bPoolAvailable)
	{
		if (UWorld *World = GetWorld())
		{
			if (URogueEnemyTrackerSubsystem *Tracker = World->GetSubsystem<URogueEnemyTrackerSubsystem>())
			{
				Tracker->UnregisterEnemy(this);
			}
		}
	}

	// 通知蓝图子类
	OnEnemyDeactivated();

	bPoolAvailable = true;
	bDead = true;
	CurrentHealth = 0.0f;

	// 只允许权威端写复制的在场位；客户端通过 OnRep 应用已收到的状态。
	if (HasAuthority())
	{
		bReplicatedActive = false;
	}
	ContactTimer = 0.0f;
	RangedAttackTimer = 0.0f;
	// 当前池周期结束，立即作废句柄；技能列表由下次激活的 ResetForPool 清理。
	ContactAttackAbilityHandle = FGameplayAbilitySpecHandle();
	RangedAttackAbilityHandle = FGameplayAbilitySpecHandle();
	BurstCooldown = 0.0f;
	BurstTimeRemaining = 0.0f;
	LastDamageNumberTime = -100.0f;
	PendingDamageNumber = 0.0f;
	CachedPlayerCharacter.Reset();
	TargetRefreshCooldown = 0.0f;
	SetActorHiddenInGame(true);
	SetActorTickEnabled(false);
	SetMovementTickForPool(false);
	SetActorEnableCollision(false);
	SetActorLocation(FVector(0.0f, 0.0f, -20000.0f), false, nullptr, ETeleportType::TeleportPhysics);
	SetOwner(nullptr);

	if (UCapsuleComponent *Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	if (UCharacterMovementComponent *Movement = GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
		Movement->Velocity = FVector::ZeroVector;
	}
}

void ARogueEnemy::Die(AActor *Killer)
{
	bDead = true;
    if (BossSkills) BossSkills->ResetForPool();

	// 通知蓝图子类
	OnEnemyDeath(Killer);

	if (ARogueGameMode *RogueGameMode = GetWorld()->GetAuthGameMode<ARogueGameMode>())
	{
		RogueGameMode->HandleEnemyKilled(this, Killer);
	}

	DeactivateToPool();
}

void ARogueEnemy::SyncHealthToAbilitySystem()
{
	if (AttributeSet == nullptr || AbilitySystemComponent == nullptr || !HasAuthority())
	{
		return;
	}

	// 顺序：先写上限再写当前值。
	// PreAttributeChange 里 Health 的上界取的是 GetMaxHealth()，反了会被旧上限钳住。
	//
	// 这一段是"初始化写入"，不是战斗伤害，必须屏蔽飘字：
	// 池复用时 AttributeSet 里还留着上一只的血量，属性变化会被 HandleHealthChanged
	// 误判成一次受伤（例如上一只残留 5 点、新一只上限 3 点 → 读成掉了 2 点血）。
	bSuppressDamageFeedback = true;

	AttributeSet->SetMaxHealth(MaxHealth);
	AttributeSet->SetHealth(CurrentHealth);

	// 敌人目前没有护甲与减伤；显式清零，避免池复用残留上一只的数值
	AttributeSet->SetMaxArmor(0.0f);
	AttributeSet->SetArmor(0.0f);
	AttributeSet->SetDamageReductionPercent(0.0f);

	// 元属性必须是 0，否则第一次结算会凭空多出一段伤害
	AttributeSet->SetDamage(0.0f);

	bSuppressDamageFeedback = false;
}

void ARogueEnemy::HandleHealthChanged(const FOnAttributeChangeData &ChangeData)
{
	// 真相源反转：AttributeSet 是权威，CurrentHealth 只是给 HUD / 追踪器 / 奖励结算读的镜像。
	// （迁移前是反过来的：CurrentHealth 是权威，属性集根本没人用。）
	CurrentHealth = ChangeData.NewValue;

	// 初始化写入不算受伤
	if (bSuppressDamageFeedback)
	{
		return;
	}

	if (ChangeData.NewValue > ChangeData.OldValue)
		RecordHealingFeedback(ChangeData.NewValue - ChangeData.OldValue);
	const float DamageTaken = ChangeData.OldValue - ChangeData.NewValue;
	if (DamageTaken > 0.0f)
	{
		ShowDamageNumber(DamageTaken);
		uint8 Style = 0;
		const FGameplayEffectSpec *DamageSpec =
			ChangeData.GEModData ? &ChangeData.GEModData->EffectSpec
								 : (AttributeSet ? AttributeSet->GetExecutingDamageSpec() : nullptr);
		if (DamageSpec)
		{
			const float Value =
				DamageSpec->GetSetByCallerMagnitude(FName(TEXT("Visual.WeaponImpactStyle")), false, 0.f);
			if (FMath::IsFinite(Value))
				Style = uint8(FMath::Clamp(FMath::RoundToInt(Value), 0, 6));
		}
		RecordHitFeedback(DamageTaken, Style);
	}
}

void ARogueEnemy::ShowDamageNumber(float AppliedDamage)
{
	if (AppliedDamage <= 0.0f || GetWorld() == nullptr)
	{
		return;
	}

	const float CurrentWorldTime = GetWorld()->GetTimeSeconds();
	const float DamageNumberInterval = bIsBoss ? 0.06f : 0.12f;
	if (CurrentWorldTime - LastDamageNumberTime < DamageNumberInterval)
	{
		// 把节流窗口内攒下的伤害累加，窗口打开时一次性弹出。
		// 为什么：伤害数字是"每个敌人每 0.12s 最多一条"（防刷屏），
		// 但伤害本身是每段都结算的。窗口内的伤害若直接丢弃，
		// 多把环绕刀片 / 多段命中在画面上就只剩一个数字，看起来像"没结算"。
		PendingDamageNumber += AppliedDamage;
		return;
	}

	LastDamageNumberTime = CurrentWorldTime;
	const float ShownDamage = PendingDamageNumber + AppliedDamage;
	PendingDamageNumber = 0.0f;

	FVector DamageLocation = GetHealthBarWorldLocation();
	DamageLocation.X += FMath::FRandRange(-24.0f, 24.0f);
	DamageLocation.Y += FMath::FRandRange(-24.0f, 24.0f);
	RogueCombatFeedback::AddDamageNumber(this, DamageLocation, ShownDamage, bIsBoss);
}

void ARogueEnemy::HandleDeathTagChanged(FGameplayTag Tag, int32 NewCount)
{
	if (NewCount <= 0 || bDead || bPendingDeath)
	{
		return;
	}

	bPendingDeath = true;
	PendingDeathSpawnId = SpawnCycleId;

	// SetTimerForNextTick 将死亡处理移出施加死亡 GE 的当前调用栈，
	// 避免回池清除 GE 时在其标签通知里重入；执行时刻取决于 TimerManager 调度。
	if (UWorld *World = GetWorld())
	{
		PendingDeathTimer = World->GetTimerManager().SetTimerForNextTick(this, &ARogueEnemy::ProcessDeath);
	}
}

// 延迟死亡通知：同一出池代次仍为零生命且保有死亡标签时，才交给 Die 结算和回收。
void ARogueEnemy::ProcessDeath()
{
	const bool bSameSpawn = PendingDeathSpawnId == SpawnCycleId;
	bPendingDeath = false;
	PendingDeathSpawnId.Invalidate();
	PendingDeathTimer.Invalidate();

	if (!bSameSpawn || bDead || bPoolAvailable || AttributeSet == nullptr ||
		AttributeSet->GetHealth() > 0.0f || AbilitySystemComponent == nullptr ||
		!AbilitySystemComponent->HasMatchingGameplayTag(RogueGameplayTags::State_Dead))
	{
		return;
	}

	// 击杀者由属性集记录（死亡 GE 本身不携带这个信息）
	AActor *Killer = AttributeSet != nullptr ? AttributeSet->GetLastDamageCauser() : nullptr;
	Die(Killer);
}

void ARogueEnemy::ApplyEnemyStyle()
{
	const bool bIsShockPillar = EnemyType == ERogueEnemyType::ShockPillar;
	const bool bShowBuiltInShockPillar = bIsShockPillar && bUseBuiltInShockPillarVisual;
	if (ShockPillarMesh != nullptr)
	{
		ShockPillarMesh->SetHiddenInGame(!bShowBuiltInShockPillar);
		ShockPillarMesh->SetVisibility(bShowBuiltInShockPillar, true);

		if (bShowBuiltInShockPillar)
		{
			ShockPillarMesh->SetRelativeLocation(CurrentArchetype.Visual.MeshLocation);
			ShockPillarMesh->SetRelativeRotation(CurrentArchetype.Visual.MeshRotation);
			ShockPillarMesh->SetRelativeScale3D(CurrentArchetype.Visual.MeshScale);

			if (ShockPillarMaterial != nullptr)
			{
				// MID 只建一次并缓存（见头文件里的说明：参数是常量，每次出池重建纯属浪费）
				if (CachedShockPillarMID == nullptr)
				{
					CachedShockPillarMID = UMaterialInstanceDynamic::Create(ShockPillarMaterial, this);
					if (CachedShockPillarMID != nullptr)
					{
						const FLinearColor PillarColor(1.0f, 0.78f, 0.05f, 1.0f);
						CachedShockPillarMID->SetVectorParameterValue(TEXT("Color"), PillarColor);
						CachedShockPillarMID->SetVectorParameterValue(TEXT("BaseColor"), PillarColor);
						CachedShockPillarMID->SetVectorParameterValue(TEXT("EmissiveColor"),
																	  PillarColor * 2.2f);
					}
				}

				UMaterialInterface *EffectivePillarMaterial = ShockPillarMaterial;
				if (CachedShockPillarMID != nullptr)
				{
					EffectivePillarMaterial = CachedShockPillarMID.Get();
				}
				ShockPillarMesh->SetMaterial(0, EffectivePillarMaterial);
			}
		}
	}

	GetCapsuleComponent()->SetCapsuleSize(CurrentArchetype.Visual.CapsuleRadius,
										  CurrentArchetype.Visual.CapsuleHalfHeight);
	GetCharacterMovement()->MaxWalkSpeed = bIsShockPillar ? 0.0f : MoveSpeed;
	GetCharacterMovement()->bOrientRotationToMovement = !bIsShockPillar;

	OnApplyVisualStyle();
}

void ARogueEnemy::OnEnemyInitialized_Implementation(const FRogueEnemyProfile &Profile)
{
	// 默认空实现，蓝图子类可重写
}

void ARogueEnemy::OnEnemyActivated_Implementation()
{
	// 默认空实现，蓝图子类可重写
}

void ARogueEnemy::OnEnemyDeactivated_Implementation()
{
	// 默认空实现，蓝图子类可重写
}

void ARogueEnemy::OnEnemyDeath_Implementation(AActor *Killer)
{
	// 默认空实现，蓝图子类可重写以播放死亡特效等
}

bool ARogueEnemy::OnBehaviorTick_Implementation(float DeltaSeconds)
{
	// 默认返回 false，不拦截基类 Tick 逻辑
	// 蓝图子类可重写返回 true 以完全自定义行为
	return false;
}

void ARogueEnemy::OnApplyVisualStyle_Implementation()
{
	// 默认空实现，蓝图子类可重写以自定义外观
}
