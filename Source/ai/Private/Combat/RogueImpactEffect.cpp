#include "Combat/RogueImpactEffect.h"

#include "Core/RogueGameMode.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Particles/ParticleSystem.h"
#include "Particles/ParticleSystemComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "TimerManager.h"

namespace
{
	int32 GActiveImpactEffectCount = 0;
}

ARogueImpactEffect::ARogueImpactEffect()
{
	PrimaryActorTick.bCanEverTick = false;
	SparkStyle.MeshLocalScale = FVector(0.18f, 0.18f, 0.18f);
	SparkStyle.MeshLocalRotation = FRotator::ZeroRotator;
	SparkStyle.ParticleScale = FVector(0.42f, 0.42f, 0.42f);
	SparkStyle.bSpawnPrimaryParticle = false;
	SparkStyle.bDeactivatePrimaryParticleImmediately = false;

	LaserStyle.MeshLocalScale = FVector(0.18f, 0.18f, 0.28f);
	LaserStyle.MeshLocalRotation = FRotator::ZeroRotator;
	LaserStyle.ParticleScale = FVector(0.34f, 0.34f, 0.34f);
	LaserStyle.bSpawnPrimaryParticle = true;
	LaserStyle.bDeactivatePrimaryParticleImmediately = true;

	ExplosionStyle.MeshLocalScale = FVector(0.42f, 0.42f, 0.08f);
	ExplosionStyle.MeshLocalRotation = FRotator(90.0f, 0.0f, 0.0f);
	ExplosionStyle.ParticleScale = FVector(0.55f, 0.55f, 0.55f);
	ExplosionStyle.bSpawnPrimaryParticle = true;
	ExplosionStyle.bDeactivatePrimaryParticleImmediately = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	EffectMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("EffectMesh"));
	EffectMesh->SetupAttachment(SceneRoot);
	EffectMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	EffectMesh->SetCastShadow(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SparkMesh(TEXT("/Game/GameAssets/StarterContent/Shapes/Shape_Sphere.Shape_Sphere"));
	if (SparkMesh.Succeeded())
	{
		SparkMeshAsset = SparkMesh.Object;
		EffectMesh->SetStaticMesh(SparkMesh.Object);
	}

	static ConstructorHelpers::FObjectFinder<UStaticMesh> LaserMesh(TEXT("/Game/GameAssets/StarterContent/Shapes/Shape_Cylinder.Shape_Cylinder"));
	if (LaserMesh.Succeeded())
	{
		LaserMeshAsset = LaserMesh.Object;
	}

	static ConstructorHelpers::FObjectFinder<UStaticMesh> ExplosionMesh(TEXT("/Game/GameAssets/StarterContent/Shapes/Shape_Torus.Shape_Torus"));
	if (ExplosionMesh.Succeeded())
	{
		ExplosionMeshAsset = ExplosionMesh.Object;
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> SparkMaterialAsset(TEXT("/Game/GameAssets/Realistic_Starter_VFX_Pack_Vol2/Materials/M_Spark_A.M_Spark_A"));
	if (SparkMaterialAsset.Succeeded())
	{
		SparkMaterial = SparkMaterialAsset.Object;
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> LaserMaterialAsset(TEXT("/Game/GameAssets/Realistic_Starter_VFX_Pack_Vol2/Materials/M_Glow_C_Inst.M_Glow_C_Inst"));
	if (LaserMaterialAsset.Succeeded())
	{
		LaserMaterial = LaserMaterialAsset.Object;
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ExplosionMaterialAsset(TEXT("/Game/GameAssets/Realistic_Starter_VFX_Pack_Vol2/Materials/M_Explosion_B.M_Explosion_B"));
	if (ExplosionMaterialAsset.Succeeded())
	{
		ExplosionMaterial = ExplosionMaterialAsset.Object;
	}

	static ConstructorHelpers::FObjectFinder<UParticleSystem> SparkParticleAsset(TEXT("/Game/GameAssets/Realistic_Starter_VFX_Pack_Vol2/Particles/Sparks/P_Sparks_B.P_Sparks_B"));
	if (SparkParticleAsset.Succeeded())
	{
		SparkParticle = SparkParticleAsset.Object;
	}

	static ConstructorHelpers::FObjectFinder<UParticleSystem> LaserParticleAsset(TEXT("/Game/GameAssets/Realistic_Starter_VFX_Pack_Vol2/Particles/Sparks/P_Sparks_E.P_Sparks_E"));
	if (LaserParticleAsset.Succeeded())
	{
		LaserParticle = LaserParticleAsset.Object;
	}

	static ConstructorHelpers::FObjectFinder<UParticleSystem> ExplosionParticleAsset(TEXT("/Game/GameAssets/Realistic_Starter_VFX_Pack_Vol2/Particles/Explosion/P_Explosion_Side.P_Explosion_Side"));
	if (ExplosionParticleAsset.Succeeded())
	{
		ExplosionParticle = ExplosionParticleAsset.Object;
	}

	static ConstructorHelpers::FObjectFinder<UParticleSystem> SmokeParticleAsset(TEXT("/Game/GameAssets/Realistic_Starter_VFX_Pack_Vol2/Particles/Smoke/P_Smoke_F.P_Smoke_F"));
	if (SmokeParticleAsset.Succeeded())
	{
		ExplosionSmokeParticle = SmokeParticleAsset.Object;
	}

}

void ARogueImpactEffect::BeginPlay()
{
	Super::BeginPlay();
	DeactivateToPool();
}

void ARogueImpactEffect::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (bCountedAsActive)
	{
		GActiveImpactEffectCount = FMath::Max(0, GActiveImpactEffectCount - 1);
		bCountedAsActive = false;
	}
	Super::EndPlay(EndPlayReason);
}

void ARogueImpactEffect::ActivatePooledEffect(AActor* InOwner, const FVector& InLocation, const FRotator& InRotation, ERogueImpactVisualStyle InStyle, const FVector& InScale, float InLifetime)
{
	SetOwner(InOwner);
	SetActorLocationAndRotation(InLocation, InRotation, false, nullptr, ETeleportType::TeleportPhysics);
	SetActorHiddenInGame(false);
	SetActorEnableCollision(false);
	EffectMesh->SetVisibility(true, true);
	bPoolAvailable = false;

	if (!bCountedAsActive)
	{
		++GActiveImpactEffectCount;
		bCountedAsActive = true;
	}

	InitializeImpactVisuals(InStyle, InScale, InRotation);

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ReturnToPoolTimerHandle);
		World->GetTimerManager().SetTimer(ReturnToPoolTimerHandle, this, &ARogueImpactEffect::DeactivateToPool, FMath::Max(MinimumLifetime, InLifetime), false);
	}
}

