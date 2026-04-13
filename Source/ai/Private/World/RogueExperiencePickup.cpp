#include "World/RogueExperiencePickup.h"

#include "Player/RogueCharacter.h"
#include "Core/RogueGameMode.h"
#include "Components/PointLightComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

ARogueExperiencePickup::ARogueExperiencePickup()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.05f;

	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	Collision->InitSphereRadius(40.0f);
	Collision->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	RootComponent = Collision;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(RootComponent);
	Mesh->SetRelativeScale3D(FVector(0.24f, 0.24f, 0.24f));
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(false);

	GlowMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GlowMesh"));
	GlowMesh->SetupAttachment(RootComponent);
	GlowMesh->SetRelativeScale3D(FVector(0.46f, 0.46f, 0.46f));
	GlowMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GlowMesh->SetCastShadow(false);

	GlowLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("GlowLight"));
	GlowLight->SetupAttachment(RootComponent);
	GlowLight->SetIntensity(780.0f);
	GlowLight->SetLightColor(FLinearColor(0.14f, 0.72f, 1.0f));
	GlowLight->SetAttenuationRadius(125.0f);
	GlowLight->SetSourceRadius(12.0f);
	GlowLight->SetSoftSourceRadius(16.0f);
	GlowLight->SetUseInverseSquaredFalloff(false);
	GlowLight->SetCastShadows(false);
	GlowLight->SetVisibility(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PickupMesh(TEXT("/Game/GameAssets/StarterContent/Shapes/Shape_Sphere.Shape_Sphere"));
	if (PickupMesh.Succeeded())
	{
		Mesh->SetStaticMesh(PickupMesh.Object);
		GlowMesh->SetStaticMesh(PickupMesh.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> PickupMaterial(TEXT("/Game/GameAssets/Realistic_Starter_VFX_Pack_Vol2/Materials/M_Glow_C_Inst.M_Glow_C_Inst"));
	if (PickupMaterial.Succeeded())
	{
		Mesh->SetMaterial(0, PickupMaterial.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> PickupGlowMaterial(TEXT("/Game/GameAssets/Vefects/Stylized_Galaxy_Shader/Galaxy/Materials/MI_VFX_Lush_Galaxy_Shader_01.MI_VFX_Lush_Galaxy_Shader_01"));
	if (PickupGlowMaterial.Succeeded())
	{
		GlowMesh->SetMaterial(0, PickupGlowMaterial.Object);
	}

	Collision->OnComponentBeginOverlap.AddDynamic(this, &ARogueExperiencePickup::HandleOverlap);
}

void ARogueExperiencePickup::BeginPlay()
{
	Super::BeginPlay();
	CachedPlayerCharacter = Cast<ARogueCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
	DeactivateToPool();
}

void ARogueExperiencePickup::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		if (ARogueGameMode* RogueGameMode = World->GetAuthGameMode<ARogueGameMode>())
		{
			RogueGameMode->UnregisterExperiencePickup(this);
		}
	}

	Super::EndPlay(EndPlayReason);
}

void ARogueExperiencePickup::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bPoolAvailable)
	{
		return;
	}

	ActiveTimeSeconds += DeltaSeconds;
	if (ActiveTimeSeconds >= PickupLifetime)
	{
		DeactivateToPool();
		return;
	}

	ARogueGameMode* RogueGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ARogueGameMode>() : nullptr;
	const bool bCullCombatEffects = RogueGameMode != nullptr && RogueGameMode->ShouldCullCombatEffects();
	float HoverAlpha = 0.5f;
	if (bCullCombatEffects)
	{
		if (!bVisualsSimplified)
		{
			bVisualsSimplified = true;
			Mesh->SetRelativeLocation(FVector::ZeroVector);
			GlowMesh->SetRelativeLocation(FVector::ZeroVector);
			GlowMesh->SetRelativeScale3D(FVector(0.40f, 0.40f, 0.40f));
		}
	}
	else
	{
		bVisualsSimplified = false;
		PulseTime += DeltaSeconds;
		HoverAlpha = 0.5f + 0.5f * FMath::Sin(PulseTime * 3.6f);
		const float HoverOffset = FMath::Sin(PulseTime * 2.0f) * 10.0f;
		Mesh->SetRelativeLocation(FVector(0.0f, 0.0f, HoverOffset));
		GlowMesh->SetRelativeLocation(FVector(0.0f, 0.0f, HoverOffset));
	}

	ARogueCharacter* PlayerCharacter = CachedPlayerCharacter.Get();
	if (!IsValid(PlayerCharacter))
	{
		PlayerCharacter = Cast<ARogueCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
		CachedPlayerCharacter = PlayerCharacter;
	}

	if (!IsValid(PlayerCharacter) || PlayerCharacter->IsDead())
	{
		return;
	}

	const float PickupRange = ActiveTimeSeconds >= AutoMagnetDelay ? 100000.0f : PlayerCharacter->GetPickupRadius();
	const float DistanceSquared = FVector::DistSquared2D(GetActorLocation(), PlayerCharacter->GetActorLocation());
	if (DistanceSquared > FMath::Square(PickupRange))
	{
		if (!bCullCombatEffects)
		{
			GlowMesh->SetRelativeScale3D(FVector(0.40f + HoverAlpha * 0.10f, 0.40f + HoverAlpha * 0.10f, 0.40f + HoverAlpha * 0.10f));
		}
		return;
	}

	const FVector NewLocation = FMath::VInterpConstantTo(GetActorLocation(), PlayerCharacter->GetActorLocation(), DeltaSeconds, AttractionSpeed);
	SetActorLocation(NewLocation);
	if (!bCullCombatEffects)
	{
		const float AttractionAlpha = 0.5f + 0.5f * FMath::Sin(PulseTime * 8.0f);
		GlowMesh->SetRelativeScale3D(FVector(0.42f + AttractionAlpha * 0.12f, 0.42f + AttractionAlpha * 0.12f, 0.42f + AttractionAlpha * 0.12f));
	}
}

