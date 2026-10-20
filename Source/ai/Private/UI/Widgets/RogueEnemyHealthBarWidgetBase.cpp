#include "UI/Widgets/RogueEnemyHealthBarWidgetBase.h"

#include "Components/ProgressBar.h"
#include "UI/RogueHUD.h"

void URogueEnemyHealthBarWidgetBase::SetOwningRogueHUD(ARogueHUD* InOwningHUD)
{
	OwningHUD = InOwningHUD;
}

void URogueEnemyHealthBarWidgetBase::UpdateBar(const FRogueEnemyHealthBarView& InView)
{
	CachedView = InView;

	if (!InView.bVisible)
	{
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	SetVisibility(ESlateVisibility::HitTestInvisible);
	// 顶部中心对齐到投影点；bRemoveDPIScale=true 表示传入的是原始像素。
	SetPositionInViewport(InView.ScreenPosition, true);

	if (FillBar != nullptr)
	{
		FillBar->SetPercent(InView.HealthPercent);
	}

	OnBarUpdated(CachedView);
}
