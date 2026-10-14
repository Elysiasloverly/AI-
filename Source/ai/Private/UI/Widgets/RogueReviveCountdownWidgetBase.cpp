#include "UI/Widgets/RogueReviveCountdownWidgetBase.h"

void URogueReviveCountdownWidgetBase::UpdateReviveCountdown(int32 SecondsRemaining, bool bVisible)
{
	OnReviveCountdownUpdated(SecondsRemaining, bVisible);
	if (!bVisible)
	{
		// 蓝图退场动画或回调不能在复活后再次把界面变为可见。
		StopAllAnimations();
	}
	SetVisibility(bVisible ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
}
