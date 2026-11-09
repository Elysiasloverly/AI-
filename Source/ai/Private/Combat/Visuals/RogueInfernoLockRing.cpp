#include "Combat/Visuals/RogueInfernoLockRing.h"
#include "Combat/Visuals/RogueGroundEffectFade.h"
#include "Components/CapsuleComponent.h"
#include "Components/MeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Enemies/RogueEnemy.h"
#include "Combat/Targeting/RogueEnemyTrackerSubsystem.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "UObject/ConstructorHelpers.h"

ARogueInfernoLockRing::ARogueInfernoLockRing()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	SetActorEnableCollision(false);
	RingMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LockRing"));
	SetRootComponent(RingMesh);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Plane(TEXT("/Engine/BasicShapes/Plane.Plane"));
	RingMesh->SetStaticMesh(Plane.Object);
	RingMesh->SetMobility(EComponentMobility::Movable);
	RingMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RingMesh->SetGenerateOverlapEvents(false);
	RingMesh->SetCanEverAffectNavigation(false);
	RingMesh->SetCastShadow(false);
	RingMesh->bReceivesDecals = false;
	SetActorHiddenInGame(true);
}
void ARogueInfernoLockRing::ActivateRing(ARogueEnemy* Target, float LockedSeconds)
{
	if (!Target || !RingMaterial || GetNetMode() == NM_DedicatedServer)
		return;

    float Radius=FMath::Max(10.f,OuterRadius);
    float EndRadius=FMath::Clamp(InnerRadius,1.f,Radius*.95f);
    if(bFitTargetSize)
    {
        const float CapsuleRadius=Target->GetCapsuleComponent()?Target->GetCapsuleComponent()->GetScaledCapsuleRadius():30.f;
        float BodyRadius=CapsuleRadius,Volume=0;
        for(auto* Mesh:TInlineComponentArray<UMeshComponent*>(Target))
        {
            if(!Mesh->IsVisible()||Mesh->bHiddenInGame||Mesh->ComponentHasTag(TEXT("Rogue.RoleAccessory"))||Mesh->ComponentHasTag(TEXT("Rogue.DamageArea")))continue;
            const FVector Extent=Mesh->Bounds.BoxExtent;
            const float CurrentVolume=Extent.X*Extent.Y*Extent.Z;
            if(CurrentVolume>Volume)
            {
                Volume=CurrentVolume;
                // The shorter horizontal extent avoids sizing the ring to a long broom or held weapon.
                BodyRadius=FMath::Max(CapsuleRadius,FMath::Min(Extent.X,Extent.Y));
            }
        }
        BodyRadius=FMath::Min(BodyRadius,CapsuleRadius*3.f);
        EndRadius=FMath::Max(EndRadius,BodyRadius+FMath::Max(0.f,BodyClearance));
        Radius=FMath::Max(Radius,EndRadius*1.8f);
    }
    FittedEndRatio=FMath::Clamp(EndRadius/Radius,.05f,.95f);
	RingMesh->SetMaterial(0, RingMaterial);
	// The 100 cm plane's authored ring radius is 0.8 of its half width (40 cm).
	SetActorScale3D(FVector(Radius / 40.f, Radius / 40.f, 1));
	SetActorRotation(FRotator::ZeroRotator);
	RingMesh->SetCustomPrimitiveDataVector3(4, FVector(Tint.R, Tint.G, Tint.B));
	CachedBirth = -1.e20f;
    CachedOpacity = -1.f;
	bAvailable = false;
	SetActorHiddenInGame(false);
	UpdateRing(Target, LockedSeconds);
}
void ARogueInfernoLockRing::UpdateRing(ARogueEnemy* Target, float LockedSeconds)
{
	if (!Target || bAvailable)
		return;
	const float HalfHeight =
		Target->GetCapsuleComponent() ? Target->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 0;
	SetActorLocation(Target->GetActorLocation() + FVector(0, 0, -HalfHeight + HeightOffset));
	const float Birth = GetWorld()->GetTimeSeconds() - FMath::Max(0.f, LockedSeconds);
    const auto* Tracker = GetWorld()->GetSubsystem<URogueEnemyTrackerSubsystem>();
    const float DisplayOpacity = FMath::Clamp(Opacity, 0.f, 1.f) * (Tracker && Tracker->ShouldCullCombatEffects() ? .78f : 1.f);
	if (!FMath::IsNearlyEqual(Birth, CachedBirth, .015f) || !FMath::IsNearlyEqual(DisplayOpacity, CachedOpacity))
	{
		CachedBirth = Birth;
        CachedOpacity = DisplayOpacity;
		RingMesh->SetCustomPrimitiveDataVector4(
			0, FVector4(Birth, FMath::Max(.1f, ShrinkSeconds),
						FittedEndRatio,
						DisplayOpacity));
	}
}
void ARogueInfernoLockRing::DeactivateRing()
{
    ARogueGroundEffectFade::CaptureActor(this);
	bAvailable = true;
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
	SetOwner(nullptr);
	CachedBirth = -1.e20f;
}
