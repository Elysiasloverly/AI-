#include "Enemies/RogueEnemy.h"

#include "Player/RogueCharacter.h"
#include "AbilitySystem/RogueAbilitySystemComponent.h"
#include "AbilitySystem/RogueAbilitySet.h"
#include "AbilitySystem/RogueAbilitySystemLibrary.h"
#include "AbilitySystem/RogueAttributeSet.h"
#include "AbilitySystem/Abilities/RogueEnemyAttackAbility.h"
#include "Core/RogueGameplayTags.h"
#include "Core/RogueSelfTest.h"
#include "Core/RogueCombatFeedback.h"
#include "Enemies/RogueEnemyArchetypes.h"
#include "Enemies/RogueEnemyProjectile.h"
#include "Enemies/RogueEnemyVisualResources.h"
#include "Core/RogueGameMode.h"
#include "Core/RogueGameState.h"
#include "Core/RoguePlayerQuery.h"
#include "Subsystems/RogueCombatPoolSubsystem.h"
#include "Subsystems/RogueEnemyTrackerSubsystem.h"
#include "Combat/RogueImpactEffect.h"
#include "World/RogueArena.h"
#include "Components/CapsuleComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "AIController.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	constexpr float GroundWaveVisualHeight = 46.0f;
	constexpr float GroundWaveRingThickness = 46.0f;
	constexpr float GroundWaveRingHeight = 10.0f;
	constexpr float GroundWaveArenaPadding = 300.0f;
	constexpr float GroundWaveFallbackFullMapRadius = 9000.0f;
	constexpr float GroundWaveMinimumExpansionSpeed = 100.0f;
	constexpr int32 GroundWaveMinRingSegments = 32;
	constexpr int32 GroundWaveMaxRingSegments = 176;
	constexpr float GroundWaveTargetSegmentLength = 180.0f;
	constexpr float UnrealBasicShapeSize = 100.0f;

	float GetGroundWaveVisualBaseOffset(const ARogueEnemy& Enemy)
	{
		return -Enemy.GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + GroundWaveVisualHeight;
	}

	float CalculateFullMapGroundWaveRadius(const ARogueEnemy& Enemy, float ConfiguredRadius)
	{
		float EffectiveRadius = FMath::Max(ConfiguredRadius, GroundWaveFallbackFullMapRadius);
		UWorld* World = Enemy.GetWorld();
		if (World == nullptr)
		{
			return EffectiveRadius;
		}

		const ARogueArena* Arena = Cast<ARogueArena>(UGameplayStatics::GetActorOfClass(World, ARogueArena::StaticClass()));
		if (Arena == nullptr)
		{
			return EffectiveRadius;
		}

		const float HalfExtent = Arena->GetArenaHalfExtent() + GroundWaveArenaPadding;
		const FVector ArenaCenter = Arena->GetActorLocation();
		const FVector Origin = Enemy.GetActorLocation();
		const FVector Corners[] =
		{
			ArenaCenter + FVector(HalfExtent, HalfExtent, 0.0f),
			ArenaCenter + FVector(HalfExtent, -HalfExtent, 0.0f),
			ArenaCenter + FVector(-HalfExtent, HalfExtent, 0.0f),
			ArenaCenter + FVector(-HalfExtent, -HalfExtent, 0.0f)
		};

		for (const FVector& Corner : Corners)
		{
			EffectiveRadius = FMath::Max(EffectiveRadius, FVector::Dist2D(Origin, Corner));
		}

		return EffectiveRadius;
	}

	float CalculateReadableGroundWaveDuration(const FRogueEnemyGroundWaveArchetype& GroundWave, float EffectiveRadius)
	{
		const float ConfiguredSpeed = FMath::Max(GroundWave.ExpansionSpeed, GroundWaveMinimumExpansionSpeed);
		const float SpeedDrivenDuration = EffectiveRadius / ConfiguredSpeed;
		return FMath::Max(GroundWave.ExpansionDuration, SpeedDrivenDuration);
	}
}

// ===========================================================
//  构造 & 生命周期
// ===========================================================

