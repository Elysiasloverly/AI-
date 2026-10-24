#include "Enemies/RogueEnemy.h"
#include "Combat/Visuals/RogueHitFlashComponent.h"

#include "Player/RogueCharacter.h"
#include "AbilitySystem/RogueAbilitySystemComponent.h"
#include "AbilitySystem/RogueAbilitySet.h"
#include "AbilitySystem/RogueAbilitySystemLibrary.h"
#include "AbilitySystem/RogueAttributeSet.h"
#include "AbilitySystem/Abilities/RogueEnemyAttackAbility.h"
#include "AbilitySystem/RogueGameplayTags.h"
#include "Debug/RogueSelfTest.h"
#include "Combat/Visuals/RogueCombatFeedback.h"
#include "Enemies/Data/RogueEnemyArchetypes.h"
#include "Combat/Projectiles/RogueEnemyProjectile.h"
#include "Framework/RogueGameMode.h"
#include "Framework/RogueGameState.h"
#include "Player/RoguePlayerQuery.h"
#include "Combat/Pooling/RogueCombatPoolSubsystem.h"
#include "Combat/Targeting/RogueEnemyTrackerSubsystem.h"
#include "Combat/Visuals/RogueImpactEffect.h"
#include "World/RogueEnemySpawnRegion.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/MeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "AIController.h"
#include "UObject/ConstructorHelpers.h"

// ===========================================================
//  构造 & 生命周期
// ===========================================================

ARogueEnemy::ARogueEnemy()
{
	PrimaryActorTick.bCanEverTick = true;
	HitFlashComponent = CreateDefaultSubobject<URogueHitFlashComponent>(TEXT("HitFlash"));

	// 原生默认使用 AAIController 并允许自动接管；蓝图默认值仍可覆盖。
	AIControllerClass = AAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	GetCapsuleComponent()->InitCapsuleSize(34.0f, 72.0f);

	// 忽略 Pawn 可避免敌群胶囊互相阻挡，也允许玩家穿过敌人。
	// 当前弹体/刀片为 WorldDynamic，并对 Pawn 请求重叠；最终结果仍由双方通道响应决定。
	// 接触攻击使用二维距离判断，不依赖敌人与玩家的阻挡事件。
	GetCapsuleComponent()->SetCollisionResponseToChannel(
		ECC_Pawn, bIgnoreOtherPawnCollision ? ECR_Ignore : ECR_Block);

	// ── GAS 组件 ──
	// ASC 挂在敌人自己身上：敌人没有 PlayerState 那一层，也不需要跨 Pawn 重生保留状态。
	// 下面显式调用 ConfigureAsEnemy 选择 Minimal 复制模式。
	AbilitySystemComponent = CreateDefaultSubobject<URogueAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->ConfigureAsEnemy();

	// 与玩家共用同一个属性集类型（只用到其中血量相关的几项）
	AttributeSet = CreateDefaultSubobject<URogueAttributeSet>(TEXT("AttributeSet"));

	// 这里选择已注册子对象列表复制 AttributeSet；传统 ReplicateSubobjects 路径也可实现子对象复制
	bReplicateUsingRegisteredSubObjectList = true;

	// 明确本类需要复制。UE 5.5 的 APawn 构造已将 bReplicates 设为 true，
	// ACharacter 继承该默认；这里重复声明意图，蓝图默认值仍可覆盖。
	bReplicates = true;
	SetReplicatingMovement(true);

	// 设置初始复制频率预算，运行时再按目标距离分档；实际发送还受相关性与带宽影响。
	// UE 5.5 废弃的是 NetUpdateFrequency 公有成员访问，使用 setter/getter，
	// 该复制频率机制本身并未废弃。
	SetNetUpdateFrequency(10.0f);
	SetMinNetUpdateFrequency(2.0f);

	// 优先级低于玩家（默认 1.0）：带宽紧张时先保玩家与弹体这类手感相关的 actor
	NetPriority = 0.6f;

	// 设置距离相关性阈值为 15000uu 的平方；默认单位下约 150m。
	// 它是相关性规则的一部分，拥有者/可见性等其他条件仍可影响结果。
	SetNetCullDistanceSquared(FMath::Square(15000.0f));

	// 敌人为地面角色；离开坡面或台阶后必须能自然落回地面。
	GetCharacterMovement()->GravityScale = 1.0f;
	GetCharacterMovement()->bEnablePhysicsInteraction = false;
	GetCharacterMovement()->MaxWalkSpeed = MoveSpeed;
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 600.0f, 0.0f);

	ShockPillarMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ShockPillarMesh"));
	ShockPillarMesh->SetupAttachment(RootComponent);
	ShockPillarMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ShockPillarMesh->SetCastShadow(true);
	ShockPillarMesh->SetVisibility(false, true);
	ShockPillarMesh->SetHiddenInGame(true);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PillarMeshAsset(TEXT("/Game/GameAssets/StarterContent/Shapes/Shape_Cylinder.Shape_Cylinder"));
	if (PillarMeshAsset.Succeeded())
	{
		ShockPillarMesh->SetStaticMesh(PillarMeshAsset.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> PillarMaterialAsset(TEXT("/Game/GameAssets/Realistic_Starter_VFX_Pack_Vol2/Materials/M_Glow_D.M_Glow_D"));
	if (PillarMaterialAsset.Succeeded())
	{
		ShockPillarMaterial = PillarMaterialAsset.Object;
	}

	// 弹体类完全由蓝图/数据表决定，C++ 不写死任何默认资产路径：
	//   · 敌人 BP 类默认值 → EnemyProjectileClass / BossProjectileClass
	//   · 原型数据表 → DT_EnemyArchetypes 的「弹体蓝图类」列
	// 两者都为空时 GetEnemyProjectileClass() 回退原生类（仅碰撞/伤害，不含外观）。

	OrbitDirection = FMath::RandBool() ? 1.0f : -1.0f;
}

void ARogueEnemy::ConfigureGroundedCollision()
{
	// 蓝图 SCS 模型可能默认 BlockAllDynamic。只修改外观组件，不影响胶囊和独立攻击触发器。
	TInlineComponentArray<UMeshComponent*> Meshes(this);
	for (UMeshComponent* CosmeticMesh : Meshes)
	{
		CosmeticMesh->SetSimulatePhysics(false);
		CosmeticMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		CosmeticMesh->SetGenerateOverlapEvents(false);
		CosmeticMesh->CanCharacterStepUpOn = ECB_No;
		CosmeticMesh->SetCanEverAffectNavigation(false);
	}
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		// 旧蓝图序列化了 0，单改 C++ 构造默认值不能修正它；保留有效的正重力覆盖值。
		if (!FMath::IsFinite(Movement->GravityScale) || Movement->GravityScale <= 0.0f)
		{
			Movement->GravityScale = 1.0f;
		}
		Movement->bEnablePhysicsInteraction = false;
	}
}

