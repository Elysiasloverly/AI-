#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "RogueSkyfallSetup.generated.h"
class UBlueprint;
class UDataTable;
class UMaterialInterface;

UCLASS()
class URogueSkyfallSetup : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    /** 只建立新落弹BP的组件；材质/模型之后直接在BP里修改。 */
    UFUNCTION(BlueprintCallable,Category="Rogue|Editor|Enemies")
    static bool BuildVisual(UBlueprint* Blueprint,UMaterialInterface* CoreMaterial,UMaterialInterface* WarningMaterial);
    /** 只修改鸦天狗行的攻击方式/时序，新参数块保留用户后续改值。 */
    UFUNCTION(BlueprintCallable,Category="Rogue|Editor|Enemies")
    static bool Configure(UDataTable* Table,UBlueprint* Blueprint);
};
