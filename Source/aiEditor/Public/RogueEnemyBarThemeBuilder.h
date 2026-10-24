#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "RogueEnemyBarThemeBuilder.generated.h"

class UWidgetBlueprint;

/** 编辑器内生成一次并保存，之后仍可在 WBP 设计器修改外观。 */
UCLASS()
class URogueEnemyBarThemeBuilder : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category = "Rogue|Editor")
	static bool BuildRefinedEnemyBar(UWidgetBlueprint* Blueprint);
};
