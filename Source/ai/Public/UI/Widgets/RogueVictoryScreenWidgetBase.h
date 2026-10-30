#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Framework/RogueRunResult.h"
#include "RogueVictoryScreenWidgetBase.generated.h"
class ARogueHUD;
class UButton;
class UTextBlock;

/** 独立胜利 WBP；只提供返回主菜单操作。布局、标题、样式可在设计器编辑。 */
UCLASS(Abstract, Blueprintable)
class AI_API URogueVictoryScreenWidgetBase : public UUserWidget
{
    GENERATED_BODY()
public:
    void SetOwningRogueHUD(ARogueHUD* HUD);
    void UpdateVictoryView(const FRogueRunResult& Result);
    UFUNCTION(BlueprintCallable, Category="Rogue|Victory") void RequestReturnToMainMenu();
protected:
    virtual void NativeConstruct() override;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UButton> MainMenuButton;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> WaveText;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> TimeText;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> KillsText;
    UFUNCTION(BlueprintImplementableEvent, Category="Rogue|Victory") void OnVictoryViewUpdated(const FRogueRunResult& Result);
private:
    TWeakObjectPtr<ARogueHUD> OwningHUD;
    bool bReturning=false;
};
