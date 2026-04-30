#include "Enemies/RogueEnemy.h"

#include "Player/RogueCharacter.h"
#include "Core/RogueCombatFeedback.h"
#include "Enemies/RogueEnemyArchetypes.h"
#include "Enemies/RogueEnemyProjectile.h"
#include "Enemies/RogueEnemyVisualResources.h"
#include "Core/RogueGameMode.h"
#include "Subsystems/RogueCombatPoolSubsystem.h"
#include "Subsystems/RogueEnemyTrackerSubsystem.h"
#include "Combat/RogueImpactEffect.h"
#include "World/RogueArena.h"
#include "Components/CapsuleComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
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
	CachedPlayerCharacter = Cast<ARogueCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
	DeactivateToPool();
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
	ARogueCharacter* PlayerCharacter = CachedPlayerCharacter.Get();
	if (!IsValid(PlayerCharacter))
	{
		PlayerCharacter = Cast<ARogueCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
		CachedPlayerCharacter = PlayerCharacter;
	}

	if (!IsValid(PlayerCharacter) || PlayerCharacter->IsDead())
	{
		return;
	}

	const FVector ToPlayer = PlayerCharacter->GetActorLocation() - GetActorLocation();
	const float DistanceToPlayer = ToPlayer.Size2D();
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
		HandleGroundWaveAttack(PlayerCharacter, DistanceToPlayer, DeltaSeconds);
		return;
	}

	TouchPlayer(DeltaSeconds);
	HandleRangedAttack(PlayerCharacter, DistanceToPlayer, DeltaSeconds);
}

// ===========================================================
//  TakeDamage
// ===========================================================

float ARogueEnemy::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	if (bDead || bPoolAvailable)
	{
		return 0.0f;
	}

	// 调用蓝图钩子，允许子类修改伤害
	const float ModifiedDamage = OnDamageTaken(DamageAmount, DamageCauser);
	const float AppliedDamage = FMath::Clamp(ModifiedDamage, 0.0f, CurrentHealth);
	CurrentHealth -= AppliedDamage;

	if (AppliedDamage > 0.0f)
	{
		const float CurrentWorldTime = GetWorld() != nullptr ? GetWorld()->GetTimeSeconds() : 0.0f;
		const float DamageNumberInterval = bIsBoss ? 0.06f : 0.12f;
		if (CurrentWorldTime - LastDamageNumberTime >= DamageNumberInterval)
		{
			LastDamageNumberTime = CurrentWorldTime;
			FVector DamageLocation = GetHealthBarWorldLocation();
			DamageLocation.X += FMath::FRandRange(-24.0f, 24.0f);
			DamageLocation.Y += FMath::FRandRange(-24.0f, 24.0f);
			RogueCombatFeedback::AddDamageNumber(this, DamageLocation, AppliedDamage, bIsBoss);
		}
	}

	if (CurrentHealth <= KINDA_SMALL_NUMBER)
	{
		Die(DamageCauser);
	}

	return AppliedDamage;
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
	bGroundWaveDamagedPlayer = false;
	LastDamageNumberTime = -100.0f;
	bDead = false;
	bPoolAvailable = false;
	SetActorTickInterval(0.0f);
	CachedPlayerCharacter = Cast<ARogueCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
	GetCharacterMovement()->MaxWalkSpeed = MoveSpeed;
	ApplyEnemyStyle();

	// 通知蓝图子类初始化完成
	OnEnemyInitialized(InProfile);
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

	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	}

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
		Movement->Velocity = FVector::ZeroVector;
		Movement->SetMovementMode(MOVE_Walking);
	}

	// 确保有 AI Controller
	if (GetController() == nullptr)
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
	ContactTimer = 0.0f;
	RangedAttackTimer = 0.0f;
	GroundWaveAttackTimer = 0.0f;
	GroundWaveElapsed = 0.0f;
	GroundWaveCurrentRadius = 0.0f;
	GroundWaveAlpha = 0.0f;
	GroundWaveEffectiveMaxRadius = 0.0f;
	GroundWaveEffectiveDuration = 0.0f;
	bGroundWaveActive = false;
	bGroundWaveDamagedPlayer = false;
	BurstCooldown = 0.0f;
	BurstTimeRemaining = 0.0f;
	LastDamageNumberTime = -100.0f;
	CachedPlayerCharacter.Reset();
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
	ARogueCharacter* PlayerCharacter = CachedPlayerCharacter.Get();
	if (!IsValid(PlayerCharacter))
	{
		PlayerCharacter = Cast<ARogueCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
		// 注意：const 方法中不修改 CachedPlayerCharacter，调用方可自行缓存
	}
	return PlayerCharacter;
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
	TouchPlayer(DeltaSeconds);
}

