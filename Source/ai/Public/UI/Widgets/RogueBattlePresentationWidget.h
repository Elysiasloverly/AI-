#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RogueBattlePresentationWidget.generated.h"
class UTextBlock;class UWidget;class UImage;
class URoguePresentationTheme;
class URogueProgressionSubsystem;

/** 本地战斗提示，从复制的波数、昼夜和 Boss 状态读取数据。 */
UCLASS(Abstract,Blueprintable)
class AI_API URogueBattlePresentationWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    void RefreshPresentation();
    UFUNCTION(BlueprintCallable,Category="演出") void ShowCue(FText Title,FText Subtitle,bool bBoss=false,float Duration=2.4f);
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="演出",meta=(DisplayName="启用自动演出")) bool bAutomatic=true;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="演出",meta=(DisplayName="演出时长",ClampMin=".5",ClampMax="4")) float CueDuration=2.4f;
    UPROPERTY(EditDefaultsOnly,BlueprintReadWrite,Category="演出") TObjectPtr<URoguePresentationTheme> PresentationTheme;
protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual void NativeTick(const FGeometry& Geometry,float DeltaSeconds) override;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UWidget> CuePanel;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> CueTitle;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> CueSubtitle;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UImage> CueSeal;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UImage> EntryVeil;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UImage> DepartureArt;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UImage> DuskArt;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UImage> DawnArt;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UWidget> RewardPanel;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> RewardAmount;
    UFUNCTION(BlueprintImplementableEvent,Category="演出") void OnCueShown(bool bBoss);
private:
    friend struct FRogueArtCompletionTestAccess;
    float Age=100,Duration=2.4f,EntryAge=0;
    bool bEntered=false,bBossVisible=false;
    bool bDepartureCue = false;
    FGuid LastBoss;
    int32 LastAnnouncedWave = 0;
    int32 CueWave = 0;
    TWeakObjectPtr<URogueProgressionSubsystem> Progression;
    TWeakObjectPtr<UImage> ActiveChapterArt;
    TWeakObjectPtr<UWorld> PresentedWorld;
    float ChapterArrivalAge = 10.f;
    float RewardAge = 10.f;
    int32 ShownReward = 0;
    void SetChapterArt(UImage* Art, float Opacity, float Slide);
    void ShowWaveCue(int32 Wave, const FText& Detail, const FText& Subtitle, bool bBoss, float Life);
    void ShowReward(int32 Amount);
};
