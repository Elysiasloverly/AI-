#include "World/RogueExperiencePickup.h"

#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

#include "Player/RogueCharacter.h"
#include "Player/RoguePlayerQuery.h"
#include "Combat/Targeting/RogueEnemyTrackerSubsystem.h"
#include "Components/SphereComponent.h"
#include "GameFramework/PlayerController.h"

ARogueExperiencePickup::ARogueExperiencePickup()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.05f;

	// 经验球由服务端获取并激活，通过 Actor 复制让相关客户端建立对应实例。
	// 预热量和初始网络开销取决于配置与测量，本模块不据固定池大小推导带宽。
	bReplicates = true;

	// 服务端计算磁吸位移，客户端通过标准移动复制跟随根组件位置/旋转。
	// FRepMovement 不携带缩放，所以 ExperienceValue 单独复制，两侧各自换算外观尺寸。
	SetReplicatingMovement(true);

	// 只建碰撞：网格、特效、灯光全部由 BP 子类用 SCS 组件添加，
	// 代码里不出现任何资产路径，换外观不需要改 C++。
	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	Collision->InitSphereRadius(40.0f);
	Collision->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	RootComponent = Collision;

	Collision->OnComponentBeginOverlap.AddDynamic(this, &ARogueExperiencePickup::HandleOverlap);
}

void ARogueExperiencePickup::BeginPlay()
{
	Super::BeginPlay();

	// 服务端出生后归池；客户端保留初始复制值，并在下方重建本地表现。
	// DeactivateToPool 已限制活跃位仅由权威端写入，但客户端无条件归池仍会隐藏活跃实例。
	if (HasAuthority())
	{
		DeactivateToPool();
	}

	// 客户端：按复制过来的"在场"状态落地（初始复制值也走这条路）。
	ApplyReplicatedActiveState();
}

void ARogueExperiencePickup::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// 本类显式复制活跃位与经验值；标准移动载荷负责位姿，不含经验值与缩放。
	DOREPLIFETIME_CONDITION(ARogueExperiencePickup, bReplicatedActive, COND_None);
	DOREPLIFETIME_CONDITION(ARogueExperiencePickup, ExperienceValue, COND_None);
}

void ARogueExperiencePickup::OnRep_ReplicatedActive()
{
	ApplyReplicatedActiveState();
}

void ARogueExperiencePickup::OnRep_ExperienceValue()
{
	// 合并会让经验值变大，客户端要跟着改尺寸，否则队友看到的球一直是旧大小。
	ApplyExperienceScale();

	if (!bPoolAvailable)
	{
		OnPickupMerged(ExperienceValue);
	}
}

bool ARogueExperiencePickup::IsNetRelevantFor(const AActor* RealViewer, const AActor* ViewTarget, const FVector& SrcLocation) const
{
	// AActor 默认会剔除隐藏且无碰撞的对象；归池后仍需把失活状态送到附近客户端。
	// 保留距离剔除，避免远处的池对象向所有玩家复制。
	if (bPoolAvailable && IsWithinNetRelevancyDistance(SrcLocation))
	{
		return true;
	}

	return Super::IsNetRelevantFor(RealViewer, ViewTarget, SrcLocation);
}

bool ARogueExperiencePickup::ShouldSimplifyVisuals() const
{
	// GetWorld() 在 const 成员里返回非 const 指针，WorldSubsystem 查询不受影响。
	UWorld* World = GetWorld();
	const URogueEnemyTrackerSubsystem* Tracker = World != nullptr
		? World->GetSubsystem<URogueEnemyTrackerSubsystem>()
		: nullptr;

	return Tracker != nullptr && Tracker->ShouldCullCombatEffects();
}

