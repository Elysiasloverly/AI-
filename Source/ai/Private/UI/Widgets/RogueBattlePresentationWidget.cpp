#include "UI/Widgets/RogueBattlePresentationWidget.h"
#include "Framework/RogueGameState.h"
#include "UI/RoguePresentationTheme.h"
#include "UI/RogueHUD.h"
#include "GameFramework/PlayerController.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Engine/World.h"
#include "Progression/Meta/RogueProgressionSubsystem.h"
#include "Internationalization/Text.h"

void URogueBattlePresentationWidget::NativeConstruct()
{
    Super::NativeConstruct();SetVisibility(ESlateVisibility::HitTestInvisible);
    if(CuePanel)CuePanel->SetVisibility(ESlateVisibility::Collapsed);
    if (DepartureArt) DepartureArt->SetVisibility(ESlateVisibility::Collapsed);
    SetChapterArt(nullptr, 0.f, 0.f);
    if (DuskArt) DuskArt->SetVisibility(ESlateVisibility::Collapsed);
    if (DawnArt) DawnArt->SetVisibility(ESlateVisibility::Collapsed);
    if (RewardPanel) RewardPanel->SetVisibility(ESlateVisibility::Collapsed);
    Progression = URogueProgressionSubsystem::GetForPlayer(GetOwningPlayer());
    if (auto* Local = Progression.Get())
        Local->OnBossRewardCommitted.AddUObject(this, &ThisClass::ShowReward);
    if(EntryVeil){EntryVeil->SetVisibility(ESlateVisibility::HitTestInvisible);EntryVeil->SetRenderOpacity(.65f);}
}
void URogueBattlePresentationWidget::NativeDestruct()
{
    if (auto* Local = Progression.Get()) Local->OnBossRewardCommitted.RemoveAll(this);
    Progression.Reset();
    Super::NativeDestruct();
}

void URogueBattlePresentationWidget::ShowReward(int32 Amount)
{
    if (!RewardPanel || !RewardAmount || Amount <= 0) return;
    ShownReward = RewardAge < 2.4f ? ShownReward + Amount : Amount;
    RewardAge = 0.f;
    RewardAmount->SetText(FText::Format(NSLOCTEXT("RogueBattle", "IceBrickReceipt", "冰砖 +{0}"), FText::AsNumber(ShownReward)));
    RewardPanel->SetRenderOpacity(0.f);
    RewardPanel->SetVisibility(ESlateVisibility::Collapsed);
}

void URogueBattlePresentationWidget::SetChapterArt(UImage* Art, float Opacity, float Slide)
{
    if (auto* Previous = ActiveChapterArt.Get(); Previous && Previous != Art)
        Previous->SetVisibility(ESlateVisibility::Collapsed);
    ActiveChapterArt = Art;
    if (!Art) return;
    Art->SetVisibility(ESlateVisibility::HitTestInvisible);
    Art->SetRenderOpacity(Opacity);
    Art->SetRenderTranslation(FVector2D(Slide, 0));
}

void URogueBattlePresentationWidget::ShowCue(FText Title,FText Subtitle,bool bBoss,float Life)
{
    Age=0;Duration=FMath::Clamp(Life,.5f,4.f);
    CueWave = 0;
    bDepartureCue = false;
    if (DepartureArt) DepartureArt->SetVisibility(ESlateVisibility::Collapsed);
    if (CuePanel)
    {
        CuePanel->SetRenderOpacity(0.f);
        CuePanel->SetVisibility(ESlateVisibility::HitTestInvisible);
    }
    if(CueTitle)CueTitle->SetText(Title);if(CueSubtitle)CueSubtitle->SetText(Subtitle);
    if(CueSeal)CueSeal->SetColorAndOpacity(bBoss?FLinearColor(.85f,.30f,.21f,1):FLinearColor(.63f,.72f,.82f,1));
    OnCueShown(bBoss);
}
void URogueBattlePresentationWidget::ShowWaveCue(int32 Wave, const FText& Detail, const FText& Subtitle, bool bBoss, float Life)
{
    const FText WaveLabel = FText::Format(NSLOCTEXT("RogueBattle", "WaveCue", "第 {0} 波"), FText::AsNumber(Wave));
    const FText Title = Detail.IsEmpty() ? WaveLabel
        : FText::Format(NSLOCTEXT("RogueBattle", "WaveCueWithDetail", "{0} · {1}"), WaveLabel, Detail);
    ShowCue(Title, Subtitle, bBoss, Life);
    LastAnnouncedWave = Wave;
    CueWave = Wave;
}