ARogueEnemy::ARogueEnemy()
{
	PrimaryActorTick.bCanEverTick = true;

	// 确保蓝图子类也使用 AAIController，并在 Spawn 时自动 Possess
	AIControllerClass = AAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

	GetCapsuleComponent()->InitCapsuleSize(34.0f, 72.0f);

	// ── 敌人之间互不阻挡（默认开启）──
	// 背景：敌人的位移是 `AddActorWorldOffset(..., bSweep=true)`（见 HandleDefaultBehavior），
	//      胶囊之间默认互相 Block，几十只挤向玩家时会互相顶死成"肉墙"，谁都到不了玩家身边。
	// 做法：胶囊忽略 Pawn 通道。
	// 为什么这样改是安全的：
	//   · 玩家弹体 / 环绕刀片的 ObjectType 是 ECC_WorldDynamic，靠 "Overlap ECC_Pawn" 探测敌人
	//     （RogueProjectile.cpp:23-27、RogueOrbitingBlade.cpp:20-23），走的是"弹体→敌人"这条通道，
	//     与本改动无关 → 子弹与刀片照常命中。
	//   · 接触伤害用的是距离判定（PerformContactDamage/TouchPlayer 里的 DistSquared2D），
	//     不依赖碰撞事件 → 照常扣血。
	// 代价：玩家本身也是 Pawn，所以玩家不再被敌群挡住（可以穿过敌人）。
	//      若要做到"敌人之间不挡、但仍然挡住玩家"，需要给敌人单独开一个 ObjectType 通道
	//      （DefaultEngine.ini 加 Custom Channel，并让弹体改查该通道），改动面更大，故未采用。
	GetCapsuleComponent()->SetCollisionResponseToChannel(
		ECC_Pawn, bIgnoreOtherPawnCollision ? ECR_Ignore : ECR_Block);

	// ── GAS 组件 ──
	// ASC 挂在敌人自己身上：敌人没有 PlayerState 那一层，也不需要跨 Pawn 重生保留状态。
	// Minimal 复制模式在 URogueAbilitySystemComponent 的构造函数里设好（ConfigureAsEnemy）。
	AbilitySystemComponent = CreateDefaultSubobject<URogueAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->ConfigureAsEnemy();

	// 与玩家共用同一个属性集类型（只用到其中血量相关的几项）
	AttributeSet = CreateDefaultSubobject<URogueAttributeSet>(TEXT("AttributeSet"));

	// AttributeSet 是普通 UObject，要复制必须走"已注册子对象列表"这条路
	bReplicateUsingRegisteredSubObjectList = true;

	// ── 联机复制预算 ──
	// 敌人是"数量最多、单体最不重要"的类别：对象池预热量 160，配合随波次增长的存活上限
	// ── 最关键的一行：敌人必须复制 ──
	//
	// 敌人是**服务端生成**的（池预热也在 GameMode.BeginPlay、同样只在服务端跑），
	// 不开复制的话客户端根本收不到这些 Actor —— 表现是"主机看得见怪、客户端一只都看不见"，
	// 而且所有依赖它的复制全部失效：ASC/属性集（血条、伤害飘字）、地面波表现态、bReplicatedActive。
	//
	// ⚠️ 这个坑很隐蔽：`ACharacter` / `APawn` 的构造函数**都不会**自动打开 bReplicates
	// （引擎 Character.cpp / Pawn.cpp 里都没有这一行），AActor 的默认值是 false。
	// 单机与主机上一切正常，只有客户端能看出问题 —— 所以它能一直活到联机联调才暴露。
	// 玩家角色之所以没事，是因为 BP_RogueCharacter 上勾了 Replicates；敌人这边漏了。
	//
	// 注意：蓝图默认值在 C++ 构造函数之后应用，所以**如果敌人 BP 上把 Replicates 手动取消勾选**，
	// 会覆盖这里的设置 —— 那种情况下需要在 BP 里勾回来。
	bReplicates = true;
	SetReplicatingMovement(true);

	// 复制预算：敌人数量上限很高（40 + 每波 8）在后期可达 250+。若沿用 AActor 默认的 NetUpdateFrequency = 100，
	// 峰值会产生 2.5 万次/秒量级的 actor 更新尝试，再叠加每个敌人的 ASC 与属性复制
	// —— 联机下这是最先崩的地方，所以必须显式压预算。
	//
	// 注意 NetUpdateFrequency 在 5.5 已标记 UE_DEPRECATED（Actor.h:831），
	// 直接对公有成员赋值只能编过但会告警，正确做法是用 SetNetUpdateFrequency()。
	//
	// 这里给的是**保守基线**，运行期还会按"离玩家多远"分档调整（见 UpdateReplicationBudget）。
	SetNetUpdateFrequency(10.0f);
	SetMinNetUpdateFrequency(2.0f);

	// 优先级低于玩家（默认 1.0）：带宽紧张时先保玩家与弹体这类手感相关的 actor
	NetPriority = 0.6f;

	// 超出 150m 不再复制给该客户端（这是引擎默认值，显式写出来是为了让预算可见可调）
	// 5.5 起公有成员已废弃，必须走 setter（否则每次编译都有一条 C4996）
	SetNetCullDistanceSquared(FMath::Square(15000.0f));

	GetCharacterMovement()->GravityScale = 0.0f;
	GetCharacterMovement()->MaxWalkSpeed = MoveSpeed;
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 600.0f, 0.0f);

	ShockPillarMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ShockPillarMesh"));
	ShockPillarMesh->SetupAttachment(RootComponent);
	ShockPillarMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ShockPillarMesh->SetCastShadow(true);
	ShockPillarMesh->SetVisibility(false, true);
	ShockPillarMesh->SetHiddenInGame(true);

	GroundWaveMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GroundWaveMesh"));
	GroundWaveMesh->SetupAttachment(RootComponent);
	GroundWaveMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GroundWaveMesh->SetCastShadow(false);
	GroundWaveMesh->SetVisibility(false, true);
	GroundWaveMesh->SetHiddenInGame(true);

	GroundWaveRingSegments = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("GroundWaveRingSegments"));
	GroundWaveRingSegments->SetupAttachment(RootComponent);
	GroundWaveRingSegments->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GroundWaveRingSegments->SetCastShadow(false);
	GroundWaveRingSegments->SetVisibility(false, true);
	GroundWaveRingSegments->SetHiddenInGame(true);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PillarMeshAsset(TEXT("/Game/GameAssets/StarterContent/Shapes/Shape_Cylinder.Shape_Cylinder"));
	if (PillarMeshAsset.Succeeded())
	{
		ShockPillarMesh->SetStaticMesh(PillarMeshAsset.Object);
	}

	static ConstructorHelpers::FObjectFinder<UStaticMesh> WaveMeshAsset(TEXT("/Game/GameAssets/StarterContent/Shapes/Shape_Cube.Shape_Cube"));
	if (WaveMeshAsset.Succeeded())
	{
		GroundWaveRingSegments->SetStaticMesh(WaveMeshAsset.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> PillarMaterialAsset(TEXT("/Game/GameAssets/Realistic_Starter_VFX_Pack_Vol2/Materials/M_Glow_D.M_Glow_D"));
	if (PillarMaterialAsset.Succeeded())
	{
		ShockPillarMaterial = PillarMaterialAsset.Object;
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> WaveMaterialAsset(TEXT("/Game/GameAssets/Realistic_Starter_VFX_Pack_Vol2/Materials/M_Glow_C_Inst.M_Glow_C_Inst"));
	if (WaveMaterialAsset.Succeeded())
	{
		ShockWaveMaterial = WaveMaterialAsset.Object;
	}

	OrbitDirection = FMath::RandBool() ? 1.0f : -1.0f;
}

void ARogueEnemy::BeginPlay()
{
	Super::BeginPlay();

	// GAS 初始化。敌人既是 OwnerActor 也是 AvatarActor（没有 PlayerState 那一层）。
	// 必须在 BeginPlay 做而不是构造里：构造阶段 World / 复制都还没就绪。
	if (AbilitySystemComponent != nullptr)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this);
	}

	// AttributeSet 是普通 UObject（不是组件），要复制必须登记进 Actor 的已注册子对象列表，
	// 且只需服务端登记 —— 客户端那份实例由构造函数创建，等属性同步即可。
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

	// 服务端：池化对象"出生即在池里"，这是它的初始状态。
	//
	// ⚠️ 客户端**绝不能**在这里调 DeactivateToPool()：
	// 它会写本地状态，其中的 bReplicatedActive 是**权威独占**的复制位 ——
	// 客户端一写就把刚复制过来的真实值（true）覆盖成 false，
	// 紧接着 ApplyReplicatedActiveState() 读到 false，于是把自己隐藏+停 Tick+关碰撞，
	// 表现就是"客户端永远看不见这只怪"。
	// （这不是推测：实测日志里能看到 Active=1 紧跟 Active=0，就是这条路径。）
	if (HasAuthority())
	{
		DeactivateToPool();
	}

	// 客户端：按复制过来的"在场"状态落地。
	// 初始复制值也会走这条路径（实测 RepNotify 对初始值是会触发的，但这里兜一次更稳）。
	ApplyReplicatedActiveState();
}