void ARogueImpactEffect::DeactivateToPool()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ReturnToPoolTimerHandle);
	}

	if (bCountedAsActive)
	{
		GActiveImpactEffectCount = FMath::Max(0, GActiveImpactEffectCount - 1);
		bCountedAsActive = false;
	}

	bPoolAvailable = true;
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
	SetOwner(nullptr);
	EffectMesh->SetVisibility(false, true);
	SetActorScale3D(FVector::OneVector);
}

void ARogueImpactEffect::InitializeImpactVisuals(ERogueImpactVisualStyle InStyle, const FVector& InScale, const FRotator& InRotation)
{
	UStaticMesh* EffectStaticMesh = SparkMeshAsset.Get();
	UMaterialInterface* EffectMaterial = SparkMaterial.Get();
	const FRogueImpactStyleConfig* StyleConfig = &SparkStyle;
	switch (InStyle)
	{
	case ERogueImpactVisualStyle::Laser:
		EffectStaticMesh = LaserMeshAsset.Get();
		EffectMaterial = LaserMaterial.Get();
		StyleConfig = &LaserStyle;
		break;
	case ERogueImpactVisualStyle::Explosion:
		EffectStaticMesh = ExplosionMeshAsset.Get();
		EffectMaterial = ExplosionMaterial.Get();
		StyleConfig = &ExplosionStyle;
		break;
	case ERogueImpactVisualStyle::Spark:
	default:
		break;
	}

	if (EffectStaticMesh != nullptr)
	{
		EffectMesh->SetStaticMesh(EffectStaticMesh);
	}

	if (EffectMaterial != nullptr)
	{
		EffectMesh->SetMaterial(0, EffectMaterial);
	}

	EffectMesh->SetRelativeLocation(FVector::ZeroVector);
	EffectMesh->SetRelativeRotation(StyleConfig->MeshLocalRotation);
	EffectMesh->SetRelativeScale3D(StyleConfig->MeshLocalScale);
	SetActorRotation(InRotation);
	SetActorScale3D(InScale);

	UParticleSystem* SelectedParticle = nullptr;
	switch (InStyle)
	{
	case ERogueImpactVisualStyle::Laser:
		SelectedParticle = LaserParticle.Get();
		break;
	case ERogueImpactVisualStyle::Explosion:
		SelectedParticle = ExplosionParticle.Get();
		break;
	case ERogueImpactVisualStyle::Spark:
	default:
		break;
	}

	if (UWorld* World = GetWorld())
	{
		if (StyleConfig->bSpawnPrimaryParticle && SelectedParticle != nullptr)
		{
			if (UParticleSystemComponent* ParticleComponent = UGameplayStatics::SpawnEmitterAtLocation(World, SelectedParticle, FTransform(GetActorRotation(), GetActorLocation(), StyleConfig->ParticleScale)))
			{
				ParticleComponent->bAutoDestroy = true;
				ParticleComponent->SetCastShadow(false);
				if (StyleConfig->bDeactivatePrimaryParticleImmediately)
				{
					ParticleComponent->DeactivateSystem();
				}
			}
		}

		if (InStyle == ERogueImpactVisualStyle::Explosion && ExplosionSmokeParticle != nullptr)
		{
			if (UParticleSystemComponent* SmokeComponent = UGameplayStatics::SpawnEmitterAtLocation(World, ExplosionSmokeParticle, FTransform(GetActorRotation(), GetActorLocation(), ExplosionSmokeScale)))
			{
				SmokeComponent->bAutoDestroy = true;
				SmokeComponent->SetCastShadow(false);
				SmokeComponent->DeactivateSystem();
			}
		}
	}
}

