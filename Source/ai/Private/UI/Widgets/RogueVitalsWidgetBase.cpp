#include "UI/Widgets/RogueVitalsWidgetBase.h"

#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "UI/RogueHUD.h"

void URogueVitalsWidgetBase::SetOwningRogueHUD(ARogueHUD* InOwningHUD)
{
	OwningHUD = InOwningHUD;
}

void URogueVitalsWidgetBase::UpdateVitals(const FRoguePlayerVitalsView& InView)
{
	if (bHasView && CachedView.Equals(InView))
	{
		return;
	}

	bHasView = true;
	CachedView = InView;

	// 状态条只负责显示，不拦截鼠标/手柄输入。
	SetVisibility(InView.bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);

	// 命名绑定的进度条（如果有）由 C++ 直接更新；WBP 不写事件图也能显示。
	if (HealthBar != nullptr)
	{
		HealthBar->SetPercent(InView.HealthPercent);
	}
	if (ArmorBar != nullptr)
	{
		ArmorBar->SetPercent(InView.ArmorPercent);
		ArmorBar->SetVisibility(InView.bHasArmor ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (ExperienceBar != nullptr)
	{
		ExperienceBar->SetPercent(InView.ExperiencePercent);
	}

	if (HealthText != nullptr)
	{
		HealthText->SetText(FText::FromString(FString::Printf(
			TEXT("生命 %.0f / %.0f"), InView.CurrentHealth, InView.MaxHealth)));
	}
	if (ArmorText != nullptr)
	{
		ArmorText->SetText(FText::FromString(FString::Printf(
			TEXT("护甲 %.0f / %.0f"), InView.CurrentArmor, InView.MaxArmor)));
		ArmorText->SetVisibility(InView.bHasArmor ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
	if (ExperienceText != nullptr)
	{
		ExperienceText->SetText(FText::FromString(FString::Printf(
			TEXT("经验 %d / %d"), InView.Experience, InView.ExperienceToNextLevel)));
	}
	if (WaveText != nullptr)
	{
		WaveText->SetText(FText::FromString(FString::Printf(TEXT("第 %d 波"), InView.CurrentWave)));
	}
	if (GoldText != nullptr)
	{
		GoldText->SetText(FText::FromString(FString::Printf(TEXT("金币 %d"), InView.Money)));
	}

	OnVitalsUpdated(CachedView);
}
