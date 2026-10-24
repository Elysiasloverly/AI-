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
    if (Seconds != SecondsRemaining || bVisible != bCountdownVisible)
    {
        SecondsRemaining = Seconds;
        bCountdownVisible = bVisible;
        // 使用命名占位，方便在蓝图类默认值里修改中文文案。
        if (CountdownText) { CountdownText->SetText(FText::Format(CountdownFormat, FFormatNamedArguments{{TEXT("Seconds"), FText::AsNumber(Seconds)}})); }
        OnCountdownUpdated(Seconds, bVisible);
    }
}
