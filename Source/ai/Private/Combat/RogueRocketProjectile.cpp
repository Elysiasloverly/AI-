#include "Combat/RogueRocketProjectile.h"

#include "Combat/RogueImpactEffect.h"
#include "Enemies/RogueEnemy.h"
#include "Subsystems/RogueEnemyTrackerSubsystem.h"
// #include "Components/PointLightComponent.h"
#include "Components/SphereComponent.h"
// #include "Components/StaticMeshComponent.h"
// #include "Engine/StaticMesh.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
// #include "Materials/MaterialInterface.h"
#include "Particles/ParticleSystem.h"
// #include "UObject/ConstructorHelpers.h"

ARogueRocketProjectile::ARogueRocketProjectile()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.03f;

	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	Collision->InitSphereRadius(CollisionRadius);
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Collision->SetCollisionObjectType(ECC_WorldDynamic);
	Collision->SetCollisionResponseToAllChannels(ECR_Ignore);
	Collision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Collision->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	RootComponent = Collision;


	// Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	// Mesh->SetupAttachment(RootComponent);
	// Mesh->SetRelativeLocation(FVector(0.0f, 0.0f, 18.0f));
	// Mesh->SetRelativeScale3D(VisualConfig.MeshScale);
	// Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	// Mesh->SetCastShadow(false);

	// BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	// BodyMesh->SetupAttachment(RootComponent);
	// BodyMesh->SetRelativeLocation(FVector(0.0f, 0.0f, -6.0f));
	// BodyMesh->SetRelativeScale3D(VisualConfig.BodyScale);
	// BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	// BodyMesh->SetCastShadow(false);

	// ExhaustCoreMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ExhaustCoreMesh"));
	// ExhaustCoreMesh->SetupAttachment(RootComponent);
	// ExhaustCoreMesh->SetRelativeLocation(FVector(0.0f, 0.0f, -28.0f));
	// ExhaustCoreMesh->SetRelativeScale3D(VisualConfig.ExhaustCoreScale);
	// ExhaustCoreMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	// ExhaustCoreMesh->SetCastShadow(false);

	// ExhaustGlowMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ExhaustGlowMesh"));
	// ExhaustGlowMesh->SetupAttachment(RootComponent);
	// ExhaustGlowMesh->SetRelativeLocation(FVector(0.0f, 0.0f, -42.0f));
	// ExhaustGlowMesh->SetRelativeScale3D(VisualConfig.ExhaustGlowScale);
	// ExhaustGlowMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	// ExhaustGlowMesh->SetCastShadow(false);

	// FinLeftMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FinLeftMesh"));
	// FinLeftMesh->SetupAttachment(RootComponent);
	// FinLeftMesh->SetRelativeLocation(FVector(0.0f, 12.0f, -8.0f));
	// FinLeftMesh->SetRelativeScale3D(FVector(0.04f, 0.16f, 0.16f));
	// FinLeftMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	// FinLeftMesh->SetCastShadow(false);

	// FinRightMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FinRightMesh"));
	// FinRightMesh->SetupAttachment(RootComponent);
	// FinRightMesh->SetRelativeLocation(FVector(0.0f, -12.0f, -8.0f));
	// FinRightMesh->SetRelativeScale3D(FVector(0.04f, 0.16f, 0.16f));
	// FinRightMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	// FinRightMesh->SetCastShadow(false);

	// AuraMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("AuraMesh"));
	// AuraMesh->SetupAttachment(RootComponent);
	// AuraMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 4.0f));
	// AuraMesh->SetRelativeScale3D(VisualConfig.AuraScale);
	// AuraMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	// AuraMesh->SetCastShadow(false);

	// TrailMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TrailMesh"));
	// TrailMesh->SetupAttachment(RootComponent);
	// TrailMesh->SetRelativeLocation(FVector(0.0f, 0.0f, -56.0f));
	// TrailMesh->SetRelativeRotation(FRotator(0.0f, 0.0f, 0.0f));
	// TrailMesh->SetRelativeScale3D(VisualConfig.TrailScale);
	// TrailMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	// TrailMesh->SetCastShadow(false);

	// RocketLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("RocketLight"));
	// RocketLight->SetupAttachment(RootComponent);
	// RocketLight->SetRelativeLocation(FVector(0.0f, 0.0f, -24.0f));
	// RocketLight->SetIntensity(VisualConfig.LightIntensity);
	// RocketLight->SetLightColor(FLinearColor(0.22f, 0.70f, 1.0f));
	// RocketLight->SetAttenuationRadius(180.0f);
	// RocketLight->SetSourceRadius(18.0f);
	// RocketLight->SetSoftSourceRadius(24.0f);
	// RocketLight->SetUseInverseSquaredFalloff(false);
	// RocketLight->SetCastShadows(false);

	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->SetUpdatedComponent(Collision);
	ProjectileMovement->InitialSpeed = 900.0f;
	ProjectileMovement->MaxSpeed = 900.0f;
	ProjectileMovement->ProjectileGravityScale = 0.0f;
	ProjectileMovement->bRotationFollowsVelocity = true;
	ProjectileMovement->bShouldBounce = false;
	ProjectileMovement->bSweepCollision = true;

	// static ConstructorHelpers::FObjectFinder<UStaticMesh> RocketMesh(TEXT("/Game/GameAssets/StarterContent/Shapes/Shape_Cone.Shape_Cone"));
	// if (RocketMesh.Succeeded())
	// {
	// 	Mesh->SetStaticMesh(RocketMesh.Object);
	// }

	// static ConstructorHelpers::FObjectFinder<UStaticMesh> RocketBodyMesh(TEXT("/Game/GameAssets/StarterContent/Shapes/Shape_Cylinder.Shape_Cylinder"));
	// if (RocketBodyMesh.Succeeded())
	// {
	// 	BodyMesh->SetStaticMesh(RocketBodyMesh.Object);
	// }

	// static ConstructorHelpers::FObjectFinder<UStaticMesh> ExhaustMesh(TEXT("/Game/GameAssets/StarterContent/Shapes/Shape_Sphere.Shape_Sphere"));
	// if (ExhaustMesh.Succeeded())
	// {
	// 	ExhaustCoreMesh->SetStaticMesh(ExhaustMesh.Object);
	// 	ExhaustGlowMesh->SetStaticMesh(ExhaustMesh.Object);
	// 	AuraMesh->SetStaticMesh(ExhaustMesh.Object);
	// }

	// static ConstructorHelpers::FObjectFinder<UStaticMesh> FinMesh(TEXT("/Game/GameAssets/StarterContent/Shapes/Shape_Cube.Shape_Cube"));
	// if (FinMesh.Succeeded())
	// {
	// 	FinLeftMesh->SetStaticMesh(FinMesh.Object);
	// 	FinRightMesh->SetStaticMesh(FinMesh.Object);
	// }

	// static ConstructorHelpers::FObjectFinder<UStaticMesh> TrailStaticMesh(TEXT("/Game/GameAssets/StarterContent/Shapes/Shape_Cylinder.Shape_Cylinder"));
	// if (TrailStaticMesh.Succeeded())
	// {
	// 	TrailMesh->SetStaticMesh(TrailStaticMesh.Object);
	// }

	// static ConstructorHelpers::FObjectFinder<UMaterialInterface> RocketMaterial(TEXT("/Game/GameAssets/HologramShader/Materials/HologramShader/MI/MI_Hologram_21.MI_Hologram_21"));
	// if (RocketMaterial.Succeeded())
	// {
	// 	Mesh->SetMaterial(0, RocketMaterial.Object);
	// }

	// static ConstructorHelpers::FObjectFinder<UMaterialInterface> RocketBodyMaterial(TEXT("/Game/GameAssets/Vefects/Stylized_Galaxy_Shader/Demo/Shared/Materials/MI_Opaque_Dark_01.MI_Opaque_Dark_01"));
	// if (RocketBodyMaterial.Succeeded())
	// {
	// 	BodyMesh->SetMaterial(0, RocketBodyMaterial.Object);
	// 	FinLeftMesh->SetMaterial(0, RocketBodyMaterial.Object);
	// 	FinRightMesh->SetMaterial(0, RocketBodyMaterial.Object);
	// }

	// static ConstructorHelpers::FObjectFinder<UMaterialInterface> ExhaustCoreMaterial(TEXT("/Game/GameAssets/Realistic_Starter_VFX_Pack_Vol2/Materials/M_Glow_A.M_Glow_A"));
	// if (ExhaustCoreMaterial.Succeeded())
	// {
	// 	ExhaustCoreMesh->SetMaterial(0, ExhaustCoreMaterial.Object);
	// }

	// static ConstructorHelpers::FObjectFinder<UMaterialInterface> ExhaustGlowMaterial(TEXT("/Game/GameAssets/Realistic_Starter_VFX_Pack_Vol2/Materials/M_Glow_D.M_Glow_D"));
	// if (ExhaustGlowMaterial.Succeeded())
	// {
	// 	ExhaustGlowMesh->SetMaterial(0, ExhaustGlowMaterial.Object);
	// }

	// static ConstructorHelpers::FObjectFinder<UMaterialInterface> AuraMaterial(TEXT("/Game/GameAssets/HologramShader/Materials/HologramShader/MI/MI_Hologram_18.MI_Hologram_18"));
	// if (AuraMaterial.Succeeded())
	// {
	// 	AuraMesh->SetMaterial(0, AuraMaterial.Object);
	// }

	// static ConstructorHelpers::FObjectFinder<UMaterialInterface> TrailMaterial(TEXT("/Game/GameAssets/Realistic_Starter_VFX_Pack_Vol2/Materials/M_Glow_C_Inst.M_Glow_C_Inst"));
	// if (TrailMaterial.Succeeded())
	// {
	// 	TrailMesh->SetMaterial(0, TrailMaterial.Object);
	// }

	Collision->OnComponentBeginOverlap.AddDynamic(this, &ARogueRocketProjectile::HandleOverlap);
	ProjectileMovement->OnProjectileStop.AddDynamic(this, &ARogueRocketProjectile::HandleProjectileStop);
	
}

