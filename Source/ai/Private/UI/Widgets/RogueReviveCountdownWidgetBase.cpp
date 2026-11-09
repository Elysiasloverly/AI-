#include "UI/Widgets/RogueReviveCountdownWidgetBase.h"
#include "Components/Image.h"
#include "Materials/MaterialInstanceDynamic.h"

void URogueReviveCountdownWidgetBase::SetFrostSealProgress(float Progress)
{
	Progress = FMath::Clamp(Progress, 0.f, 1.f);
	if (!RevivalSeal || FMath::IsNearlyEqual(LastSealProgress, Progress, .004f)) return;
	if (auto* Material = RevivalSeal->GetDynamicMaterial())
	{
		Material->SetScalarParameterValue(TEXT("Charge"), Progress);
		LastSealProgress = Progress;
	}
}

void URogueReviveCountdownWidgetBase::UpdateReviveCountdown(int32 SecondsRemaining, bool bVisible)
{
	OnReviveCountdownUpdated(SecondsRemaining, bVisible);
	if (!bVisible)
	{
		// 停止现有动画；回调返回后强制应用本次可见性。后续异步修改仍由 HUD 漂移检查纠正。
		StopAllAnimations();
		SetFrostSealProgress(0.f);
	}
	SetVisibility(bVisible ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
}
