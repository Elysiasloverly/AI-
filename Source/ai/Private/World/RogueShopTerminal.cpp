#include "World/RogueShopTerminal.h"

#include "Player/RogueCharacter.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

ARogueShopTerminal::ARogueShopTerminal()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	BaseMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BaseMesh"));
	BaseMesh->SetupAttachment(SceneRoot);
	BaseMesh->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	BaseMesh->SetCastShadow(false);

	GlowMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GlowMesh"));
	GlowMesh->SetupAttachment(SceneRoot);
	GlowMesh->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	GlowMesh->SetCastShadow(false);

	InteractionBox = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionBox"));
	InteractionBox->SetupAttachment(SceneRoot);
	InteractionBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	InteractionBox->SetBoxExtent(FVector(150.0f, 150.0f, 150.0f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Game/GameAssets/StarterContent/Shapes/Shape_Cube.Shape_Cube"));
	if (CubeMesh.Succeeded())
	{
		BaseMesh->SetStaticMesh(CubeMesh.Object);
		GlowMesh->SetStaticMesh(CubeMesh.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BaseMaterial(TEXT("/Game/GameAssets/Vefects/Stylized_Galaxy_Shader/Demo/Shared/Materials/MI_Opaque_Dark_01.MI_Opaque_Dark_01"));
	if (BaseMaterial.Succeeded())
	{
		BaseMesh->SetMaterial(0, BaseMaterial.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> GlowMaterial(TEXT("/Game/GameAssets/HologramShader/Materials/HologramShader/MI/MI_Hologram_8.MI_Hologram_8"));
	if (GlowMaterial.Succeeded())
	{
		GlowMesh->SetMaterial(0, GlowMaterial.Object);
	}

	BaseMesh->SetRelativeScale3D(FVector(1.2f, 1.2f, 2.4f));
	BaseMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 120.0f));
	GlowMesh->SetRelativeScale3D(FVector(0.82f, 0.82f, 2.05f));
	GlowMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 138.0f));
}

void ARogueShopTerminal::BeginPlay()
{
	Super::BeginPlay();

	InteractionBox->OnComponentBeginOverlap.AddDynamic(this, &ARogueShopTerminal::HandleOverlapBegin);
	InteractionBox->OnComponentEndOverlap.AddDynamic(this, &ARogueShopTerminal::HandleOverlapEnd);
}

FVector ARogueShopTerminal::GetPromptWorldLocation() const
{
	return GetActorLocation() + FVector(0.0f, 0.0f, 220.0f);
}

void ARogueShopTerminal::HandleOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (ARogueCharacter* PlayerCharacter = Cast<ARogueCharacter>(OtherActor))
	{
		PlayerCharacter->SetNearbyShopTerminal(this);
	}
}

void ARogueShopTerminal::HandleOverlapEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (ARogueCharacter* PlayerCharacter = Cast<ARogueCharacter>(OtherActor))
	{
		PlayerCharacter->ClearNearbyShopTerminal(this);
	}
}