void ARogueEnemy::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	// 此时蓝图组件已经创建，在第一次物理更新前关闭配饰碰撞。
	ConfigureGroundedCollision();
}

void ARogueEnemy::BeginPlay()
{
	Super::BeginPlay();
	ConfigureGroundedCollision();

	// GAS 初始化。敌人既是 OwnerActor 也是 AvatarActor（没有 PlayerState 那一层）。
	// 必须在 BeginPlay 做而不是构造里：构造阶段 World / 复制都还没就绪。
	if (AbilitySystemComponent != nullptr)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this);
	}

	// 当前采用已注册子对象列表，由服务端在 ASC 上登记属性集供复制。
	// 客户端已有构造出的属性集，后续接受复制值。
	if (HasAuthority() && AbilitySystemComponent != nullptr && AttributeSet != nullptr)
	{
		AbilitySystemComponent->AddReplicatedSubObject(AttributeSet);
	}

	// 订阅"血量变化"与"死亡标签"这两条 GAS 事件。
	// 这是迁移的关键形态变化：原来血量是敌人自己减、死亡是自己判断；
	// 现在 AttributeSet 是真相源，敌人退化成**观察者**，只同步镜像并做善后。
	if (AbilitySystemComponent != nullptr)
	{
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(URogueAttributeSet::GetHealthAttribute())
			.AddUObject(this, &ARogueEnemy::HandleHealthChanged);

		AbilitySystemComponent->RegisterGameplayTagEvent(RogueGameplayTags::State_Dead, EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &ARogueEnemy::HandleDeathTagChanged);
	}

	// 出生/出池时还没有位置可言（池里的怪停在上一次死亡的地方），
	// 这里不选目标，只把节流清零，让第一次 Tick 立刻选一次。
	CachedPlayerCharacter = nullptr;
	TargetRefreshCooldown = 0.0f;

	// 权威端初始化为回池；客户端通过复制的在场标记应用本地状态，
	// 避免在 BeginPlay 用回池逻辑覆盖已收到的活动状态。
	if (HasAuthority())
	{
		DeactivateToPool();
	}

	// BeginPlay 同样应用当前在场值，避免仅依赖初始 RepNotify；
	// 初始值与默认相同等情况并不保证产生一次通知。
	ApplyReplicatedActiveState();
}

UAbilitySystemComponent* ARogueEnemy::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void ARogueEnemy::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// "这只怪现在在不在场"。池化状态本身不复制（见头文件里的说明），
	// 但客户端必须知道它，否则怪在客户端永远是"死的、池里的"。
	DOREPLIFETIME_CONDITION(ARogueEnemy, bReplicatedActive, COND_None);
	DOREPLIFETIME(ARogueEnemy, HitFeedback);
}

void ARogueEnemy::OnRep_ReplicatedActive()
{
	ApplyReplicatedActiveState();
}

void ARogueEnemy::ApplyReplicatedActiveState()
{
	ConfigureGroundedCollision();
	// 只在客户端做：服务端的状态由 ActivatePooledEnemy / DeactivateToPool 直接维护，
	// 这里再写一遍反而可能覆盖掉服务端刚设好的值。
	if (HasAuthority())
	{
		return;
	}

	if (bReplicatedActive)
	{
		// 在场：把"池里 + 已死"这套本地状态翻回来。
		bPoolAvailable = false;
		bDead = false;
		// 显隐也按当前在场位应用，避免本地隐藏状态与活动状态不一致。
		SetActorHiddenInGame(false);
		SetActorTickEnabled(true);

		// 客户端本地索敌与血条遍历依赖此 World 的追踪器，服务端注册不会复制该容器。
		if (URogueEnemyTrackerSubsystem* Tracker = GetWorld() != nullptr ? GetWorld()->GetSubsystem<URogueEnemyTrackerSubsystem>() : nullptr)
		{
			Tracker->RegisterEnemy(this);
		}

		// 客户端只需要能被查询到（本地弹体靠 overlap/trace 判断命中），
		// 不需要物理模拟 —— QueryOnly 比 QueryAndPhysics 便宜。
		SetActorEnableCollision(true);
		if (UCapsuleComponent* Capsule = GetCapsuleComponent())
		{
			Capsule->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
			Capsule->SetCollisionResponseToChannel(
				ECC_Pawn, bIgnoreOtherPawnCollision ? ECR_Ignore : ECR_Block);
		}
	}
	else
	{
		if (HitFlashComponent) { HitFlashComponent->ClearHitFlash(); }
		bPoolAvailable = true;
		bDead = true;
		SetActorHiddenInGame(true);
		SetActorTickEnabled(false);
		SetActorEnableCollision(false);
		if (UCapsuleComponent* Capsule = GetCapsuleComponent())
		{
			Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}

		// 同理：离场就从本机追踪器里摘掉，否则客户端的索敌会一直盯着一只不可见的怪
		if (URogueEnemyTrackerSubsystem* Tracker = GetWorld() != nullptr ? GetWorld()->GetSubsystem<URogueEnemyTrackerSubsystem>() : nullptr)
		{
			Tracker->UnregisterEnemy(this);
		}
	}
	ConsumeReplicatedHitFeedback();
}

void ARogueEnemy::Multicast_SpawnCosmeticProjectile_Implementation(FVector_NetQuantize SpawnLocation, FVector_NetQuantizeNormal Direction, float Speed, bool bLargeVisual, TSubclassOf<ARogueEnemyProjectile> ProjectileClass)
{
	// 服务端调用的多播会在服务端及收到此 RPC 的客户端执行；不保证全员收到。
	// 此实现跳过权威端，避免再生成一份已由 FireRangedShot 创建的弹体。
	if (HasAuthority())
	{
		return;
	}

	UWorld* World = GetWorld();
	URogueCombatPoolSubsystem* Pools = World != nullptr ? World->GetSubsystem<URogueCombatPoolSubsystem>() : nullptr;
	if (Pools == nullptr)
	{
		return;
	}

	const FRotator SpawnRotation = Direction.Rotation();
	ARogueEnemyProjectile* Projectile = Pools->AcquireEnemyProjectile(
		ProjectileClass, this, this, SpawnLocation, SpawnRotation);
	if (Projectile == nullptr)
	{
		return;
	}

	// 客户端副本伤害为 0，只有命中表现；服务端权威弹体在后续命中时结算。
	Projectile->ActivatePooledProjectile(this, this, SpawnLocation, SpawnRotation, Direction, Speed, /*ProjectileDamage=*/0.0f, bLargeVisual);
}

