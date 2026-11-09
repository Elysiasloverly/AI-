// 敌人攻击：服务器决策并激活能力；多播入口在各端生成攻击表现，不在客户端重复结算。
#include "Enemies/RogueEnemy.h"
#include "Enemies/Components/RogueBossSkillComponent.h"
#include "AbilitySystem/Abilities/RogueEnemyAttackAbility.h"
#include "Combat/Visuals/RogueImpactEffect.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Player/RogueCharacter.h"
#include "AbilitySystem/RogueAbilitySystemComponent.h"
#include "AbilitySystem/RogueAbilitySet.h"
#include "AbilitySystem/RogueAbilitySystemLibrary.h"
#include "AbilitySystem/RogueGameplayTags.h"
#include "Debug/RogueSelfTest.h"
#include "Enemies/Data/RogueEnemyArchetypes.h"
#include "Combat/Projectiles/RogueEnemyProjectile.h"
#include "Framework/RogueGameState.h"
#include "Combat/Pooling/RogueCombatPoolSubsystem.h"
#include "Components/CapsuleComponent.h"

bool ARogueEnemy::AreNormalAttacksSuppressed() const
{ return BossSkills && BossSkills->SuppressesNormalAttacks(); }
bool ARogueEnemy::IsBossSkillBlockingMovement() const
{ return BossSkills && BossSkills->SuppressesMovement(); }

void ARogueEnemy::GrantAttackAbilities()
{
	// 敌人攻击由服务端授予；ServerOnly 指执行策略，技能列表是否复制由 ASC 的复制条件决定。
	if (!HasAuthority() || AbilitySystemComponent == nullptr)
	{
		return;
	}

	// 先清空句柄：换原型/换池周期后旧句柄不该再被使用。
	ContactAttackAbilityHandle = FGameplayAbilitySpecHandle();
	RangedAttackAbilityHandle = FGameplayAbilitySpecHandle();

	if (RogueEnemyArchetypes::IsHealerType(EnemyType))
	{
		return;
	}

	// 压测对照开关（见 RogueSelfTest.h）：关掉授予就能量出"GAS 这部分到底贵多少"。
	if (RogueSelfTest::AreEnemyAttackAbilitiesDisabled())
	{
		return;
	}

	// ---- ① 优先：数据驱动的能力集 ----
	//
	// ⚠️ 这里能读到**正确的** CurrentArchetype，是因为授予被放在 `InitializeEnemy()` 的**末尾**
	//    （原型赋值之后）。早期版本放在 `ActivatePooledEnemy()` 里，会照着**上一只怪**的原型
	//    发技能，症状是"远程怪不远程、震荡柱不起波"且没有任何报错。详见 URogueAbilitySet 的类注释。
	if (URogueAbilitySet *AbilitySet = CurrentArchetype.AttackAbilitySet.LoadSynchronous())
	{
		TArray<FGameplayAbilitySpecHandle> Handles;
		AbilitySet->GiveToAbilitySystem(AbilitySystemComponent, this, &Handles);

		// 按**能力类**把句柄分配到三个槽位，而不是按条目下标 ——
		// 下标约定太容易被"改资产时调了下顺序"破坏，而那种破坏是静默的（攻击悄悄换了种类）。
		//
		// 三个槽位是 AI 侧固有的：`Tick` 里的决策只有"接触 / 远程"两条，
		// 所以能力集里**多出来的**能力不会有 AI 去驱动（它仍可被别处激活）。
		// 也就是说这条边界是：**能力集决定"这种敌人有哪些攻击"，AI 决定"什么时候打"。**
		for (const FGameplayAbilitySpecHandle &Handle : Handles)
		{
			const FGameplayAbilitySpec *Spec = AbilitySystemComponent->FindAbilitySpecFromHandle(Handle);
			UClass *AbilityClass =
				(Spec != nullptr && Spec->Ability != nullptr) ? Spec->Ability->GetClass() : nullptr;
			if (AbilityClass == nullptr)
			{
				continue;
			}

			if (AbilityClass->IsChildOf(URogueEnemyContactAttackAbility::StaticClass()) &&
				!CurrentArchetype.Ranged.bUsesRangedAttack)
			{
				ContactAttackAbilityHandle = Handle;
			}
			else if (AbilityClass->IsChildOf(URogueEnemyRangedAttackAbility::StaticClass()))
			{
				RangedAttackAbilityHandle = Handle;
			}
		}

		UE_LOG(LogTemp, Verbose, TEXT("[Rogue] 敌人攻击能力来自数据驱动能力集 %s（%d 条）：接触=%d 远程=%d"),
			   *AbilitySet->GetName(), Handles.Num(), ContactAttackAbilityHandle.IsValid() ? 1 : 0,
			   RangedAttackAbilityHandle.IsValid() ? 1 : 0);
		return;
	}

	// ---- ② 回退：按原型标志推导（改造前的硬编码逻辑，老数据表走这条）----

	// 有远程攻击的原型不再授予接触能力；所有攻击都走远程 GAS 槽。
	if (!CurrentArchetype.Ranged.bUsesRangedAttack)
	{
		ContactAttackAbilityHandle = AbilitySystemComponent->GiveAbility(
			FGameplayAbilitySpec(URogueEnemyContactAttackAbility::StaticClass(), /*Level=*/1,
								 /*InputID=*/INDEX_NONE, /*SourceObject=*/this));
	}

	if (CurrentArchetype.Ranged.bUsesRangedAttack)
	{
		RangedAttackAbilityHandle = AbilitySystemComponent->GiveAbility(
			FGameplayAbilitySpec(URogueEnemyRangedAttackAbility::StaticClass(), 1, INDEX_NONE, this));
	}
}

