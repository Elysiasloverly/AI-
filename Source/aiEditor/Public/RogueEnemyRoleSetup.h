#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "RogueEnemyRoleSetup.generated.h"
class UBlueprint;
class UMaterialInterface;
UCLASS()
class URogueEnemyRoleSetup : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    /** Variant: 0绿色范围，1绿色连线，2Boss标记，3飞行标记。只用于新建蓝图。 */
    UFUNCTION(BlueprintCallable,Category="Rogue|Editor|Enemies")
    static bool BuildEffect(UBlueprint* Blueprint,int32 Variant);
    UFUNCTION(BlueprintCallable,Category="Rogue|Editor|Enemies")
    static bool ReplaceCircleWithDecal(UBlueprint* Blueprint,UMaterialInterface* Material);
    /** 仅编辑指定SCS模型尺寸，保持原比例、朝向和模型底部；不改碰撞或其它配饰。 */
    UFUNCTION(BlueprintCallable,Category="Rogue|Editor|Enemies")
    static bool NormalizeBodySize(UBlueprint* Blueprint,FName BodyName,float Height);
    UFUNCTION(BlueprintCallable,Category="Rogue|Editor|Enemies")
    static bool BuildHealer(UBlueprint* Blueprint,bool bSingle);
};
