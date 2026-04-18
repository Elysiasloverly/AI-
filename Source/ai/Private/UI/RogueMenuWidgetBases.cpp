#include "UI/RogueMenuWidgetBases.h"

#include "UI/RogueHUD.h"

void URogueShopOfferCardWidgetBase::InitializeOfferCard(ARogueHUD* InOwningHUD, int32 InOfferIndex)
{
	OwningRogueHUD = InOwningHUD;
	OfferIndex = InOfferIndex;
}

void URogueShopOfferCardWidgetBase::UpdateOfferView(const FRogueShopOfferViewData& InViewData)
{
	OnOfferViewUpdated(InViewData);
}

void URogueShopOfferCardWidgetBase::RequestPurchaseOffer()
{
	if (ARogueHUD* RogueHUD = OwningRogueHUD.Get())
	{
		RogueHUD->RequestSelectOfferFromShop(OfferIndex);
	}
}

void URogueUpgradeCardWidgetBase::InitializeUpgradeCard(ARogueHUD* InOwningHUD, int32 InUpgradeIndex)
{
	OwningRogueHUD = InOwningHUD;
	UpgradeIndex = InUpgradeIndex;
}

void URogueUpgradeCardWidgetBase::UpdateUpgradeCardView(const FRogueUpgradeCardViewData& InViewData)
{
	OnUpgradeCardViewUpdated(InViewData);
}

void URogueUpgradeCardWidgetBase::RequestSelectUpgrade()
{
	if (ARogueHUD* RogueHUD = OwningRogueHUD.Get())
	{
		RogueHUD->RequestSelectUpgradeFromWidget(UpgradeIndex);
	}
}

void URoguePauseMenuWidgetBase::SetOwningRogueHUD(ARogueHUD* InOwningHUD)
{
	OwningRogueHUD = InOwningHUD;
}

void URoguePauseMenuWidgetBase::UpdatePauseView(const FRoguePauseMenuViewData& InViewData)
{
	SetVisibility(InViewData.bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
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

void URogueShopWidgetBase::SetOwningRogueHUD(ARogueHUD* InOwningHUD)
{
	OwningRogueHUD = InOwningHUD;
}

void URogueShopWidgetBase::UpdateShopView(const FRogueShopViewData& InViewData)
{
	SetVisibility(InViewData.bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	OnShopViewUpdated(InViewData);
}

void URogueShopWidgetBase::RequestSelectOffer(int32 OfferIndex)
{
	if (ARogueHUD* RogueHUD = OwningRogueHUD.Get())
	{
		RogueHUD->RequestSelectOfferFromShop(OfferIndex);
	}
}

void URogueShopWidgetBase::RequestRefreshShop()
{
	if (ARogueHUD* RogueHUD = OwningRogueHUD.Get())
	{
		RogueHUD->RequestRefreshFromShop();
	}
}

void URogueShopWidgetBase::RequestCloseShop()
{
	if (ARogueHUD* RogueHUD = OwningRogueHUD.Get())
	{
		RogueHUD->RequestCloseFromShop();
	}
}

void URogueUpgradeSelectionWidgetBase::SetOwningRogueHUD(ARogueHUD* InOwningHUD)
{
	OwningRogueHUD = InOwningHUD;
}

void URogueUpgradeSelectionWidgetBase::UpdateUpgradeSelectionView(const FRogueUpgradeSelectionViewData& InViewData)
{
	SetVisibility(InViewData.bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	OnUpgradeSelectionViewUpdated(InViewData);
}

void URogueSettingsMenuWidgetBase::SetOwningRogueHUD(ARogueHUD* InOwningHUD)
{
	OwningRogueHUD = InOwningHUD;
}

void URogueSettingsMenuWidgetBase::UpdateSettingsView(const FRogueSettingsMenuViewData& InViewData)
{
	SetVisibility(InViewData.bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
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

void URogueDeathScreenWidgetBase::SetOwningRogueHUD(ARogueHUD* InOwningHUD)
{
	OwningRogueHUD = InOwningHUD;
}

void URogueDeathScreenWidgetBase::UpdateDeathView(const FRogueDeathViewData& InViewData)
{
	SetVisibility(InViewData.bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	OnDeathViewUpdated(InViewData);
}

void URogueDeathScreenWidgetBase::RequestRestartRound()
{
	if (ARogueHUD* RogueHUD = OwningRogueHUD.Get())
	{
		RogueHUD->RequestRestartAfterDeath();
	}
}

void URogueDeathScreenWidgetBase::RequestQuitGame()
{
	if (ARogueHUD* RogueHUD = OwningRogueHUD.Get())
	{
		RogueHUD->RequestQuitAfterDeath();
	}
}