bool ARogueEnemy::TryActivateAttackAbility(const FGameplayAbilitySpecHandle &Handle)
{
	if (AbilitySystemComponent == nullptr || !Handle.IsValid())
	{
		return false;
	}

	return AbilitySystemComponent->TryActivateAbility(Handle);
}

void ARogueEnemy::BackOffMissingAttack(float &Timer, const FGameplayAbilitySpecHandle &Handle)
{
	// 缺失句柄时至少退避 0.5s，避免满足距离条件后每 Tick 重试。
	// 有效句柄的激活失败可能由正常冷却/标签阻挡造成，这里不额外延长节拍。
	if (!Handle.IsValid())
	{
		Timer = FMath::Max(Timer, 0.5f);
	}
}

TSubclassOf<ARogueEnemyProjectile> ARogueEnemy::GetEnemyProjectileClass() const
{
	if (bIsBoss && BossProjectileClass != nullptr)
	{
		return BossProjectileClass;
	}

	// 原型数据表可以按敌人类型单独指定弹体；没有配置时再回退敌人 BP 的默认值。
	if (CurrentArchetype.ProjectileClass != nullptr)
	{
		return CurrentArchetype.ProjectileClass;
	}

	return EnemyProjectileClass != nullptr
			   ? EnemyProjectileClass
			   : TSubclassOf<ARogueEnemyProjectile>(ARogueEnemyProjectile::StaticClass());
}

TSubclassOf<ARogueEnemyProjectile> ARogueEnemy::GetBossProjectileClass() const
{
	return BossProjectileClass != nullptr ? BossProjectileClass : GetEnemyProjectileClass();
}

bool ARogueEnemy::HasTouchingPlayer() const
{
	TArray<ARogueCharacter *, TInlineAllocator<4>> Players;
	GatherAlivePlayers(Players);

	const UCapsuleComponent *MyCapsule = GetCapsuleComponent();
	const float MyRadius = MyCapsule != nullptr ? MyCapsule->GetScaledCapsuleRadius() : 34.0f;
	const FVector MyLocation = GetActorLocation();

	for (const ARogueCharacter *PlayerCharacter : Players)
	{
		const UCapsuleComponent *PlayerCapsule = PlayerCharacter->GetCapsuleComponent();
		const float PlayerRadius = PlayerCapsule != nullptr ? PlayerCapsule->GetScaledCapsuleRadius() : 34.0f;
		const float DamageDistance = MyRadius + PlayerRadius + 8.0f;
		if (FVector::DistSquared2D(MyLocation, PlayerCharacter->GetActorLocation()) <=
			FMath::Square(DamageDistance))
		{
			return true;
		}
	}

	return false;
}