void ARogueEnemy::Multicast_SpawnCosmeticBulletRing_Implementation(FVector_NetQuantize Center, float StartYaw, float SpawnRadius, const FRogueEnemyBulletRingSettings& Settings, TSubclassOf<ARogueEnemyProjectile> ProjectileClass)
{
	if (!HasAuthority())
	{
		SpawnBulletRing(Center, StartYaw, SpawnRadius, Settings, 0.0f, ProjectileClass);
	}
}

bool ARogueEnemy::SpawnBulletRing(const FVector& Center, float StartYaw, float SpawnRadius, const FRogueEnemyBulletRingSettings& Settings, float ProjectileDamage, TSubclassOf<ARogueEnemyProjectile> ProjectileClass)
{
	UWorld* World = GetWorld();
	if (World == nullptr) { return false; }
	bool bSpawnedAny = false;
	URogueCombatPoolSubsystem* Pools = World->GetSubsystem<URogueCombatPoolSubsystem>();
	const FRogueEnemyBulletRingSettings Ring = Settings.Sanitized();
	const TSubclassOf<ARogueEnemyProjectile> ClassToSpawn = ProjectileClass != nullptr ? ProjectileClass : GetEnemyProjectileClass();
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
		ARogueEnemyProjectile* Projectile = Pools != nullptr
			? Pools->AcquireEnemyProjectile(ClassToSpawn, this, this, Location, Rotation)
			: World->SpawnActor<ARogueEnemyProjectile>(ClassToSpawn, Location, Rotation, Params);
		if (Projectile != nullptr)
		{
			bSpawnedAny = true;
			Projectile->ActivatePooledProjectile(this, this, Location, Rotation, Direction,
				Ring.ProjectileSpeed, ProjectileDamage, false, Ring.ProjectileLifetime, Ring.ProjectileScale, true);
		}
	}
	return bSpawnedAny;
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

void ARogueEnemy::UpdateReplicationBudget(float DistanceToPlayer)
{
	// 只有服务端会向客户端发数据；客户端上这个函数是空操作。
	// 用 HasAuthority() 而不是 GetNetMode()：与 GAS 侧保持同一套判定口径。
	if (!HasAuthority())
	{
		return;
	}

	// 分档依据：近处玩家盯着看（血条、被击反馈），远处只需要"大概在那里"。
	// 阈值与上面的逻辑 Tick 降频阈值对齐（950 / 1700），避免两套阈值各自漂移。
	float DesiredFrequency = 5.0f;
	if (DistanceToPlayer <= 950.0f)
	{
		DesiredFrequency = 30.0f;
	}
	else if (DistanceToPlayer <= 1700.0f)
	{
		DesiredFrequency = 15.0f;
	}

	// 只在跨档时修改，避免不必要的重复 setter 调用。
	if (!FMath::IsNearlyEqual(GetNetUpdateFrequency(), DesiredFrequency, 0.5f))
	{
		SetNetUpdateFrequency(DesiredFrequency);
	}
}

void ARogueEnemy::HandleHealthChanged(const FOnAttributeChangeData& ChangeData)
{
	// 真相源反转：AttributeSet 是权威，CurrentHealth 只是给 HUD / 追踪器 / 奖励结算读的镜像。
	// （迁移前是反过来的：CurrentHealth 是权威，属性集根本没人用。）
	CurrentHealth = ChangeData.NewValue;

	// 初始化写入不算受伤
	if (bSuppressDamageFeedback)
	{
		return;
	}

	const float DamageTaken = ChangeData.OldValue - ChangeData.NewValue;
	if (DamageTaken > 0.0f)
	{
		ShowDamageNumber(DamageTaken);
		RecordHitFeedback(DamageTaken);
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

	// SetTimerForNextTick 将死亡处理移出施加死亡 GE 的当前调用栈，
	// 避免回池清除 GE 时在其标签通知里重入；执行时刻取决于 TimerManager 调度。
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(this, &ARogueEnemy::ProcessDeath);
	}
}

void ARogueEnemy::ProcessDeath()
{
	bPendingDeath = false;

	if (bDead || bPoolAvailable)
	{
		return;
	}

	// 击杀者由属性集记录（死亡 GE 本身不携带这个信息）
	AActor* Killer = AttributeSet != nullptr ? AttributeSet->GetLastDamageCauser() : nullptr;
	Die(Killer);
}

void ARogueEnemy::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HitFlashComponent) { HitFlashComponent->ClearHitFlash(); }
	if (UWorld* World = GetWorld())
	{
		if (URogueEnemyTrackerSubsystem* Tracker = World->GetSubsystem<URogueEnemyTrackerSubsystem>())
		{
			Tracker->UnregisterEnemy(this);
		}
	}

	Super::EndPlay(EndPlayReason);
}

// ===========================================================
//  Tick
// ===========================================================

