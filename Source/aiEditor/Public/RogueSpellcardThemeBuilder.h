#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "RogueSpellcardThemeBuilder.generated.h"

class UWidgetBlueprint;
class UTexture2D;

/** 一次性编辑器样式工具。装饰保存在 WBP，游戏运行时不会重新套用。 */
UCLASS()
class URogueSpellcardThemeBuilder : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    /** 保留现有控件、排版与事件；应用月夜、朱红和少量符卡边饰。 */
    UFUNCTION(BlueprintCallable, Category="Rogue|Editor|Theme")
    static bool ApplyRestrainedSpellcardTheme(UWidgetBlueprint* Blueprint, UTexture2D* Background, UTexture2D* Seal);
};