UAbilitySystemComponent* ARogueEnemy::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void ARogueEnemy::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// 地面波表现态：客户端据此自己画那条扩散的波纹（AI 只在服务端跑，见 Tick）
	DOREPLIFETIME_CONDITION(ARogueEnemy, bGroundWaveActive, COND_None);
	DOREPLIFETIME_CONDITION(ARogueEnemy, GroundWaveCurrentRadius, COND_None);
	DOREPLIFETIME_CONDITION(ARogueEnemy, GroundWaveAlpha, COND_None);
	DOREPLIFETIME_CONDITION(ARogueEnemy, GroundWaveEffectiveMaxRadius, COND_None);
	DOREPLIFETIME_CONDITION(ARogueEnemy, GroundWaveEffectiveDuration, COND_None);

	// "这只怪现在在不在场"。池化状态本身不复制（见头文件里的说明），
	// 但客户端必须知道它，否则怪在客户端永远是"死的、池里的"。
	DOREPLIFETIME_CONDITION(ARogueEnemy, bReplicatedActive, COND_None);
}

void ARogueEnemy::OnRep_ReplicatedActive()
{
	ApplyReplicatedActiveState();
}

void ARogueEnemy::ApplyReplicatedActiveState()
{
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
		// 显隐也要按这一位对齐，不能只依赖 bHidden 的复制：
		// 中途加入的客户端会先收到"这只怪是活的"（bHidden=false），紧接着 BeginPlay 里
		// 那次本地 DeactivateToPool() 又把它隐藏掉，而服务端的 bHidden 之后没再变化、
		// 不会再发一次 false —— 结果那只怪对这名客户端永久隐形。
		SetActorHiddenInGame(false);
		SetActorTickEnabled(true);

		// ★ 客户端也必须把敌人登记进**本机的**追踪器。
		//
		// 为什么关键：客户端的"武器索敌"（ARogueWeaponBase::FindNearestEnemy →
		// URogueEnemyTrackerSubsystem::FindNearestEnemyInRange）和 "HUD 敌人血条"
		// （ARogueHUD::DrawEnemyHealthBars 遍历同一个子系统）都依赖它，
		// 而注册原本只发生在**服务端的生成路径**（URogueSpawnSubsystem::SpawnConfiguredEnemy）。
		// 结果就是客户端：有武器、有输入，但**索敌永远返回 null → 自己一发子弹都不打**，
		// 同时敌人血条也一条都画不出来。
		// 追踪器是 WorldSubsystem、纯本机数据，客户端登记自己的副本完全安全。
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
}