void ARogueRocketProjectile::BeginPlay()
{
	Super::BeginPlay();
	DeactivateToPool();
}

void ARogueRocketProjectile::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bPoolAvailable)
	{
		return;
	}

	RemainingLifetime -= DeltaSeconds;
	if (RemainingLifetime <= 0.0f)
	{
		DeactivateToPool();
		return;
	}

	// URogueEnemyTrackerSubsystem* Tracker = GetWorld() ? GetWorld()->GetSubsystem<URogueEnemyTrackerSubsystem>() : nullptr;
	// const bool bCullCombatEffects = Tracker != nullptr && Tracker->ShouldCullCombatEffects();
	// if (bCullCombatEffects)
	// {
	// 	if (!bVisualsSimplified)
	// 	{
	// 		bVisualsSimplified = true;
	// 		ExhaustCoreMesh->SetRelativeScale3D(VisualConfig.SimplifiedExhaustCoreScale);
	// 		ExhaustGlowMesh->SetRelativeScale3D(VisualConfig.SimplifiedExhaustGlowScale);
	// 		AuraMesh->SetRelativeScale3D(VisualConfig.SimplifiedAuraScale);
	// 		TrailMesh->SetRelativeScale3D(VisualConfig.SimplifiedTrailScale);
	// 		if (RocketLight != nullptr)
	// 		{
	// 			RocketLight->SetIntensity(VisualConfig.SimplifiedLightIntensity);
	// 		}
	// 	}
	// 	return;
	// }

	// bVisualsSimplified = false;

	// PulseTime += DeltaSeconds;
	// const float PulseAlpha = 0.5f + 0.5f * FMath::Sin(PulseTime * VisualConfig.PulseSpeed);

	// ExhaustCoreMesh->SetRelativeScale3D(VisualConfig.SimplifiedExhaustCoreScale + VisualConfig.ExhaustCorePulseAmplitude * PulseAlpha);
	// ExhaustGlowMesh->SetRelativeScale3D(VisualConfig.SimplifiedExhaustGlowScale + VisualConfig.ExhaustGlowPulseAmplitude * PulseAlpha);
	// AuraMesh->SetRelativeScale3D(VisualConfig.SimplifiedAuraScale + VisualConfig.AuraPulseAmplitude * PulseAlpha);
	// TrailMesh->SetRelativeScale3D(VisualConfig.SimplifiedTrailScale + VisualConfig.TrailPulseAmplitude * PulseAlpha);
	// AuraMesh->AddLocalRotation(FRotator(0.0f, VisualConfig.AuraRotationSpeed * DeltaSeconds, 0.0f));

	// if (RocketLight != nullptr)
	// {
	// 	RocketLight->SetIntensity(VisualConfig.SimplifiedLightIntensity + PulseAlpha * VisualConfig.LightPulseAmplitude);
	// }
}

