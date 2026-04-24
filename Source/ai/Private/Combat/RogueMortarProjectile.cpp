#include "Combat/RogueMortarProjectile.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

ARogueMortarProjectile::ARogueMortarProjectile()
{
	LandingIndicatorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LandingIndicatorMesh"));
	LandingIndicatorMesh->SetupAttachment(RootComponent);
	LandingIndicatorMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	LandingIndicatorMesh->SetCastShadow(false);
	LandingIndicatorMesh->SetUsingAbsoluteLocation(true);
	LandingIndicatorMesh->SetUsingAbsoluteRotation(true);
	LandingIndicatorMesh->SetUsingAbsoluteScale(true);
	LandingIndicatorMesh->SetHiddenInGame(true);
	LandingIndicatorMesh->SetRelativeRotation(FRotator(0.0f, 0.0f, 0.0f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> IndicatorMesh(TEXT("/Game/GameAssets/StarterContent/Shapes/Shape_Torus.Shape_Torus"));
	if (IndicatorMesh.Succeeded())
	{
		LandingIndicatorMesh->SetStaticMesh(IndicatorMesh.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> IndicatorMaterial(TEXT("/Game/GameAssets/HologramShader/Materials/HologramShader/MI/MI_Hologram_8.MI_Hologram_8"));
	if (IndicatorMaterial.Succeeded())
	{
		LandingIndicatorMaterial = IndicatorMaterial.Object;
		LandingIndicatorMesh->SetMaterial(0, LandingIndicatorMaterial);
	}
}

void ARogueMortarProjectile::BeginPlay()
{
	Super::BeginPlay();

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
