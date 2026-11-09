#include "RogueEnemyFolderSetup.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Combat/Projectiles/RogueEnemyProjectile.h"
#include "Enemies/RogueEnemy.h"
#include "Enemies/Data/RogueEnemyDataTable.h"
#include "Engine/Blueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "UObject/ObjectRedirector.h"

bool URogueEnemyFolderSetup::AssignProjectile(UDataTable* Table,UBlueprint* Enemy,UBlueprint* Projectile)
{
    if (!Table||Table->GetRowStruct()!=FRogueEnemyArchetypeRow::StaticStruct()||!Enemy||!Projectile||!Projectile->GeneratedClass) return false;
    auto* CDO=Enemy->GeneratedClass?Cast<ARogueEnemy>(Enemy->GeneratedClass->GetDefaultObject()):nullptr;
    if (!CDO||!Projectile->GeneratedClass->IsChildOf(ARogueEnemyProjectile::StaticClass())) return false;
    for (auto& Pair:Table->GetRowMap())
    {
        auto* Row=reinterpret_cast<FRogueEnemyArchetypeRow*>(Pair.Value);
        if (Row->EnemyType!=CDO->GetEnemyType()) continue;
        Table->Modify();Enemy->Modify();CDO->Modify();
        Row->ProjectileClass=Projectile->GeneratedClass;CDO->EnemyProjectileClass=Projectile->GeneratedClass;
        FBlueprintEditorUtils::MarkBlueprintAsModified(Enemy);Table->MarkPackageDirty();return true;
    }
    return false;
}
int32 URogueEnemyFolderSetup::FixMovedRedirectors(const TArray<FString>& OldPackages)
{
    TArray<UObjectRedirector*> Items;
    for (const auto& Package:OldPackages)
    {
        if (!Package.StartsWith(TEXT("/Game/BP/Enemy/"))) continue;
        const FString Object=Package+TEXT(".")+FPackageName::GetShortName(Package);
        if (auto* R=FindObject<UObjectRedirector>(nullptr,*Object)) Items.Add(R);
    }
    if (!Items.IsEmpty()) FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get().FixupReferencers(Items,false,ERedirectFixupMode::DeleteFixedUpRedirectors);
    return Items.Num();
}
bool URogueEnemyFolderSetup::CompileBlueprint(UBlueprint* B)
{
    if (!B) return false;
    FKismetEditorUtilities::CompileBlueprint(B);return B->Status!=BS_Error;
}
