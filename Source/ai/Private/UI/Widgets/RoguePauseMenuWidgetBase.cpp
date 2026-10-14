#include "UI/Widgets/RoguePauseMenuWidgetBase.h"
#include "UI/RogueHUD.h"

#include "RogueMenuWidgetHelpers.h"

void URoguePauseMenuWidgetBase::SetOwningRogueHUD(ARogueHUD* InOwningHUD)
{
	OwningRogueHUD = InOwningHUD;
}

void URoguePauseMenuWidgetBase::UpdatePauseView(const FRoguePauseMenuViewData& InViewData)
{
	SetVisibility(RogueMenuWidgetHelpers::GetMenuVisibility(InViewData.bVisible));
	OnPauseViewUpdated(InViewData);
}

void URoguePauseMenuWidgetBase::RequestResumeGame()
{
	if (ARogueHUD* RogueHUD = OwningRogueHUD.Get())
	{
		RogueHUD->RequestResumeFromPauseMenu();
	}
}

void URoguePauseMenuWidgetBase::RequestOpenSettings()
{
	if (ARogueHUD* RogueHUD = OwningRogueHUD.Get())
	{
		RogueHUD->RequestOpenSettingsFromPauseMenu();
	}
}

void URoguePauseMenuWidgetBase::RequestQuitGame()
{
	if (ARogueHUD* RogueHUD = OwningRogueHUD.Get())
	{
		RogueHUD->RequestQuitFromPauseMenu();
	}
}
