#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "RogueUIThemeBuilder.generated.h"
class UWidgetBlueprint;
class UTexture2D;
class UUserWidget;
class APlayerController;
/** 仅编辑器执行一次；生成后的控件与画刷均保存于 WBP，可继续在设计器修改。 */
UCLASS()
class URogueUIThemeBuilder : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    /** 编辑器游戏预览用静态示例数据，不购买卡牌、不改存档。1 商店、2 选卡；3–12 为设置、暂停、结算、复活、多人、搜房、大厅、教程、昼夜和战斗 HUD，13 为商店与倒计时叠加。 */
    UFUNCTION(BlueprintCallable, Category="Rogue|Editor|Preview") static UUserWidget* CreatePagePreview(APlayerController* Owner, int32 Page);
    UFUNCTION(BlueprintCallable, Category="Rogue|Editor")
    static bool ApplyTouhouTheme(UWidgetBlueprint* Blueprint, UTexture2D* Background, UTexture2D* Paper, UTexture2D* Indigo, UTexture2D* Button);
    UFUNCTION(BlueprintCallable, Category="Rogue|Editor") static bool BuildBossHealthBar(UWidgetBlueprint* Blueprint);
    UFUNCTION(BlueprintCallable, Category="Rogue|Editor") static bool BuildAnimatedVitals(UWidgetBlueprint* Blueprint);
    UFUNCTION(BlueprintCallable, Category="Rogue|Editor") static bool AddDashCooldownToVitals(UWidgetBlueprint* Blueprint);
    UFUNCTION(BlueprintCallable, Category="Rogue|Editor") static bool BuildTouhouVitals(UWidgetBlueprint* Blueprint);
    UFUNCTION(BlueprintCallable, Category="Rogue|Editor") static bool BuildReadableTreeNode(UWidgetBlueprint* Blueprint);
    UFUNCTION(BlueprintCallable, Category="Rogue|Editor") static bool BuildReadableCardPage(UWidgetBlueprint* Blueprint);
    UFUNCTION(BlueprintCallable, Category="Rogue|Editor") static bool BuildAdditionalLayouts(UWidgetBlueprint* Blueprint);
    UFUNCTION(BlueprintCallable, Category="Rogue|Editor") static bool RoundWidgetCorners(UWidgetBlueprint* Blueprint);
    UFUNCTION(BlueprintCallable, Category="Rogue|Editor|Preview") static FString InspectMenuLayers(APlayerController* Owner);
    UFUNCTION(BlueprintCallable, Category="Rogue|Editor") static FString DescribePresentation(UWidgetBlueprint* Blueprint);
};