void ARogueRocketProjectile::InitializeRocket(const FVector& InDirection, float InSpeed, float InDamage, float InExplosionRadius)
{
	Damage = InDamage;
	ExplosionRadius = InExplosionRadius;

	if (ProjectileMovement != nullptr)
	{
		ProjectileMovement->SetUpdatedComponent(Collision);
		ProjectileMovement->SetComponentTickEnabled(true);
		ProjectileMovement->InitialSpeed = InSpeed;
		ProjectileMovement->MaxSpeed = InSpeed;
		ProjectileMovement->Velocity = InDirection.GetSafeNormal() * InSpeed;
		ProjectileMovement->UpdateComponentVelocity();
		ProjectileMovement->Activate(true);
	}
}

void ARogueRocketProjectile::ActivatePooledRocket(AActor* InOwner, APawn* InInstigator, const FVector& SpawnLocation, const FRotator& SpawnRotation, const FVector& InDirection, float InSpeed, float InDamage, float InExplosionRadius)
{
	SetOwner(InOwner);
	SetInstigator(InInstigator);
	SetActorLocationAndRotation(SpawnLocation, SpawnRotation, false, nullptr, ETeleportType::TeleportPhysics);
	SetActorHiddenInGame(false);
	SetActorTickEnabled(true);
	SetActorEnableCollision(true);

	bPoolAvailable = false;
	bExploded = false;
	// bVisualsSimplified = false;
	// PulseTime = 0.0f;
	RemainingLifetime = PooledLifetime;
	Damage = InDamage;
	ExplosionRadius = InExplosionRadius;

	if (Collision != nullptr)
	{
		Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Collision->ClearMoveIgnoreActors();
		if (InOwner != nullptr)
		{
			Collision->IgnoreActorWhenMoving(InOwner, true);
		}

		if (InInstigator != nullptr)
		{
			Collision->IgnoreActorWhenMoving(InInstigator, true);
		}
	}

	// if (RocketLight != nullptr)
	// {
	// 	RocketLight->SetIntensity(VisualConfig.LightIntensity);
	// }

	if (ProjectileMovement != nullptr)
	{
		ProjectileMovement->OnProjectileStop.RemoveDynamic(this, &ARogueRocketProjectile::HandleProjectileStop);
		ProjectileMovement->OnProjectileStop.AddDynamic(this, &ARogueRocketProjectile::HandleProjectileStop);
		ProjectileMovement->ProjectileGravityScale = 0.0f;
	}

	// ExhaustCoreMesh->SetRelativeScale3D(VisualConfig.ExhaustCoreScale);
	// ExhaustGlowMesh->SetRelativeScale3D(VisualConfig.ExhaustGlowScale);
	// AuraMesh->SetRelativeScale3D(VisualConfig.AuraScale);
	// TrailMesh->SetRelativeScale3D(VisualConfig.TrailScale);

	InitializeRocket(InDirection, InSpeed, InDamage, InExplosionRadius);
}

