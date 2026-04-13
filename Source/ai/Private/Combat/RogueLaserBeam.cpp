#include "Combat/RogueLaserBeam.h"

#include "Combat/RogueImpactEffect.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

ARogueLaserBeam::ARogueLaserBeam()
{
	PrimaryActorTick.bCanEverTick = false;
	PlayerVisualStyle.BeamThickness = 0.10f;
	PlayerVisualStyle.GlowThickness = 0.18f;
	PlayerVisualStyle.GlowLengthScaleMultiplier = 1.02f;
	PlayerVisualStyle.StartRingSize = 0.22f;
	PlayerVisualStyle.StartRingDepth = 0.10f;
	PlayerVisualStyle.EndRingSize = 0.30f;
	PlayerVisualStyle.EndRingDepth = 0.12f;
	PlayerVisualStyle.ImpactEffectScale = FVector(0.22f, 0.22f, 0.22f);
	PlayerVisualStyle.ImpactEffectLifetime = 0.12f;
	PlayerVisualStyle.bShowStartAndEndRings = true;

	InfernoVisualStyle.BeamThickness = 0.14f;
	InfernoVisualStyle.GlowThickness = 0.24f;
	InfernoVisualStyle.GlowLengthScaleMultiplier = 1.04f;
	InfernoVisualStyle.StartRingSize = 0.18f;
	InfernoVisualStyle.StartRingDepth = 0.10f;
	InfernoVisualStyle.EndRingSize = 0.24f;
	InfernoVisualStyle.EndRingDepth = 0.12f;
	InfernoVisualStyle.ImpactEffectScale = FVector(0.18f, 0.18f, 0.18f);
	InfernoVisualStyle.ImpactEffectLifetime = 0.12f;
	InfernoVisualStyle.bShowStartAndEndRings = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	BeamMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BeamMesh"));
	BeamMesh->SetupAttachment(SceneRoot);
	BeamMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BeamMesh->SetCastShadow(false);

	GlowMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GlowMesh"));
	GlowMesh->SetupAttachment(SceneRoot);
	GlowMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GlowMesh->SetCastShadow(false);

	StartRingMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StartRingMesh"));
	StartRingMesh->SetupAttachment(SceneRoot);
	StartRingMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	StartRingMesh->SetCastShadow(false);
	StartRingMesh->SetRelativeRotation(FRotator(90.0f, 0.0f, 0.0f));

	EndRingMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("EndRingMesh"));
	EndRingMesh->SetupAttachment(SceneRoot);
	EndRingMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	EndRingMesh->SetCastShadow(false);
	EndRingMesh->SetRelativeRotation(FRotator(90.0f, 0.0f, 0.0f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> BeamMeshAsset(TEXT("/Game/GameAssets/StarterContent/Shapes/Shape_Cube.Shape_Cube"));
	if (BeamMeshAsset.Succeeded())
	{
		BeamMesh->SetStaticMesh(BeamMeshAsset.Object);
		GlowMesh->SetStaticMesh(BeamMeshAsset.Object);
	}

	static ConstructorHelpers::FObjectFinder<UStaticMesh> RingMeshAsset(TEXT("/Game/GameAssets/StarterContent/Shapes/Shape_Torus.Shape_Torus"));
	if (RingMeshAsset.Succeeded())
	{
		StartRingMesh->SetStaticMesh(RingMeshAsset.Object);
		EndRingMesh->SetStaticMesh(RingMeshAsset.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BeamMaterial(TEXT("/Game/GameAssets/Realistic_Starter_VFX_Pack_Vol2/Materials/M_Glow_C_Inst.M_Glow_C_Inst"));
	if (BeamMaterial.Succeeded())
	{
		PlayerBeamMaterial = BeamMaterial.Object;
		BeamMesh->SetMaterial(0, PlayerBeamMaterial);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> GlowMaterial(TEXT("/Game/GameAssets/Vefects/Stylized_Galaxy_Shader/Galaxy/Materials/MI_VFX_Lush_Galaxy_Shader_01_Inverted.MI_VFX_Lush_Galaxy_Shader_01_Inverted"));
	if (GlowMaterial.Succeeded())
	{
		PlayerGlowMaterial = GlowMaterial.Object;
		GlowMesh->SetMaterial(0, PlayerGlowMaterial);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> RingMaterial(TEXT("/Game/GameAssets/HologramShader/Materials/HologramShader/MI/MI_Hologram_10.MI_Hologram_10"));
	if (RingMaterial.Succeeded())
	{
		PlayerRingMaterial = RingMaterial.Object;
		StartRingMesh->SetMaterial(0, PlayerRingMaterial);
		EndRingMesh->SetMaterial(0, PlayerRingMaterial);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> InfernoBeamMaterialAsset(TEXT("/Game/GameAssets/StarterContent/Materials/M_Metal_Copper.M_Metal_Copper"));
	if (InfernoBeamMaterialAsset.Succeeded())
	{
		InfernoBeamMaterial = InfernoBeamMaterialAsset.Object;
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> InfernoGlowMaterialAsset(TEXT("/Game/GameAssets/Realistic_Starter_VFX_Pack_Vol2/Materials/M_Glow_A.M_Glow_A"));
	if (InfernoGlowMaterialAsset.Succeeded())
	{
		InfernoGlowMaterial = InfernoGlowMaterialAsset.Object;
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> InfernoRingMaterialAsset(TEXT("/Game/GameAssets/HologramShader/Materials/HologramShader/MI/MI_Hologram_3.MI_Hologram_3"));
	if (InfernoRingMaterialAsset.Succeeded())
	{
		InfernoRingMaterial = InfernoRingMaterialAsset.Object;
	}
}

void ARogueLaserBeam::BeginPlay()
{
	Super::BeginPlay();
	DeactivateToPool();
}

void ARogueLaserBeam::InitializeBeam(const FVector& StartLocation, const FVector& EndLocation, bool bUseInfernoStyle, bool bSpawnImpactEffect, float BeamLifetime)
{
	ActivatePooledBeam(GetOwner(), StartLocation, EndLocation, bUseInfernoStyle, bSpawnImpactEffect, BeamLifetime);
}

void ARogueLaserBeam::ActivatePooledBeam(AActor* InOwner, const FVector& StartLocation, const FVector& EndLocation, bool bUseInfernoStyle, bool bSpawnImpactEffect, float BeamLifetime)
{
	SetOwner(InOwner);
	SetActorHiddenInGame(false);
	bPoolAvailable = false;

	const FVector BeamVector = EndLocation - StartLocation;
	const float Length = BeamVector.Size();
	if (Length <= KINDA_SMALL_NUMBER)
	{
		DeactivateToPool();
		return;
	}

	const FVector Direction = BeamVector / Length;
	const FVector MidPoint = StartLocation + BeamVector * 0.5f;
	const FRogueLaserBeamVisualStyle& VisualStyle = bUseInfernoStyle ? InfernoVisualStyle : PlayerVisualStyle;
	const float BeamLengthScale = FMath::Max(MinimumBeamLengthScale, Length / FMath::Max(1.0f, BeamLengthDivisor));

	SetActorLocation(MidPoint);
	SetActorRotation(Direction.Rotation());

	if (bUseInfernoStyle)
	{
		if (InfernoBeamMaterial != nullptr)
		{
			BeamMesh->SetMaterial(0, InfernoBeamMaterial);
		}
		if (InfernoGlowMaterial != nullptr)
		{
			GlowMesh->SetMaterial(0, InfernoGlowMaterial);
		}
		if (InfernoRingMaterial != nullptr)
		{
			StartRingMesh->SetMaterial(0, InfernoRingMaterial);
			EndRingMesh->SetMaterial(0, InfernoRingMaterial);
		}
	}
	else
	{
		if (PlayerBeamMaterial != nullptr)
		{
			BeamMesh->SetMaterial(0, PlayerBeamMaterial);
		}
		if (PlayerGlowMaterial != nullptr)
		{
			GlowMesh->SetMaterial(0, PlayerGlowMaterial);
		}
		if (PlayerRingMaterial != nullptr)
		{
			StartRingMesh->SetMaterial(0, PlayerRingMaterial);
			EndRingMesh->SetMaterial(0, PlayerRingMaterial);
		}
	}

	StartRingMesh->SetVisibility(VisualStyle.bShowStartAndEndRings);
	EndRingMesh->SetVisibility(VisualStyle.bShowStartAndEndRings);

	BeamMesh->SetRelativeScale3D(FVector(BeamLengthScale, VisualStyle.BeamThickness, VisualStyle.BeamThickness));
	GlowMesh->SetRelativeScale3D(FVector(BeamLengthScale * VisualStyle.GlowLengthScaleMultiplier, VisualStyle.GlowThickness, VisualStyle.GlowThickness));
	StartRingMesh->SetRelativeLocation(FVector(-Length * 0.5f, 0.0f, 0.0f));
	StartRingMesh->SetRelativeScale3D(FVector(VisualStyle.StartRingSize, VisualStyle.StartRingSize, VisualStyle.StartRingDepth));
	EndRingMesh->SetRelativeLocation(FVector(Length * 0.5f, 0.0f, 0.0f));
	EndRingMesh->SetRelativeScale3D(FVector(VisualStyle.EndRingSize, VisualStyle.EndRingSize, VisualStyle.EndRingDepth));

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ReturnToPoolTimerHandle);
		World->GetTimerManager().SetTimer(ReturnToPoolTimerHandle, this, &ARogueLaserBeam::DeactivateToPool, FMath::Max(MinimumBeamLifetime, BeamLifetime), false);
	}

	if (bSpawnImpactEffect)
	{
		ARogueImpactEffect::SpawnImpactEffect(GetWorld(), EndLocation, Direction.Rotation(), ERogueImpactVisualStyle::Laser, VisualStyle.ImpactEffectScale, VisualStyle.ImpactEffectLifetime, this);
	}
}

void ARogueLaserBeam::DeactivateToPool()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ReturnToPoolTimerHandle);
	}

	bPoolAvailable = true;
	SetActorHiddenInGame(true);
	SetOwner(nullptr);
	StartRingMesh->SetVisibility(false);
	EndRingMesh->SetVisibility(false);
}
