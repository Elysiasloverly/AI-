#include "World/RogueArena.h"

#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Particles/ParticleSystem.h"
#include "UObject/ConstructorHelpers.h"

ARogueArena::ARogueArena()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;
	SceneRoot->SetMobility(EComponentMobility::Static);

	FloorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FloorMesh"));
	FloorMesh->SetupAttachment(SceneRoot);

	NorthWall = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("NorthWall"));
	NorthWall->SetupAttachment(SceneRoot);

	SouthWall = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SouthWall"));
	SouthWall->SetupAttachment(SceneRoot);

	EastWall = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("EastWall"));
	EastWall->SetupAttachment(SceneRoot);

	WestWall = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WestWall"));
	WestWall->SetupAttachment(SceneRoot);

	NorthTrim = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("NorthTrim"));
	NorthTrim->SetupAttachment(SceneRoot);

	SouthTrim = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SouthTrim"));
	SouthTrim->SetupAttachment(SceneRoot);

	EastTrim = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("EastTrim"));
	EastTrim->SetupAttachment(SceneRoot);

	WestTrim = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WestTrim"));
	WestTrim->SetupAttachment(SceneRoot);

	CenterRing = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CenterRing"));
	CenterRing->SetupAttachment(SceneRoot);

	NorthBeacon = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("NorthBeacon"));
	NorthBeacon->SetupAttachment(SceneRoot);

	SouthBeacon = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SouthBeacon"));
	SouthBeacon->SetupAttachment(SceneRoot);

	EastBeacon = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("EastBeacon"));
	EastBeacon->SetupAttachment(SceneRoot);

	WestBeacon = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WestBeacon"));
	WestBeacon->SetupAttachment(SceneRoot);

	CenterFillLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("CenterFillLight"));
	CenterFillLight->SetupAttachment(SceneRoot);

	NorthAccentLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("NorthAccentLight"));
	NorthAccentLight->SetupAttachment(SceneRoot);

	SouthAccentLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("SouthAccentLight"));
	SouthAccentLight->SetupAttachment(SceneRoot);

	EastAccentLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("EastAccentLight"));
	EastAccentLight->SetupAttachment(SceneRoot);

	WestAccentLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("WestAccentLight"));
	WestAccentLight->SetupAttachment(SceneRoot);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Game/GameAssets/StarterContent/Shapes/Shape_Cube.Shape_Cube"));
	if (CubeMesh.Succeeded())
	{
		const TArray<TObjectPtr<UStaticMeshComponent>> Meshes = { FloorMesh, NorthWall, SouthWall, EastWall, WestWall, NorthTrim, SouthTrim, EastTrim, WestTrim };
		for (UStaticMeshComponent* Mesh : Meshes)
		{
			Mesh->SetStaticMesh(CubeMesh.Object);
			Mesh->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
			Mesh->SetMobility(EComponentMobility::Static);
			Mesh->SetCastShadow(false);
		}

		FloorMesh->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		NorthWall->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		SouthWall->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		EastWall->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		WestWall->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	}

	static ConstructorHelpers::FObjectFinder<UStaticMesh> TorusMesh(TEXT("/Game/GameAssets/StarterContent/Shapes/Shape_Torus.Shape_Torus"));
	if (TorusMesh.Succeeded())
	{
		CenterRing->SetStaticMesh(TorusMesh.Object);
		CenterRing->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
		CenterRing->SetMobility(EComponentMobility::Static);
		CenterRing->SetCastShadow(false);
	}

	static ConstructorHelpers::FObjectFinder<UStaticMesh> BeaconMesh(TEXT("/Game/GameAssets/StarterContent/Shapes/Shape_Cylinder.Shape_Cylinder"));
	if (BeaconMesh.Succeeded())
	{
		const TArray<TObjectPtr<UStaticMeshComponent>> Beacons = { NorthBeacon, SouthBeacon, EastBeacon, WestBeacon };
		for (UStaticMeshComponent* Beacon : Beacons)
		{
			Beacon->SetStaticMesh(BeaconMesh.Object);
			Beacon->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
			Beacon->SetMobility(EComponentMobility::Static);
			Beacon->SetCastShadow(false);
		}
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> FloorMaterial(TEXT("/Game/GameAssets/Vefects/Stylized_Galaxy_Shader/Demo/Scene/Materials/MI_Vefects_Grid_02.MI_Vefects_Grid_02"));
	if (FloorMaterial.Succeeded())
	{
		FloorMesh->SetMaterial(0, FloorMaterial.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> WallMaterial(TEXT("/Game/GameAssets/Vefects/Stylized_Galaxy_Shader/Demo/Shared/Materials/MI_Opaque_Dark_01.MI_Opaque_Dark_01"));
	if (WallMaterial.Succeeded())
	{
		NorthWall->SetMaterial(0, WallMaterial.Object);
		SouthWall->SetMaterial(0, WallMaterial.Object);
		EastWall->SetMaterial(0, WallMaterial.Object);
		WestWall->SetMaterial(0, WallMaterial.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> TrimMaterial(TEXT("/Game/GameAssets/Grid_Materials/Materials/Instances/MI_BlockOut_Grass_Neon_2.MI_BlockOut_Grass_Neon_2"));
	if (TrimMaterial.Succeeded())
	{
		NorthTrim->SetMaterial(0, TrimMaterial.Object);
		SouthTrim->SetMaterial(0, TrimMaterial.Object);
		EastTrim->SetMaterial(0, TrimMaterial.Object);
		WestTrim->SetMaterial(0, TrimMaterial.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> CenterRingMaterial(TEXT("/Game/GameAssets/Vefects/Stylized_Galaxy_Shader/Galaxy/Materials/MI_VFX_Lush_Galaxy_Shader_01_Inverted.MI_VFX_Lush_Galaxy_Shader_01_Inverted"));
	if (CenterRingMaterial.Succeeded())
	{
		CenterRing->SetMaterial(0, CenterRingMaterial.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BeaconMaterial(TEXT("/Game/GameAssets/HologramShader/Materials/HologramShader/MI/MI_Hologram_8.MI_Hologram_8"));
	if (BeaconMaterial.Succeeded())
	{
		NorthBeacon->SetMaterial(0, BeaconMaterial.Object);
		SouthBeacon->SetMaterial(0, BeaconMaterial.Object);
		EastBeacon->SetMaterial(0, BeaconMaterial.Object);
		WestBeacon->SetMaterial(0, BeaconMaterial.Object);
	}

	static ConstructorHelpers::FObjectFinder<UParticleSystem> CornerParticle(TEXT("/Game/GameAssets/Realistic_Starter_VFX_Pack_Vol2/Particles/Sparks/P_Embers_A.P_Embers_A"));
	if (CornerParticle.Succeeded())
	{
		AmbientCornerParticle = CornerParticle.Object;
	}

	FloorMesh->SetRelativeLocation(FVector(0.0f, 0.0f, -60.0f));
	FloorMesh->SetRelativeScale3D(FVector(120.0f, 120.0f, 1.0f));

	NorthWall->SetRelativeLocation(FVector(ArenaHalfExtent + 50.0f, 0.0f, WallHeight * 0.5f - 60.0f));
	NorthWall->SetRelativeScale3D(FVector(1.0f, 121.0f, WallHeight / 100.0f));

	SouthWall->SetRelativeLocation(FVector(-ArenaHalfExtent - 50.0f, 0.0f, WallHeight * 0.5f - 60.0f));
	SouthWall->SetRelativeScale3D(FVector(1.0f, 121.0f, WallHeight / 100.0f));

	EastWall->SetRelativeLocation(FVector(0.0f, ArenaHalfExtent + 50.0f, WallHeight * 0.5f - 60.0f));
	EastWall->SetRelativeScale3D(FVector(121.0f, 1.0f, WallHeight / 100.0f));

	WestWall->SetRelativeLocation(FVector(0.0f, -ArenaHalfExtent - 50.0f, WallHeight * 0.5f - 60.0f));
	WestWall->SetRelativeScale3D(FVector(121.0f, 1.0f, WallHeight / 100.0f));

	NorthTrim->SetRelativeLocation(FVector(ArenaHalfExtent - 30.0f, 0.0f, -18.0f));
	NorthTrim->SetRelativeScale3D(FVector(0.18f, 114.0f, 0.06f));

	SouthTrim->SetRelativeLocation(FVector(-ArenaHalfExtent + 30.0f, 0.0f, -18.0f));
	SouthTrim->SetRelativeScale3D(FVector(0.18f, 114.0f, 0.06f));

	EastTrim->SetRelativeLocation(FVector(0.0f, ArenaHalfExtent - 30.0f, -18.0f));
	EastTrim->SetRelativeScale3D(FVector(114.0f, 0.18f, 0.06f));

	WestTrim->SetRelativeLocation(FVector(0.0f, -ArenaHalfExtent + 30.0f, -18.0f));
	WestTrim->SetRelativeScale3D(FVector(114.0f, 0.18f, 0.06f));

	CenterRing->SetRelativeLocation(FVector(0.0f, 0.0f, -56.0f));
	CenterRing->SetRelativeRotation(FRotator(90.0f, 0.0f, 0.0f));
	CenterRing->SetRelativeScale3D(FVector(7.5f, 7.5f, 0.08f));

	NorthBeacon->SetRelativeLocation(FVector(ArenaHalfExtent - 540.0f, 0.0f, 92.0f));
	NorthBeacon->SetRelativeScale3D(FVector(0.48f, 0.48f, 2.20f));

	SouthBeacon->SetRelativeLocation(FVector(-ArenaHalfExtent + 540.0f, 0.0f, 92.0f));
	SouthBeacon->SetRelativeScale3D(FVector(0.48f, 0.48f, 2.20f));

	EastBeacon->SetRelativeLocation(FVector(0.0f, ArenaHalfExtent - 540.0f, 92.0f));
	EastBeacon->SetRelativeScale3D(FVector(0.48f, 0.48f, 2.20f));

	WestBeacon->SetRelativeLocation(FVector(0.0f, -ArenaHalfExtent + 540.0f, 92.0f));
	WestBeacon->SetRelativeScale3D(FVector(0.48f, 0.48f, 2.20f));

	const TArray<TObjectPtr<UPointLightComponent>> AccentLights = { CenterFillLight, NorthAccentLight, SouthAccentLight, EastAccentLight, WestAccentLight };
	for (UPointLightComponent* LightComponent : AccentLights)
	{
		LightComponent->SetMobility(EComponentMobility::Static);
		LightComponent->SetCastShadows(false);
		LightComponent->SetUseInverseSquaredFalloff(false);
		LightComponent->SetSourceRadius(36.0f);
		LightComponent->SetSoftSourceRadius(56.0f);
		LightComponent->SetVisibility(true);
	}

	CenterFillLight->SetRelativeLocation(FVector(0.0f, 0.0f, 440.0f));
	CenterFillLight->SetIntensity(1500.0f);
	CenterFillLight->SetAttenuationRadius(4600.0f);
	CenterFillLight->SetLightColor(FLinearColor(0.28f, 0.45f, 0.86f));

	NorthAccentLight->SetRelativeLocation(NorthBeacon->GetRelativeLocation() + FVector(0.0f, 0.0f, 90.0f));
	NorthAccentLight->SetIntensity(3600.0f);
	NorthAccentLight->SetAttenuationRadius(2100.0f);
	NorthAccentLight->SetLightColor(FLinearColor(0.12f, 0.80f, 1.0f));

	SouthAccentLight->SetRelativeLocation(SouthBeacon->GetRelativeLocation() + FVector(0.0f, 0.0f, 90.0f));
	SouthAccentLight->SetIntensity(3200.0f);
	SouthAccentLight->SetAttenuationRadius(2100.0f);
	SouthAccentLight->SetLightColor(FLinearColor(0.92f, 0.22f, 0.90f));

	EastAccentLight->SetRelativeLocation(EastBeacon->GetRelativeLocation() + FVector(0.0f, 0.0f, 90.0f));
	EastAccentLight->SetIntensity(3400.0f);
	EastAccentLight->SetAttenuationRadius(2100.0f);
	EastAccentLight->SetLightColor(FLinearColor(0.20f, 0.62f, 1.0f));

	WestAccentLight->SetRelativeLocation(WestBeacon->GetRelativeLocation() + FVector(0.0f, 0.0f, 90.0f));
	WestAccentLight->SetIntensity(3400.0f);
	WestAccentLight->SetAttenuationRadius(2100.0f);
	WestAccentLight->SetLightColor(FLinearColor(0.72f, 0.32f, 1.0f));
}

void ARogueArena::BeginPlay()
{
	Super::BeginPlay();

	if (GetWorld() == nullptr)
	{
		return;
	}

	if (AmbientCornerParticle != nullptr)
	{
		const TArray<FVector> CornerOffsets =
		{
			FVector(ArenaHalfExtent - 260.0f, ArenaHalfExtent - 260.0f, 30.0f),
			FVector(ArenaHalfExtent - 260.0f, -ArenaHalfExtent + 260.0f, 30.0f),
			FVector(-ArenaHalfExtent + 260.0f, ArenaHalfExtent - 260.0f, 30.0f),
			FVector(-ArenaHalfExtent + 260.0f, -ArenaHalfExtent + 260.0f, 30.0f)
		};

		for (const FVector& CornerOffset : CornerOffsets)
		{
			UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), AmbientCornerParticle, FTransform(FRotator::ZeroRotator, GetActorLocation() + CornerOffset, FVector(1.35f, 1.35f, 1.35f)));
		}
	}
}
