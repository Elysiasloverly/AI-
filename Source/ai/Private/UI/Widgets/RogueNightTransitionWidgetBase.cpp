#include "UI/Widgets/RogueNightTransitionWidgetBase.h"
#include "Components/TextBlock.h"
#include "Framework/RogueGameState.h"

void URogueNightTransitionWidgetBase::RefreshCountdown()
{
    const ARogueGameState* State = GetWorld() ? GetWorld()->GetGameState<ARogueGameState>() : nullptr;
    const bool bVisible = State && !State->IsGameOver()
        && (State->GetBattleStage().Phase == ERogueBattleStage::Countdown || State->GetBattleStage().Phase == ERogueBattleStage::Travelling);
    const int32 Seconds = bVisible ? FMath::Max(0, FMath::CeilToInt(State->GetBattleStage().CountdownDeadline - State->GetServerWorldTimeSeconds())) : 0;
    SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    const bool bNight=!State || State->GetBattleStage().bNextStageIsNight;
    if (Seconds != SecondsRemaining || bVisible != bCountdownVisible || bNight!=bLastNextStageIsNight)
    {
        SecondsRemaining = Seconds;
        bLastNextStageIsNight=bNight;
        if (TitleText) TitleText->SetText(FText::FromString(bNight?TEXT("暮色将至"):TEXT("曙光再临")));
        if (DescriptionText) DescriptionText->SetText(FText::FromString(bNight
            ? TEXT("稍作休整。黑夜中敌群更加密集，增援来得更快。") : TEXT("稍作休整。白昼将至，敌群数量与生成速度恢复基础配置。")));
        bCountdownVisible = bVisible;
        // 使用命名占位，方便在蓝图类默认值里修改中文文案。
        if (CountdownText) { CountdownText->SetText(FText::Format(bNight?CountdownFormat:DayCountdownFormat, FFormatNamedArguments{{TEXT("Seconds"), FText::AsNumber(Seconds)}})); }
        OnCountdownUpdated(Seconds, bVisible);
    }
}
