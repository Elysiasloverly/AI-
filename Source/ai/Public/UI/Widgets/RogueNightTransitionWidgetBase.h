#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RogueNightTransitionWidgetBase.generated.h"
class UTextBlock;

/** 布局/静态文案在 WBP 设计器编辑；仅 CountdownText 由服务器截止时刻更新。 */
UCLASS(Abstract, Blueprintable)
class AI_API URogueNightTransitionWidgetBase : public UUserWidget
{
    GENERATED_BODY()
public:
    void RefreshCountdown();
    UFUNCTION(BlueprintPure, Category="昼夜提示") int32 GetSecondsRemaining() const { return SecondsRemaining; }
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="昼夜提示", meta=(DisplayName="倒计时格式", ToolTip="{Seconds} 为剩余秒数。"))
    FText CountdownFormat = NSLOCTEXT("RogueStage", "Countdown", "距离黑夜降临：{Seconds} 秒");
protected:
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> CountdownText;
    UFUNCTION(BlueprintImplementableEvent, Category="昼夜提示") void OnCountdownUpdated(int32 Seconds, bool bVisible);
private:
    int32 SecondsRemaining = -1;
    bool bCountdownVisible = false;
};
