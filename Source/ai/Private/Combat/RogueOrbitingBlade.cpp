#include "Combat/RogueOrbitingBlade.h"

#include "Combat/RogueImpactEffect.h"
#include "Enemies/RogueEnemy.h"
#include "Core/RogueGameMode.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

ARogueOrbitingBlade::ARogueOrbitingBlade()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.03f;

	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	Collision->InitSphereRadius(VisualConfig.CollisionRadius);
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Collision->SetCollisionObjectType(ECC_WorldDynamic);
	Collision->SetCollisionResponseToAllChannels(ECR_Ignore);
	Collision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	RootComponent = Collision;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(RootComponent);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetRelativeRotation(VisualConfig.MeshRotation);
	Mesh->SetRelativeScale3D(VisualConfig.MeshScale);
	Mesh->SetCastShadow(false);

	GlowMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GlowMesh"));
	GlowMesh->SetupAttachment(RootComponent);
	GlowMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GlowMesh->SetRelativeRotation(VisualConfig.MeshRotation);
	GlowMesh->SetRelativeScale3D(VisualConfig.GlowScale);
	GlowMesh->SetCastShadow(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> BladeMesh(TEXT("/Game/GameAssets/StarterContent/Shapes/Shape_Wedge_A.Shape_Wedge_A"));
	if (BladeMesh.Succeeded())
	{
		Mesh->SetStaticMesh(BladeMesh.Object);
		GlowMesh->SetStaticMesh(BladeMesh.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BladeMaterial(TEXT("/Game/GameAssets/StarterContent/Materials/M_Tech_Hex_Tile.M_Tech_Hex_Tile"));
	if (BladeMaterial.Succeeded())
	{
		Mesh->SetMaterial(0, BladeMaterial.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> GlowMaterial(TEXT("/Game/GameAssets/StarterContent/Materials/M_Tech_Hex_Tile_Pulse.M_Tech_Hex_Tile_Pulse"));
	if (GlowMaterial.Succeeded())
	{
		GlowMesh->SetMaterial(0, GlowMaterial.Object);
	}

	Collision->OnComponentBeginOverlap.AddDynamic(this, &ARogueOrbitingBlade::HandleOverlap);
}

void ARogueOrbitingBlade::BeginPlay()
{
	Super::BeginPlay();
	DeactivateToPool();
}

void ARogueOrbitingBlade::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bPoolAvailable)
	{
		return;
	}

	UpdateCooldowns(DeltaSeconds);

	if (!OwnerActor.IsValid())
	{
		DeactivateToPool();
		return;
	}

	ARogueGameMode* RogueGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ARogueGameMode>() : nullptr;
	const bool bCullCombatEffects = RogueGameMode != nullptr && RogueGameMode->ShouldCullCombatEffects();
	if (bCullCombatEffects)
	{
		if (!bVisualsSimplified && GlowMesh != nullptr)
		{
			bVisualsSimplified = true;
			GlowMesh->SetRelativeScale3D(VisualConfig.SimplifiedGlowScale);
		}
		return;
	}

	bVisualsSimplified = false;

	const float PulseAlpha = 0.5f + 0.5f * FMath::Sin(GetWorld()->GetTimeSeconds() * VisualConfig.PulseSpeed + static_cast<float>(BladeIndex) * VisualConfig.PulsePhaseOffsetPerBlade);
	if (GlowMesh != nullptr)
	{
		GlowMesh->SetRelativeScale3D(VisualConfig.GlowPulseBaseScale + VisualConfig.GlowPulseAmplitude * PulseAlpha);
	}
}

void ARogueOrbitingBlade::ActivatePooledBlade(AActor* InOwnerActor)
{
	OwnerActor = InOwnerActor;
	SetOwner(InOwnerActor);
	SetActorHiddenInGame(false);
	SetActorTickEnabled(true);
	SetActorEnableCollision(true);
	bPoolAvailable = false;
	bVisualsSimplified = false;
	HitCooldowns.Reset();

	if (Collision != nullptr)
	{
		Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Collision->ClearMoveIgnoreActors();
		if (InOwnerActor != nullptr)
		{
			Collision->IgnoreActorWhenMoving(InOwnerActor, true);
		}
	}
}

void ARogueOrbitingBlade::DeactivateToPool()
{
	bPoolAvailable = true;
	bVisualsSimplified = false;
	HitCooldowns.Reset();
	OwnerActor.Reset();
	SetOwner(nullptr);
	SetActorHiddenInGame(true);
	SetActorTickEnabled(false);
	SetActorEnableCollision(false);

	if (Collision != nullptr)
	{
		Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Collision->ClearMoveIgnoreActors();
	}
}

void ARogueOrbitingBlade::ConfigureBlade(AActor* InOwnerActor, int32 InBladeIndex, int32 InBladeCount, float InOrbitRadius, float InRotationSpeed, float InDamage, float InSharedOrbitAngle)
{
	OwnerActor = InOwnerActor;
	BladeIndex = InBladeIndex;
	BladeCount = FMath::Max(1, InBladeCount);
	OrbitRadius = InOrbitRadius;
	RotationSpeed = InRotationSpeed;
	Damage = InDamage;
	SharedOrbitAngle = InSharedOrbitAngle;

	if (OwnerActor.IsValid())
	{
		Collision->IgnoreActorWhenMoving(OwnerActor.Get(), true);
	}

	UpdateTransform();
}

void ARogueOrbitingBlade::HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (bPoolAvailable)
	{
		return;
	}

	if (!OwnerActor.IsValid() || !IsValid(OtherActor) || OtherActor == OwnerActor.Get())
	{
		return;
	}

	ARogueEnemy* Enemy = Cast<ARogueEnemy>(OtherActor);
	if (!IsValid(Enemy) || Enemy->IsDead() || HitCooldowns.Contains(Enemy))
	{
		return;
	}

	HitCooldowns.Add(Enemy, VisualConfig.HitCooldown);
	ARogueImpactEffect::SpawnImpactEffect(GetWorld(), Enemy->GetActorLocation() + FVector(0.0f, 0.0f, 45.0f), GetActorRotation(), ERogueImpactVisualStyle::Spark, VisualConfig.ImpactEffectScale, VisualConfig.ImpactEffectLifetime, this);
	UGameplayStatics::ApplyDamage(Enemy, Damage, nullptr, OwnerActor.Get(), UDamageType::StaticClass());
}

void ARogueOrbitingBlade::UpdateCooldowns(float DeltaSeconds)
{
	TArray<TObjectPtr<AActor>> KeysToRemove;
	for (TPair<TObjectPtr<AActor>, float>& Pair : HitCooldowns)
	{
		Pair.Value -= DeltaSeconds;
		if (Pair.Value <= 0.0f || !IsValid(Pair.Key))
		{
			KeysToRemove.Add(Pair.Key);
		}
	}

	for (AActor* Actor : KeysToRemove)
	{
		HitCooldowns.Remove(Actor);
	}
}

void ARogueOrbitingBlade::UpdateTransform()
{
	if (!OwnerActor.IsValid())
	{
		return;
	}

	const float SlotOffset = BladeCount > 0 ? (360.0f / static_cast<float>(BladeCount)) * static_cast<float>(BladeIndex) : 0.0f;
	const float TotalAngle = SharedOrbitAngle + SlotOffset;
	const FVector OrbitOffset = FVector(FMath::Cos(FMath::DegreesToRadians(TotalAngle)), FMath::Sin(FMath::DegreesToRadians(TotalAngle)), 0.0f) * OrbitRadius;
	const FVector OwnerLocation = OwnerActor->GetActorLocation() + FVector(0.0f, 0.0f, 60.0f);

	SetActorLocation(OwnerLocation + OrbitOffset);
	SetActorRotation((OwnerLocation - GetActorLocation()).Rotation());
}
