#include "UI/Widgets/RogueVictoryScreenWidgetBase.h"
#include "UI/RogueHUD.h"
#include "Audio/RogueMusicSubsystem.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
void URogueVictoryScreenWidgetBase::SetOwningRogueHUD(ARogueHUD* HUD) { OwningHUD=HUD; }
void URogueVictoryScreenWidgetBase::NativeConstruct()
{
    Super::NativeConstruct();
    URogueMusicSubsystem::BindWidgetButtonSounds(this);
    if (MainMenuButton) MainMenuButton->OnClicked.AddUniqueDynamic(this,&ThisClass::RequestReturnToMainMenu);
}
void URogueVictoryScreenWidgetBase::UpdateVictoryView(const FRogueRunResult& Result)
{
    const bool bVisible=Result.Outcome==ERogueRunOutcome::Victory;
    Reveal.SetVisible(this,bVisible);
    if (bVisible)
    {
        if (WaveText) WaveText->SetText(FText::AsNumber(Result.Wave));
        const int32 Seconds=FMath::Max(0,FMath::FloorToInt(Result.RunTimeSeconds));
        if (TimeText) TimeText->SetText(FText::FromString(FString::Printf(TEXT("%02d:%02d"),Seconds/60,Seconds%60)));
        if (KillsText) KillsText->SetText(FText::AsNumber(Result.EnemiesDefeated));
        OnVictoryViewUpdated(Result);
    }
    else StopAllAnimations();
    SetVisibility(bVisible?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
}
void URogueVictoryScreenWidgetBase::RequestReturnToMainMenu()
{
    if (bReturning) return;
    if (auto* HUD=OwningHUD.Get())
    {
        bReturning=true;
        if (MainMenuButton) MainMenuButton->SetIsEnabled(false);
        HUD->RequestReturnToMainMenuAfterDeath();
    }
}

void URogueVictoryScreenWidgetBase::NativeTick(const FGeometry& G,float DT)
{Super::NativeTick(G,DT);Reveal.Tick(this,DT);}
