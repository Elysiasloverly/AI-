#include "RogueSkyfallSetup.h"
#include "Combat/Projectiles/RogueEnemySkyfallProjectile.h"
#include "Components/DecalComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Enemies/Data/RogueEnemyDataTable.h"
#include "Engine/Blueprint.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "Engine/StaticMesh.h"
#include "Kismet2/BlueprintEditorUtils.h"

bool URogueSkyfallSetup::BuildVisual(UBlueprint* B,UMaterialInterface* Core,UMaterialInterface* Warning)
{
    if (!B || !B->SimpleConstructionScript || !Core || !Warning) return false;
    B->Modify();
    if (!B->SimpleConstructionScript->FindSCSNode(TEXT("SkyfallCore")))
    {
        auto* N=B->SimpleConstructionScript->CreateNode(UStaticMeshComponent::StaticClass(),TEXT("SkyfallCore"));
        auto* M=Cast<UStaticMeshComponent>(N->ComponentTemplate);
        M->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere.Sphere")));
        M->SetMaterial(0,Core); M->SetRelativeScale3D(FVector(1.3f)); M->SetCastShadow(false);
        M->SetCollisionEnabled(ECollisionEnabled::NoCollision); M->SetGenerateOverlapEvents(false);
        B->SimpleConstructionScript->AddNode(N);
    }
    if (!B->SimpleConstructionScript->FindSCSNode(TEXT("GroundWarning")))
    {
        auto* N=B->SimpleConstructionScript->CreateNode(UDecalComponent::StaticClass(),TEXT("GroundWarning"));
        auto* D=Cast<UDecalComponent>(N->ComponentTemplate); D->SetDecalMaterial(Warning);
        D->DecalSize=FVector(200,180,180); D->SetRelativeRotation(FRotator(-90,0,0));
        D->ComponentTags.Add(TEXT("Rogue.SkyfallWarning")); D->SetFadeScreenSize(0); D->SortOrder=20;
        B->SimpleConstructionScript->AddNode(N);
    }
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(B); return true;
}

bool URogueSkyfallSetup::Configure(UDataTable* T,UBlueprint* B)
{
    if (!T || T->GetRowStruct()!=FRogueEnemyArchetypeRow::StaticStruct() || !B || !B->GeneratedClass) return false;
    for (auto& Pair:T->GetRowMap())
    {
        auto* R=reinterpret_cast<FRogueEnemyArchetypeRow*>(Pair.Value);
        if (R->EnemyType!=ERogueEnemyType::Marksman) continue;
        T->Modify(); auto& P=R->SpecialParams.Marksman;
        P.ShotPattern=ERogueEnemyShotPattern::Skyfall; P.Skyfall.ProjectileClass=B->GeneratedClass;
        P.InitialCooldownMin=1.2f; P.InitialCooldownMax=2.2f; P.NextShotCooldown=5.2f;
        P.bSpawnMuzzleEffect=false;
        // Boss仍使用同一落弹逻辑；只缩短攻击周期，不压缩玩家看到的红圈/下落时间。
        if (R->BossOverride.bEnableBossOverride) R->BossOverride.BossNextShotCooldown=4.2f;
        T->MarkPackageDirty(); return true;
    }
    return false;
}
