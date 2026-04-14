#include "Combat/RogueProjectile.h"

#include "Combat/RogueImpactEffect.h"
#include "Enemies/RogueEnemy.h"
#include "Subsystems/RogueEnemyTrackerSubsystem.h"
#include "Components/PointLightComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

ARogueProjectile::ARogueProjectile()
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
	Collision->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Ignore);
	RootComponent = Collision;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(RootComponent);
	Mesh->SetRelativeScale3D(FVector(VisualConfig.MeshScale));
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(false);

	GlowMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GlowMesh"));
	GlowMesh->SetupAttachment(RootComponent);
	GlowMesh->SetRelativeScale3D(FVector(VisualConfig.GlowScale));
	GlowMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GlowMesh->SetCastShadow(false);

	RingMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RingMesh"));
	RingMesh->SetupAttachment(RootComponent);
	RingMesh->SetRelativeScale3D(FVector(VisualConfig.RingScale));
	RingMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RingMesh->SetCastShadow(false);

	TrailMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TrailMesh"));
	TrailMesh->SetupAttachment(RootComponent);
	TrailMesh->SetRelativeLocation(VisualConfig.TrailOffset);
	TrailMesh->SetRelativeRotation(VisualConfig.TrailRotation);
	TrailMesh->SetRelativeScale3D(VisualConfig.TrailScale);
	TrailMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TrailMesh->SetCastShadow(false);

	GlowLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("GlowLight"));
	GlowLight->SetupAttachment(RootComponent);
	GlowLight->SetIntensity(VisualConfig.LightIntensity);
	GlowLight->SetLightColor(VisualConfig.LightColor);
	GlowLight->SetAttenuationRadius(VisualConfig.LightAttenuationRadius);
	GlowLight->SetSourceRadius(VisualConfig.LightSourceRadius);
	GlowLight->SetSoftSourceRadius(VisualConfig.LightSoftSourceRadius);
	GlowLight->SetUseInverseSquaredFalloff(false);
	GlowLight->SetCastShadows(false);
	GlowLight->SetVisibility(false);

	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->SetUpdatedComponent(Collision);
	ProjectileMovement->InitialSpeed = Speed;
	ProjectileMovement->MaxSpeed = Speed;
	ProjectileMovement->ProjectileGravityScale = 0.0f;
	ProjectileMovement->bRotationFollowsVelocity = false;
	ProjectileMovement->bShouldBounce = false;
	ProjectileMovement->bInitialVelocityInLocalSpace = false;
	ProjectileMovement->bSweepCollision = true;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> ProjectileMesh(TEXT("/Game/GameAssets/StarterContent/Shapes/Shape_Sphere.Shape_Sphere"));
	if (ProjectileMesh.Succeeded())
	{
		Mesh->SetStaticMesh(ProjectileMesh.Object);
		GlowMesh->SetStaticMesh(ProjectileMesh.Object);
	}

	RingMesh->SetStaticMesh(ProjectileMesh.Succeeded() ? ProjectileMesh.Object : nullptr);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> TrailStaticMesh(TEXT("/Game/GameAssets/StarterContent/Shapes/Shape_Cylinder.Shape_Cylinder"));
	if (TrailStaticMesh.Succeeded())
	{
		TrailMesh->SetStaticMesh(TrailStaticMesh.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> CoreMaterial(TEXT("/Game/GameAssets/Realistic_Starter_VFX_Pack_Vol2/Materials/M_Glow_C_Inst.M_Glow_C_Inst"));
	if (CoreMaterial.Succeeded())
	{
		Mesh->SetMaterial(0, CoreMaterial.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> GlowMaterial(TEXT("/Game/GameAssets/HologramShader/Materials/HologramShader/MI/MI_Hologram_10.MI_Hologram_10"));
	if (GlowMaterial.Succeeded())
	{
		GlowMesh->SetMaterial(0, GlowMaterial.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> RingMaterial(TEXT("/Game/GameAssets/HologramShader/Materials/HologramShader/MI/MI_Hologram_12.MI_Hologram_12"));
	if (RingMaterial.Succeeded())
	{
		RingMesh->SetMaterial(0, RingMaterial.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> TrailMaterial(TEXT("/Game/GameAssets/Realistic_Starter_VFX_Pack_Vol2/Materials/M_Glow_C_Inst.M_Glow_C_Inst"));
	if (TrailMaterial.Succeeded())
	{
		TrailMesh->SetMaterial(0, TrailMaterial.Object);
	}

	Collision->OnComponentBeginOverlap.AddDynamic(this, &ARogueProjectile::HandleOverlap);
	ProjectileMovement->OnProjectileStop.AddDynamic(this, &ARogueProjectile::HandleProjectileStop);
}

void ARogueProjectile::BeginPlay()
{
	Super::BeginPlay();
	DeactivateToPool();
}

void ARogueProjectile::ActivatePooledProjectile(AActor* InOwner, APawn* InInstigator, const FVector& SpawnLocation, const FRotator& SpawnRotation, const FVector& InDirection, float InSpeed, float InDamage)
{
	TravelDirection = InDirection.GetSafeNormal();
	Speed = InSpeed;
	Damage = InDamage;
	SetOwner(InOwner);
	SetInstigator(InInstigator);
	SetActorLocationAndRotation(SpawnLocation, SpawnRotation, false, nullptr, ETeleportType::TeleportPhysics);
	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);
	SetActorTickEnabled(true);
	bPoolAvailable = false;
	bResolvedImpact = false;
	bVisualsSimplified = false;
	PulseTime = 0.0f;
	RemainingLifetime = PooledLifetime;
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Collision->SetGenerateOverlapEvents(true);
	Collision->ClearMoveIgnoreActors();

	if (AActor* OwnerActor = GetOwner())
	{
		Collision->IgnoreActorWhenMoving(OwnerActor, true);
	}

	if (APawn* InstigatorPawn = GetInstigator())
	{
		Collision->IgnoreActorWhenMoving(InstigatorPawn, true);
	}

	if (ProjectileMovement != nullptr)
	{
		ProjectileMovement->OnProjectileStop.RemoveDynamic(this, &ARogueProjectile::HandleProjectileStop);
		ProjectileMovement->OnProjectileStop.AddDynamic(this, &ARogueProjectile::HandleProjectileStop);
		ProjectileMovement->SetUpdatedComponent(Collision);
		ProjectileMovement->SetComponentTickEnabled(true);
		ProjectileMovement->StopMovementImmediately();
		ProjectileMovement->InitialSpeed = Speed;
		ProjectileMovement->MaxSpeed = Speed;
		ProjectileMovement->Velocity = TravelDirection * Speed;
		ProjectileMovement->UpdateComponentVelocity();
		ProjectileMovement->Activate(true);
	}
}

void ARogueProjectile::DeactivateToPool()
{
	bPoolAvailable = true;
	bResolvedImpact = true;

	if (ProjectileMovement != nullptr)
	{
		ProjectileMovement->OnProjectileStop.RemoveDynamic(this, &ARogueProjectile::HandleProjectileStop);
		ProjectileMovement->SetUpdatedComponent(Collision);
		ProjectileMovement->StopMovementImmediately();
		ProjectileMovement->Deactivate();
		ProjectileMovement->Velocity = FVector::ZeroVector;
	}

	bResolvedImpact = false;
	bVisualsSimplified = false;
	PulseTime = 0.0f;
	RemainingLifetime = 0.0f;
	SetActorTickEnabled(false);
	SetActorEnableCollision(false);
	SetActorHiddenInGame(true);
	SetOwner(nullptr);
	SetInstigator(nullptr);
	Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Collision->SetGenerateOverlapEvents(false);
	Collision->ClearMoveIgnoreActors();
}

void ARogueProjectile::Tick(float DeltaSeconds)
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

	URogueEnemyTrackerSubsystem* Tracker = GetWorld() ? GetWorld()->GetSubsystem<URogueEnemyTrackerSubsystem>() : nullptr;
	const bool bCullCombatEffects = Tracker != nullptr && Tracker->ShouldCullCombatEffects();
	if (bCullCombatEffects)
	{
		if (!bVisualsSimplified)
		{
			bVisualsSimplified = true;
			if (Mesh != nullptr)
			{
				Mesh->SetRelativeScale3D(FVector(VisualConfig.SimplifiedMeshScale));
			}
			if (GlowMesh != nullptr)
			{
				GlowMesh->SetRelativeScale3D(FVector(VisualConfig.SimplifiedGlowScale));
			}
			if (RingMesh != nullptr)
			{
				RingMesh->SetRelativeScale3D(FVector(VisualConfig.SimplifiedRingScale));
			}
			if (TrailMesh != nullptr)
			{
				TrailMesh->SetRelativeScale3D(VisualConfig.SimplifiedTrailScale);
			}
		}
		return;
	}

	bVisualsSimplified = false;

	PulseTime += DeltaSeconds;
	const float PulseAlpha = 0.5f + 0.5f * FMath::Sin(PulseTime * VisualConfig.PulseSpeed);
	const float CoreScale = VisualConfig.CorePulseBaseScale + PulseAlpha * VisualConfig.CorePulseAmplitude;
	const float GlowScale = VisualConfig.GlowPulseBaseScale + PulseAlpha * VisualConfig.GlowPulseAmplitude;
	const float RingScale = VisualConfig.RingPulseBaseScale + PulseAlpha * VisualConfig.RingPulseAmplitude;

	if (Mesh != nullptr)
	{
		Mesh->SetRelativeScale3D(FVector(CoreScale, CoreScale, CoreScale));
	}

	if (GlowMesh != nullptr)
	{
		GlowMesh->SetRelativeScale3D(FVector(GlowScale, GlowScale, GlowScale));
	}

	if (RingMesh != nullptr)
	{
		RingMesh->SetRelativeScale3D(FVector(RingScale, RingScale, RingScale));
	}

	if (TrailMesh != nullptr)
	{
		const float TrailRadius = VisualConfig.TrailPulseBaseRadius + PulseAlpha * VisualConfig.TrailPulseRadiusAmplitude;
		const float TrailLength = VisualConfig.TrailPulseBaseLength + PulseAlpha * VisualConfig.TrailPulseLengthAmplitude;
		TrailMesh->SetRelativeScale3D(FVector(TrailRadius, TrailRadius, TrailLength));
	}
}

void ARogueProjectile::InitializeProjectile(const FVector& InDirection, float InSpeed, float InDamage)
{
	TravelDirection = InDirection.GetSafeNormal();
	Speed = InSpeed;
	Damage = InDamage;

	if (ProjectileMovement != nullptr && !bPoolAvailable)
	{
		ProjectileMovement->SetUpdatedComponent(Collision);
		ProjectileMovement->SetComponentTickEnabled(true);
		ProjectileMovement->StopMovementImmediately();
		ProjectileMovement->InitialSpeed = Speed;
		ProjectileMovement->MaxSpeed = Speed;
		ProjectileMovement->Velocity = TravelDirection * Speed;
		ProjectileMovement->UpdateComponentVelocity();
		ProjectileMovement->Activate(true);
	}
}

void ARogueProjectile::HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (bPoolAvailable || bResolvedImpact || IsActorBeingDestroyed() || !IsValid(OtherActor) || OtherActor == GetOwner())
	{
		return;
	}

	ARogueEnemy* Enemy = Cast<ARogueEnemy>(OtherActor);
	if (!IsValid(Enemy))
	{
		return;
	}

	bResolvedImpact = true;
	UWorld* World = GetWorld();
	AActor* DamageCauser = IsValid(GetOwner()) ? GetOwner() : this;
	FVector ImpactLocation = Enemy->GetActorLocation() + FVector(0.0f, 0.0f, 40.0f);
	if (!SweepResult.ImpactPoint.IsNearlyZero())
	{
		ImpactLocation = FVector(SweepResult.ImpactPoint);
	}
	if (World != nullptr)
	{
		ARogueImpactEffect::SpawnImpactEffect(World, ImpactLocation, SweepResult.ImpactNormal.Rotation(), ERogueImpactVisualStyle::Spark, VisualConfig.ImpactEffectScale, VisualConfig.ImpactEffectLifetime, this);
	}
	UGameplayStatics::ApplyDamage(Enemy, Damage, nullptr, DamageCauser, UDamageType::StaticClass());
	DeactivateToPool();
}

void ARogueProjectile::HandleProjectileStop(const FHitResult& ImpactResult)
{
	if (bPoolAvailable || bResolvedImpact || IsActorBeingDestroyed())
	{
		return;
	}

	bResolvedImpact = true;
	UWorld* World = GetWorld();
	FVector ImpactLocation = GetActorLocation();
	if (!ImpactResult.ImpactPoint.IsNearlyZero())
	{
		ImpactLocation = FVector(ImpactResult.ImpactPoint);
	}
	if (World != nullptr)
	{
		ARogueImpactEffect::SpawnImpactEffect(World, ImpactLocation, ImpactResult.ImpactNormal.Rotation(), ERogueImpactVisualStyle::Spark, VisualConfig.StopImpactEffectScale, VisualConfig.StopImpactEffectLifetime, this);
	}
	DeactivateToPool();
}
