#include "UI/Widgets/RogueSettingsMenuWidgetBase.h"
#include "UI/RogueHUD.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"

#include "RogueMenuWidgetHelpers.h"

void URogueSettingsMenuWidgetBase::NativeConstruct()
{
	Super::NativeConstruct();
	if (auto* Button = Cast<UButton>(GetWidgetFromName(TEXT("Button_MusicMinus"))))
		Button->OnClicked.AddUniqueDynamic(this, &ThisClass::RequestMusicVolumeDown);
	if (auto* Button = Cast<UButton>(GetWidgetFromName(TEXT("Button_MusicPlus"))))
		Button->OnClicked.AddUniqueDynamic(this, &ThisClass::RequestMusicVolumeUp);
}

void URogueSettingsMenuWidgetBase::SetOwningRogueHUD(ARogueHUD* InOwningHUD)
{
	OwningRogueHUD = InOwningHUD;
}

void URogueSettingsMenuWidgetBase::UpdateSettingsView(const FRogueSettingsMenuViewData& InViewData)
{
	SetVisibility(RogueMenuWidgetHelpers::GetMenuVisibility(InViewData.bVisible));
	OnSettingsViewUpdated(InViewData);
	// 保留原有 0–4 行蓝图事件，新增音乐行由同一份视图数据更新。
	if (InViewData.Rows.IsValidIndex(5))
	{
		if (auto* Text = Cast<UTextBlock>(GetWidgetFromName(TEXT("Text_Label5")))) Text->SetText(InViewData.Rows[5].LabelText);
		if (auto* Text = Cast<UTextBlock>(GetWidgetFromName(TEXT("Text_Value5")))) Text->SetText(InViewData.Rows[5].ValueText);
	}
}

void URogueSettingsMenuWidgetBase::RequestBackToPauseMenu()
{
	if (ARogueHUD* RogueHUD = OwningRogueHUD.Get())
	{
		RogueHUD->RequestBackFromSettingsMenu();
	}
}

void URogueSettingsMenuWidgetBase::RequestApplySettings()
{
	if (ARogueHUD* RogueHUD = OwningRogueHUD.Get())
	{
		RogueHUD->RequestApplyFromSettingsMenu();
	}
}

void URogueSettingsMenuWidgetBase::RequestVolumeDown()
{
	if (ARogueHUD* RogueHUD = OwningRogueHUD.Get())
	{
		RogueHUD->RequestVolumeAdjustFromSettingsMenu(false);
	}
}

void URogueSettingsMenuWidgetBase::RequestVolumeUp()
{
	if (ARogueHUD* RogueHUD = OwningRogueHUD.Get())
	{
		RogueHUD->RequestVolumeAdjustFromSettingsMenu(true);
	}
}

void URogueSettingsMenuWidgetBase::RequestMusicVolumeDown()
{
	if (ARogueHUD* HUD = OwningRogueHUD.Get()) HUD->RequestMusicVolumeAdjustFromSettingsMenu(false);
}

void URogueSettingsMenuWidgetBase::RequestMusicVolumeUp()
{
	if (ARogueHUD* HUD = OwningRogueHUD.Get()) HUD->RequestMusicVolumeAdjustFromSettingsMenu(true);
}

void URogueSettingsMenuWidgetBase::RequestQualityDown()
{
	if (ARogueHUD* RogueHUD = OwningRogueHUD.Get())
	{
		RogueHUD->RequestQualityAdjustFromSettingsMenu(false);
	}
}

void URogueSettingsMenuWidgetBase::RequestQualityUp()
{
	if (ARogueHUD* RogueHUD = OwningRogueHUD.Get())
	{
		RogueHUD->RequestQualityAdjustFromSettingsMenu(true);
	}
}

void URogueSettingsMenuWidgetBase::RequestResolutionDown()
{
	if (ARogueHUD* RogueHUD = OwningRogueHUD.Get())
	{
		RogueHUD->RequestResolutionAdjustFromSettingsMenu(false);
	}
}

void URogueSettingsMenuWidgetBase::RequestResolutionUp()
{
	if (ARogueHUD* RogueHUD = OwningRogueHUD.Get())
	{
		RogueHUD->RequestResolutionAdjustFromSettingsMenu(true);
	}
}

void URogueSettingsMenuWidgetBase::RequestFrameLimitDown()
{
	if (ARogueHUD* RogueHUD = OwningRogueHUD.Get())
	{
		RogueHUD->RequestFrameLimitAdjustFromSettingsMenu(false);
	}
}

void URogueSettingsMenuWidgetBase::RequestFrameLimitUp()
{
	if (ARogueHUD* RogueHUD = OwningRogueHUD.Get())
	{
		RogueHUD->RequestFrameLimitAdjustFromSettingsMenu(true);
	}
}

void URogueSettingsMenuWidgetBase::RequestToggleDisplayMode()
{
	if (ARogueHUD* RogueHUD = OwningRogueHUD.Get())
	{
		RogueHUD->RequestToggleDisplayModeFromSettingsMenu();
	}
}