void ARogueRocketProjectile::DeactivateToPool()
{
	SetActorHiddenInGame(true);
	SetActorTickEnabled(false);
	SetActorEnableCollision(false);

	if (Collision != nullptr)
	{
		Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Collision->ClearMoveIgnoreActors();
	}

	bPoolAvailable = true;
	bExploded = true;
	RemainingLifetime = 0.0f;

	if (ProjectileMovement != nullptr)
	{
		ProjectileMovement->OnProjectileStop.RemoveDynamic(this, &ARogueRocketProjectile::HandleProjectileStop);
		ProjectileMovement->SetUpdatedComponent(Collision);
		ProjectileMovement->StopMovementImmediately();
		ProjectileMovement->Deactivate();
		ProjectileMovement->Velocity = FVector::ZeroVector;
		ProjectileMovement->ProjectileGravityScale = 0.0f;
	}

	bExploded = false;
	// PulseTime = 0.0f;
	// bVisualsSimplified = false;
	SetOwner(nullptr);
	SetInstigator(nullptr);
}

void ARogueRocketProjectile::HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (bPoolAvailable)
	{
		return;
	}

	if (!IsValid(OtherActor) || OtherActor == GetOwner())
	{
		return;
	}

	Explode();
}

void ARogueRocketProjectile::HandleProjectileStop(const FHitResult& ImpactResult)
{
	if (bPoolAvailable)
	{
		return;
	}

	Explode();
}

