#include "Combat/Visuals/RogueRevivalBurst.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
constexpr float BurstDuration = .86f;
constexpr int32 CrystalCount = 8;
}

ARogueRevivalBurst::ARogueRevivalBurst()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bAllowTickOnDedicatedServer = false;
	SetActorEnableCollision(false);
	Seal = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FrostSeal"));
	SetRootComponent(Seal);
	Crystals = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("IceCrystals"));
	Crystals->SetupAttachment(Seal);
	for (UMeshComponent* Mesh : {static_cast<UMeshComponent*>(Seal.Get()), static_cast<UMeshComponent*>(Crystals.Get())})
	{
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->SetGenerateOverlapEvents(false);
		Mesh->SetCanEverAffectNavigation(false);
		Mesh->SetCastShadow(false);
	}
}

void ARogueRevivalBurst::BeginPlay()
{
	Super::BeginPlay();
	SealMaterial = Seal->CreateDynamicMaterialInstance(0);
	if (Crystals->GetStaticMesh())
	{
		for (int32 Index = 0; Index < CrystalCount; ++Index) Crystals->AddInstance(FTransform::Identity);
	}
	ApplyPose(0.f);
	SetLifeSpan(BurstDuration);
}

void ARogueRevivalBurst::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Age += FMath::Max(DeltaSeconds, 0.f);
	ApplyPose(FMath::Clamp(Age / BurstDuration, 0.f, 1.f));
}

void ARogueRevivalBurst::ApplyPose(float Progress)
{
	if (SealMaterial) SealMaterial->SetScalarParameterValue(TEXT("Progress"), Progress);
	const float Converge = FMath::SmoothStep(0.f, .55f, Progress);
	const float Scatter = FMath::SmoothStep(.56f, .95f, Progress);
	const float Size = .6f * FMath::SmoothStep(0.f, .15f, Progress) * (1.f - FMath::SmoothStep(.68f, 1.f, Progress));
	const int32 Count = Crystals->GetInstanceCount();
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const float Angle = 2.f * PI * Index / Count + .8f * Converge + .16f * Scatter;
		const FVector Radial(FMath::Cos(Angle), FMath::Sin(Angle), 0.f);
		const FVector Position = Radial * FMath::Lerp(170.f, 34.f, Converge) + Radial * 52.f * Scatter
			+ FVector(0.f, 0.f, 20.f + 112.f * Converge + 22.f * Scatter + 10.f * (Index % 3));
		const FRotator Facing(55.f + 20.f * Converge, FMath::RadiansToDegrees(Angle) + 180.f, 0.f);
		Crystals->UpdateInstanceTransform(Index, FTransform(Facing, Position, FVector(FMath::Max(.001f, Size))),
			false, Index == Count - 1, true);
	}
}
