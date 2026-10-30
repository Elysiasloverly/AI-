#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "RogueEnemyFolderSetup.generated.h"
class UBlueprint;
class UDataTable;
UCLASS()
class URogueEnemyFolderSetup : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    /** Save an independent projectile reference in the enemy BP fallback and its own data row. */
    UFUNCTION(BlueprintCallable,Category="Rogue|Editor|Enemies")
    static bool AssignProjectile(UDataTable* Table,UBlueprint* Enemy,UBlueprint* Projectile);
    /** Fix only explicitly listed moved packages; callers must back up all referencers first. */
    UFUNCTION(BlueprintCallable,Category="Rogue|Editor|Enemies")
    static int32 FixMovedRedirectors(const TArray<FString>& OldPackages);
    UFUNCTION(BlueprintCallable,Category="Rogue|Editor|Enemies")
    static bool CompileBlueprint(UBlueprint* Blueprint);
};