void ARogueEnemy::Multicast_SpawnCosmeticProjectile_Implementation(FVector_NetQuantize SpawnLocation, FVector_NetQuantizeNormal Direction, float Speed, bool bLargeVisual)
{
	// 这个函数在所有端都会执行。服务端**直接返回** ——
	// 服务端已经生成过"真实"弹体了（带伤害与命中判定），再生成一发就会双份。
	// 所以这里的语义实际是"仅客户端"的广播。
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
		ARogueEnemyProjectile::StaticClass(), this, this, SpawnLocation, SpawnRotation);
	if (Projectile == nullptr)
	{
		return;
	}

	// 客户端这发**不参与伤害**：伤害早已在服务端结算。
	// 传 0 伤害是为了万一它碰到了什么也不会造成二次扣血。
	Projectile->ActivatePooledProjectile(this, this, SpawnLocation, SpawnRotation, Direction, Speed, /*ProjectileDamage=*/0.0f, bLargeVisual);
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

	// 只有跨档才写：SetNetUpdateFrequency 会触碰网络对象的更新频率，
	// 每帧无脑写会让 NetDriver 反复重排优先级队列。
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

	// 延到本帧末尾再处理死亡。
	// 原因：这个回调正在"施加授予 State.Dead 的 GE"的过程中执行，
	// 而死亡处理链路（Die → DeactivateToPool → ResetForPool）会去移除那个 GE，
	// 在它自己的标签回调里移除它属于重入。SetTimerForNextTick 把它挪出当前调用栈。
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
	// 客户端上的敌人是**复制代理**：位置、朝向、血量、地面波扩散状态都由服务端同步过来。
	// 在这里再跑一遍 AI 有三个害处：
	//   1) 本地算出来的位移会和复制数据打架，表现为抖动/回弹；
	//   2) 白白消耗客户端 CPU —— 200+ 敌人的情况非常明显；
	//   3) 攻击/伤害类逻辑在客户端跑没有意义（伤害只在服务端结算）。
	// 所以客户端只根据同步过来的地面波状态刷新环形视觉。
	if (!HasAuthority())
	{
		UpdateGroundWaveVisualFromReplication();

		// 客户端自己轮询复制过来的血量 —— 这是**实测得出的必要做法**：
		// 属性变化回调（HandleHealthChanged）在"属性经复制到达客户端"这条路径上不会触发
		// （取证：客户端 ShowDamageNumber 一次都没被调用，而服务端同一局调用了 222 次），
		// 所以客户端必须自己比对上一帧的镜像值，才能：
		//   · 刷新 CurrentHealth 镜像（血条读的是这个）
		//   · 产生伤害飘字
		// 表现上飘字会比主机晚一个复制间隔（由 UpdateReplicationBudget 分档决定），这是固有代价。
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
	if (TargetRefreshCooldown <= 0.0f)
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

void ARogueEnemy::UpdateGroundWaveVisualFromReplication()
{
	// 客户端没有 AI 推进，全靠这几个复制值驱动环形视觉。
	// 用 bGroundWaveActive 判断"在不在放"，半径直接取同步值。
	if (!bGroundWaveActive)
	{
		if (bClientGroundWaveVisible)
		{
			bClientGroundWaveVisible = false;
			HideGroundWave();
		}
		return;
	}

	bClientGroundWaveVisible = true;
	UpdateGroundWaveRingVisual(GroundWaveCurrentRadius);
	OnGroundWaveUpdated(GroundWaveCurrentRadius, GroundWaveAlpha);
}

void ARogueEnemy::HandleDefaultBehavior(float DeltaSeconds)
{
	// 目标已由 Tick 里的 RefreshTargetPlayer() 选好（活着的玩家里最近的那个）。
	ARogueCharacter* PlayerCharacter = GetPlayerCharacter();
	if (!IsValid(PlayerCharacter) || PlayerCharacter->IsDead())
	{
		return;
	}

	const FVector ToPlayer = PlayerCharacter->GetActorLocation() - GetActorLocation();
	const float DistanceToPlayer = ToPlayer.Size2D();

	// 按距离分档调整**网络**复制频率（与上面的逻辑 Tick 降频是两件事：
	// 逻辑频率决定"多久想一次"，复制频率决定"多久告诉客户端一次"）。
	UpdateReplicationBudget(DistanceToPlayer);

	const bool bUsesGroundWave = CurrentArchetype.GroundWave.bUsesGroundWave;
	const float DesiredTickInterval = (bIsBoss || bUsesGroundWave || bGroundWaveActive)
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
	}

	if (bUsesGroundWave)
	{
		DecideGroundWaveAttack(PlayerCharacter, DistanceToPlayer, DeltaSeconds);
		return;
	}

	// 接触与远程攻击都走"AI 决策 → 能力执行"（见各自函数的说明）。
	DecideContactAttack(DeltaSeconds);
	DecideRangedAttack(PlayerCharacter, DistanceToPlayer, DeltaSeconds);
}

// ===========================================================
//  攻击能力：授予 / 激活
// ===========================================================

void ARogueEnemy::GrantAttackAbilities()
{
	// 敌人没有"本人客户端"，能力只在服务端有；客户端连技能列表都没有（ServerOnly）。
	if (!HasAuthority() || AbilitySystemComponent == nullptr)
	{
		return;
	}

	// 先清空句柄：换原型/换池周期后旧句柄不该再被使用。
	ContactAttackAbilityHandle = FGameplayAbilitySpecHandle();
	RangedAttackAbilityHandle = FGameplayAbilitySpecHandle();
	GroundWaveAttackAbilityHandle = FGameplayAbilitySpecHandle();

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
		// 三个槽位是 AI 侧固有的：`Tick` 里的决策分成"接触 / 远程 / 地面波"三条，
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

			if (AbilityClass->IsChildOf(URogueEnemyContactAttackAbility::StaticClass()))
			{
				ContactAttackAbilityHandle = Handle;
			}
			else if (AbilityClass->IsChildOf(URogueEnemyRangedAttackAbility::StaticClass()))
			{
				RangedAttackAbilityHandle = Handle;
			}
			else if (AbilityClass->IsChildOf(URogueEnemyGroundWaveAttackAbility::StaticClass()))
			{
				GroundWaveAttackAbilityHandle = Handle;
			}
		}

		UE_LOG(LogTemp, Verbose, TEXT("[Rogue] 敌人攻击能力来自数据驱动能力集 %s（%d 条）：接触=%d 远程=%d 地面波=%d"),
			*AbilitySet->GetName(), Handles.Num(),
			ContactAttackAbilityHandle.IsValid() ? 1 : 0,
			RangedAttackAbilityHandle.IsValid() ? 1 : 0,
			GroundWaveAttackAbilityHandle.IsValid() ? 1 : 0);
		return;
	}

	// ---- ② 回退：按原型标志推导（改造前的硬编码逻辑，老数据表走这条）----

	// 用地面波的敌人（震荡柱）不贴身打人 —— 改造前 HandleDefaultBehavior 里
	// `if (bUsesGroundWave) { HandleGroundWaveAttack(); return; }` 就是这个语义。
	if (!CurrentArchetype.GroundWave.bUsesGroundWave)
	{
		ContactAttackAbilityHandle = AbilitySystemComponent->GiveAbility(
			FGameplayAbilitySpec(URogueEnemyContactAttackAbility::StaticClass(), /*Level=*/1, /*InputID=*/INDEX_NONE, /*SourceObject=*/this));
	}

	if (CurrentArchetype.Ranged.bUsesRangedAttack)
	{
		RangedAttackAbilityHandle = AbilitySystemComponent->GiveAbility(
			FGameplayAbilitySpec(URogueEnemyRangedAttackAbility::StaticClass(), 1, INDEX_NONE, this));
	}

	if (CurrentArchetype.GroundWave.bUsesGroundWave)
	{
		GroundWaveAttackAbilityHandle = AbilitySystemComponent->GiveAbility(
			FGameplayAbilitySpec(URogueEnemyGroundWaveAttackAbility::StaticClass(), 1, INDEX_NONE, this));
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
	// 能力句柄无效 = 这只怪身上根本没有这种攻击能力（结构性问题，不是"这次不能打"）。
	// 不处理的话 AI 会**每帧**重试一次注定失败的激活：实测在 200 敌人压测里
	// （用 Rogue.SelfTest.DisableEnemyAttackAbilities 人为关掉授予）跑出了 370 万次空转尝试。
	// 这里退避半秒，代价是"能力真的缺失时"最多晚半秒才发现，收益是不会有每帧空转。
	//
	// ⚠️ 只在**句柄无效**时退避，不在"激活被冷却挡住"时退避 ——
	// 后者是正常情况（冷却 GE 与 AI 节流可能差一帧），退避会让攻击变慢。
	if (!Handle.IsValid())
	{
		Timer = FMath::Max(Timer, 0.5f);
	}
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
		ContactTimer = ContactInterval;
	}
	else
	{
		BackOffMissingAttack(ContactTimer, ContactAttackAbilityHandle);
	}
}

