#include "Enemies/RogueEnemy.h"

#include "Player/RogueCharacter.h"
#include "Core/RogueCombatFeedback.h"
#include "Enemies/RogueEnemyArchetypes.h"
#include "Enemies/RogueEnemyProjectile.h"
#include "Enemies/RogueEnemyVisualResources.h"
#include "Core/RogueGameMode.h"
#include "Combat/RogueImpactEffect.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"

ARogueEnemy::ARogueEnemy()
{
	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(34.0f, 72.0f);
	GetCharacterMovement()->GravityScale = 0.0f;
	GetCharacterMovement()->MaxWalkSpeed = MoveSpeed;
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 600.0f, 0.0f);

	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	BodyMesh->SetupAttachment(RootComponent);
	BodyMesh->SetRelativeLocation(FVector(0.0f, 0.0f, -44.0f));
	BodyMesh->SetRelativeScale3D(FVector(0.7f, 0.7f, 1.1f));
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BodyMesh->SetCastShadow(false);

	RogueEnemyVisualResources::LoadDefaultResources(VisualResourceLibrary);
	if (VisualResourceLibrary.HunterMesh != nullptr)
	{
		BodyMesh->SetStaticMesh(VisualResourceLibrary.HunterMesh);
	}

	if (VisualResourceLibrary.HunterMaterial != nullptr)
	{
		BodyMesh->SetMaterial(0, VisualResourceLibrary.HunterMaterial);
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
		if (ARogueGameMode* RogueGameMode = World->GetAuthGameMode<ARogueGameMode>())
		{
			RogueGameMode->UnregisterEnemy(this);
		}
	}

	Super::EndPlay(EndPlayReason);
}

void ARogueEnemy::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bDead || bPoolAvailable)
	{
		return;
	}

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
	const float DesiredTickInterval = bIsBoss ? 0.0f : (DistanceToPlayer > 1700.0f ? 0.08f : (DistanceToPlayer > 950.0f ? 0.04f : 0.0f));
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

	TouchPlayer(DeltaSeconds);
	HandleRangedAttack(PlayerCharacter, DistanceToPlayer, DeltaSeconds);
}

float ARogueEnemy::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	if (bDead || bPoolAvailable)
	{
		return 0.0f;
	}

	const float AppliedDamage = FMath::Clamp(DamageAmount, 0.0f, CurrentHealth);
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

void ARogueEnemy::InitializeEnemy(const FRogueEnemyProfile& InProfile)
{
	EnemyType = InProfile.EnemyType;
	MaxHealth = InProfile.Health;
	CurrentHealth = MaxHealth;
	MoveSpeed = InProfile.Speed;
	ContactDamage = InProfile.Damage;
	ExperienceReward = InProfile.ExperienceReward;
	bIsBoss = InProfile.bIsBoss;
	CurrentArchetype = RogueEnemyArchetypes::BuildEnemyArchetype(EnemyType, bIsBoss);
	BurstCooldown = CurrentArchetype.Movement.Model == ERogueEnemyMovementModel::BurstCharge ? FMath::FRandRange(0.8f, 1.8f) : 0.0f;
	BurstTimeRemaining = 0.0f;
	ContactTimer = 0.0f;
	RangedAttackTimer = CurrentArchetype.Ranged.bUsesRangedAttack
		? FMath::FRandRange(CurrentArchetype.Ranged.InitialCooldownMin, CurrentArchetype.Ranged.InitialCooldownMax)
		: 0.0f;
	LastDamageNumberTime = -100.0f;
	bDead = false;
	bPoolAvailable = false;
	SetActorTickInterval(0.0f);
	CachedPlayerCharacter = Cast<ARogueCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
	GetCharacterMovement()->MaxWalkSpeed = MoveSpeed;
	ApplyEnemyStyle();
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
}

void ARogueEnemy::DeactivateToPool()
{
	if (!bPoolAvailable)
	{
		if (UWorld* World = GetWorld())
		{
			if (ARogueGameMode* RogueGameMode = World->GetAuthGameMode<ARogueGameMode>())
			{
				RogueGameMode->UnregisterEnemy(this);
			}
		}
	}

	bPoolAvailable = true;
	bDead = true;
	CurrentHealth = 0.0f;
	ContactTimer = 0.0f;
	RangedAttackTimer = 0.0f;
	BurstCooldown = 0.0f;
	BurstTimeRemaining = 0.0f;
	LastDamageNumberTime = -100.0f;
	CachedPlayerCharacter.Reset();
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
		ARogueGameMode* RogueGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ARogueGameMode>() : nullptr;
		ARogueEnemyProjectile* Projectile = RogueGameMode != nullptr
			? RogueGameMode->AcquireEnemyProjectile(ARogueEnemyProjectile::StaticClass(), this, this, SpawnLocation, Direction.Rotation())
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

void ARogueEnemy::Die(AActor* Killer)
{
	bDead = true;

	if (ARogueGameMode* RogueGameMode = GetWorld()->GetAuthGameMode<ARogueGameMode>())
	{
		RogueGameMode->HandleEnemyKilled(this, Killer);
	}

	DeactivateToPool();
}

void ARogueEnemy::ApplyEnemyStyle()
{
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
}

FVector ARogueEnemy::GetHealthBarWorldLocation() const
{
	const float VerticalOffset = GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + (bIsBoss ? 92.0f : 56.0f);
	return GetActorLocation() + FVector(0.0f, 0.0f, VerticalOffset);
}

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