void ARogueEnemy::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bDead || bPoolAvailable)
	{
		return;
	}

	// ── 客户端分支：只做表现，不跑任何 AI ──
	// 客户端上的敌人是**复制代理**：位置、朝向、血量由服务端同步过来。
	// 在这里再跑一遍 AI 有三个害处：
	//   1) 本地算出来的位移会和复制数据打架，表现为抖动/回弹；
	//   2) 白白消耗客户端 CPU —— 200+ 敌人的情况非常明显；
	//   3) 攻击/伤害类逻辑在客户端跑没有意义（伤害只在服务端结算）。
	if (!HasAuthority())
	{
		// 客户端从已复制的 Health 更新镜像，并以差值生成飘字。
		// 标准 GAS RepNotify 可经 GAMEPLAYATTRIBUTE_REPNOTIFY 通知变化，
		// 不能泛称属性复制不会触发回调；此轮询只是当前实现的补偿路径。
		// 复制可能合并多次变化，表现差值不等同于逐次权威伤害事件。
		if (AttributeSet != nullptr)
		{
			const float ReplicatedHealth = AttributeSet->GetHealth();
			if (ReplicatedHealth < CurrentHealth - KINDA_SMALL_NUMBER && !bSuppressDamageFeedback)
			{
				ShowDamageNumber(CurrentHealth - ReplicatedHealth);
			}
			CurrentHealth = ReplicatedHealth;
		}

		return;
	}

	// 目标重选必须在蓝图行为与基类默认行为之前做：
	// 它们全部依赖 GetPlayerCharacter() 拿到的那个目标。
	TargetRefreshCooldown -= DeltaSeconds;
	if (TargetRefreshCooldown <= 0.0f || GetPlayerCharacter() == nullptr)
	{
		TargetRefreshCooldown = TargetRefreshInterval;
		RefreshTargetPlayer();
	}

	// 尝试蓝图行为覆盖
	if (OnBehaviorTick(DeltaSeconds))
	{
		return;
	}

	// 基类默认行为
	HandleDefaultBehavior(DeltaSeconds);
}

void ARogueEnemy::HandleDefaultBehavior(float DeltaSeconds)
{
	// 目标已由 Tick 里的 RefreshTargetPlayer() 选好（活着的玩家里最近的那个）。
	ARogueCharacter* PlayerCharacter = GetPlayerCharacter();
	if (!IsValid(PlayerCharacter) || !PlayerCharacter->IsAlive())
	{
		return;
	}

	const FVector ToPlayer = PlayerCharacter->GetActorLocation() - GetActorLocation();
	const float DistanceToPlayer = ToPlayer.Size2D();

	// 按距离分档调整**网络**复制频率（与上面的逻辑 Tick 降频是两件事：
	// 逻辑频率决定"多久想一次"，复制频率决定"多久告诉客户端一次"）。
	UpdateReplicationBudget(DistanceToPlayer);

	const float DesiredTickInterval = bIsBoss
		? 0.0f
		: (DistanceToPlayer > 1700.0f ? 0.08f : (DistanceToPlayer > 950.0f ? 0.04f : 0.0f));
	if (!FMath::IsNearlyEqual(PrimaryActorTick.TickInterval, DesiredTickInterval, KINDA_SMALL_NUMBER))
	{
		SetActorTickInterval(DesiredTickInterval);
	}

	const FVector MoveDirection = GetMovementDirection(ToPlayer, DistanceToPlayer, DeltaSeconds);

	if (!MoveDirection.IsNearlyZero())
	{
		const FRotator DesiredRotation = MoveDirection.Rotation();
		SetActorRotation(FMath::RInterpTo(GetActorRotation(), DesiredRotation, DeltaSeconds, 8.0f));
		AddActorWorldOffset(MoveDirection * MoveSpeed * DeltaSeconds, true);
		// 手动平移绕过了 CharacterMovement 的位移，不能继续沿用旧位置的地面缓存。
		GetCharacterMovement()->bForceNextFloorCheck = true;
	}

	// 远程原型不再贴身攻击：接触槽只在没有远程攻击时启用。
	if (!CurrentArchetype.Ranged.bUsesRangedAttack)
	{
		DecideContactAttack(DeltaSeconds);
	}
	DecideRangedAttack(PlayerCharacter, DeltaSeconds);
}

// ===========================================================
//  攻击能力：授予 / 激活
// ===========================================================

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
	if (URogueAbilitySet* AbilitySet = CurrentArchetype.AttackAbilitySet.LoadSynchronous())
	{
		TArray<FGameplayAbilitySpecHandle> Handles;
		AbilitySet->GiveToAbilitySystem(AbilitySystemComponent, this, &Handles);

		// 按**能力类**把句柄分配到三个槽位，而不是按条目下标 ——
		// 下标约定太容易被"改资产时调了下顺序"破坏，而那种破坏是静默的（攻击悄悄换了种类）。
		//
		// 三个槽位是 AI 侧固有的：`Tick` 里的决策只有"接触 / 远程"两条，
		// 所以能力集里**多出来的**能力不会有 AI 去驱动（它仍可被别处激活）。
		// 也就是说这条边界是：**能力集决定"这种敌人有哪些攻击"，AI 决定"什么时候打"。**
		for (const FGameplayAbilitySpecHandle& Handle : Handles)
		{
			const FGameplayAbilitySpec* Spec = AbilitySystemComponent->FindAbilitySpecFromHandle(Handle);
			UClass* AbilityClass = (Spec != nullptr && Spec->Ability != nullptr) ? Spec->Ability->GetClass() : nullptr;
			if (AbilityClass == nullptr)
			{
				continue;
			}

			if (AbilityClass->IsChildOf(URogueEnemyContactAttackAbility::StaticClass())
				&& !CurrentArchetype.Ranged.bUsesRangedAttack)
			{
				ContactAttackAbilityHandle = Handle;
			}
			else if (AbilityClass->IsChildOf(URogueEnemyRangedAttackAbility::StaticClass()))
			{
				RangedAttackAbilityHandle = Handle;
			}
		}

		UE_LOG(LogTemp, Verbose, TEXT("[Rogue] 敌人攻击能力来自数据驱动能力集 %s（%d 条）：接触=%d 远程=%d"),
			*AbilitySet->GetName(), Handles.Num(),
			ContactAttackAbilityHandle.IsValid() ? 1 : 0,
			RangedAttackAbilityHandle.IsValid() ? 1 : 0);
		return;
	}

	// ---- ② 回退：按原型标志推导（改造前的硬编码逻辑，老数据表走这条）----

	// 有远程攻击的原型不再授予接触能力；所有攻击都走远程 GAS 槽。
	if (!CurrentArchetype.Ranged.bUsesRangedAttack)
	{
		ContactAttackAbilityHandle = AbilitySystemComponent->GiveAbility(
			FGameplayAbilitySpec(URogueEnemyContactAttackAbility::StaticClass(), /*Level=*/1, /*InputID=*/INDEX_NONE, /*SourceObject=*/this));
	}

	if (CurrentArchetype.Ranged.bUsesRangedAttack)
	{
		RangedAttackAbilityHandle = AbilitySystemComponent->GiveAbility(
			FGameplayAbilitySpec(URogueEnemyRangedAttackAbility::StaticClass(), 1, INDEX_NONE, this));
	}
}