void ARogueEnemy::DecideContactAttack(float DeltaSeconds)
{
    if (AreNormalAttacksSuppressed()) return;
	ContactTimer = FMath::Max(0.0f, ContactTimer - DeltaSeconds);
	if (ContactTimer > 0.0f)
	{
		return;
	}

	// 决策判据只有距离：没人在身上就什么都不做（改造前 TouchPlayer 也是先判距离，
	// 区别是它在同一个函数里接着把伤害结算了）。
	if (!HasTouchingPlayer())
	{
		return;
	}

	RogueEnemyAttackStats::CountContactAttempt();
	if (TryActivateAttackAbility(ContactAttackAbilityHandle))
	{
		ContactTimer = GetContactInterval();
	}
	else
	{
		BackOffMissingAttack(ContactTimer, ContactAttackAbilityHandle);
	}
}

void ARogueEnemy::DecideRangedAttack(ARogueCharacter *PlayerCharacter, float DeltaSeconds)
{
    if (AreNormalAttacksSuppressed()) return;
	if (!IsValid(PlayerCharacter))
	{
		return;
	}

	const FRogueEnemyRangedArchetype &RangedArchetype = CurrentArchetype.Ranged;
	if (!RangedArchetype.bUsesRangedAttack)
	{
		return;
	}

	RangedAttackTimer = FMath::Max(0.0f, RangedAttackTimer - DeltaSeconds);
	if (RangedAttackTimer > 0.0f)
	{
		return;
	}

	// 当前设计不再限制攻击距离：只要目标存活、冷却结束就尝试开火。
	// 原型数据里的 MinimumRange / MaximumRange / AttackRangeMinFactor 已删除，
	// 不再有数据字段可以恢复距离门控。
	RogueEnemyAttackStats::CountRangedAttempt();
	if (TryActivateAttackAbility(RangedAttackAbilityHandle))
	{
		RangedAttackTimer = RangedArchetype.NextShotCooldown;
	}
	else
	{
		BackOffMissingAttack(RangedAttackTimer, RangedAttackAbilityHandle);
	}
}

bool ARogueEnemy::ExecuteContactAttack()
{
    if (AreNormalAttacksSuppressed()) return false;
	// 接触伤害对**所有**贴身的玩家结算，而不是只对当前锁定的目标。
	// 单机时这与原来等价；联机时如果只看目标，
	// 敌人从队友身上直接穿过去是不掉血的（"我怎么没被打"的来源之一）。
	TArray<ARogueCharacter *, TInlineAllocator<4>> Players;
	GatherAlivePlayers(Players);

	const UCapsuleComponent *MyCapsule = GetCapsuleComponent();
	const float MyRadius = MyCapsule != nullptr ? MyCapsule->GetScaledCapsuleRadius() : 34.0f;
	const FVector MyLocation = GetActorLocation();
	bool bDamagedAnyone = false;

	for (ARogueCharacter *PlayerCharacter : Players)
	{
		const UCapsuleComponent *PlayerCapsule = PlayerCharacter->GetCapsuleComponent();
		const float PlayerRadius = PlayerCapsule != nullptr ? PlayerCapsule->GetScaledCapsuleRadius() : 34.0f;
		const float DamageDistance = MyRadius + PlayerRadius + 8.0f;
		if (FVector::DistSquared2D(MyLocation, PlayerCharacter->GetActorLocation()) >
			FMath::Square(DamageDistance))
		{
			continue;
		}

		// 接触伤害是"贴到玩家身上"造成的，Tag 用 Damage.Source.Contact
		RogueAbilitySystem::ApplyDamageToTarget(this, PlayerCharacter, ContactDamage,
												RogueGameplayTags::Damage_Source_Contact);
		bDamagedAnyone = true;
	}

	if (bDamagedAnyone)
	{
		NotifyAttackFeedback(MyLocation);
	}
	return bDamagedAnyone;
}

bool ARogueEnemy::ExecuteRangedAttack()
{
    if (AreNormalAttacksSuppressed()) return false;
	ARogueCharacter *PlayerCharacter = GetPlayerCharacter();
	if (!HasAuthority() || bDead || bPoolAvailable || !IsValid(PlayerCharacter) || GetWorld() == nullptr)
	{
		return false;
	}

	FireRangedShot(PlayerCharacter);
	return true;
}