void ARogueRocketProjectile::Explode()
{
	if (bExploded || bPoolAvailable || IsActorBeingDestroyed())
	{
		return;
	}

	bExploded = true;
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		DeactivateToPool();
		return;
	}

	const FVector ExplosionLocation = GetActorLocation();
	const FVector ExplosionScale(ExplosionRadius / 220.0f, ExplosionRadius / 220.0f, 1.0f);
	AActor* DamageCauser = IsValid(GetOwner()) ? GetOwner() : this;
	AController* InstigatorController = GetInstigatorController();

	if (Collision != nullptr)
	{
		Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	SetActorEnableCollision(false);
	if (ProjectileMovement != nullptr)
	{
		ProjectileMovement->OnProjectileStop.RemoveDynamic(this, &ARogueRocketProjectile::HandleProjectileStop);
		ProjectileMovement->StopMovementImmediately();
		ProjectileMovement->Deactivate();
	}

	ARogueImpactEffect::SpawnImpactEffect(World, ExplosionLocation, FRotator::ZeroRotator, ERogueImpactVisualStyle::Explosion, ExplosionScale, ImpactEffectLifetime, this);

	URogueEnemyTrackerSubsystem* Tracker = World->GetSubsystem<URogueEnemyTrackerSubsystem>();
	const bool bCullCombatEffects = Tracker != nullptr && Tracker->ShouldCullCombatEffects();
	if (!bCullCombatEffects && ExplosionBurstEffect != nullptr)
	{
		UGameplayStatics::SpawnEmitterAtLocation(World, ExplosionBurstEffect, FTransform(FRotator::ZeroRotator, ExplosionLocation, FVector(0.52f, 0.52f, 0.52f)));
	}

	TArray<ARogueEnemy*> AffectedEnemies;
	TArray<float> PendingDamages;
	if (!IsValid(Tracker))
	{
		DeactivateToPool();
		return;
	}

	TArray<ARogueEnemy*> NearbyEnemies;
	Tracker->CollectEnemiesInRange(ExplosionLocation, ExplosionRadius, NearbyEnemies, 0, false);
	for (ARogueEnemy* Enemy : NearbyEnemies)
	{
		if (!IsValid(Enemy) || Enemy->IsDead())
		{
			continue;
		}

		const float Distance = FVector::Dist2D(ExplosionLocation, Enemy->GetActorLocation());
		if (Distance > ExplosionRadius)
		{
			continue;
		}

		const float DistanceAlpha = 1.0f - FMath::Clamp(Distance / ExplosionRadius, 0.0f, 1.0f);
		const float FinalDamage = FMath::Lerp(Damage * 0.45f, Damage, DistanceAlpha);
		AffectedEnemies.Add(Enemy);
		PendingDamages.Add(FinalDamage);
	}

	for (int32 Index = 0; Index < AffectedEnemies.Num(); ++Index)
	{
		ARogueEnemy* Enemy = AffectedEnemies[Index];
		if (!IsValid(Enemy) || Enemy->IsDead())
		{
			continue;
		}

		UGameplayStatics::ApplyDamage(Enemy, PendingDamages[Index], InstigatorController, DamageCauser, UDamageType::StaticClass());
	}

	DeactivateToPool();
}
