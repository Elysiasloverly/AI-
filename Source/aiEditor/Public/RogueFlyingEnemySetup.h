#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "RogueFlyingEnemySetup.generated.h"
class UBlueprint;
class UDataTable;
class URogueGameBalanceAsset;
UCLASS()
class URogueFlyingEnemySetup : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    /** 只添加新类型，不改已有敌人行和波次曲线；由安装脚本备份并保存。 */
    UFUNCTION(BlueprintCallable, Category="Rogue|Editor|Enemies")
    static bool Configure(UDataTable* Table, URogueGameBalanceAsset* Balance);
    /** 只为新创建的蓝图建立可编辑的SCS外观。 */
    UFUNCTION(BlueprintCallable, Category="Rogue|Editor|Enemies")
    static bool BuildVisual(UBlueprint* Blueprint, int32 Variant, bool bProjectile);
};