void ARogueExperiencePickup::ApplyReplicatedActiveState()
{
	// 服务端自己就是权威，状态由 ActivatePooledPickup / DeactivateToPool 维护
	if (HasAuthority())
	{
		return;
	}

	// 刚复制出来的池对象本来是"未激活"，此时不该补发一次回收通知。
	const bool bWasActive = !bPoolAvailable;

	if (bReplicatedActive)
	{
		bPoolAvailable = false;
		SetActorHiddenInGame(false);
		SetActorTickEnabled(true);
		SetActorEnableCollision(true);
		if (Collision != nullptr)
		{
			// 客户端只需要能被查询到（本地表现靠 Tick，拾取判定只在服务端）
			Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		}

		// 复制可能晚于一帧到达，入场时按当前经验值补一次尺寸与外观初始化。
		ApplyExperienceScale();
		OnPickupActivated(ExperienceValue);
	}
	else
	{
		bPoolAvailable = true;
		SetActorHiddenInGame(true);
		SetActorTickEnabled(false);
		SetActorEnableCollision(false);
		if (Collision != nullptr)
		{
			Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}

		if (bWasActive)
		{
			OnPickupDeactivated();
		}
	}
}

void ARogueExperiencePickup::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		if (URogueEnemyTrackerSubsystem* Tracker = World->GetSubsystem<URogueEnemyTrackerSubsystem>())
		{
			Tracker->UnregisterExperiencePickup(this);
		}
	}

	Super::EndPlay(EndPlayReason);
}

void ARogueExperiencePickup::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bPoolAvailable)
	{
		return;
	}

	// 悬浮、缩放脉冲、拖尾这类表现由 BP 自己驱动（BP 的 Event Tick 与本函数挂在同一个
	// Actor Tick 上，回池时 SetActorTickEnabled(false) 会一起停掉，不需要额外开关）。

	// ── 以下是权威逻辑：只在服务端跑 ──
	// 生命周期、磁吸位移都由服务端决定，客户端只接收复制结果。
	// 如果客户端也跑磁吸，它会和复制过来的位置打架（抖动/回弹）。
	if (!HasAuthority())
	{
		return;
	}

	ActiveTimeSeconds += DeltaSeconds;
	if (ActiveTimeSeconds >= PickupLifetime)
	{
		DeactivateToPool();
		return;
	}

	// 每次本 Actor 的 Tick 重新挑目标，以响应玩家移动、濒死或退出；默认 Tick 间隔为 0.05 秒。
	// 缓存一个"玩家 0"会让球朝错误的人飞（而且位移是复制的，全队都会看到球跑偏）。
	ARogueCharacter* MagnetTarget = FindMagnetTarget();
	if (!IsValid(MagnetTarget))
	{
		return;
	}

	const FVector NewLocation = FMath::VInterpConstantTo(GetActorLocation(), MagnetTarget->GetActorLocation(), DeltaSeconds, AttractionSpeed);
	SetActorLocation(NewLocation);
	// 移动可能已触发重叠并归池；不要继续更新已被收取的球。
	if (bPoolAvailable)
	{
		return;
	}

	// 开启碰撞时已在玩家内部或没有重叠事件时，抵达后仍应由服务端完成收取。
	if (FVector::DistSquared(GetActorLocation(), MagnetTarget->GetActorLocation()) <= FMath::Square(Collision->GetScaledSphereRadius()))
	{
		TryCollect(MagnetTarget);
	}
}

ARogueCharacter* ARogueExperiencePickup::FindMagnetTarget() const
{
	const UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return nullptr;
	}

	// 超过自动吸附延迟就不再挑半径，直接吸向最近的活着的玩家
	// （等价于旧版把 PickupRange 放大到 100000，但目标换成了"最近的人"而不是玩家 0）。
	const bool bIgnorePickupRange = ActiveTimeSeconds >= AutoMagnetDelay;
	const FVector MyLocation = GetActorLocation();

	// 存活玩家列表统一从 RoguePlayerQuery 取（口径只有一份，见那里的说明）
	TArray<ARogueCharacter*, TInlineAllocator<4>> AlivePlayers;
	RoguePlayerQuery::CollectAlivePlayers(World, AlivePlayers);

	ARogueCharacter* BestTarget = nullptr;
	float BestDistanceSquared = TNumericLimits<float>::Max();

	for (ARogueCharacter* Candidate : AlivePlayers)
	{
		const float DistanceSquared = FVector::DistSquared2D(MyLocation, Candidate->GetActorLocation());

		// 半径用**这个玩家自己的** PickupRadius 判定：
		// 升级点过拾取半径的玩家，磁吸范围理应比队友大，而不是被队友的半径限制住。
		if (!bIgnorePickupRange && DistanceSquared > FMath::Square(Candidate->GetPickupRadius()))
		{
			continue;
		}

		if (DistanceSquared < BestDistanceSquared)
		{
			BestDistanceSquared = DistanceSquared;
			BestTarget = Candidate;
		}
	}

	return BestTarget;
}