void ARogueEnemy::DecideRangedAttack(ARogueCharacter* PlayerCharacter, float DistanceToPlayer, float DeltaSeconds)
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

	const bool bInAttackRange =
		DistanceToPlayer >= RangedArchetype.MinimumRange * RangedArchetype.AttackRangeMinFactor &&
		DistanceToPlayer <= RangedArchetype.MaximumRange;
	if (!bInAttackRange)
	{
		return;
	}

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

void ARogueEnemy::DecideGroundWaveAttack(ARogueCharacter* PlayerCharacter, float DistanceToPlayer, float DeltaSeconds)
{
	if (!CurrentArchetype.GroundWave.bUsesGroundWave || !IsValid(PlayerCharacter))
	{
		return;
	}

	// 波在扩散期间由能力逐帧推进，AI 完全不参与 —— 只有"没在放波"时才考虑起新的一波。
	if (bGroundWaveActive)
	{
		return;
	}

	GroundWaveAttackTimer = FMath::Max(0.0f, GroundWaveAttackTimer - DeltaSeconds);
	if (GroundWaveAttackTimer > 0.0f)
	{
		return;
	}

	const float EffectiveMaxRadius = CalculateFullMapGroundWaveRadius(*this, CurrentArchetype.GroundWave.MaxRadius);
	if (DistanceToPlayer > EffectiveMaxRadius + 220.0f)
	{
		return;
	}

	// 冷却在"波结束"时由能力给（改造前是 UpdateGroundWave 结束分支里设 GroundWaveAttackTimer），
	// 所以这里激活成功后不动计时器 —— 动了就会变成"起波即冷却"。
	RogueEnemyAttackStats::CountGroundWaveAttempt();
	TryActivateAttackAbility(GroundWaveAttackAbilityHandle);
	if (!TryActivateAttackAbility(GroundWaveAttackAbilityHandle))
	{
		BackOffMissingAttack(GroundWaveAttackTimer, GroundWaveAttackAbilityHandle);
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

	return bDamagedAnyone;
}

bool ARogueEnemy::ExecuteRangedAttack()
{
	ARogueCharacter* PlayerCharacter = GetPlayerCharacter();
	if (!IsValid(PlayerCharacter) || GetWorld() == nullptr)
	{
		return false;
	}

	FireRangedShot(PlayerCharacter);
	return true;
}

void ARogueEnemy::ExecuteStartGroundWave()
{
	StartGroundWave();
}

void ARogueEnemy::ExecuteAdvanceGroundWave(float DeltaSeconds)
{
	UpdateGroundWave(DeltaSeconds);
}

// ===========================================================
//  生命周期：初始化 / 激活 / 回收
// ===========================================================

void ARogueEnemy::InitializeEnemy(const FRogueEnemyProfile& InProfile)
{
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

	BurstCooldown = CurrentArchetype.Movement.Model == ERogueEnemyMovementModel::BurstCharge ? FMath::FRandRange(0.8f, 1.8f) : 0.0f;
	BurstTimeRemaining = 0.0f;
	ContactTimer = 0.0f;
	RangedAttackTimer = CurrentArchetype.Ranged.bUsesRangedAttack
		? FMath::FRandRange(CurrentArchetype.Ranged.InitialCooldownMin, CurrentArchetype.Ranged.InitialCooldownMax)
		: 0.0f;
	GroundWaveAttackTimer = CurrentArchetype.GroundWave.bUsesGroundWave
		? FMath::FRandRange(CurrentArchetype.GroundWave.InitialCooldownMin, CurrentArchetype.GroundWave.InitialCooldownMax)
		: 0.0f;
	GroundWaveElapsed = 0.0f;
	GroundWaveCurrentRadius = 0.0f;
	GroundWaveAlpha = 0.0f;
	GroundWaveEffectiveMaxRadius = 0.0f;
	GroundWaveEffectiveDuration = 0.0f;
	bGroundWaveActive = false;
	GroundWaveHitPlayers.Reset();
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

	// ⚠️ 攻击能力必须**在这里**授予，而不是 ActivatePooledEnemy 里：
	//    生成方的调用顺序是 ActivatePooledEnemy() → InitializeEnemy()
	//    （见 URogueSpawnSubsystem::SpawnEnemy），而 CurrentArchetype 是本函数上面才赋值的。
	//    在 ActivatePooledEnemy 里授予会照着**上一只怪**的原型发技能
	//    （第一次生成时甚至是默认原型）—— 症状是"远程怪不会远程、震荡柱不起波"，
	//    而且因为技能列表看起来正常，日志里一点报错都没有。
	//    ASC->ResetForPool() 已经在 ActivatePooledEnemy 里跑过，所以这里是干净的。
	GrantAttackAbilities();
}

void ARogueEnemy::ActivatePooledEnemy(AActor* InOwner, const FVector& SpawnLocation, const FRotator& SpawnRotation)
{
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

	// 攻击能力的授予**不在这里**：本函数由生成方在 InitializeEnemy() **之前**调用
	// （见 URogueSpawnSubsystem::SpawnEnemy），这时 CurrentArchetype 还是上一只留下的值，
	// 照着它授予会得到"错的攻击组合"。所以授予放在 InitializeEnemy() 的末尾。

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
}

void ARogueEnemy::DeactivateToPool()
{
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

	// bReplicatedActive 是**权威独占**的状态位：客户端写它等于破坏刚复制到的真实值
	// （客户端的 BeginPlay 与死亡处理都会走到这里，见 BeginPlay 里的说明）。
	if (HasAuthority())
	{
		bReplicatedActive = false;
	}
	ContactTimer = 0.0f;
	RangedAttackTimer = 0.0f;
	GroundWaveAttackTimer = 0.0f;
	// 攻击能力句柄一并作废：技能列表已经在下次激活时被 ResetForPool 清掉，
	// 留着旧句柄只会在日志里出现"激活了一个不存在的技能"这种噪音。
	ContactAttackAbilityHandle = FGameplayAbilitySpecHandle();
	RangedAttackAbilityHandle = FGameplayAbilitySpecHandle();
	GroundWaveAttackAbilityHandle = FGameplayAbilitySpecHandle();
	GroundWaveElapsed = 0.0f;
	GroundWaveCurrentRadius = 0.0f;
	GroundWaveAlpha = 0.0f;
	GroundWaveEffectiveMaxRadius = 0.0f;
	GroundWaveEffectiveDuration = 0.0f;
	bGroundWaveActive = false;
	GroundWaveHitPlayers.Reset();
	BurstCooldown = 0.0f;
	BurstTimeRemaining = 0.0f;
	LastDamageNumberTime = -100.0f;
	PendingDamageNumber = 0.0f;
	CachedPlayerCharacter.Reset();
	TargetRefreshCooldown = 0.0f;
	HideGroundWave();
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
	// 只读缓存：缓存的刷新由 Tick 里的 RefreshTargetPlayer() 负责（服务端）。
	// 客户端跑的是复制代理，不跑 AI，这里拿到什么都无所谓。
	return CachedPlayerCharacter.Get();
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
	// 场上暂时没有活人时敌人就没有目标 —— 表现为原地站着等，这是期望行为：
	// 10 秒后倒地者复活，它们自然会重新锁定。

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
	if (!CurrentArchetype.Ranged.bUsesRangedAttack)
	{
		return false;
	}
	const float Distance = GetDistanceToPlayer();
	return Distance >= CurrentArchetype.Ranged.MinimumRange * CurrentArchetype.Ranged.AttackRangeMinFactor
		&& Distance <= CurrentArchetype.Ranged.MaximumRange;
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
		ContactTimer = ContactInterval;
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

void ARogueEnemy::OnGroundWaveStarted_Implementation(float MaxRadius, float ExpansionDuration)
{
	// 默认空实现，震荡柱蓝图可重写
}

void ARogueEnemy::OnGroundWaveUpdated_Implementation(float Radius, float Alpha)
{
	// 默认空实现，震荡柱蓝图可重写
}

void ARogueEnemy::OnGroundWaveFinished_Implementation()
{
	// 默认空实现，震荡柱蓝图可重写
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
	if (PlayerCharacter->IsDead())
	{
		return;
	}

	const FVector ShotOrigin = GetActorLocation() + FVector(0.0f, 0.0f, bIsBoss ? 78.0f : 54.0f) + GetActorForwardVector() * (bIsBoss ? 54.0f : 36.0f);
	const FVector TargetLocation = PlayerCharacter->GetActorLocation() + FVector(0.0f, 0.0f, 48.0f);
	const FVector ShotVector = TargetLocation - ShotOrigin;
	if (ShotVector.IsNearlyZero())
	{
		return;
	}

	const FRogueEnemyRangedArchetype& RangedArchetype = CurrentArchetype.Ranged;
	const FVector ShotDirection = ShotVector.GetSafeNormal();
	const FVector RightVector = FVector::CrossProduct(ShotDirection, FVector::UpVector).GetSafeNormal();
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = this;
	SpawnParameters.Instigator = this;

	auto SpawnProjectile = [&](const FVector& SpawnLocation, const FVector& Direction, float ProjectileSpeed, float ProjectileDamage, bool bLargeVisual)
	{
		// 服务端生成"真实"弹体（带伤害与命中判定）
		URogueCombatPoolSubsystem* Pools = GetWorld() ? GetWorld()->GetSubsystem<URogueCombatPoolSubsystem>() : nullptr;
		ARogueEnemyProjectile* Projectile = Pools != nullptr
			? Pools->AcquireEnemyProjectile(ARogueEnemyProjectile::StaticClass(), this, this, SpawnLocation, Direction.Rotation())
			: GetWorld()->SpawnActor<ARogueEnemyProjectile>(ARogueEnemyProjectile::StaticClass(), SpawnLocation, Direction.Rotation(), SpawnParameters);
		if (Projectile != nullptr)
		{
			Projectile->ActivatePooledProjectile(this, this, SpawnLocation, Direction.Rotation(), Direction, ProjectileSpeed, ProjectileDamage, bLargeVisual);
		}

		// 同时广播给客户端，让它们在本地生成一发**纯表现**的同款弹体。
		// 注意不要在服务端也走这条：Multicast 的实现里用 HasAuthority() 提前返回了，
		// 所以这行调用在服务端只起到"发出去"的作用。
		Multicast_SpawnCosmeticProjectile(SpawnLocation, Direction, ProjectileSpeed, bLargeVisual);
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
//  地面波的起波与推进
// ===========================================================
//  ⚠️ 这两个函数现在**只被地面波攻击能力调用**（ExecuteStartGroundWave / ExecuteAdvanceGroundWave）。
//  改造前是 AI 的 HandleGroundWaveAttack 每帧自己判断"该不该起波、要不要推进"，
//  现在 AI 只决定"起不起一波"，推进由持续激活的能力逐帧驱动。

void ARogueEnemy::StartGroundWave()
{
	bGroundWaveActive = true;
	GroundWaveHitPlayers.Reset();
	GroundWaveElapsed = 0.0f;
	GroundWaveCurrentRadius = 24.0f;
	GroundWaveAlpha = 0.0f;
	GroundWaveEffectiveMaxRadius = CalculateFullMapGroundWaveRadius(*this, CurrentArchetype.GroundWave.MaxRadius);
	GroundWaveEffectiveDuration = CalculateReadableGroundWaveDuration(CurrentArchetype.GroundWave, GroundWaveEffectiveMaxRadius);

	if (bUseBuiltInShockPillarVisual && GroundWaveMesh != nullptr)
	{
		GroundWaveMesh->SetHiddenInGame(true);
		GroundWaveMesh->SetVisibility(false, true);
	}

	UpdateGroundWaveRingVisual(GroundWaveCurrentRadius);

	if (GetWorld() != nullptr)
	{
		// 地面波起始特效同样要走 cue（扩散过程本身靠复制的地面波状态驱动）。
		// (0.95, 0.95, 0.32) 的含义是"同一个 Spark 素材贴地压扁"：
		// PlayImpactEffect 会把 X 当统一缩放、Z/X 当压扁比例传给 cue。
		ARogueGameState::PlayImpactEffect(
			this,
			GetActorLocation() + FVector(0.0f, 0.0f, 8.0f),
			FRotator::ZeroRotator,
			ERogueImpactVisualStyle::Spark,
			FVector(0.95f, 0.95f, 0.32f));
	}

	OnGroundWaveStarted(GroundWaveEffectiveMaxRadius, GroundWaveEffectiveDuration);
}

void ARogueEnemy::UpdateGroundWave(float DeltaSeconds)
{
	const FRogueEnemyGroundWaveArchetype& GroundWave = CurrentArchetype.GroundWave;
	const float Duration = FMath::Max(GroundWaveEffectiveDuration, KINDA_SMALL_NUMBER);
	GroundWaveElapsed += DeltaSeconds;

	const float Alpha = FMath::Clamp(GroundWaveElapsed / Duration, 0.0f, 1.0f);
	const float Radius = FMath::Lerp(24.0f, FMath::Max(GroundWaveEffectiveMaxRadius, 24.0f), Alpha);
	GroundWaveCurrentRadius = Radius;
	GroundWaveAlpha = Alpha;

	UpdateGroundWaveRingVisual(Radius);

	// 逐个玩家判定：波面扫到谁就给谁结算一次。
	// 单机时等价于原来的"整轮只打一次"；联机时必须逐人记录，
	// 否则先被扫到的那个玩家会吃掉整轮伤害，队友站在同一圈上反而无伤。
	TArray<ARogueCharacter*, TInlineAllocator<4>> Players;
	GatherAlivePlayers(Players);

	const FVector MyLocation = GetActorLocation();
	for (ARogueCharacter* PlayerCharacter : Players)
	{
		const bool bAlreadyHit = GroundWaveHitPlayers.ContainsByPredicate(
			[PlayerCharacter](const TWeakObjectPtr<ARogueCharacter>& Hit) { return Hit.Get() == PlayerCharacter; });
		if (bAlreadyHit || !CanGroundWaveHitPlayer(PlayerCharacter))
		{
			continue;
		}

		const float DistanceToPlayer = FVector::Dist2D(MyLocation, PlayerCharacter->GetActorLocation());
		const float PlayerRadius = PlayerCharacter->GetCapsuleComponent() != nullptr
			? PlayerCharacter->GetCapsuleComponent()->GetScaledCapsuleRadius()
			: 34.0f;
		const float HitBandHalfWidth = GroundWave.HitThickness * 0.5f + PlayerRadius;
		if (FMath::Abs(DistanceToPlayer - Radius) <= HitBandHalfWidth)
		{
			// 地面波是范围伤害，Tag 用 Damage.Source.Area
			RogueAbilitySystem::ApplyDamageToTarget(this, PlayerCharacter, ContactDamage * GroundWave.DamageMultiplier, RogueGameplayTags::Damage_Source_Area);
			GroundWaveHitPlayers.Add(PlayerCharacter);
		}
	}

	OnGroundWaveUpdated(Radius, Alpha);

	if (Alpha >= 1.0f)
	{
		bGroundWaveActive = false;
		GroundWaveHitPlayers.Reset();
		GroundWaveElapsed = 0.0f;
		GroundWaveCurrentRadius = 0.0f;
		GroundWaveAlpha = 0.0f;
		GroundWaveEffectiveMaxRadius = 0.0f;
		GroundWaveEffectiveDuration = 0.0f;
		GroundWaveAttackTimer = GroundWave.AttackCooldown;
		HideGroundWave();
		OnGroundWaveFinished();
	}
}

void ARogueEnemy::UpdateGroundWaveRingVisual(float Radius)
{
	if (!bUseBuiltInShockPillarVisual || GroundWaveRingSegments == nullptr)
	{
		return;
	}

	GroundWaveRingSegments->ClearInstances();
	if (Radius <= KINDA_SMALL_NUMBER)
	{
		GroundWaveRingSegments->SetHiddenInGame(true);
		GroundWaveRingSegments->SetVisibility(false, true);
		return;
	}

	const float Circumference = 2.0f * UE_PI * Radius;
	const int32 SegmentCount = FMath::Clamp(FMath::CeilToInt(Circumference / GroundWaveTargetSegmentLength), GroundWaveMinRingSegments, GroundWaveMaxRingSegments);
	const float SegmentLength = FMath::Max(Circumference / static_cast<float>(SegmentCount) * 0.94f, 12.0f);
	const FVector SegmentScale(
		SegmentLength / UnrealBasicShapeSize,
		GroundWaveRingThickness / UnrealBasicShapeSize,
		GroundWaveRingHeight / UnrealBasicShapeSize);
	const float BaseOffset = GetGroundWaveVisualBaseOffset(*this);

	for (int32 SegmentIndex = 0; SegmentIndex < SegmentCount; ++SegmentIndex)
	{
		const float AngleRadians = 2.0f * UE_PI * static_cast<float>(SegmentIndex) / static_cast<float>(SegmentCount);
		const float AngleDegrees = FMath::RadiansToDegrees(AngleRadians);
		const FVector SegmentLocation(FMath::Cos(AngleRadians) * Radius, FMath::Sin(AngleRadians) * Radius, BaseOffset);
		const FRotator SegmentRotation(0.0f, AngleDegrees + 90.0f, 0.0f);
		GroundWaveRingSegments->AddInstance(FTransform(SegmentRotation, SegmentLocation, SegmentScale));
	}

	GroundWaveRingSegments->SetHiddenInGame(false);
	GroundWaveRingSegments->SetVisibility(true, true);
}

void ARogueEnemy::HideGroundWave()
{
	if (GroundWaveMesh != nullptr)
	{
		GroundWaveMesh->SetHiddenInGame(true);
		GroundWaveMesh->SetVisibility(false, true);
	}

	if (GroundWaveRingSegments != nullptr)
	{
		GroundWaveRingSegments->ClearInstances();
		GroundWaveRingSegments->SetHiddenInGame(true);
		GroundWaveRingSegments->SetVisibility(false, true);
	}
}

bool ARogueEnemy::CanGroundWaveHitPlayer(const ARogueCharacter* PlayerCharacter) const
{
	if (!IsValid(PlayerCharacter) || !CurrentArchetype.GroundWave.bJumpCanDodge)
	{
		return IsValid(PlayerCharacter);
	}

	const UCharacterMovementComponent* PlayerMovement = PlayerCharacter->GetCharacterMovement();
	if (PlayerMovement != nullptr && PlayerMovement->IsFalling())
	{
		return false;
	}

	const UCapsuleComponent* PlayerCapsule = PlayerCharacter->GetCapsuleComponent();
	const float PlayerHalfHeight = PlayerCapsule != nullptr ? PlayerCapsule->GetScaledCapsuleHalfHeight() : 88.0f;
	const float PlayerFeetZ = PlayerCharacter->GetActorLocation().Z - PlayerHalfHeight;
	const float WaveZ = GetActorLocation().Z - GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	return PlayerFeetZ <= WaveZ + 30.0f;
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
	const bool bIsShockPillar = EnemyType == ERogueEnemyType::ShockPillar && CurrentArchetype.GroundWave.bUsesGroundWave;
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

	if (GroundWaveMesh != nullptr)
	{
		if (bUseBuiltInShockPillarVisual && ShockWaveMaterial != nullptr)
		{
			// 同 ShockPillarMesh：MID 只建一次并缓存
			if (CachedShockWaveMID == nullptr)
			{
				CachedShockWaveMID = UMaterialInstanceDynamic::Create(ShockWaveMaterial, this);
				if (CachedShockWaveMID != nullptr)
				{
					const FLinearColor WaveColor(1.0f, 0.62f, 0.08f, 1.0f);
					CachedShockWaveMID->SetVectorParameterValue(TEXT("Color"), WaveColor);
					CachedShockWaveMID->SetVectorParameterValue(TEXT("BaseColor"), WaveColor);
					CachedShockWaveMID->SetVectorParameterValue(TEXT("EmissiveColor"), WaveColor * 3.0f);
				}
			}

			UMaterialInterface* EffectiveWaveMaterial = ShockWaveMaterial;
			if (CachedShockWaveMID != nullptr)
			{
				EffectiveWaveMaterial = CachedShockWaveMID.Get();
			}

			GroundWaveMesh->SetMaterial(0, EffectiveWaveMaterial);
			if (GroundWaveRingSegments != nullptr)
			{
				GroundWaveRingSegments->SetMaterial(0, EffectiveWaveMaterial);
			}
		}
		HideGroundWave();
	}

	GetCapsuleComponent()->SetCapsuleSize(CurrentArchetype.Visual.CapsuleRadius, CurrentArchetype.Visual.CapsuleHalfHeight);
	GetCharacterMovement()->MaxWalkSpeed = bIsShockPillar ? 0.0f : MoveSpeed;
	GetCharacterMovement()->bOrientRotationToMovement = !bIsShockPillar;

	OnApplyVisualStyle();

	/*
	if (BodyMesh == nullptr)
	{
		return;
	}

	const FRogueEnemyVisualArchetype& VisualArchetype = CurrentArchetype.Visual;
	FRogueEnemyVisualResourceSelection VisualSelection = RogueEnemyVisualResources::ResolveSelection(VisualResourceLibrary, VisualArchetype.VisualKey);
	UStaticMesh* SelectedMesh = VisualSelection.Mesh;
	UMaterialInterface* SelectedMaterial = VisualSelection.Material;

	if (VisualArchetype.bUseBossMaterialOverride)
	{
		SelectedMaterial = VisualResourceLibrary.BossMaterial != nullptr ? VisualResourceLibrary.BossMaterial : SelectedMaterial;
	}

	GetCapsuleComponent()->SetCapsuleSize(VisualArchetype.CapsuleRadius, VisualArchetype.CapsuleHalfHeight);

	if (SelectedMesh != nullptr)
	{
		BodyMesh->SetStaticMesh(SelectedMesh);
	}

	if (SelectedMaterial != nullptr)
	{
		BodyMesh->SetMaterial(0, SelectedMaterial);
	}

	BodyMesh->SetRelativeLocation(VisualArchetype.MeshLocation);
	BodyMesh->SetRelativeRotation(VisualArchetype.MeshRotation);
	BodyMesh->SetRelativeScale3D(VisualArchetype.MeshScale);
	GetCharacterMovement()->MaxWalkSpeed = MoveSpeed;

	// 通知蓝图子类可自定义外观
	OnApplyVisualStyle();
	*/
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