void ARogueEnemy::PerformContactDamage(float DeltaSeconds)
{
	// 行为树 / 蓝图入口（`OnBehaviorTick` 被蓝图重写的敌人不走基类默认行为，需要它）。
	// 语义与改造前一致：决策 + **直接**结算。
	// 基类默认行为走的是另一条路（DecideContactAttack → 接触攻击能力），
	// 两者共用同一个结算函数 ExecuteContactAttack()，所以伤害口径只有一份。
	ContactTimer = FMath::Max(0.0f, ContactTimer - DeltaSeconds);
	if (ContactTimer > 0.0f)
	{
		return;
	}

	if (ExecuteContactAttack())
	{
		// 计时器是**整只怪共享**的：同时贴住两个玩家时两人一起挨打，
		// 而不是"第一个人吃掉这次攻击、第二个人白嫖"。
		ContactTimer = GetContactInterval();
	}
}

void ARogueEnemy::FireProjectileAtPlayer()
{
	// 行为树 / 蓝图入口：立即打一发（不看冷却），与改造前语义一致。
	ExecuteRangedAttack();
}

void ARogueEnemy::UpdateRangedAttackCooldown(float DeltaSeconds)
{
	RangedAttackTimer = FMath::Max(0.0f, RangedAttackTimer - DeltaSeconds);
}

void ARogueEnemy::ResetRangedAttackCooldown()
{
	RangedAttackTimer = CurrentArchetype.Ranged.NextShotCooldown;
}

void ARogueEnemy::SetRangedAttackCooldown(float NewCooldown)
{
	RangedAttackTimer = FMath::Max(0.0f, NewCooldown);
}

