#pragma once
#include "CoreMinimal.h"
#include "Extensions/UserWidgetExtension.h"
#include "Components/Widget.h"
#include "RogueMenuMotionExtension.generated.h"
class UButton;
/** Local menu presentation only; caches authored transforms and never changes layout, input or sound. */
UCLASS(BlueprintType)
class AI_API URogueMenuMotionExtension : public UUserWidgetExtension
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable,Category="界面|动效") static void Attach(UUserWidget* Widget);
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="界面|动效") bool bEnabled=true;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="界面|动效",meta=(ClampMin="1",ClampMax="1.05")) float HoverScale=1.018f;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="界面|动效",meta=(ClampMin="0",ClampMax="12")) float HoverLift=2;
    virtual void Construct()override;
    virtual void Destruct()override;
    virtual bool RequiresTick()const override{return true;}
    virtual void Tick(const FGeometry& Geometry,float Delta)override;
private:
    friend struct FRogueMenuMotionTestAccess;
    struct FButtonMotion
    {
        TWeakObjectPtr<UButton> Button;FWidgetTransform Authored;FVector2D Pivot;float Opacity=1,Scale=1,Y=0;bool bMoving=false;
    };
    TArray<FButtonMotion> Buttons;
    TWeakObjectPtr<UWidget> Content;
    FWidgetTransform ContentBase;
    float ContentOpacity=1,Age=0;bool bMain=false,bWasEnabled=true;
    void VisibilityChanged(ESlateVisibility Visibility);
    void Restore();
};