void ARogueExperiencePickup::ApplyExperienceScale()
{
	// 尺寸跟着经验值走：BP 的网格/灯光都挂在根组件下，会一起放大。
	// 注意这里同时放大了碰撞球半径，与磁吸抵达判定、BP 手感的观感一致。
	SetActorScale3D(FVector(FMath::Clamp(1.0f + static_cast<float>(ExperienceValue) * 0.015f, 1.0f, 2.4f)));
}

void ARogueExperiencePickup::InitializePickup(int32 InExperienceValue)
{
	ExperienceValue = FMath::Max(1, InExperienceValue);
	ActiveTimeSeconds = 0.0f;
	ApplyExperienceScale();
	OnPickupActivated(ExperienceValue);
}

void ARogueExperiencePickup::AddExperienceValue(int32 InExperienceValue)
{
	ExperienceValue += InExperienceValue;
	ApplyExperienceScale();

	// 合并后刷新寿命与自动磁吸计时，避免新经验随旧球立即过期。
	ActiveTimeSeconds = 0.0f;

	OnPickupMerged(ExperienceValue);
}

void ARogueExperiencePickup::ActivatePooledPickup(AActor* InOwner, const FVector& SpawnLocation, int32 InExperienceValue)
{
	if (!HasAuthority())
	{
		return;
	}

	// 先完整设置本次激活的数据，再开启会同步触发重叠回调的碰撞。
	SetActorEnableCollision(false);
	SetOwner(InOwner);
	SetActorLocation(SpawnLocation, false, nullptr, ETeleportType::TeleportPhysics);
	bPoolAvailable = false;
	bReplicatedActive = true;
	SetActorHiddenInGame(false);
	SetActorTickEnabled(true);
	if (Collision != nullptr)
	{
		Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	}
	// 经验值与外观必须排在开启碰撞之前：重叠回调可能立刻把球收走并归池。
	InitializePickup(InExperienceValue);
	SetActorEnableCollision(true);
	// 碰撞回调可能已将球收取归池，此后不能再覆盖活跃位。
	ForceNetUpdate();
}

void ARogueExperiencePickup::DeactivateToPool()
{
	const bool bWasActive = !bPoolAvailable;

	if (bWasActive)
	{
		if (UWorld* World = GetWorld())
		{
			if (URogueEnemyTrackerSubsystem* Tracker = World->GetSubsystem<URogueEnemyTrackerSubsystem>())
			{
				Tracker->UnregisterExperiencePickup(this);
			}
		}
	}

	bPoolAvailable = true;
	ActiveTimeSeconds = 0.0f;
	if (HasAuthority())
	{
		// 权威独占的状态位：客户端写它会覆盖刚复制到的真实值（见 BeginPlay 的说明）
		bReplicatedActive = false;
		ExperienceValue = 1;
	}
	SetActorHiddenInGame(true);
	SetActorTickEnabled(false);
	SetActorEnableCollision(false);
	SetActorScale3D(FVector::OneVector);
	if (Collision != nullptr)
	{
		Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	SetOwner(nullptr);

	// 只有真的从"在场"变"回池"才通知，避免出生时的初始化归池触发一次空回收。
	if (bWasActive)
	{
		OnPickupDeactivated();
	}

	if (HasAuthority())
	{
		ForceNetUpdate();
	}
}

bool ARogueExperiencePickup::TryCollect(ARogueCharacter* PlayerCharacter)
{
	if (!HasAuthority() || bPoolAvailable || !IsValid(PlayerCharacter) || !PlayerCharacter->IsAlive())
	{
		return false;
	}

	// 先锁定收取结果并归池，再发经验；升级通知或重复重叠不能再次发奖。
	const int32 CollectedExperience = ExperienceValue;
	DeactivateToPool();
	PlayerCharacter->AddExperience(CollectedExperience);
	return true;
}

void ARogueExperiencePickup::HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	TryCollect(Cast<ARogueCharacter>(OtherActor));
}