void URogueBattlePresentationWidget::RefreshPresentation()
{
    UWorld* World = GetWorld();
    if (!bAutomatic || !World) return;
    const auto* State = World->GetGameState<ARogueGameState>();
    if (!State) return;
    if (PresentedWorld.Get() != World)
    {
        PresentedWorld = World;
        bEntered = bBossVisible = false;
        LastBoss.Invalidate();
        LastAnnouncedWave = CueWave = 0;
        EntryAge = 0.f;
    }
    if (State->IsGameOver())
    {
        Age = 100;
        ChapterArrivalAge = 10.f;
        SetChapterArt(nullptr, 0.f, 0.f);
        if (CuePanel) CuePanel->SetVisibility(ESlateVisibility::Collapsed);
        return;
    }
    const auto& Stage = State->GetBattleStage();
    const bool bInCombat = Stage.Phase == ERogueBattleStage::Day || Stage.Phase == ERogueBattleStage::Night;
    const int32 Wave = State->GetCurrentWave();
    const auto* PC = GetOwningPlayer();
    const auto* HUD = PC ? Cast<ARogueHUD>(PC->GetHUD()) : nullptr;
    const bool bNewWave = bInCombat && Wave > 0 && Wave != LastAnnouncedWave &&
        (!HUD || !HUD->IsCoveringBattleFeedback());
    const bool bShowEntry = bNewWave && !bEntered;
    if (bShowEntry)
    {
        bEntered = true;
        const FString Map = World->GetMapName();
        FString Name = TEXT("迷途之森");
        if(Map.Contains(TEXT("Sakura")))Name=TEXT("樱庭神社");else if(Map.Contains(TEXT("Misty")))Name=TEXT("雾湖冰洲");
        else if(Map.Contains(TEXT("Bamboo")))Name=TEXT("竹林月径");else if(Map.Contains(TEXT("Scarlet")))Name=TEXT("红魔钟庭");
        const bool bNight = Map.Contains(TEXT("Night"));
        if (!bNight && Wave == 1 && DepartureArt)
        {
            ShowWaveCue(Wave, FText::FromString(TEXT("Baka 出发！")), FText::FromString(TEXT("幻想乡挑战 · 第一站 · ") + Name), false, 3.2f);
            bDepartureCue = true;
            DepartureArt->SetRenderOpacity(0.f);
            DepartureArt->SetVisibility(ESlateVisibility::HitTestInvisible);
        }
        else
        {
            ShowWaveCue(Wave, FText::FromString(Name), FText::FromString(bNight ? TEXT("月影篇 · 夜色中的退治") : TEXT("破晓篇 · 灵风再起")), false, CueDuration);
            if (Wave > 1)
            {
                ChapterArrivalAge = 0.f;
                SetChapterArt(bNight ? DuskArt.Get() : DawnArt.Get(), 1.f, 0.f);
            }
        }
    }
    const auto& Boss=State->GetBossHealthView();
    if(Boss.bVisible&&Boss.SpawnId.IsValid()&&Boss.SpawnId!=LastBoss)
    {
        LastBoss=Boss.SpawnId;
        const FText* Name=PresentationTheme?PresentationTheme->BossNames.Find(Boss.EnemyType):nullptr;
        const FText* Spell=PresentationTheme?PresentationTheme->BossSpellTitles.Find(Boss.EnemyType):nullptr;
        const FText Title = Name ? FText::FromString(Name->ToString() + TEXT(" · 异变显现")) : FText::FromString(TEXT("异变显现"));
        const FText Subtitle = Spell ? *Spell : FText::FromString(TEXT("妖力凝聚 · 留意符阵与弹幕"));
        // Boss 状态与波数独立复制。波次横幅还在播放时，将波数一并带入 Boss 提示。
        if (bNewWave || (CueWave == Wave && Wave > 0 && Age < Duration))
            ShowWaveCue(Wave, Title, Subtitle, true, CueDuration);
        else
            ShowCue(Title, Subtitle, true, CueDuration);
    }
    else if (bNewWave && !bShowEntry)
    {
        ShowWaveCue(Wave, FText::GetEmpty(), NSLOCTEXT("RogueBattle", "WaveCueSubtitle", "敌群来袭"), false, 1.8f);
    }
    else if (bBossVisible && !Boss.bVisible && bInCombat && !(CueWave == Wave && Age < Duration))
    {
        ShowCue(FText::FromString(TEXT("符阵退治")), FText::FromString(TEXT("灵光散尽，前路再启")), false, 1.8f);
    }
    bBossVisible = Boss.bVisible;
    if(EntryVeil&&(Stage.Phase==ERogueBattleStage::Countdown||Stage.Phase==ERogueBattleStage::Travelling))
    {
        const float Remaining=Stage.CountdownDeadline-State->GetServerWorldTimeSeconds();
        const float Fade=FMath::Clamp((.7f-Remaining)/.7f,0.f,1.f);
        EntryVeil->SetVisibility(Fade>0?ESlateVisibility::HitTestInvisible:ESlateVisibility::Collapsed);
        EntryVeil->SetRenderOpacity(Fade);
        const float Reveal = Stage.Phase == ERogueBattleStage::Travelling
            ? 1.f : FMath::SmoothStep(0.f, 1.2f, 1.2f - Remaining);
        SetChapterArt(Reveal > 0.f ? (Stage.bNextStageIsNight ? DuskArt.Get() : DawnArt.Get()) : nullptr,
            Reveal, 26.f * (1.f - Reveal));
    }
    else if (ChapterArrivalAge >= 1.25f && ActiveChapterArt.IsValid())
    {
        SetChapterArt(nullptr, 0.f, 0.f);
    }
}
void URogueBattlePresentationWidget::NativeTick(const FGeometry& G,float DT)
{
    Super::NativeTick(G,DT);Age+=FMath::Max(0.f,DT);EntryAge+=FMath::Max(0.f,DT);
    if(CuePanel&&Age<Duration)
    {
        const float In=FMath::SmoothStep(0.f,.24f,Age),Out=1-FMath::SmoothStep(Duration-.45f,Duration,Age);
        CuePanel->SetRenderOpacity(In*Out);CuePanel->SetRenderTranslation(FVector2D(0,12*(1-In)-6*(1-Out)));
        if(CueSeal)CueSeal->SetRenderTransformAngle(-4+8*FMath::Clamp(Age/Duration,0.f,1.f));
    }
    else if(CuePanel&&CuePanel->GetVisibility()!=ESlateVisibility::Collapsed)CuePanel->SetVisibility(ESlateVisibility::Collapsed);
    if (bDepartureCue && DepartureArt)
    {
        if (Age >= Duration)
        {
            DepartureArt->SetVisibility(ESlateVisibility::Collapsed);
            bDepartureCue = false;
        }
        else
        {
            const float In = FMath::SmoothStep(0.f, .35f, Age);
            const float Out = 1.f - FMath::SmoothStep(Duration - .6f, Duration, Age);
            DepartureArt->SetRenderOpacity(In * Out);
            DepartureArt->SetRenderTranslation(FVector2D(18.f * (1.f - In), 0.f));
        }
    }
    const auto* PC = GetOwningPlayer();
    const auto* HUD = PC ? Cast<ARogueHUD>(PC->GetHUD()) : nullptr;
    if (RewardPanel && RewardAge < 2.4f && HUD && HUD->IsCoveringBattleFeedback())
    {
        RewardPanel->SetVisibility(ESlateVisibility::Collapsed);
    }
    else if (RewardPanel && RewardAge < 2.4f)
    {
        RewardPanel->SetVisibility(ESlateVisibility::HitTestInvisible);
        RewardAge += FMath::Max(0.f, DT);
        const float In = FMath::SmoothStep(0.f, .22f, RewardAge);
        const float Out = FMath::SmoothStep(1.95f, 2.4f, RewardAge);
        RewardPanel->SetRenderOpacity(In * (1.f - Out));
        RewardPanel->SetRenderTranslation(FVector2D(34.f * (1.f - In), -10.f * Out));
        if (RewardAge >= 2.4f) RewardPanel->SetVisibility(ESlateVisibility::Collapsed);
    }
    if (ChapterArrivalAge < 1.25f)
    {
        ChapterArrivalAge += FMath::Max(0.f, DT);
        const float Out = FMath::SmoothStep(.22f, 1.25f, ChapterArrivalAge);
        SetChapterArt(Out < 1.f ? ActiveChapterArt.Get() : nullptr, 1.f - Out, -40.f * Out);
    }
    if(EntryVeil&&EntryAge<.8f)EntryVeil->SetRenderOpacity(.65f*(1-FMath::SmoothStep(0.f,.8f,EntryAge)));
    else if(EntryVeil)
    {
        const auto* State=GetWorld()?GetWorld()->GetGameState<ARogueGameState>():nullptr;
        const bool bLeaving=State&&(State->GetBattleStage().Phase==ERogueBattleStage::Countdown||State->GetBattleStage().Phase==ERogueBattleStage::Travelling);
        if(!bLeaving&&EntryVeil->GetVisibility()!=ESlateVisibility::Collapsed)EntryVeil->SetVisibility(ESlateVisibility::Collapsed);
    }
}
