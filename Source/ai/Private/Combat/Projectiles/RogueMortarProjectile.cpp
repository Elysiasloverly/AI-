#include "Combat/Projectiles/RogueMortarProjectile.h"

#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"

ARogueMortarProjectile::ARogueMortarProjectile()
{
	// 落点指示圈由 BP 的 SCS 提供；C++ 不再创建或加载占位网格。
}

void ARogueMortarProjectile::BeginPlay()
{
	Super::BeginPlay();

	TArray<UStaticMeshComponent*> MeshComponents;
	GetComponents(MeshComponents);
	for (UStaticMeshComponent* Candidate : MeshComponents)
	{
		if (Candidate != nullptr
			&& (Candidate->ComponentHasTag(TEXT("RogueVisual.LandingIndicator"))
				|| Candidate->GetFName().ToString().StartsWith(TEXT("LandingIndicatorMesh"))))
		{
			LandingIndicatorMesh = Candidate;
			LandingIndicatorMesh->SetUsingAbsoluteLocation(true);
			LandingIndicatorMesh->SetUsingAbsoluteRotation(true);
			LandingIndicatorMesh->SetUsingAbsoluteScale(true);
			break;
		}
	}

	if (LandingIndicatorMesh != nullptr)
	{
		LandingIndicatorMesh->SetHiddenInGame(true);
	}
}

void ARogueMortarProjectile::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (IsAvailableInPool() || !bMortarProjectileActive)
	{
		return;
	}

	UpdateLandingIndicator(DeltaSeconds);
}

void ARogueMortarProjectile::ActivatePooledMortar(
	AActor* InOwner,
	APawn* InInstigator,
	const FVector& SpawnLocation,
	const FVector& InTargetLocation,
	float InDamage,
	float InExplosionRadius,
	float InLaunchSpeed)
{
	const FVector ArcTargetLocation = InTargetLocation + FVector(0.0f, 0.0f, 12.0f);
	const FVector TargetOffset = ArcTargetLocation - SpawnLocation;
	const FVector HorizontalOffset(TargetOffset.X, TargetOffset.Y, 0.0f);
	const float HorizontalDistance = HorizontalOffset.Size();

	FVector HorizontalDirection = HorizontalDistance > KINDA_SMALL_NUMBER
		? HorizontalOffset / HorizontalDistance
		: FVector::ZeroVector;
	if (HorizontalDirection.IsNearlyZero())
	{
		if (IsValid(InOwner))
		{
			HorizontalDirection = InOwner->GetActorForwardVector().GetSafeNormal2D();
		}
		if (HorizontalDirection.IsNearlyZero())
		{
			HorizontalDirection = FVector::ForwardVector;
		}
	}

	const float RequestedHorizontalSpeed = FMath::Max(MinimumLaunchSpeed, InLaunchSpeed);
	const float ArcTimeMultiplier = FMath::Lerp(0.85f, 1.35f, FMath::Clamp(MortarArcParam, 0.0f, 1.0f));
	const float RawFlightTime = HorizontalDistance > KINDA_SMALL_NUMBER
		? HorizontalDistance / RequestedHorizontalSpeed * ArcTimeMultiplier
		: MinimumFlightTime;
	const float FlightTime = FMath::Clamp(RawFlightTime, MinimumFlightTime, FMath::Max(MinimumFlightTime, MaximumFlightTime));
	const float HorizontalSpeed = HorizontalDistance > KINDA_SMALL_NUMBER
		? HorizontalDistance / FlightTime
		: RequestedHorizontalSpeed * 0.35f;
	const float GravityMagnitude = FMath::Abs((GetWorld() != nullptr ? GetWorld()->GetGravityZ() : -980.0f) * MortarGravityScale);
	const float VerticalSpeed = TargetOffset.Z / FlightTime + 0.5f * GravityMagnitude * FlightTime;
	const FVector LaunchVelocity = HorizontalDirection * HorizontalSpeed + FVector(0.0f, 0.0f, VerticalSpeed);
	const float LaunchSpeed = FMath::Max(MinimumLaunchSpeed, LaunchVelocity.Size());

	ActivatePooledRocket(InOwner, InInstigator, SpawnLocation, LaunchVelocity.Rotation(), LaunchVelocity.GetSafeNormal(), LaunchSpeed, InDamage, InExplosionRadius);

	if (ProjectileMovement != nullptr)
	{
		ProjectileMovement->ProjectileGravityScale = MortarGravityScale;
		ProjectileMovement->Velocity = LaunchVelocity;
		ProjectileMovement->InitialSpeed = LaunchVelocity.Size();
		ProjectileMovement->MaxSpeed = LaunchVelocity.Size() * 1.15f;
		ProjectileMovement->UpdateComponentVelocity();
	}

	bMortarProjectileActive = true;
	IndicatorPulseTime = 0.0f;
	TargetLocation = FVector(InTargetLocation.X, InTargetLocation.Y, LandingIndicatorHeight);

	const float IndicatorScaleXY = FMath::Max(0.45f, InExplosionRadius / LandingIndicatorScaleDivisor);
	IndicatorBaseScale = FVector(IndicatorScaleXY, IndicatorScaleXY, 0.10f);

	if (LandingIndicatorMesh != nullptr)
	{
		LandingIndicatorMesh->SetWorldLocation(TargetLocation);
		LandingIndicatorMesh->SetWorldRotation(FRotator::ZeroRotator);
		LandingIndicatorMesh->SetWorldScale3D(IndicatorBaseScale);
		LandingIndicatorMesh->SetHiddenInGame(false);
	}
}

void ARogueMortarProjectile::DeactivateToPool()
{
	bMortarProjectileActive = false;
	IndicatorPulseTime = 0.0f;

	if (LandingIndicatorMesh != nullptr)
	{
		LandingIndicatorMesh->SetHiddenInGame(true);
	}

	Super::DeactivateToPool();
}

void ARogueMortarProjectile::UpdateLandingIndicator(float DeltaSeconds)
{
	if (LandingIndicatorMesh == nullptr)
	{
		return;
	}

	IndicatorPulseTime += DeltaSeconds;
	const float PulseAlpha = 1.0f + FMath::Sin(IndicatorPulseTime * LandingIndicatorPulseSpeed) * LandingIndicatorPulseAmplitude;
	LandingIndicatorMesh->SetWorldLocation(TargetLocation);
	LandingIndicatorMesh->SetWorldScale3D(IndicatorBaseScale * PulseAlpha);
}