void ARogueEnemy::FireProjectileAtPlayer()
{
	ARogueCharacter* PlayerCharacter = GetPlayerCharacter();
	if (IsValid(PlayerCharacter))
	{
		FireRangedShot(PlayerCharacter);
	}
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

float ARogueEnemy::OnDamageTaken_Implementation(float DamageAmount, AActor* DamageCauser)
{
	// 默认直接返回原始伤害值，蓝图子类可重写以修改伤害
	return DamageAmount;
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

// ===========================================================
//  内部逻辑：接触伤害
// ===========================================================

void ARogueEnemy::TouchPlayer(float DeltaSeconds)
{
	ContactTimer -= DeltaSeconds;

	ARogueCharacter* PlayerCharacter = CachedPlayerCharacter.Get();
	if (!IsValid(PlayerCharacter))
	{
		PlayerCharacter = Cast<ARogueCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
		CachedPlayerCharacter = PlayerCharacter;
	}

	if (!IsValid(PlayerCharacter))
	{
		return;
	}

	const float DamageDistance = GetCapsuleComponent()->GetScaledCapsuleRadius() + PlayerCharacter->GetCapsuleComponent()->GetScaledCapsuleRadius() + 8.0f;
	if (FVector::DistSquared2D(GetActorLocation(), PlayerCharacter->GetActorLocation()) > FMath::Square(DamageDistance))
	{
		return;
	}

	if (ContactTimer <= 0.0f)
	{
		UGameplayStatics::ApplyDamage(PlayerCharacter, ContactDamage, nullptr, this, UDamageType::StaticClass());
		ContactTimer = ContactInterval;
	}
}

// ===========================================================
//  内部逻辑：远程攻击
// ===========================================================

void ARogueEnemy::HandleRangedAttack(ARogueCharacter* PlayerCharacter, float DistanceToPlayer, float DeltaSeconds)
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

	FireRangedShot(PlayerCharacter);
	RangedAttackTimer = RangedArchetype.NextShotCooldown;
}

void ARogueEnemy::FireRangedShot(ARogueCharacter* PlayerCharacter)
{
	if (!IsValid(PlayerCharacter) || GetWorld() == nullptr)
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
		URogueCombatPoolSubsystem* Pools = GetWorld() ? GetWorld()->GetSubsystem<URogueCombatPoolSubsystem>() : nullptr;
		ARogueEnemyProjectile* Projectile = Pools != nullptr
			? Pools->AcquireEnemyProjectile(ARogueEnemyProjectile::StaticClass(), this, this, SpawnLocation, Direction.Rotation())
			: GetWorld()->SpawnActor<ARogueEnemyProjectile>(ARogueEnemyProjectile::StaticClass(), SpawnLocation, Direction.Rotation(), SpawnParameters);
		if (Projectile != nullptr)
		{
			Projectile->ActivatePooledProjectile(this, this, SpawnLocation, Direction.Rotation(), Direction, ProjectileSpeed, ProjectileDamage, bLargeVisual);
		}
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
		ARogueImpactEffect::SpawnImpactEffect(
			GetWorld(),
			ShotOrigin,
			ShotDirection.Rotation(),
			ERogueImpactVisualStyle::Spark,
			RangedArchetype.MuzzleEffectScale,
			RangedArchetype.MuzzleEffectLifetime,
			this);
	}
}

// ===========================================================
//  内部逻辑：地面波攻击
// ===========================================================

void ARogueEnemy::HandleGroundWaveAttack(ARogueCharacter* PlayerCharacter, float DistanceToPlayer, float DeltaSeconds)
{
	if (!CurrentArchetype.GroundWave.bUsesGroundWave || !IsValid(PlayerCharacter))
	{
		return;
	}

	if (!bGroundWaveActive)
	{
		GroundWaveAttackTimer = FMath::Max(0.0f, GroundWaveAttackTimer - DeltaSeconds);
		const float EffectiveMaxRadius = CalculateFullMapGroundWaveRadius(*this, CurrentArchetype.GroundWave.MaxRadius);
		if (GroundWaveAttackTimer <= 0.0f && DistanceToPlayer <= EffectiveMaxRadius + 220.0f)
		{
			StartGroundWave();
		}
	}

	if (bGroundWaveActive)
	{
		UpdateGroundWave(DeltaSeconds, PlayerCharacter);
	}
}