bool ARogueEnemy::TryActivateAttackAbility(const FGameplayAbilitySpecHandle& Handle)
{
	if (AbilitySystemComponent == nullptr || !Handle.IsValid())
	{
		return false;
	}

	return AbilitySystemComponent->TryActivateAbility(Handle);
}

void ARogueEnemy::BackOffMissingAttack(float& Timer, const FGameplayAbilitySpecHandle& Handle)
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
	TArray<ARogueCharacter*, TInlineAllocator<4>> Players;
	GatherAlivePlayers(Players);

	const UCapsuleComponent* MyCapsule = GetCapsuleComponent();
	const float MyRadius = MyCapsule != nullptr ? MyCapsule->GetScaledCapsuleRadius() : 34.0f;
	const FVector MyLocation = GetActorLocation();

	for (const ARogueCharacter* PlayerCharacter : Players)
	{
		const UCapsuleComponent* PlayerCapsule = PlayerCharacter->GetCapsuleComponent();
		const float PlayerRadius = PlayerCapsule != nullptr ? PlayerCapsule->GetScaledCapsuleRadius() : 34.0f;
		const float DamageDistance = MyRadius + PlayerRadius + 8.0f;
		if (FVector::DistSquared2D(MyLocation, PlayerCharacter->GetActorLocation()) <= FMath::Square(DamageDistance))
		{
			return true;
		}
	}

	return false;
}

// ===========================================================
//  攻击决策（AI 侧）
// ===========================================================