void ARogueExperiencePickup::InitializePickup(int32 InExperienceValue)
{
	ExperienceValue = FMath::Max(1, InExperienceValue);
	PulseTime = 0.0f;
	ActiveTimeSeconds = 0.0f;
	SetActorScale3D(FVector(FMath::Clamp(1.0f + static_cast<float>(ExperienceValue) * 0.015f, 1.0f, 2.4f)));
}

void ARogueExperiencePickup::AddExperienceValue(int32 InExperienceValue)
{
	ExperienceValue += InExperienceValue;
	SetActorScale3D(FVector(FMath::Clamp(1.0f + static_cast<float>(ExperienceValue) * 0.015f, 1.0f, 2.4f)));
}

void ARogueExperiencePickup::ActivatePooledPickup(AActor* InOwner, const FVector& SpawnLocation, int32 InExperienceValue)
{
	SetOwner(InOwner);
	SetActorLocation(SpawnLocation, false, nullptr, ETeleportType::TeleportPhysics);
	SetActorHiddenInGame(false);
	SetActorTickEnabled(true);
	SetActorEnableCollision(true);
	if (Collision != nullptr)
	{
		Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	}

	bPoolAvailable = false;
	bVisualsSimplified = false;
	InitializePickup(InExperienceValue);
}

void ARogueExperiencePickup::DeactivateToPool()
{
	if (!bPoolAvailable)
	{
		if (UWorld* World = GetWorld())
		{
			if (ARogueGameMode* RogueGameMode = World->GetAuthGameMode<ARogueGameMode>())
			{
				RogueGameMode->UnregisterExperiencePickup(this);
			}
		}
	}

	bPoolAvailable = true;
	bVisualsSimplified = false;
	PulseTime = 0.0f;
	ActiveTimeSeconds = 0.0f;
	ExperienceValue = 1;
	CachedPlayerCharacter.Reset();
	SetActorHiddenInGame(true);
	SetActorTickEnabled(false);
	SetActorEnableCollision(false);
	SetActorScale3D(FVector::OneVector);
	if (Collision != nullptr)
	{
		Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	SetOwner(nullptr);
}

void ARogueExperiencePickup::HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (bPoolAvailable)
	{
		return;
	}

	ARogueCharacter* PlayerCharacter = Cast<ARogueCharacter>(OtherActor);
	if (!IsValid(PlayerCharacter))
	{
		return;
	}

	PlayerCharacter->AddExperience(ExperienceValue);
	DeactivateToPool();
}
