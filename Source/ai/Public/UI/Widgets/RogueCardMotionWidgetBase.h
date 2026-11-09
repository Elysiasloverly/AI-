#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Shared/RogueTypes.h"
#include "RogueCardMotionWidgetBase.generated.h"
class UButton;
class URoguePresentationTheme;

/** Cosmetic card response. Never changes panel layout, price, selection or button delegates. */
UCLASS(Abstract,Blueprintable)
class AI_API URogueCardMotionWidgetBase : public UUserWidget
{
    GENERATED_BODY()
public:
    UPROPERTY(EditDefaultsOnly,BlueprintReadWrite,Category="Rogue|美术",meta=(DisplayName="图标主题")) TObjectPtr<URoguePresentationTheme> PresentationTheme;
    void ApplyCardIcon(ERogueUpgradeType Type);
    UPROPERTY(EditDefaultsOnly,BlueprintReadWrite,Category="Rogue|卡片动效",meta=(DisplayName="启用柔和交互")) bool bEnableCardMotion=true;
    UPROPERTY(EditDefaultsOnly,BlueprintReadWrite,Category="Rogue|卡片动效",meta=(DisplayName="悬停放大倍率",ClampMin="1",ClampMax="1.06")) float HoverScale=1.018f;
    UPROPERTY(EditDefaultsOnly,BlueprintReadWrite,Category="Rogue|卡片动效",meta=(DisplayName="按下缩小倍率",ClampMin=".95",ClampMax="1")) float PressScale=.987f;
    UPROPERTY(EditDefaultsOnly,BlueprintReadWrite,Category="Rogue|卡片动效",meta=(DisplayName="过渡速度",ClampMin="1",ClampMax="40")) float MotionSpeed=18;
protected:
    virtual UButton* GetMotionButton()const{return nullptr;}
    virtual void NativeConstruct()override;
    virtual void NativeDestruct()override;
    virtual void NativeTick(const FGeometry& Geometry,float DeltaSeconds)override;
    void ApplyMotion(float Delta,bool bHovered,bool bPressed,bool bEnabled);
    friend struct FRogueCardMotionTestAccess;
private:
    FWidgetTransform BaseTransform;
    FVector2D BasePivot=FVector2D::ZeroVector;
    float MotionScale=1;
    bool bCaptured=false;
};
