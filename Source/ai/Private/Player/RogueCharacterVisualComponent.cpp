#include "Player/RogueCharacterVisualComponent.h"

#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

URogueCharacterVisualComponent::URogueCharacterVisualComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	BodyMesh->SetupAttachment(this);
	BodyMesh->SetRelativeLocation(FVector(0.0f, 0.0f, -24.0f));
	BodyMesh->SetRelativeScale3D(BodyBaseScale);
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BodyMesh->SetCastShadow(false);

	ChestPlateMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ChestPlateMesh"));
	ChestPlateMesh->SetupAttachment(this);
	ChestPlateMesh->SetRelativeLocation(FVector(24.0f, 0.0f, -23.0f));
	ChestPlateMesh->SetRelativeRotation(FRotator(0.0f, 45.0f, 0.0f));
	ChestPlateMesh->SetRelativeScale3D(FVector(0.18f, 0.36f, 0.22f));
	ChestPlateMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ChestPlateMesh->SetCastShadow(false);

	CoreMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CoreMesh"));
	CoreMesh->SetupAttachment(this);
	CoreMesh->SetRelativeLocation(FVector(0.0f, 0.0f, -24.0f));
	CoreMesh->SetRelativeScale3D(CoreBaseScale);
	CoreMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CoreMesh->SetCastShadow(false);

	HaloMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HaloMesh"));
	HaloMesh->SetupAttachment(this);
	HaloMesh->SetRelativeLocation(FVector(0.0f, 0.0f, -24.0f));
	HaloMesh->SetRelativeRotation(FRotator(90.0f, 0.0f, 0.0f));
	HaloMesh->SetRelativeScale3D(HaloBaseScale);
	HaloMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HaloMesh->SetCastShadow(false);

	ShellMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ShellMesh"));
	ShellMesh->SetupAttachment(this);
	ShellMesh->SetRelativeLocation(FVector(0.0f, 0.0f, -24.0f));
	ShellMesh->SetRelativeScale3D(FVector(1.12f, 1.12f, 1.12f));
	ShellMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ShellMesh->SetCastShadow(false);

	ShoulderLeftMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ShoulderLeftMesh"));
	ShoulderLeftMesh->SetupAttachment(this);
	ShoulderLeftMesh->SetRelativeLocation(FVector(0.0f, 26.0f, -26.0f));
	ShoulderLeftMesh->SetRelativeScale3D(FVector(0.18f, 0.24f, 0.20f));
	ShoulderLeftMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ShoulderLeftMesh->SetCastShadow(false);

	ShoulderRightMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ShoulderRightMesh"));
	ShoulderRightMesh->SetupAttachment(this);
	ShoulderRightMesh->SetRelativeLocation(FVector(0.0f, -26.0f, -26.0f));
	ShoulderRightMesh->SetRelativeScale3D(FVector(0.18f, 0.24f, 0.20f));
	ShoulderRightMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ShoulderRightMesh->SetCastShadow(false);

	BackpackMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BackpackMesh"));
	BackpackMesh->SetupAttachment(this);
	BackpackMesh->SetRelativeLocation(FVector(-18.0f, 0.0f, -32.0f));
	BackpackMesh->SetRelativeScale3D(FVector(0.22f, 0.42f, 0.28f));
	BackpackMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BackpackMesh->SetCastShadow(false);

	ThrusterLeftMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ThrusterLeftMesh"));
	ThrusterLeftMesh->SetupAttachment(this);
	ThrusterLeftMesh->SetRelativeLocation(FVector(-22.0f, 14.0f, -40.0f));
	ThrusterLeftMesh->SetRelativeScale3D(FVector(0.10f, 0.10f, 0.16f));
	ThrusterLeftMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ThrusterLeftMesh->SetCastShadow(false);

	ThrusterRightMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ThrusterRightMesh"));
	ThrusterRightMesh->SetupAttachment(this);
	ThrusterRightMesh->SetRelativeLocation(FVector(-22.0f, -14.0f, -40.0f));
	ThrusterRightMesh->SetRelativeScale3D(FVector(0.10f, 0.10f, 0.16f));
	ThrusterRightMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ThrusterRightMesh->SetCastShadow(false);

	HeadMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HeadMesh"));
	HeadMesh->SetupAttachment(this);
	HeadMesh->SetRelativeLocation(FVector(18.0f, 0.0f, -12.0f));
	HeadMesh->SetRelativeScale3D(FVector(0.16f, 0.16f, 0.16f));
	HeadMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HeadMesh->SetCastShadow(false);

	VisorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VisorMesh"));
	VisorMesh->SetupAttachment(this);
	VisorMesh->SetRelativeLocation(FVector(34.0f, 0.0f, -12.0f));
	VisorMesh->SetRelativeScale3D(FVector(0.08f, 0.23f, 0.10f));
	VisorMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	VisorMesh->SetCastShadow(false);

	WingLeftMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WingLeftMesh"));
	WingLeftMesh->SetupAttachment(this);
	WingLeftMesh->SetRelativeLocation(FVector(-8.0f, 26.0f, -28.0f));
	WingLeftMesh->SetRelativeRotation(FRotator(0.0f, 0.0f, 8.0f));
	WingLeftMesh->SetRelativeScale3D(FVector(0.11f, 0.32f, 0.14f));
	WingLeftMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WingLeftMesh->SetCastShadow(false);

	WingRightMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WingRightMesh"));
	WingRightMesh->SetupAttachment(this);
	WingRightMesh->SetRelativeLocation(FVector(-8.0f, -26.0f, -28.0f));
	WingRightMesh->SetRelativeRotation(FRotator(0.0f, 180.0f, -8.0f));
	WingRightMesh->SetRelativeScale3D(FVector(0.11f, 0.32f, 0.14f));
	WingRightMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WingRightMesh->SetCastShadow(false);

	TailFinMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TailFinMesh"));
	TailFinMesh->SetupAttachment(this);
	TailFinMesh->SetRelativeLocation(FVector(-42.0f, 0.0f, -18.0f));
	TailFinMesh->SetRelativeScale3D(FVector(0.12f, 0.12f, 0.34f));
	TailFinMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TailFinMesh->SetCastShadow(false);

	BodyLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("BodyLight"));
	BodyLight->SetupAttachment(this);
	BodyLight->SetRelativeLocation(FVector(0.0f, 0.0f, -12.0f));
	BodyLight->SetIntensity(580.0f);
	BodyLight->SetAttenuationRadius(240.0f);
	BodyLight->SetSourceRadius(20.0f);
	BodyLight->SetSoftSourceRadius(28.0f);
	BodyLight->SetLightColor(FLinearColor(0.22f, 0.68f, 1.0f));
	BodyLight->SetUseInverseSquaredFalloff(false);
	BodyLight->SetCastShadows(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Game/GameAssets/StarterContent/Shapes/Shape_Cube.Shape_Cube"));
	if (CubeMesh.Succeeded())
	{
		VisorMesh->SetStaticMesh(CubeMesh.Object);
		ShoulderLeftMesh->SetStaticMesh(CubeMesh.Object);
		ShoulderRightMesh->SetStaticMesh(CubeMesh.Object);
		BackpackMesh->SetStaticMesh(CubeMesh.Object);
		TailFinMesh->SetStaticMesh(CubeMesh.Object);
	}

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Game/GameAssets/StarterContent/Shapes/Shape_Sphere.Shape_Sphere"));
	if (SphereMesh.Succeeded())
	{
		BodyMesh->SetStaticMesh(SphereMesh.Object);
		CoreMesh->SetStaticMesh(SphereMesh.Object);
		ShellMesh->SetStaticMesh(SphereMesh.Object);
		ThrusterLeftMesh->SetStaticMesh(SphereMesh.Object);
		ThrusterRightMesh->SetStaticMesh(SphereMesh.Object);
		HeadMesh->SetStaticMesh(SphereMesh.Object);
	}

	static ConstructorHelpers::FObjectFinder<UStaticMesh> TorusMesh(TEXT("/Game/GameAssets/StarterContent/Shapes/Shape_Torus.Shape_Torus"));
	if (TorusMesh.Succeeded())
	{
		HaloMesh->SetStaticMesh(TorusMesh.Object);
	}

	static ConstructorHelpers::FObjectFinder<UStaticMesh> WedgeMesh(TEXT("/Game/GameAssets/StarterContent/Shapes/Shape_Wedge_B.Shape_Wedge_B"));
	if (WedgeMesh.Succeeded())
	{
		ChestPlateMesh->SetStaticMesh(WedgeMesh.Object);
		WingLeftMesh->SetStaticMesh(WedgeMesh.Object);
		WingRightMesh->SetStaticMesh(WedgeMesh.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BodyMaterial(TEXT("/Game/GameAssets/Vefects/Stylized_Galaxy_Shader/Demo/Shared/Materials/MI_Opaque_Dark_01.MI_Opaque_Dark_01"));
	if (BodyMaterial.Succeeded())
	{
		BodyMesh->SetMaterial(0, BodyMaterial.Object);
		ChestPlateMesh->SetMaterial(0, BodyMaterial.Object);
		HeadMesh->SetMaterial(0, BodyMaterial.Object);
		BackpackMesh->SetMaterial(0, BodyMaterial.Object);
		TailFinMesh->SetMaterial(0, BodyMaterial.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> AccentMaterial(TEXT("/Game/GameAssets/Vefects/Stylized_Galaxy_Shader/Demo/Scene/Materials/MI_Vefects_Grid_01.MI_Vefects_Grid_01"));
	if (AccentMaterial.Succeeded())
	{
		ShoulderLeftMesh->SetMaterial(0, AccentMaterial.Object);
		ShoulderRightMesh->SetMaterial(0, AccentMaterial.Object);
		WingLeftMesh->SetMaterial(0, AccentMaterial.Object);
		WingRightMesh->SetMaterial(0, AccentMaterial.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> CoreMaterial(TEXT("/Game/GameAssets/Vefects/Stylized_Galaxy_Shader/Galaxy/Materials/MI_VFX_Lush_Galaxy_Shader_02.MI_VFX_Lush_Galaxy_Shader_02"));
	if (CoreMaterial.Succeeded())
	{
		CoreMesh->SetMaterial(0, CoreMaterial.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> EnergyMaterial(TEXT("/Game/GameAssets/Realistic_Starter_VFX_Pack_Vol2/Materials/M_Glow_C_Inst.M_Glow_C_Inst"));
	if (EnergyMaterial.Succeeded())
	{
		HaloMesh->SetMaterial(0, EnergyMaterial.Object);
		VisorMesh->SetMaterial(0, EnergyMaterial.Object);
		ThrusterLeftMesh->SetMaterial(0, EnergyMaterial.Object);
		ThrusterRightMesh->SetMaterial(0, EnergyMaterial.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShellMaterial(TEXT("/Game/GameAssets/HologramShader/Materials/HologramShader/MI/MI_Hologram_12.MI_Hologram_12"));
	if (ShellMaterial.Succeeded())
	{
		ShellMesh->SetMaterial(0, ShellMaterial.Object);
	}

	ChestPlateMesh->SetVisibility(false, true);
	ShoulderLeftMesh->SetVisibility(false, true);
	ShoulderRightMesh->SetVisibility(false, true);
	BackpackMesh->SetVisibility(false, true);
	ThrusterLeftMesh->SetVisibility(false, true);
	ThrusterRightMesh->SetVisibility(false, true);
	HeadMesh->SetVisibility(false, true);
	VisorMesh->SetVisibility(false, true);
	WingLeftMesh->SetVisibility(false, true);
	WingRightMesh->SetVisibility(false, true);
	TailFinMesh->SetVisibility(false, true);
}

void URogueCharacterVisualComponent::TickVisuals(float DeltaSeconds, float HorizontalSpeed, bool bDashActive, float DashDuration, float DashTimeRemaining)
{
	VisualPulseTime += DeltaSeconds * (2.0f + HorizontalSpeed / 320.0f);
	const float PulseAlpha = 0.5f + 0.5f * FMath::Sin(VisualPulseTime * 2.8f);
	const float DashAlpha = bDashActive ? (0.55f + 0.45f * (DashDuration > 0.0f ? DashTimeRemaining / DashDuration : 0.0f)) : 0.0f;
	const float CoreScaleAlpha = 0.92f + PulseAlpha * 0.18f + DashAlpha * 0.18f;
	const float BodyScaleAlpha = 0.96f + PulseAlpha * 0.08f + DashAlpha * 0.12f;
	const float HaloScaleAlpha = 0.94f + PulseAlpha * 0.12f + DashAlpha * 0.14f;

	if (BodyMesh != nullptr)
	{
		BodyMesh->SetRelativeScale3D(BodyBaseScale * BodyScaleAlpha);
	}

	if (CoreMesh != nullptr)
	{
		CoreMesh->SetRelativeScale3D(CoreBaseScale * CoreScaleAlpha);
	}

	if (HaloMesh != nullptr)
	{
		HaloMesh->SetRelativeScale3D(HaloBaseScale * HaloScaleAlpha);
		HaloMesh->AddLocalRotation(FRotator(0.0f, (75.0f + DashAlpha * 140.0f) * DeltaSeconds, 0.0f));
	}

	if (ShellMesh != nullptr)
	{
		const float ShellScaleAlpha = 0.98f + PulseAlpha * 0.10f + DashAlpha * 0.18f;
		ShellMesh->SetRelativeScale3D(BodyBaseScale * 1.14f * ShellScaleAlpha);
		ShellMesh->AddLocalRotation(FRotator(0.0f, (-42.0f - DashAlpha * 80.0f) * DeltaSeconds, 0.0f));
	}

	if (BodyLight != nullptr)
	{
		BodyLight->SetIntensity(520.0f + PulseAlpha * 260.0f + DashAlpha * 540.0f);
	}
}
