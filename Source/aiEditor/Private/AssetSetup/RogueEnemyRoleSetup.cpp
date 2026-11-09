#include "RogueEnemyRoleSetup.h"
#include "Engine/Blueprint.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "Components/DecalComponent.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "AssetRegistry/AssetRegistryModule.h"
bool URogueEnemyRoleSetup::SetRoleAccessory(UBlueprint* B,FName Name,UStaticMesh* Mesh,UMaterialInterface* Material,FTransform Transform)
{
    if(!B||!B->SimpleConstructionScript||!Mesh||Name.IsNone())return false;
    B->Modify();auto* Node=B->SimpleConstructionScript->FindSCSNode(Name);
    if(!Node){Node=B->SimpleConstructionScript->CreateNode(UStaticMeshComponent::StaticClass(),Name);B->SimpleConstructionScript->AddNode(Node);}
    auto* C=Cast<UStaticMeshComponent>(Node->ComponentTemplate);if(!C)return false;
    C->Modify();C->SetStaticMesh(Mesh);C->SetMaterial(0,Material);C->SetRelativeTransform(Transform);
    C->SetCollisionEnabled(ECollisionEnabled::NoCollision);C->SetGenerateOverlapEvents(false);C->SetCastShadow(false);
    C->SetCanEverAffectNavigation(false);C->ComponentTags.AddUnique(TEXT("Rogue.RoleAccessory"));
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(B);return true;
}
namespace
{
    UMaterial* ColorMaterial(const FString& Name,FLinearColor Color)
    {
        const FString PackagePath=TEXT("/Game/GameAssets/Enemies/Utility/")+Name;
        if (auto* M=LoadObject<UMaterial>(nullptr,*(PackagePath+TEXT(".")+Name))) return M;
        auto* P=CreatePackage(*PackagePath);auto* M=NewObject<UMaterial>(P,*Name,RF_Public|RF_Standalone);
        auto* C=NewObject<UMaterialExpressionConstant3Vector>(M);C->Constant=Color;
        M->GetExpressionCollection().AddExpression(C);M->SetShadingModel(MSM_Unlit);
        M->GetEditorOnlyData()->EmissiveColor.Expression=C;M->TwoSided=true;
        M->PostEditChange();M->MarkPackageDirty();FAssetRegistryModule::AssetCreated(M);return M;
    }
    UStaticMeshComponent* Add(UBlueprint* B,const TCHAR* Name,const TCHAR* Mesh,FVector Scale,FVector Location,UMaterialInterface* M,FName Tag)
    {
        if (B->SimpleConstructionScript->FindSCSNode(Name)) return nullptr;
        auto* N=B->SimpleConstructionScript->CreateNode(UStaticMeshComponent::StaticClass(),Name);
        auto* C=Cast<UStaticMeshComponent>(N->ComponentTemplate);C->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,Mesh));
        if (M) C->SetMaterial(0,M);
        C->SetRelativeLocation(Location);C->SetRelativeScale3D(Scale);C->ComponentTags.Add(Tag);
        C->SetCollisionEnabled(ECollisionEnabled::NoCollision);C->SetGenerateOverlapEvents(false);C->SetCastShadow(false);
        B->SimpleConstructionScript->AddNode(N);return C;
    }
}
bool URogueEnemyRoleSetup::BuildEffect(UBlueprint* B,int32 Variant)
{
    if (!B||!B->SimpleConstructionScript||Variant<0||Variant>3) return false;
    B->Modify();
    auto* Material=ColorMaterial(Variant==2?TEXT("M_EnemyBossCrimson"):(Variant==3?TEXT("M_EnemyFlightCyan"):TEXT("M_EnemyHealingGreen")),
        Variant==2?FLinearColor(1,.06f,.12f):(Variant==3?FLinearColor(.05f,.75f,1):FLinearColor(.04f,1,.18f)));
    if (Variant==1)
        Add(B,TEXT("HealingBeam"),TEXT("/Engine/BasicShapes/Cylinder.Cylinder"),FVector(.045f,.045f,1),FVector::ZeroVector,Material,TEXT("Rogue.EffectMain"));
    else
        Add(B,TEXT("RangeRing"),TEXT("/Game/GameAssets/StarterContent/Shapes/Shape_Torus.Shape_Torus"),FVector(1,1,.08f),FVector::ZeroVector,Material,TEXT("Rogue.EffectMain"));
    if (Variant==2)
    {
        auto* C=Add(B,TEXT("BossHeadRune"),TEXT("/Engine/BasicShapes/Cube.Cube"),FVector(.3f),FVector::ZeroVector,Material,TEXT("Rogue.EffectMarker"));
        if (C) C->SetRelativeRotation(FRotator(45,45,45));
    }
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(B);return true;
}
bool URogueEnemyRoleSetup::BuildHealer(UBlueprint* B,bool bSingle)
{
    if (!B||!B->SimpleConstructionScript) return false;
    const TCHAR* Path=bSingle?TEXT("/Game/GameAssets/CharacterAssetse/Silver_Maid/02_Silver_Maid_UEFixed.02_Silver_Maid_UEFixed")
        :TEXT("/Game/GameAssets/CharacterAssetse/Mint_Black/03_Mint_Black_Hat_UEFixed.03_Mint_Black_Hat_UEFixed");
    auto* Mesh=LoadObject<UStaticMesh>(nullptr,Path);if (!Mesh) return false;
    B->Modify();const FVector Position(0,0,-72-Mesh->GetBoundingBox().Min.Z*2);
    Add(B,TEXT("HealerBody"),Path,FVector(2),Position,nullptr,TEXT("Rogue.EnemyBody"));
    auto* Green=ColorMaterial(TEXT("M_EnemyHealingGreen"),FLinearColor(.04f,1,.18f));
    const float Top=-72+Mesh->GetBoundingBox().GetSize().Z*2+20;
    auto* Rune=Add(B,TEXT("HealerRune"),TEXT("/Engine/BasicShapes/Cube.Cube"),bSingle?FVector(.12f,.12f,.3f):FVector(.22f),FVector(0,0,Top),Green,TEXT("Rogue.EnemyBody"));
    if (Rune) Rune->SetRelativeRotation(FRotator(45,45,45));
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(B);return true;
}

