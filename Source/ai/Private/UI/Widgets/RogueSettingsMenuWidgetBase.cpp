#include "UI/Widgets/RogueSettingsMenuWidgetBase.h"
#include "UI/RogueHUD.h"

#include "RogueMenuWidgetHelpers.h"

void URogueSettingsMenuWidgetBase::SetOwningRogueHUD(ARogueHUD* InOwningHUD)
{
	OwningRogueHUD = InOwningHUD;
}

void URogueSettingsMenuWidgetBase::UpdateSettingsView(const FRogueSettingsMenuViewData& InViewData)
{
	SetVisibility(RogueMenuWidgetHelpers::GetMenuVisibility(InViewData.bVisible));
	OnSettingsViewUpdated(InViewData);
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