void ARogueEnemy::StartGroundWave()
{
	bGroundWaveActive = true;
	bGroundWaveDamagedPlayer = false;
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
		ARogueImpactEffect::SpawnImpactEffect(
			GetWorld(),
			GetActorLocation() + FVector(0.0f, 0.0f, 8.0f),
			FRotator::ZeroRotator,
			ERogueImpactVisualStyle::Spark,
			FVector(0.95f, 0.95f, 0.32f),
			0.28f,
			this);
	}

	OnGroundWaveStarted(GroundWaveEffectiveMaxRadius, GroundWaveEffectiveDuration);
}

void ARogueEnemy::UpdateGroundWave(float DeltaSeconds, ARogueCharacter* PlayerCharacter)
{
	const FRogueEnemyGroundWaveArchetype& GroundWave = CurrentArchetype.GroundWave;
	const float Duration = FMath::Max(GroundWaveEffectiveDuration, KINDA_SMALL_NUMBER);
	GroundWaveElapsed += DeltaSeconds;

	const float Alpha = FMath::Clamp(GroundWaveElapsed / Duration, 0.0f, 1.0f);
	const float Radius = FMath::Lerp(24.0f, FMath::Max(GroundWaveEffectiveMaxRadius, 24.0f), Alpha);
	GroundWaveCurrentRadius = Radius;
	GroundWaveAlpha = Alpha;

	UpdateGroundWaveRingVisual(Radius);

	if (!bGroundWaveDamagedPlayer && IsValid(PlayerCharacter) && CanGroundWaveHitPlayer(PlayerCharacter))
	{
		const float DistanceToPlayer = FVector::Dist2D(GetActorLocation(), PlayerCharacter->GetActorLocation());
		const float PlayerRadius = PlayerCharacter->GetCapsuleComponent() != nullptr
			? PlayerCharacter->GetCapsuleComponent()->GetScaledCapsuleRadius()
			: 34.0f;
		const float HitBandHalfWidth = GroundWave.HitThickness * 0.5f + PlayerRadius;
		if (FMath::Abs(DistanceToPlayer - Radius) <= HitBandHalfWidth)
		{
			UGameplayStatics::ApplyDamage(PlayerCharacter, ContactDamage * GroundWave.DamageMultiplier, nullptr, this, UDamageType::StaticClass());
			bGroundWaveDamagedPlayer = true;
		}
	}

	OnGroundWaveUpdated(Radius, Alpha);

	if (Alpha >= 1.0f)
	{
		bGroundWaveActive = false;
		bGroundWaveDamagedPlayer = false;
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
				UMaterialInstanceDynamic* PillarMaterialInstance = UMaterialInstanceDynamic::Create(ShockPillarMaterial, this);
				if (PillarMaterialInstance != nullptr)
				{
					const FLinearColor PillarColor(1.0f, 0.78f, 0.05f, 1.0f);
					PillarMaterialInstance->SetVectorParameterValue(TEXT("Color"), PillarColor);
					PillarMaterialInstance->SetVectorParameterValue(TEXT("BaseColor"), PillarColor);
					PillarMaterialInstance->SetVectorParameterValue(TEXT("EmissiveColor"), PillarColor * 2.2f);
					ShockPillarMesh->SetMaterial(0, PillarMaterialInstance);
				}
				else
				{
					ShockPillarMesh->SetMaterial(0, ShockPillarMaterial);
				}
			}
		}
	}

	if (GroundWaveMesh != nullptr)
	{
		if (bUseBuiltInShockPillarVisual && ShockWaveMaterial != nullptr)
		{
			UMaterialInterface* EffectiveWaveMaterial = ShockWaveMaterial;
			UMaterialInstanceDynamic* WaveMaterialInstance = UMaterialInstanceDynamic::Create(ShockWaveMaterial, this);
			if (WaveMaterialInstance != nullptr)
			{
				const FLinearColor WaveColor(1.0f, 0.62f, 0.08f, 1.0f);
				WaveMaterialInstance->SetVectorParameterValue(TEXT("Color"), WaveColor);
				WaveMaterialInstance->SetVectorParameterValue(TEXT("BaseColor"), WaveColor);
				WaveMaterialInstance->SetVectorParameterValue(TEXT("EmissiveColor"), WaveColor * 3.0f);
				EffectiveWaveMaterial = WaveMaterialInstance;
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