void ARogueEnemy::DecideContactAttack(float DeltaSeconds)
{
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

void ARogueEnemy::DecideRangedAttack(ARogueCharacter* PlayerCharacter, float DeltaSeconds)
{
	if (!IsValid(PlayerCharacter))
	{
		return;
	}

	const FRogueEnemyRangedArchetype& RangedArchetype = CurrentArchetype.Ranged;
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

// ===========================================================
//  攻击执行（能力侧调用）
// ===========================================================

bool ARogueEnemy::ExecuteContactAttack()
{
	// 接触伤害对**所有**贴身的玩家结算，而不是只对当前锁定的目标。
	// 单机时这与原来等价；联机时如果只看目标，
	// 敌人从队友身上直接穿过去是不掉血的（"我怎么没被打"的来源之一）。
	TArray<ARogueCharacter*, TInlineAllocator<4>> Players;
	GatherAlivePlayers(Players);

	const UCapsuleComponent* MyCapsule = GetCapsuleComponent();
	const float MyRadius = MyCapsule != nullptr ? MyCapsule->GetScaledCapsuleRadius() : 34.0f;
	const FVector MyLocation = GetActorLocation();
	bool bDamagedAnyone = false;

	for (ARogueCharacter* PlayerCharacter : Players)
	{
		const UCapsuleComponent* PlayerCapsule = PlayerCharacter->GetCapsuleComponent();
		const float PlayerRadius = PlayerCapsule != nullptr ? PlayerCapsule->GetScaledCapsuleRadius() : 34.0f;
		const float DamageDistance = MyRadius + PlayerRadius + 8.0f;
		if (FVector::DistSquared2D(MyLocation, PlayerCharacter->GetActorLocation()) > FMath::Square(DamageDistance))
		{
			continue;
		}

		// 接触伤害是"贴到玩家身上"造成的，Tag 用 Damage.Source.Contact
		RogueAbilitySystem::ApplyDamageToTarget(this, PlayerCharacter, ContactDamage, RogueGameplayTags::Damage_Source_Contact);
		bDamagedAnyone = true;
	}

	if (bDamagedAnyone) { NotifyAttackAudio(MyLocation); }
	return bDamagedAnyone;
}

bool ARogueEnemy::ExecuteRangedAttack()
{
	ARogueCharacter* PlayerCharacter = GetPlayerCharacter();
	if (!HasAuthority() || bDead || bPoolAvailable || !IsValid(PlayerCharacter) || GetWorld() == nullptr)
	{
		return false;
	}

	FireRangedShot(PlayerCharacter);
	return true;
}

// ===========================================================
//  生命周期：初始化 / 激活 / 回收
// ===========================================================

void ARogueEnemy::InitializeEnemy(const FRogueEnemyProfile& InProfile)
{
	AttackAudioState.Reset();
	if (HitFlashComponent) { HitFlashComponent->ClearHitFlash(); }
	EnemyType = InProfile.EnemyType;
	MaxHealth = InProfile.Health;
	CurrentHealth = MaxHealth;
	MoveSpeed = InProfile.Speed;
	ContactDamage = InProfile.Damage;
	ExperienceReward = InProfile.ExperienceReward;
	bIsBoss = InProfile.bIsBoss;

	// 优先从 DataTable 读取原型配置，未配置则回退到硬编码
	if (EnemyArchetypeDataTable != nullptr)
	{
		CurrentArchetype = RogueEnemyArchetypes::BuildEnemyArchetypeFromDataTable(EnemyArchetypeDataTable, EnemyType, bIsBoss);
	}
	else
	{
		CurrentArchetype = RogueEnemyArchetypes::BuildEnemyArchetype(EnemyType, bIsBoss);
	}

    // 每次出池都从原始原型重建，再应用阶段倍率，避免回池复用时重复减半。
    const ARogueGameMode* Mode = GetWorld() ? GetWorld()->GetAuthGameMode<ARogueGameMode>() : nullptr;
    AttackSpeedMultiplier = Mode ? FMath::Max(1.f, Mode->GetEnemyAttackSpeedMultiplier()) : 1.f;
    CurrentArchetype.Ranged.InitialCooldownMin /= AttackSpeedMultiplier;
    CurrentArchetype.Ranged.InitialCooldownMax /= AttackSpeedMultiplier;
    CurrentArchetype.Ranged.NextShotCooldown /= AttackSpeedMultiplier;
	BurstCooldown = CurrentArchetype.Movement.Model == ERogueEnemyMovementModel::BurstCharge ? FMath::FRandRange(0.8f, 1.8f) : 0.0f;
	BurstTimeRemaining = 0.0f;
	ContactTimer = 0.0f;
	RangedAttackTimer = CurrentArchetype.Ranged.bUsesRangedAttack
		? FMath::FRandRange(CurrentArchetype.Ranged.InitialCooldownMin, CurrentArchetype.Ranged.InitialCooldownMax)
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

	// 把这一只的血量上限/当前值写进 GAS 属性。
	// 池复用时 AttributeSet 是同一个对象，必须在这里重写，
	// 否则下一只怪会带着上一只的血量上限（这也是 P2-25 类问题的同类隐患）。
	SyncHealthToAbilitySystem();

	// 通知蓝图子类初始化完成
	OnEnemyInitialized(InProfile);
	ConfigureGroundedCollision();

	// 生成顺序为 ActivatePooledEnemy → InitializeEnemy。先清旧 ASC 状态，
	// 再按本次已加载的 CurrentArchetype 授予，避免使用上次池周期的原型。
	GrantAttackAbilities();
}

void ARogueEnemy::ActivatePooledEnemy(AActor* InOwner, const FVector& SpawnLocation, const FRotator& SpawnRotation)
{
	if (HitFlashComponent) { HitFlashComponent->ClearHitFlash(); }
	if (HasAuthority())
	{
		const uint32 Generation = HitFeedback.Generation + 1;
		HitFeedback = FRogueEnemyHitFeedback();
		HitFeedback.Generation = Generation;
		HitFeedback.bActive = true;
	}
	SpawnCycleId = FGuid::NewGuid();
	ConfigureGroundedCollision();
	SetOwner(InOwner);
	SetActorLocationAndRotation(SpawnLocation, SpawnRotation, false, nullptr, ETeleportType::TeleportPhysics);
	SetActorHiddenInGame(false);
	SetActorTickEnabled(true);
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

	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

		// 这里再应用一次"敌人之间互不阻挡"：构造函数里设过，但蓝图子类的属性值
		// 是在构造之后才加载的（bIgnoreOtherPawnCollision 可能在 BP 上被改过），
		// 所以每次激活时兜一次，保证开关在运行时确实生效。
		Capsule->SetCollisionResponseToChannel(
			ECC_Pawn, bIgnoreOtherPawnCollision ? ECR_Ignore : ECR_Block);
	}

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
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

void ARogueEnemy::DeactivateToPool()
{
	AttackAudioState.Reset();
	if (HitFlashComponent) { HitFlashComponent->ClearHitFlash(); }
	if (HasAuthority()) { HitFeedback.bActive = false; }
	if (!bPoolAvailable)
	{
		if (UWorld* World = GetWorld())
		{
			if (URogueEnemyTrackerSubsystem* Tracker = World->GetSubsystem<URogueEnemyTrackerSubsystem>())
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
	SetActorEnableCollision(false);
	SetActorLocation(FVector(0.0f, 0.0f, -20000.0f), false, nullptr, ETeleportType::TeleportPhysics);
	SetOwner(nullptr);

	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
		Movement->Velocity = FVector::ZeroVector;
	}
}

// ===========================================================
//  状态查询接口实现
// ===========================================================

FVector ARogueEnemy::GetHealthBarWorldLocation() const
{
	const float VerticalOffset = GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + (bIsBoss ? 92.0f : 56.0f);
	return GetActorLocation() + FVector(0.0f, 0.0f, VerticalOffset);
}

// ===========================================================
//  动作接口实现
// ===========================================================

ARogueCharacter* ARogueEnemy::GetPlayerCharacter() const
{
	// 缓存目标失去存活状态后立即失效，蓝图行为也不能在下一次重选前继续追踪他。
	ARogueCharacter* PlayerCharacter = CachedPlayerCharacter.Get();
	return IsValid(PlayerCharacter) && PlayerCharacter->IsAlive() ? PlayerCharacter : nullptr;
}

void ARogueEnemy::GatherAlivePlayers(TArray<ARogueCharacter*, TInlineAllocator<4>>& OutPlayers) const
{
	// 实现统一在 RoguePlayerQuery 里：全项目只有一份"谁算活着"的定义
	//（曾经四处各写一遍，口径分叉就会出现"怪物还在打倒地的人"这类 bug）。
	RoguePlayerQuery::CollectAlivePlayers(GetWorld(), OutPlayers);
}

void ARogueEnemy::RefreshTargetPlayer()
{
	// 联机的目标策略：活着的玩家里最近的那一个。
	// 换了策略（仇恨/威胁/固定目标）也只改这里，调用方不用动。
	TArray<ARogueCharacter*, TInlineAllocator<4>> Players;
	GatherAlivePlayers(Players);

	// 刻意**不**再"退回尸体"当目标：按设计，倒下的人处于无法控制状态，
	// 也不应该被怪物索敌（否则会出现"围着尸体打"的观感）。
	// 场上没有存活玩家时不再索敌；团灭与待复活取消由 GameMode 处理。

	ARogueCharacter* Nearest = nullptr;
	float NearestDistanceSquared = TNumericLimits<float>::Max();

	for (ARogueCharacter* Candidate : Players)
	{
		const float DistanceSquared = FVector::DistSquared2D(GetActorLocation(), Candidate->GetActorLocation());
		if (DistanceSquared < NearestDistanceSquared)
		{
			NearestDistanceSquared = DistanceSquared;
			Nearest = Candidate;
		}
	}

	CachedPlayerCharacter = Nearest;
}

float ARogueEnemy::GetDistanceToPlayer() const
{
	const ARogueCharacter* PlayerCharacter = GetPlayerCharacter();
	if (!IsValid(PlayerCharacter))
	{
		return FLT_MAX;
	}
	return FVector::Dist2D(GetActorLocation(), PlayerCharacter->GetActorLocation());
}

FVector ARogueEnemy::GetDirectionToPlayer() const
{
	const ARogueCharacter* PlayerCharacter = GetPlayerCharacter();
	if (!IsValid(PlayerCharacter))
	{
		return FVector::ZeroVector;
	}
	return (PlayerCharacter->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
}

FVector ARogueEnemy::GetVectorToPlayer() const
{
	const ARogueCharacter* PlayerCharacter = GetPlayerCharacter();
	if (!IsValid(PlayerCharacter))
	{
		return FVector::ZeroVector;
	}
	return PlayerCharacter->GetActorLocation() - GetActorLocation();
}

bool ARogueEnemy::IsPlayerInRange(float Range) const
{
	return GetDistanceToPlayer() <= Range;
}

bool ARogueEnemy::IsInRangedAttackRange() const
{
	// 攻击距离限制已取消：有远程攻击的原型在任何距离都视为“在射程内”。
	// 保留该函数是为了兼容蓝图/调试调用，不再返回距离判定结果。
	return CurrentArchetype.Ranged.bUsesRangedAttack;
}

void ARogueEnemy::MoveInDirection(const FVector& Direction, float SpeedMultiplier, float DeltaSeconds)
{
	if (Direction.IsNearlyZero() || DeltaSeconds <= 0.0f)
	{
		return;
	}

	const FVector NormalizedDirection = Direction.GetSafeNormal2D();
	const FRotator DesiredRotation = NormalizedDirection.Rotation();
	SetActorRotation(FMath::RInterpTo(GetActorRotation(), DesiredRotation, DeltaSeconds, 8.0f));
	AddActorWorldOffset(NormalizedDirection * MoveSpeed * SpeedMultiplier * DeltaSeconds, true);
	// 手动平移绕过了 CharacterMovement 的位移，不能继续沿用旧位置的地面缓存。
	GetCharacterMovement()->bForceNextFloorCheck = true;
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

FVector ARogueEnemy::CalculateArchetypeMovement(float DeltaSeconds)
{
	const ARogueCharacter* PlayerCharacter = GetPlayerCharacter();
	if (!IsValid(PlayerCharacter))
	{
		return FVector::ZeroVector;
	}

	const FVector ToPlayer = PlayerCharacter->GetActorLocation() - GetActorLocation();
	const float DistanceToPlayer = ToPlayer.Size2D();
	return GetMovementDirection(ToPlayer, DistanceToPlayer, DeltaSeconds);
}

void ARogueEnemy::UpdateBurstTimers(float DeltaSeconds)
{
	const FRogueEnemyMovementArchetype& MovementArchetype = CurrentArchetype.Movement;
	BurstCooldown -= DeltaSeconds;
	BurstTimeRemaining -= DeltaSeconds;

	if (BurstCooldown <= 0.0f)
	{
		BurstCooldown = MovementArchetype.BurstCycleTime;
		BurstTimeRemaining = MovementArchetype.BurstDuration;
	}
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

// ===========================================================
//  BlueprintNativeEvent 默认实现
// ===========================================================

void ARogueEnemy::OnEnemyInitialized_Implementation(const FRogueEnemyProfile& Profile)
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

void ARogueEnemy::OnEnemyDeath_Implementation(AActor* Killer)
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

void ARogueEnemy::FireRangedShot(ARogueCharacter* PlayerCharacter)
{
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

	const FRogueEnemyRangedArchetype& RangedArchetype = CurrentArchetype.Ranged;
	const TSubclassOf<ARogueEnemyProjectile> ProjectileClass = GetEnemyProjectileClass();
	if (RangedArchetype.ShotPattern == ERogueEnemyShotPattern::RadialRing)
	{
		const FRogueEnemyBulletRingSettings Settings = RangedArchetype.BulletRing.Sanitized();
		const UCapsuleComponent* Capsule = GetCapsuleComponent();
		const FVector Center = GetActorLocation() + FVector(0.0f, 0.0f,
			-Capsule->GetScaledCapsuleHalfHeight() + Settings.HeightAboveGround);
		const float SpawnRadius = Capsule->GetScaledCapsuleRadius() + 24.0f;
		const float StartYaw = GetActorRotation().Yaw;
		const bool bSpawnedRing = SpawnBulletRing(Center, StartYaw, SpawnRadius, Settings, ContactDamage * Settings.DamageMultiplier, ProjectileClass);
		Multicast_SpawnCosmeticBulletRing(Center, StartYaw, SpawnRadius, Settings, ProjectileClass);
		if (bSpawnedRing) { NotifyAttackAudio(Center); }
		return;
	}

	const FVector ShotOrigin = GetActorLocation() + FVector(0.0f, 0.0f, bIsBoss ? 78.0f : 54.0f) + GetActorForwardVector() * (bIsBoss ? 54.0f : 36.0f);
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
	auto SpawnProjectile = [&](const FVector& SpawnLocation, const FVector& Direction, float ProjectileSpeed, float ProjectileDamage, bool bLargeVisual)
	{
		// 服务端生成"真实"弹体（带伤害与命中判定）
		URogueCombatPoolSubsystem* Pools = GetWorld() ? GetWorld()->GetSubsystem<URogueCombatPoolSubsystem>() : nullptr;
		ARogueEnemyProjectile* Projectile = Pools != nullptr
			? Pools->AcquireEnemyProjectile(ProjectileClass, this, this, SpawnLocation, Direction.Rotation())
			: GetWorld()->SpawnActor<ARogueEnemyProjectile>(ProjectileClass, SpawnLocation, Direction.Rotation(), SpawnParameters);
		if (Projectile != nullptr)
		{
			bSpawnedAny = true;
			Projectile->ActivatePooledProjectile(this, this, SpawnLocation, Direction.Rotation(), Direction, ProjectileSpeed, ProjectileDamage, bLargeVisual);
		}

		// 同时广播给客户端，让它们在本地生成一发**纯表现**的同款弹体。
		// 注意不要在服务端也走这条：Multicast 的实现里用 HasAuthority() 提前返回了，
		// 所以这行调用在服务端只起到"发出去"的作用。
		Multicast_SpawnCosmeticProjectile(SpawnLocation, Direction, ProjectileSpeed, bLargeVisual, ProjectileClass);
	};

	if (RangedArchetype.ShotPattern == ERogueEnemyShotPattern::Spread3)
	{
		const float CenterIndex = (static_cast<float>(RangedArchetype.SpreadShotCount) - 1.0f) * 0.5f;
		for (int32 ShotIndex = 0; ShotIndex < RangedArchetype.SpreadShotCount; ++ShotIndex)
		{
			const float OffsetIndex = static_cast<float>(ShotIndex) - CenterIndex;
			const float SpreadAngle = OffsetIndex * RangedArchetype.SpreadAngleStep;
			const FVector SpreadDirection = ShotDirection.RotateAngleAxis(SpreadAngle, FVector::UpVector).GetSafeNormal();
			const FVector SpreadSpawnLocation = ShotOrigin + RightVector * OffsetIndex * RangedArchetype.SpreadSpawnOffset;
			SpawnProjectile(SpreadSpawnLocation, SpreadDirection, RangedArchetype.ProjectileSpeed, ContactDamage * RangedArchetype.ProjectileDamageMultiplier, RangedArchetype.bLargeVisual);
		}
	}
	else
	{
		SpawnProjectile(ShotOrigin, ShotDirection, RangedArchetype.ProjectileSpeed, ContactDamage * RangedArchetype.ProjectileDamageMultiplier, RangedArchetype.bLargeVisual);
	}

	if (bSpawnedAny) { NotifyAttackAudio(ShotOrigin); }

	if (RangedArchetype.bSpawnMuzzleEffect)
	{
		// 走 GameplayCue：AI 只在服务端跑，本机直接 SpawnImpactEffect 的话只有主机看得见枪口火光。
		// cue 由敌人自己的 ASC 多播出去（敌人 ASC 是 Minimal 模式，而 Minimal 天生复制 cue）。
		ARogueGameState::PlayImpactEffect(
			this,
			ShotOrigin,
			ShotDirection.Rotation(),
			ERogueImpactVisualStyle::Spark,
			RangedArchetype.MuzzleEffectScale);
	}
}


// ===========================================================
//  内部逻辑：死亡
// ===========================================================

void ARogueEnemy::Die(AActor* Killer)
{
	bDead = true;

	// 通知蓝图子类
	OnEnemyDeath(Killer);

	if (ARogueGameMode* RogueGameMode = GetWorld()->GetAuthGameMode<ARogueGameMode>())
	{
		RogueGameMode->HandleEnemyKilled(this, Killer);
	}

	DeactivateToPool();
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
						CachedShockPillarMID->SetVectorParameterValue(TEXT("EmissiveColor"), PillarColor * 2.2f);
					}
				}

				UMaterialInterface* EffectivePillarMaterial = ShockPillarMaterial;
				if (CachedShockPillarMID != nullptr)
				{
					EffectivePillarMaterial = CachedShockPillarMID.Get();
				}
				ShockPillarMesh->SetMaterial(0, EffectivePillarMaterial);
			}
		}
	}

	GetCapsuleComponent()->SetCapsuleSize(CurrentArchetype.Visual.CapsuleRadius, CurrentArchetype.Visual.CapsuleHalfHeight);
	GetCharacterMovement()->MaxWalkSpeed = bIsShockPillar ? 0.0f : MoveSpeed;
	GetCharacterMovement()->bOrientRotationToMovement = !bIsShockPillar;

	OnApplyVisualStyle();

}

// ===========================================================
//  内部逻辑：移动方向计算
// ===========================================================

FVector ARogueEnemy::GetMovementDirection(const FVector& ToPlayer, float DistanceToPlayer, float DeltaSeconds)
{
	const FRogueEnemyMovementArchetype& MovementArchetype = CurrentArchetype.Movement;
	const FVector TowardPlayer = ToPlayer.GetSafeNormal2D();
	if (TowardPlayer.IsNearlyZero())
	{
		return FVector::ZeroVector;
	}

	switch (MovementArchetype.Model)
	{
	case ERogueEnemyMovementModel::Stationary:
		return FVector::ZeroVector;
	case ERogueEnemyMovementModel::Direct:
		return TowardPlayer * MovementArchetype.BaseMoveMultiplier;
	case ERogueEnemyMovementModel::BurstCharge:
		BurstCooldown -= DeltaSeconds;
		BurstTimeRemaining -= DeltaSeconds;

		if (BurstCooldown <= 0.0f)
		{
			BurstCooldown = MovementArchetype.BurstCycleTime;
			BurstTimeRemaining = MovementArchetype.BurstDuration;
		}

		return TowardPlayer * (BurstTimeRemaining > 0.0f ? MovementArchetype.BurstActiveMultiplier : MovementArchetype.BurstIdleMultiplier);
	case ERogueEnemyMovementModel::Orbit:
		{
			const FVector Tangent = FVector(-TowardPlayer.Y, TowardPlayer.X, 0.0f) * OrbitDirection;
			if (DistanceToPlayer > MovementArchetype.FarDistance)
			{
				return (TowardPlayer * MovementArchetype.FarTowardWeight + Tangent * MovementArchetype.FarTangentWeight).GetSafeNormal2D();
			}

			if (DistanceToPlayer < MovementArchetype.CloseDistance)
			{
				return (TowardPlayer * MovementArchetype.CloseTowardWeight + Tangent * MovementArchetype.CloseTangentWeight).GetSafeNormal2D();
			}

			return (Tangent * MovementArchetype.MidTangentWeight + TowardPlayer * MovementArchetype.MidTowardWeight).GetSafeNormal2D();
		}
	case ERogueEnemyMovementModel::RangedKite:
		{
			const FVector Tangent = FVector(-TowardPlayer.Y, TowardPlayer.X, 0.0f) * OrbitDirection;
			if (DistanceToPlayer > MovementArchetype.FarDistance)
			{
				return (TowardPlayer * MovementArchetype.FarTowardWeight + Tangent * MovementArchetype.FarTangentWeight).GetSafeNormal2D();
			}

			if (DistanceToPlayer < MovementArchetype.CloseDistance)
			{
				return (TowardPlayer * MovementArchetype.CloseTowardWeight + Tangent * MovementArchetype.CloseTangentWeight).GetSafeNormal2D();
			}

			const float MidTowardWeight = MovementArchetype.bUseRangeBiasForMidToward
				? FMath::Clamp((MovementArchetype.PreferredRange - DistanceToPlayer) / FMath::Max(MovementArchetype.PreferredRange, 1.0f), -1.0f, 1.0f) * MovementArchetype.MidTowardWeight
				: MovementArchetype.MidTowardWeight;
			return (Tangent * MovementArchetype.MidTangentWeight + TowardPlayer * MidTowardWeight).GetSafeNormal2D();
		}
	case ERogueEnemyMovementModel::ZigZagChase:
		{
			const FVector Tangent = FVector(-TowardPlayer.Y, TowardPlayer.X, 0.0f) * OrbitDirection;
			const float ZigZagAlpha = FMath::Sin(GetGameTimeSinceCreation() * MovementArchetype.ZigZagFrequency + OrbitDirection * 1.6f);
			if (DistanceToPlayer < MovementArchetype.CloseDistance)
			{
				return (TowardPlayer * MovementArchetype.CloseTowardWeight + Tangent * MovementArchetype.CloseTangentWeight * FMath::Sign(ZigZagAlpha)).GetSafeNormal2D();
			}

			return (TowardPlayer * MovementArchetype.ZigZagTowardWeight + Tangent * ZigZagAlpha * MovementArchetype.ZigZagTangentWeight).GetSafeNormal2D();
		}
	default:
		return TowardPlayer * MovementArchetype.BaseMoveMultiplier;
	}
}
