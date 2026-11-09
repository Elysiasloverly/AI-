#include "Enemies/RogueEnemy.h"
#include "Enemies/Components/RogueBossSkillComponent.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/World.h"
#include "Debug/RoguePerfStats.h"
#include "Combat/RogueCombatCollision.h"
#include "Enemies/Components/RogueEnemyPresentationComponent.h"
#include "Enemies/Components/RogueEnemySupportComponent.h"
#include "Enemies/Components/RogueEnemyStationaryHazardComponent.h"
#include "Enemies/Components/RogueEnemyObstacleMovementComponent.h"
#include "Combat/Visuals/RogueHitFlashComponent.h"

#include "Player/RogueCharacter.h"
#include "AbilitySystem/RogueAbilitySystemComponent.h"
#include "AbilitySystem/RogueAttributeSet.h"
#include "AbilitySystem/RogueGameplayTags.h"
#include "Enemies/Data/RogueEnemyArchetypes.h"
#include "Combat/Targeting/RogueEnemyTrackerSubsystem.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/MeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "Materials/MaterialInterface.h"
#include "AIController.h"
#include "UObject/ConstructorHelpers.h"

// ===========================================================

ARogueEnemy::ARogueEnemy()
{
	PrimaryActorTick.bCanEverTick = true;
    BossSkills=CreateDefaultSubobject<URogueBossSkillComponent>(TEXT("BossSkills"));
	HitFlashComponent = CreateDefaultSubobject<URogueHitFlashComponent>(TEXT("HitFlash"));
    EnemyPresentation=CreateDefaultSubobject<URogueEnemyPresentationComponent>(TEXT("EnemyPresentation"));
    EnemySupport=CreateDefaultSubobject<URogueEnemySupportComponent>(TEXT("EnemySupport"));
    StationaryHazard=CreateDefaultSubobject<URogueEnemyStationaryHazardComponent>(TEXT("StationaryHazard"));
    ObstacleMovement=CreateDefaultSubobject<URogueEnemyObstacleMovementComponent>(TEXT("ObstacleMovement"));

	// 原生默认使用 AAIController 并允许自动接管；蓝图默认值仍可覆盖。
	AIControllerClass = AAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	GetCapsuleComponent()->InitCapsuleSize(34.0f, 72.0f);

	// 忽略 Pawn 可避免敌群胶囊互相阻挡，也允许玩家穿过敌人。
	// 玩家弹体/刀片使用 WorldDynamic 并对 Pawn 请求重叠；敌方弹体使用独立通道过滤友军。
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

void ARogueEnemy::SetMovementTickForPool(bool bActive)
{
    if(ObstacleMovement)ObstacleMovement->ResetForPool();
    if (UCharacterMovementComponent* Movement = GetCharacterMovement())
    {
        if (!bActive)
        {
            // Actor tick and component tick are independent. Repeated returns must not overwrite the saved state.
            if (!bMovementTickSuspendedForPool)
            {
                bResumeMovementTickAfterPool = Movement->IsComponentTickEnabled();
                bMovementTickSuspendedForPool = true;
            }
            Movement->StopMovementImmediately();
            Movement->SetComponentTickEnabled(false);
        }
        else if (bMovementTickSuspendedForPool)
        {
            Movement->SetComponentTickEnabled(bResumeMovementTickAfterPool);
            Movement->bForceNextFloorCheck = true;
            bMovementTickSuspendedForPool = false;
        }
    }
}

void ARogueEnemy::ConfigureGroundedCollision()
{
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionResponseToChannel(RogueCombatCollision::EnemyProjectileChannel, ECR_Ignore);
		// 镜头弹簧臂仍避开地形，敌人经过镜头和玩家之间时不收缩镜头距离。
		Capsule->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	}

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
		if (!RogueEnemyArchetypes::IsFlyingType(EnemyType) && (!FMath::IsFinite(Movement->GravityScale) || Movement->GravityScale <= 0.0f))
		{
			Movement->GravityScale = 1.0f;
		}
        if (RogueEnemyArchetypes::IsFlyingType(EnemyType))
        {
            Movement->GravityScale=0; Movement->SetMovementMode(MOVE_Flying);
        }
        if (RogueEnemyArchetypes::IsHealerType(EnemyType))
        { Movement->bRunPhysicsWithNoController=true;Movement->bCanWalkOffLedges=false;Movement->SetMovementMode(MOVE_Walking); }
		Movement->bEnablePhysicsInteraction = false;
	}
}

