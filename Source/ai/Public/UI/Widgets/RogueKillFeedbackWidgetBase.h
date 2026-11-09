#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RogueKillFeedbackWidgetBase.generated.h"
class UTextBlock;
class UBorder;
/** Lightweight local confirmation. This streak is cosmetic and grants no reward or combat bonus. */
UCLASS(Blueprintable)
class AI_API URogueKillFeedbackWidgetBase : public UUserWidget
{
    GENERATED_BODY()
public:
    void SetBottomInset(float Height);
    UFUNCTION(BlueprintCallable,Category="击杀反馈") void RegisterKills(int32 Count,bool bBoss);
    UFUNCTION(BlueprintPure,Category="击杀反馈") int32 GetStreakCount()const{return StreakCount;}
    UPROPERTY(EditDefaultsOnly,BlueprintReadWrite,Category="击杀反馈",meta=(ClampMin=".5",ClampMax="10")) float StreakWindow=2.5f;
    UPROPERTY(EditDefaultsOnly,BlueprintReadWrite,Category="击杀反馈",meta=(ClampMin=".1",ClampMax="3")) float HoldDuration=.8f;
    UPROPERTY(EditDefaultsOnly,BlueprintReadWrite,Category="击杀反馈",meta=(ClampMin=".1",ClampMax="2")) float FadeDuration=.35f;
    UPROPERTY(EditDefaultsOnly,BlueprintReadWrite,Category="击杀反馈") bool bBlueprintHandlesMotion=false;
protected:
    friend struct FRogueKillFeedbackTestAccess;
    virtual void NativeConstruct()override;
    virtual void NativeTick(const FGeometry& Geometry,float DeltaSeconds)override;
    UFUNCTION(BlueprintImplementableEvent,Category="击杀反馈") void OnKillFeedback(int32 Count,bool bBoss);
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> KillText;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> ChainText;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UBorder> SpiritSeal;
private:
    int32 StreakCount=0;
    float Age=100,LastKillTime=-100;
    float AttackStartOpacity=0;
    bool bBossNotice=false;
    bool bDockCaptured=false;
    FVector2D AuthoredDock;
};