void ARogueEnemy::FireRangedShot(ARogueCharacter *PlayerCharacter)
{
    if (AreNormalAttacksSuppressed()) return;
	if (!IsValid(PlayerCharacter) || GetWorld() == nullptr)
	{
		return;
	}

	// 濒死的玩家不是目标：不开火、不刷弹体、也不播枪口火光。
	//
	// 为什么这条检查必须在这里（而不是只在 HandleDefaultBehavior 里）：
	// 本函数是"开火"的唯一出口，测试里出现过"玩家已经倒地、怪物仍然每秒朝他开一枪"
	// （伤害被 State.Dead 闸门挡下，所以只表现为一堆无意义弹体和特效）。
	// 把闸门放在出口上，无论上游是谁调用的都拦得住。
	if (!HasAuthority() || bDead || bPoolAvailable || !PlayerCharacter->IsAlive())
	{
		return;
	}

	if (RogueEnemyArchetypes::IsFlyingType(EnemyType))
	{
		StartFlyingAttack(PlayerCharacter);
		return;
	}
	const FRogueEnemyRangedArchetype &RangedArchetype = CurrentArchetype.Ranged;
	const TSubclassOf<ARogueEnemyProjectile> ProjectileClass = GetEnemyProjectileClass();
	if (RangedArchetype.ShotPattern == ERogueEnemyShotPattern::Skyfall)
	{
		StartSkyfallAttack(PlayerCharacter);
		return;
	}
	if (RangedArchetype.ShotPattern == ERogueEnemyShotPattern::RadialRing)
	{
		const FRogueEnemyBulletRingSettings Settings = RangedArchetype.BulletRing.Sanitized();
		const UCapsuleComponent *Capsule = GetCapsuleComponent();
		const FVector Center =
			GetActorLocation() +
			FVector(0.0f, 0.0f, -Capsule->GetScaledCapsuleHalfHeight() + Settings.HeightAboveGround);
		const float SpawnRadius = Capsule->GetScaledCapsuleRadius() + 24.0f;
		const float StartYaw = GetActorRotation().Yaw;
		const bool bSpawnedRing = SpawnBulletRing(Center, StartYaw, SpawnRadius, Settings,
												  ContactDamage * Settings.DamageMultiplier, ProjectileClass);
		Multicast_SpawnCosmeticBulletRing(Center, StartYaw, SpawnRadius, Settings, ProjectileClass);
		if (bSpawnedRing)
		{
			NotifyAttackFeedback(Center);
		}
		return;
	}

	const FVector ShotOrigin = GetActorLocation() + FVector(0.0f, 0.0f, bIsBoss ? 78.0f : 54.0f) +
							   GetActorForwardVector() * (bIsBoss ? 54.0f : 36.0f);
	const FVector TargetLocation = PlayerCharacter->GetActorLocation() + FVector(0.0f, 0.0f, 48.0f);
	const FVector ShotVector = TargetLocation - ShotOrigin;
	if (ShotVector.IsNearlyZero())
	{
		return;
	}

	const FVector ShotDirection = ShotVector.GetSafeNormal();
	const FVector RightVector = FVector::CrossProduct(ShotDirection, FVector::UpVector).GetSafeNormal();
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = this;
	SpawnParameters.Instigator = this;

	bool bSpawnedAny = false;
	auto SpawnProjectile = [&](const FVector &SpawnLocation, const FVector &Direction, float ProjectileSpeed,
							   float ProjectileDamage, bool bLargeVisual) {
		// 服务端生成"真实"弹体（带伤害与命中判定）
		URogueCombatPoolSubsystem *Pools =
			GetWorld() ? GetWorld()->GetSubsystem<URogueCombatPoolSubsystem>() : nullptr;
		ARogueEnemyProjectile *Projectile =
			Pools != nullptr ? Pools->AcquireEnemyProjectile(ProjectileClass, this, this, SpawnLocation,
															 Direction.Rotation())
							 : GetWorld()->SpawnActor<ARogueEnemyProjectile>(
								   ProjectileClass, SpawnLocation, Direction.Rotation(), SpawnParameters);
		if (Projectile != nullptr)
		{
			bSpawnedAny = true;
			Projectile->ActivatePooledProjectile(this, this, SpawnLocation, Direction.Rotation(), Direction,
												 ProjectileSpeed, ProjectileDamage, bLargeVisual);
		}

		// 同时广播给客户端，让它们在本地生成一发**纯表现**的同款弹体。
		// 注意不要在服务端也走这条：Multicast 的实现里用 HasAuthority() 提前返回了，
		// 所以这行调用在服务端只起到"发出去"的作用。
		Multicast_SpawnCosmeticProjectile(SpawnLocation, Direction, ProjectileSpeed, bLargeVisual,
										  ProjectileClass);
	};

	if (RangedArchetype.ShotPattern == ERogueEnemyShotPattern::Spread3)
	{
		const float CenterIndex = (static_cast<float>(RangedArchetype.SpreadShotCount) - 1.0f) * 0.5f;
		for (int32 ShotIndex = 0; ShotIndex < RangedArchetype.SpreadShotCount; ++ShotIndex)
		{
			const float OffsetIndex = static_cast<float>(ShotIndex) - CenterIndex;
			const float SpreadAngle = OffsetIndex * RangedArchetype.SpreadAngleStep;
			const FVector SpreadDirection =
				ShotDirection.RotateAngleAxis(SpreadAngle, FVector::UpVector).GetSafeNormal();
			const FVector SpreadSpawnLocation =
				ShotOrigin + RightVector * OffsetIndex * RangedArchetype.SpreadSpawnOffset;
			SpawnProjectile(SpreadSpawnLocation, SpreadDirection, RangedArchetype.ProjectileSpeed,
							ContactDamage * RangedArchetype.ProjectileDamageMultiplier,
							RangedArchetype.bLargeVisual);
		}
	}
	else
	{
		SpawnProjectile(ShotOrigin, ShotDirection, RangedArchetype.ProjectileSpeed,
						ContactDamage * RangedArchetype.ProjectileDamageMultiplier,
						RangedArchetype.bLargeVisual);
	}

	if (bSpawnedAny)
	{
		NotifyAttackFeedback(ShotOrigin);
	}

	if (RangedArchetype.bSpawnMuzzleEffect)
	{
		// 走 GameplayCue：AI 只在服务端跑，本机直接 SpawnImpactEffect 的话只有主机看得见枪口火光。
		// cue 由敌人自己的 ASC 多播出去（敌人 ASC 是 Minimal 模式，而 Minimal 天生复制 cue）。
		ARogueGameState::PlayImpactEffect(this, ShotOrigin, ShotDirection.Rotation(),
										  ERogueImpactVisualStyle::Spark, RangedArchetype.MuzzleEffectScale);
	}
}