void ARogueEnemy::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	// PIE重建后属性集可能不再带默认子对象标志；按原名找默认子对象会漏掉它。
	// ASC已在组件初始化中登记现有属性集，优先取回同一个实际结算对象。
	if (AbilitySystemComponent != nullptr)
	{
		if (const auto* RegisteredSet = AbilitySystemComponent->GetSet<URogueAttributeSet>())
		{
			AttributeSet = const_cast<URogueAttributeSet*>(RegisteredSet);
		}
	}
	// 旧蓝图可能把这个原生子对象的引用序列化成 None，但子对象本身仍存在，
	// ASC 也能发现它。若不恢复引用，InitializeEnemy 会跳过生命写入，造成
	// HUD 显示档案中的满血，而 GAS 仍为 0：受到任意伤害后就会死亡。
	if (AttributeSet == nullptr)
	{
		AttributeSet = Cast<URogueAttributeSet>(GetDefaultSubobjectByName(TEXT("AttributeSet")));
	}
	if (ensureMsgf(AttributeSet != nullptr, TEXT("Enemy %s is missing its native AttributeSet"), *GetPathName())
		&& AbilitySystemComponent != nullptr)
	{
		AbilitySystemComponent->AddAttributeSetSubobject(AttributeSet.Get());
	}
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
    DOREPLIFETIME(ARogueEnemy, HealingFeedback);
    DOREPLIFETIME(ARogueEnemy, RuntimeProfile);
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
        SetMovementTickForPool(true);
		// 在场：把"池里 + 已死"这套本地状态翻回来。
		bPoolAvailable = false;
		bDead = false;
		// 显隐也按当前在场位应用，避免本地隐藏状态与活动状态不一致。
		SetActorHiddenInGame(false);
        RefreshUtilityComponents();
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
        SetMovementTickForPool(false);
        if (EnemyPresentation) EnemyPresentation->ResetForPool();
        if (EnemySupport) EnemySupport->ResetForPool();
        if (StationaryHazard) StationaryHazard->ResetForPool();
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
    ConsumeReplicatedHealingFeedback();
    if (BossSkills) BossSkills->RefreshPresentation();
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


void ARogueEnemy::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (BossSkills) BossSkills->ResetForPool();
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

void ARogueEnemy::Tick(float DeltaSeconds)
{
    CSV_SCOPED_TIMING_STAT(RoguePerf, EnemyTick);
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
		// 若复制的ASC替换了登记对象，同步使用它，避免继续读旧缓存。
		if (AbilitySystemComponent != nullptr)
		{
			if (const auto* RegisteredSet = AbilitySystemComponent->GetSet<URogueAttributeSet>())
			{
				AttributeSet = const_cast<URogueAttributeSet*>(RegisteredSet);
			}
		}
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
        ConsumeReplicatedHealingFeedback();

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

    const bool bFreezeMovement=IsBossSkillBlockingMovement();
    const bool bSuspendAttacks=AreNormalAttacksSuppressed();
    if (!bFreezeMovement) TickFlight(DeltaSeconds);
    if (!bSuspendAttacks) TickFlyingAttack(DeltaSeconds);
    if (bFreezeMovement && bSuspendAttacks) return; // 蓄力/维持时，普通行为图也不能偷偷位移。
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

	const float DesiredTickInterval = (bIsBoss || RogueEnemyArchetypes::IsHealerType(EnemyType))
		? 0.0f
		: (DistanceToPlayer > 1700.0f ? 0.08f : (DistanceToPlayer > 950.0f ? 0.04f : 0.0f));
	if (!FMath::IsNearlyEqual(PrimaryActorTick.TickInterval, DesiredTickInterval, KINDA_SMALL_NUMBER))
	{
		SetActorTickInterval(DesiredTickInterval);
	}

	const FVector MoveDirection = GetMovementDirection(ToPlayer, DistanceToPlayer, DeltaSeconds);
    if (RogueEnemyArchetypes::IsHealerType(EnemyType))
    {
        if (!MoveDirection.IsNearlyZero()) MoveInDirection(MoveDirection,1.f,DeltaSeconds);
        else GetCharacterMovement()->StopMovementImmediately();
        return;
    }

	if (!MoveDirection.IsNearlyZero())
	{
        // Preserve burst / retreat magnitude while all native and BP movement shares obstacle routing.
        MoveInDirection(MoveDirection,MoveDirection.Size2D(),DeltaSeconds);
	}
    else if(ObstacleMovement)ObstacleMovement->ResetForPool();

	// 远程原型不再贴身攻击：接触槽只在没有远程攻击时启用。
	if (!CurrentArchetype.Ranged.bUsesRangedAttack)
	{
		DecideContactAttack(DeltaSeconds);
	}
	DecideRangedAttack(PlayerCharacter, DeltaSeconds);
}

FVector ARogueEnemy::GetHealthBarWorldLocation() const
{
	const float VerticalOffset = GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + (bIsBoss ? 92.0f : 56.0f);
	return GetActorLocation() + FVector(0.0f, 0.0f, VerticalOffset);
}
