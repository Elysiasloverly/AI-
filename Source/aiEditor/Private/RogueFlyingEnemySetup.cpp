#include "RogueFlyingEnemySetup.h"
#include "Enemies/Data/RogueEnemyAssetPaths.h"
#include "Enemies/Data/RogueEnemyDataTable.h"
#include "Framework/RogueGameBalanceAsset.h"
#include "Enemies/RogueEnemy.h"
#include "Combat/Projectiles/RogueEnemyProjectile.h"
#include "Engine/Blueprint.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "AssetRegistry/AssetRegistryModule.h"

bool URogueFlyingEnemySetup::Configure(UDataTable* Table, URogueGameBalanceAsset* Balance)
{
    if (!Table || Table->GetRowStruct()!=FRogueEnemyArchetypeRow::StaticStruct() || !Balance) { return false; }
    const ERogueEnemyType Types[]={ERogueEnemyType::SkyMoth,ERogueEnemyType::WindReaver,ERogueEnemyType::StarLantern};
    const TCHAR* Names[]={TEXT("SkyMoth"),TEXT("WindReaver"),TEXT("StarLantern")};
    const int32 Waves[]={8,14,22};
    const int32 Alive[]={4,3,2}, Budgets[]={6,4,3};
    const float HP[]={0.85f,1.1f,1.2f}, Speed[]={0.9f,1.05f,0.75f}, Damage[]={0.85f,1.f,0.9f};
    Table->Modify(); Balance->Modify();
    for (int32 I=0;I<3;++I)
    {
        if (!Table->FindRow<FRogueEnemyArchetypeRow>(Names[I],TEXT("FlyingSetup"),false))
        {
            FRogueEnemyArchetypeRow Row; Row.EnemyType=Types[I];
            Row.Visual=RogueEnemyArchetypes::BuildEnemyArchetype(Types[I],false).Visual;
            Row.BossVisual=RogueEnemyArchetypes::BuildEnemyArchetype(Types[I],true).Visual;
            Row.Flying.AttackCooldown=I==0?3.6f:(I==1?3.2f:5.f);
            Row.Flying.ShotCount=I==1?3:9;
            Row.Flying.ProjectileSpeed=I==0?650.f:(I==1?780.f:580.f);
            Row.Flying.DamageMultiplier=I==0?0.45f:(I==1?0.75f:0.55f);
            Row.MaxAlive=Alive[I]; Row.MaxSpawnPerWave=Budgets[I];
            const FString Path=RogueEnemyAssetPaths::ProjectileClassPath(Names[I]);
            Row.ProjectileClass=LoadClass<ARogueEnemyProjectile>(nullptr,*Path);
            if (!Row.ProjectileClass) { return false; }
            Table->AddRow(Names[I],Row);
        }
        if (!Balance->EnemyWaveEntries.ContainsByPredicate([&](const FRogueEnemyWaveEntryRow& E){return E.EnemyType==Types[I];}))
            Balance->EnemyWaveEntries.Add({Waves[I],Types[I],1});
        if (!Balance->EnemyBalanceRows.ContainsByPredicate([&](const FRogueEnemyBalanceRow& E){return E.EnemyType==Types[I];}))
            Balance->EnemyBalanceRows.Add({Types[I],HP[I],Speed[I],Damage[I],I==0?1:2,1,1,1});
    }
    Table->MarkPackageDirty(); Balance->MarkPackageDirty(); return true;
}

bool URogueFlyingEnemySetup::BuildVisual(UBlueprint* Blueprint,int32 Variant,bool bProjectile)
{
    if (!Blueprint || !Blueprint->SimpleConstructionScript || Variant<0 || Variant>2) { return false; }
    Blueprint->Modify();
    const FLinearColor Colors[]={FLinearColor(0.5f,0.12f,1),FLinearColor(0.1f,0.8f,0.65f),FLinearColor(1,0.55f,0.08f)};
    const FString MatName=FString::Printf(TEXT("M_FlyingEnemy_%d"),Variant);
    const FString PackageName=TEXT("/Game/GameAssets/Enemies/Flying/")+MatName;
    UMaterial* Material=LoadObject<UMaterial>(nullptr,*(PackageName+TEXT(".")+MatName));
    if (!Material)
    {
        UPackage* Package=CreatePackage(*PackageName);
        Material=NewObject<UMaterial>(Package,*MatName,RF_Public|RF_Standalone);
        auto* Color=NewObject<UMaterialExpressionConstant3Vector>(Material); Color->Constant=Colors[Variant];
        Material->GetExpressionCollection().AddExpression(Color);
        Material->GetEditorOnlyData()->BaseColor.Expression=Color;
        Material->GetEditorOnlyData()->EmissiveColor.Expression=Color;
        Material->TwoSided=true; Material->PostEditChange(); Material->MarkPackageDirty();
        FAssetRegistryModule::AssetCreated(Material);
    }
    auto Add=[&](const TCHAR* Name,const TCHAR* MeshPath,FVector Scale,FVector Location,FRotator Rotation)
    {
        if (Blueprint->SimpleConstructionScript->FindSCSNode(Name)) { return; }
        USCS_Node* Node=Blueprint->SimpleConstructionScript->CreateNode(UStaticMeshComponent::StaticClass(),Name);
        auto* Mesh=Cast<UStaticMeshComponent>(Node->ComponentTemplate);
        Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,MeshPath)); Mesh->SetMaterial(0,Material);
        Mesh->SetRelativeScale3D(Scale); Mesh->SetRelativeLocation(Location); Mesh->SetRelativeRotation(Rotation);
        Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); Mesh->SetGenerateOverlapEvents(false);
        Mesh->SetMobility(EComponentMobility::Movable); Mesh->SetCastShadow(!bProjectile);
        Blueprint->SimpleConstructionScript->AddNode(Node);
    };
    const TCHAR* Sphere=TEXT("/Engine/BasicShapes/Sphere.Sphere");
    const TCHAR* Cube=TEXT("/Engine/BasicShapes/Cube.Cube");
    if (bProjectile)
    {
        Add(TEXT("BulletCore"),Sphere,FVector(0.13f),FVector::ZeroVector,FRotator::ZeroRotator);
        if (Variant==1) { Add(TEXT("WindBlade"),Cube,FVector(0.08f,0.4f,0.06f),FVector::ZeroVector,FRotator(0,25,0)); }
    }
    else
    {
        Add(TEXT("FlyingBody"),Sphere,Variant==2?FVector(0.38f,0.38f,0.65f):FVector(0.35f),FVector::ZeroVector,FRotator::ZeroRotator);
        if (Variant<2)
        {
            for (float Side:{-1.f,1.f})
                Add(Side<0?TEXT("WingLeft"):TEXT("WingRight"),Sphere,FVector(0.48f,0.65f,0.09f),FVector(-5,Side*36,5),FRotator(0,Side*25,Side*12));
        }
        else
        {
            Add(TEXT("LanternCap"),Cube,FVector(0.6f,0.6f,0.08f),FVector(0,0,30),FRotator(0,45,0));
            Add(TEXT("LanternTassel"),Cube,FVector(0.08f,0.08f,0.4f),FVector(0,0,-45),FRotator::ZeroRotator);
        }
    }
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint); return true;
}