ARogueImpactEffect* ARogueImpactEffect::SpawnImpactEffect(UWorld* World, const FVector& Location, const FRotator& Rotation, ERogueImpactVisualStyle Style, const FVector& Scale, float Lifetime, AActor* Owner)
{
	if (World == nullptr)
	{
		return nullptr;
	}

	if (const ARogueGameMode* RogueGameMode = World->GetAuthGameMode<ARogueGameMode>())
	{
		if (RogueGameMode->ShouldCullCombatEffects())
		{
			return nullptr;
		}
	}

	if ((Style == ERogueImpactVisualStyle::Spark && GActiveImpactEffectCount >= 18) ||
		(Style == ERogueImpactVisualStyle::Laser && GActiveImpactEffectCount >= 12))
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = Owner;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ARogueImpactEffect* ImpactEffect = nullptr;
	if (ARogueGameMode* RogueGameMode = World->GetAuthGameMode<ARogueGameMode>())
	{
		ImpactEffect = RogueGameMode->AcquireImpactEffect(Location, Rotation, Owner);
	}
	else
	{
		ImpactEffect = World->SpawnActor<ARogueImpactEffect>(StaticClass(), Location, Rotation, SpawnParameters);
	}

	if (ImpactEffect != nullptr)
	{
		ImpactEffect->ActivatePooledEffect(Owner, Location, Rotation, Style, Scale, Lifetime);
	}

	return ImpactEffect;
}
