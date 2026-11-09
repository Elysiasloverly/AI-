#include "RogueBeamVFXSetup.h"
#include "Combat/Visuals/RogueLaserBeam.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Blueprint.h"
#include "Engine/World.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Materials/MaterialInterface.h"
#include "UObject/UnrealType.h"

bool URogueBeamVFXSetup::ApplyBeamMaterials(UBlueprint* BP, UMaterialInterface* Core, UMaterialInterface* Glow, UMaterialInterface* Ring, bool bInferno)
{
    if (!BP || !BP->GeneratedClass || !BP->GeneratedClass->IsChildOf(ARogueLaserBeam::StaticClass()) || !Core || !Glow || !Ring) return false;
    auto* CDO = BP->GeneratedClass->GetDefaultObject<ARogueLaserBeam>();
    auto* ComponentMode = FindFProperty<FBoolProperty>(ARogueLaserBeam::StaticClass(), TEXT("bUseBlueprintComponentMaterials"));
    auto* ZAxis = FindFProperty<FBoolProperty>(ARogueLaserBeam::StaticClass(), TEXT("bGlowMeshUsesZAxis"));
    auto* StyleProperty = FindFProperty<FStructProperty>(ARogueLaserBeam::StaticClass(), bInferno ? TEXT("InfernoVisualStyle") : TEXT("PlayerVisualStyle"));
    if (!CDO || !ComponentMode || !ZAxis || !StyleProperty) return false;
    BP->Modify(); CDO->Modify();
    ComponentMode->SetPropertyValue_InContainer(CDO, true);
    ZAxis->SetPropertyValue_InContainer(CDO, true);
    TInlineComponentArray<UStaticMeshComponent*> Components;
    CDO->GetComponents(Components);
    int32 Changed = 0;
    for (UStaticMeshComponent* Mesh : Components)
    {
        UMaterialInterface* Material = nullptr;
        if (Mesh->GetFName() == TEXT("BeamMesh")) Material = Core;
        else if (Mesh->GetFName() == TEXT("GlowMesh"))
        {
            Material = Glow;
            Mesh->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f));
        }
        else if (Mesh->GetFName() == TEXT("StartRingMesh") || Mesh->GetFName() == TEXT("EndRingMesh")) Material = Ring;
        if (Material) { Mesh->Modify(); Mesh->SetMaterial(0, Material); ++Changed; }
    }
    if (Changed != 4) return false;
    auto* Style = StyleProperty->ContainerPtrToValuePtr<FRogueLaserBeamVisualStyle>(CDO);
    Style->BeamThickness = bInferno ? .115f : .085f;
    Style->GlowThickness = bInferno ? .30f : .235f;
    Style->GlowLengthScaleMultiplier = 1.f;
    Style->StartRingSize = bInferno ? .15f : .14f;
    Style->EndRingSize = bInferno ? .22f : .19f;
    Style->StartRingDepth = .035f;
    Style->EndRingDepth = .045f;
    Style->bShowStartAndEndRings = true;
    FBlueprintEditorUtils::MarkBlueprintAsModified(BP);
    FKismetEditorUtilities::CompileBlueprint(BP);
    return BP->Status != BS_Error;
}

ARogueLaserBeam* URogueBeamVFXSetup::CreatePreviewBeam(UObject* Context, bool bInferno, FVector Start, FVector End, float Lifetime)
{
    UWorld* World = Context ? Context->GetWorld() : nullptr;
    if (!World || !World->IsGameWorld()) return nullptr;
    UClass* Class = LoadClass<ARogueLaserBeam>(nullptr, bInferno
        ? TEXT("/Game/BP/Projectiles/BP_RogueHellTowerBeam.BP_RogueHellTowerBeam_C")
        : TEXT("/Game/BP/Projectiles/BP_RogueLaserBeam.BP_RogueLaserBeam_C"));
    if (!Class) return nullptr;
    FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Beam = World->SpawnActor<ARogueLaserBeam>(Class, FTransform::Identity, Params);
    if (Beam) Beam->ActivatePooledBeam(nullptr, Start, End, bInferno, false, Lifetime);
    return Beam;
}

bool URogueBeamVFXSetup::RefreshPreviewBeam(ARogueLaserBeam* Beam, bool bInferno, FVector Start, FVector End, float Lifetime)
{
    if (!IsValid(Beam) || !Beam->GetWorld() || !Beam->GetWorld()->IsGameWorld()) return false;
    Beam->ActivatePooledBeam(nullptr, Start, End, bInferno, false, Lifetime);
    return !Beam->IsAvailableInPool();
}