bool URogueEnemyRoleSetup::ReplaceCircleWithDecal(UBlueprint* B,UMaterialInterface* Material)
{
    if (!B||!B->SimpleConstructionScript||!Material) return false;
    B->Modify();
    if (auto* Old=B->SimpleConstructionScript->FindSCSNode(TEXT("RangeRing")))
        B->SimpleConstructionScript->RemoveNodeAndPromoteChildren(Old);
    auto* Node=B->SimpleConstructionScript->FindSCSNode(TEXT("RangeProjection"));
    if (!Node)
    {
        Node=B->SimpleConstructionScript->CreateNode(UDecalComponent::StaticClass(),TEXT("RangeProjection"));
        B->SimpleConstructionScript->AddNode(Node);
    }
    auto* C=Cast<UDecalComponent>(Node->ComponentTemplate);if (!C) return false;
    C->SetDecalMaterial(Material);C->DecalSize=FVector(160,100,100);
    C->SetRelativeRotation(FRotator(-90,0,0));C->ComponentTags.AddUnique(TEXT("Rogue.EffectMain"));
    C->SetFadeScreenSize(0);C->SortOrder=10;
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(B);return true;
}

bool URogueEnemyRoleSetup::NormalizeBodySize(UBlueprint* B,FName BodyName,float Height)
{
    if (!B||!B->SimpleConstructionScript||!FMath::IsFinite(Height)||Height<50||Height>300) return false;
    auto* N=B->SimpleConstructionScript->FindSCSNode(BodyName);
    auto* C=N?Cast<UStaticMeshComponent>(N->ComponentTemplate):nullptr;
    if (!C||!C->GetStaticMesh()) return false;
    const FBox Local=C->GetStaticMesh()->GetBoundingBox();
    const FBox Before=Local.TransformBy(C->GetRelativeTransform());
    if (Before.GetSize().Z<1) return false;
    B->Modify();C->Modify();
    C->SetRelativeScale3D(C->GetRelativeScale3D()*(Height/Before.GetSize().Z));
    const FBox After=Local.TransformBy(C->GetRelativeTransform());
    C->AddRelativeLocation(FVector(0,0,Before.Min.Z-After.Min.Z));
    if (auto* Rune=B->SimpleConstructionScript->FindSCSNode(TEXT("HealerRune")))
        if (auto* R=Cast<USceneComponent>(Rune->ComponentTemplate))
            R->AddRelativeLocation(FVector(0,0,Height-Before.GetSize().Z));
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(B);return true;
}