void ARogueEnemy::Multicast_SpawnCosmeticProjectile_Implementation(
	FVector_NetQuantize SpawnLocation, FVector_NetQuantizeNormal Direction, float Speed, bool bLargeVisual,
	TSubclassOf<ARogueEnemyProjectile> ProjectileClass)
{
	// 服务端调用的多播会在服务端及收到此 RPC 的客户端执行；不保证全员收到。
	// 此实现跳过权威端，避免再生成一份已由 FireRangedShot 创建的弹体。
	if (HasAuthority())
	{
		return;
	}

	UWorld *World = GetWorld();
	URogueCombatPoolSubsystem *Pools =
		World != nullptr ? World->GetSubsystem<URogueCombatPoolSubsystem>() : nullptr;
	if (Pools == nullptr)
	{
		return;
	}

	const FRotator SpawnRotation = Direction.Rotation();
	ARogueEnemyProjectile *Projectile =
		Pools->AcquireEnemyProjectile(ProjectileClass, this, this, SpawnLocation, SpawnRotation);
	if (Projectile == nullptr)
	{
		return;
	}

	// 客户端副本伤害为 0，只有命中表现；服务端权威弹体在后续命中时结算。
	Projectile->ActivatePooledProjectile(this, this, SpawnLocation, SpawnRotation, Direction, Speed,
										 /*ProjectileDamage=*/0.0f, bLargeVisual);
}

void ARogueEnemy::Multicast_SpawnCosmeticBulletRing_Implementation(
	FVector_NetQuantize Center, float StartYaw, float SpawnRadius,
	const FRogueEnemyBulletRingSettings &Settings, TSubclassOf<ARogueEnemyProjectile> ProjectileClass)
{
	if (!HasAuthority())
	{
		SpawnBulletRing(Center, StartYaw, SpawnRadius, Settings, 0.0f, ProjectileClass);
	}
}

bool ARogueEnemy::SpawnBulletRing(const FVector &Center, float StartYaw, float SpawnRadius,
								  const FRogueEnemyBulletRingSettings &Settings, float ProjectileDamage,
								  TSubclassOf<ARogueEnemyProjectile> ProjectileClass)
{
	UWorld *World = GetWorld();
	if (World == nullptr)
	{
		return false;
	}
	bool bSpawnedAny = false;
	URogueCombatPoolSubsystem *Pools = World->GetSubsystem<URogueCombatPoolSubsystem>();
	const FRogueEnemyBulletRingSettings Ring = Settings.Sanitized();
	const TSubclassOf<ARogueEnemyProjectile> ClassToSpawn =
		ProjectileClass != nullptr ? ProjectileClass : GetEnemyProjectileClass();
	for (int32 Index = 0; Index < Ring.ProjectileCount; ++Index)
	{
		// 不包含 360 度终点，避免第一发与最后一发重合；每个方向只发一颗。
		const float Yaw = StartYaw + 360.0f * static_cast<float>(Index) / Ring.ProjectileCount;
		const FRotator Rotation(0.0f, Yaw, 0.0f);
		const FVector Direction = Rotation.Vector();
		const FVector Location = Center + Direction * SpawnRadius;
		FActorSpawnParameters Params;
		Params.Owner = this;
		Params.Instigator = this;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ARogueEnemyProjectile *Projectile =
			Pools != nullptr
				? Pools->AcquireEnemyProjectile(ClassToSpawn, this, this, Location, Rotation)
				: World->SpawnActor<ARogueEnemyProjectile>(ClassToSpawn, Location, Rotation, Params);
		if (Projectile != nullptr)
		{
			bSpawnedAny = true;
			Projectile->ActivatePooledProjectile(this, this, Location, Rotation, Direction,
												 Ring.ProjectileSpeed, ProjectileDamage, false,
												 Ring.ProjectileLifetime, Ring.ProjectileScale, true);
		}
	}
	return bSpawnedAny;
}
