#pragma once

#include "CoreMinimal.h"
#include "Components/Widget.h"
#include "RogueDashRingWidget.generated.h"

class SRogueDashCooldownArc;

/** 玩家身旁的冲刺冷却圆弧。只呈现现有 GAS 冷却，不创建或修改体力数值。 */
UCLASS(meta = (DisplayName = "Rogue 冲刺圆弧"))
class AI_API URogueDashRingWidget : public UWidget
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "冲刺圆弧", meta = (ClampMin = "12", ClampMax = "100"))
    float Radius = 44.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "冲刺圆弧", meta = (ClampMin = "30", ClampMax = "360"))
    float ArcDegrees = 150.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "冲刺圆弧", meta = (ClampMin = "1", ClampMax = "12"))
    float Thickness = 4.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "冲刺圆弧")
    FLinearColor TrackColor = FLinearColor(.16f, .22f, .25f, .8f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "冲刺圆弧")
    FLinearColor CooldownColor = FLinearColor(.96f, .8f, .37f, 1.f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "冲刺圆弧")
    FLinearColor ReadyColor = FLinearColor(.73f, .94f, 1.f, 1.f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "冲刺圆弧", meta = (ClampMin = "0", ClampMax = "1"))
    float ReadyIdleOpacity = .55f;

    void SetCooldown(float Percent, bool bReady, bool bUsable);
    void AdvancePresentation(float DeltaSeconds);
    float GetCooldownPercent() const { return CooldownPercent; }

    virtual void SynchronizeProperties() override;
    virtual void ReleaseSlateResources(bool bReleaseChildren) override;

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;

private:
    TSharedPtr<SRogueDashCooldownArc> ArcWidget;
    float CooldownPercent = 0.f;
    float ReadyElapsed = 0.f;
    bool bCooldownReady = false;
    bool bCooldownUsable = false;
    void RefreshArc();
};
