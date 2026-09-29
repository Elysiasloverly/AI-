#include "World/RogueExperiencePickup.h"

#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

#include "Player/RogueCharacter.h"
#include "Core/RoguePlayerQuery.h"
#include "Subsystems/RogueEnemyTrackerSubsystem.h"
#include "Components/PointLightComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

ARogueExperiencePickup::ARogueExperiencePickup()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.05f;

	// ── 复制 ──
	// 经验球由服务端在敌人死亡时生成，不开复制的话客户端**看不到任何经验球**。
	//
	// 与对象池的冲突已核对过，代价可接受：
	//   · 预热量 64（InitialExperiencePickupPoolSize），预热的球是隐藏+停 Tick 的，
	//     它们会在开局作为隐藏 actor 复制给客户端 —— 一次性开销约十几 KB，可忽略；
	//   · 客户端的池是空的（Prewarm 只在 GameMode.BeginPlay 跑，那是服务端），
	//     所以不会出现"客户端自己池里那份 + 服务端复制那份"的双份球。
	bReplicates = true;

	// 位置也要复制：磁吸位移是**服务端**算的（见 Tick），客户端必须跟着动。
	// 光开 bReplicates 只复制状态，不保证复制位移 —— 漏了这条的表现就是
	// "服务端把球吸走了、客户端那颗球原地不动，然后突然消失"，看起来像"不会吸经验球"。
	// （APawn 在构造函数里显式调了它，说明普通 Actor 的默认值并不保险，这里显式声明。）
	SetReplicatingMovement(true);

	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	Collision->InitSphereRadius(40.0f);
	Collision->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	RootComponent = Collision;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(RootComponent);
	Mesh->SetRelativeScale3D(FVector(0.24f, 0.24f, 0.24f));
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(false);

	GlowMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GlowMesh"));
	GlowMesh->SetupAttachment(RootComponent);
	GlowMesh->SetRelativeScale3D(FVector(0.46f, 0.46f, 0.46f));
	GlowMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GlowMesh->SetCastShadow(false);

	GlowLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("GlowLight"));
	GlowLight->SetupAttachment(RootComponent);
	GlowLight->SetIntensity(780.0f);
	GlowLight->SetLightColor(FLinearColor(0.14f, 0.72f, 1.0f));
	GlowLight->SetAttenuationRadius(125.0f);
	GlowLight->SetSourceRadius(12.0f);
	GlowLight->SetSoftSourceRadius(16.0f);
	GlowLight->SetUseInverseSquaredFalloff(false);
	GlowLight->SetCastShadows(false);
	GlowLight->SetVisibility(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PickupMesh(TEXT("/Game/GameAssets/StarterContent/Shapes/Shape_Sphere.Shape_Sphere"));
	if (PickupMesh.Succeeded())
	{
		Mesh->SetStaticMesh(PickupMesh.Object);
		GlowMesh->SetStaticMesh(PickupMesh.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> PickupMaterial(TEXT("/Game/GameAssets/Realistic_Starter_VFX_Pack_Vol2/Materials/M_Glow_C_Inst.M_Glow_C_Inst"));
	if (PickupMaterial.Succeeded())
	{
		Mesh->SetMaterial(0, PickupMaterial.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> PickupGlowMaterial(TEXT("/Game/GameAssets/Vefects/Stylized_Galaxy_Shader/Galaxy/Materials/MI_VFX_Lush_Galaxy_Shader_01.MI_VFX_Lush_Galaxy_Shader_01"));
	if (PickupGlowMaterial.Succeeded())
	{
		GlowMesh->SetMaterial(0, PickupGlowMaterial.Object);
	}

	Collision->OnComponentBeginOverlap.AddDynamic(this, &ARogueExperiencePickup::HandleOverlap);
}

void ARogueExperiencePickup::BeginPlay()
{
	Super::BeginPlay();

	// 服务端：池化对象出生即在池里。
	// ⚠️ 客户端不能调 DeactivateToPool()：它会写本地的 bReplicatedActive（权威独占的复制位），
	// 把刚复制到的真实值覆盖成 false，于是客户端永远看不见这颗球。
	// 与 ARogueEnemy::BeginPlay 是同一个坑，详见那边的注释。
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

	// 只复制"在不在场"这一位（位置/缩放交给标准的移动复制）
	DOREPLIFETIME_CONDITION(ARogueExperiencePickup, bReplicatedActive, COND_None);
}

void ARogueExperiencePickup::OnRep_ReplicatedActive()
{
	ApplyReplicatedActiveState();
}

void ARogueExperiencePickup::ApplyReplicatedActiveState()
{
	// 服务端自己就是权威，状态由 ActivatePooledPickup / DeactivateToPool 维护
	if (HasAuthority())
	{
		return;
	}

	if (bReplicatedActive)
	{
		bPoolAvailable = false;
		bVisualsSimplified = false;
		SetActorHiddenInGame(false);
		SetActorTickEnabled(true);
		SetActorEnableCollision(true);
		if (Collision != nullptr)
		{
			// 客户端只需要能被查询到（本地表现靠 Tick，拾取判定只在服务端）
			Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		}
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

	URogueEnemyTrackerSubsystem* Tracker = GetWorld() ? GetWorld()->GetSubsystem<URogueEnemyTrackerSubsystem>() : nullptr;
	const bool bCullCombatEffects = Tracker != nullptr && Tracker->ShouldCullCombatEffects();
	float HoverAlpha = 0.5f;

	// ── 视觉部分：所有端都跑 ──
	// 悬浮/脉冲是纯本地表现，客户端也要动，否则复制过来的球是死的。
	if (bCullCombatEffects)
	{
		if (!bVisualsSimplified)
		{
			bVisualsSimplified = true;
			Mesh->SetRelativeLocation(FVector::ZeroVector);
			GlowMesh->SetRelativeLocation(FVector::ZeroVector);
			GlowMesh->SetRelativeScale3D(FVector(0.40f, 0.40f, 0.40f));
		}
	}
	else
	{
		bVisualsSimplified = false;
		PulseTime += DeltaSeconds;
		HoverAlpha = 0.5f + 0.5f * FMath::Sin(PulseTime * 3.6f);
		const float HoverOffset = FMath::Sin(PulseTime * 2.0f) * 10.0f;
		Mesh->SetRelativeLocation(FVector(0.0f, 0.0f, HoverOffset));
		GlowMesh->SetRelativeLocation(FVector(0.0f, 0.0f, HoverOffset));
	}

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

	// 目标必须每帧重新挑：联机时玩家会跑动/死亡/退出，
	// 缓存一个"玩家 0"会让球朝错误的人飞（而且位移是复制的，全队都会看到球跑偏）。
	ARogueCharacter* MagnetTarget = FindMagnetTarget();
	if (!IsValid(MagnetTarget))
	{
		if (!bCullCombatEffects)
		{
			GlowMesh->SetRelativeScale3D(FVector(0.40f + HoverAlpha * 0.10f, 0.40f + HoverAlpha * 0.10f, 0.40f + HoverAlpha * 0.10f));
		}
		return;
	}

	const FVector NewLocation = FMath::VInterpConstantTo(GetActorLocation(), MagnetTarget->GetActorLocation(), DeltaSeconds, AttractionSpeed);
	SetActorLocation(NewLocation);
	if (!bCullCombatEffects)
	{
		const float AttractionAlpha = 0.5f + 0.5f * FMath::Sin(PulseTime * 8.0f);
		GlowMesh->SetRelativeScale3D(FVector(0.42f + AttractionAlpha * 0.12f, 0.42f + AttractionAlpha * 0.12f, 0.42f + AttractionAlpha * 0.12f));
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

void ARogueExperiencePickup::InitializePickup(int32 InExperienceValue)
{
	ExperienceValue = FMath::Max(1, InExperienceValue);
	PulseTime = 0.0f;
	ActiveTimeSeconds = 0.0f;
	SetActorScale3D(FVector(FMath::Clamp(1.0f + static_cast<float>(ExperienceValue) * 0.015f, 1.0f, 2.4f)));
}

void ARogueExperiencePickup::AddExperienceValue(int32 InExperienceValue)
{
	ExperienceValue += InExperienceValue;
	SetActorScale3D(FVector(FMath::Clamp(1.0f + static_cast<float>(ExperienceValue) * 0.015f, 1.0f, 2.4f)));
}

void ARogueExperiencePickup::ActivatePooledPickup(AActor* InOwner, const FVector& SpawnLocation, int32 InExperienceValue)
{
	SetOwner(InOwner);
	SetActorLocation(SpawnLocation, false, nullptr, ETeleportType::TeleportPhysics);
	SetActorHiddenInGame(false);
	SetActorTickEnabled(true);
	SetActorEnableCollision(true);
	if (Collision != nullptr)
	{
		Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	}

	bPoolAvailable = false;
	bVisualsSimplified = false;
	InitializePickup(InExperienceValue);

	// 通知客户端这颗球上场了
	bReplicatedActive = true;
}

void ARogueExperiencePickup::DeactivateToPool()
{
	if (!bPoolAvailable)
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
	bVisualsSimplified = false;
	PulseTime = 0.0f;
	ActiveTimeSeconds = 0.0f;
	if (HasAuthority())
	{
		// 权威独占的状态位：客户端写它会覆盖刚复制到的真实值（见 BeginPlay 的说明）
		bReplicatedActive = false;
	}
	ExperienceValue = 1;
	SetActorHiddenInGame(true);
	SetActorTickEnabled(false);
	SetActorEnableCollision(false);
	SetActorScale3D(FVector::OneVector);
	if (Collision != nullptr)
	{
		Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	SetOwner(nullptr);
}

void ARogueExperiencePickup::HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (bPoolAvailable)
	{
		return;
	}

	ARogueCharacter* PlayerCharacter = Cast<ARogueCharacter>(OtherActor);
	if (!IsValid(PlayerCharacter))
	{
		return;
	}

	// 拾取是**权威操作**：加经验与回收都要在服务端做。
	// 客户端本地碰到球什么都不做 —— 球的位置与存活由服务端复制过来。
	// （AddExperience 内部也有权限检查，这里是第一道闸。）
	if (!HasAuthority())
	{
		return;
	}

	PlayerCharacter->AddExperience(ExperienceValue);
	DeactivateToPool();
}
