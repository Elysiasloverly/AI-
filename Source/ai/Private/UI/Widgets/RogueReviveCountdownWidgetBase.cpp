#include "UI/Widgets/RogueReviveCountdownWidgetBase.h"

void URogueReviveCountdownWidgetBase::UpdateReviveCountdown(int32 SecondsRemaining, bool bVisible)
{
	OnReviveCountdownUpdated(SecondsRemaining, bVisible);
	if (!bVisible)
	{
		// 停止现有动画；回调返回后强制应用本次可见性。后续异步修改仍由 HUD 漂移检查纠正。
		StopAllAnimations();
	}
	SetVisibility(bVisible ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
}
