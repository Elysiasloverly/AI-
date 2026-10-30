#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "RogueLogoSetup.generated.h"

class UWidgetBlueprint;
class UTexture2D;

/** 一次性编辑主菜单标题区域；结果保存在 WBP，运行时不依赖本工具。 */
UCLASS()
class URogueLogoSetup : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Rogue|Editor|Logo")
    static bool SetMainMenuLogo(UWidgetBlueprint* Blueprint, UTexture2D* Texture, float Width = 372.f, float Height = 186.f);
};
