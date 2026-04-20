#include "UI/RogueMenuWidgetBases.h"

#include "UI/RogueHUD.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"

void URogueShopOfferCardWidgetBase::NativeConstruct()
{
	Super::NativeConstruct();

	if (Button_Offer != nullptr)
	{
		Button_Offer->OnClicked.RemoveAll(this);
		Button_Offer->OnClicked.AddDynamic(this, &URogueShopOfferCardWidgetBase::HandleOfferButtonClicked);
	}
}

void URogueShopOfferCardWidgetBase::InitializeOfferCard(ARogueHUD* InOwningHUD, int32 InOfferIndex)
{
	OwningRogueHUD = InOwningHUD;
	OfferIndex = InOfferIndex;
}

void URogueShopOfferCardWidgetBase::UpdateOfferView(const FRogueShopOfferViewData& InViewData)
{
	ApplyManagedOfferView(InViewData);
	OnOfferViewUpdated(InViewData);
}

void URogueShopOfferCardWidgetBase::HandleOfferButtonClicked()
{
	RequestPurchaseOffer();
}

void URogueShopOfferCardWidgetBase::RequestPurchaseOffer()
{
	if (ARogueHUD* RogueHUD = OwningRogueHUD.Get())
	{
		RogueHUD->RequestSelectOfferFromShop(OfferIndex);
	}
}

void URogueShopOfferCardWidgetBase::ApplyManagedOfferView(const FRogueShopOfferViewData& InViewData)
{
	if (Text_Title != nullptr)
	{
		Text_Title->SetText(InViewData.TitleText);
		Text_Title->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}

	if (Text_Desc != nullptr)
	{
		Text_Desc->SetText(InViewData.DescriptionText);
		Text_Desc->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}

	if (Text_Cost != nullptr)
	{
		Text_Cost->SetText(InViewData.CostText);
		Text_Cost->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}

	if (Border_Card != nullptr)
	{
		const FLinearColor NormalColor(0.05f, 0.11f, 0.26f, 0.96f);
		const FLinearColor PurchasedColor(0.18f, 0.18f, 0.20f, 0.92f);
		const FLinearColor UnaffordableColor(0.26f, 0.08f, 0.08f, 0.92f);
		Border_Card->SetBrushColor(
			InViewData.bPurchased ? PurchasedColor : (InViewData.bAffordable ? NormalColor : UnaffordableColor));
		Border_Card->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
}

void URogueUpgradeCardWidgetBase::InitializeUpgradeCard(ARogueHUD* InOwningHUD, int32 InUpgradeIndex)
{
	OwningRogueHUD = InOwningHUD;
	UpgradeIndex = InUpgradeIndex;
}

void URogueUpgradeCardWidgetBase::NativeConstruct()
{
	Super::NativeConstruct();

	if (Button_Select != nullptr)
	{
		Button_Select->OnClicked.RemoveAll(this);
		Button_Select->OnClicked.AddDynamic(this, &URogueUpgradeCardWidgetBase::HandleSelectButtonClicked);
	}
}

void URogueUpgradeCardWidgetBase::UpdateUpgradeCardView(const FRogueUpgradeCardViewData& InViewData)
{
	ApplyManagedUpgradeCardView(InViewData);
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

void URogueUpgradeCardWidgetBase::HandleSelectButtonClicked()
{
	RequestSelectUpgrade();
}

void URogueUpgradeCardWidgetBase::ApplyManagedUpgradeCardView(const FRogueUpgradeCardViewData& InViewData)
{
	if (Text_Title != nullptr)
	{
		Text_Title->SetText(InViewData.TitleText);
		Text_Title->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}

	if (Text_Desc != nullptr)
	{
		Text_Desc->SetText(InViewData.DescriptionText);
		Text_Desc->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}

	if (Border_Card != nullptr)
	{
		Border_Card->SetBrushColor(FLinearColor(0.05f, 0.11f, 0.26f, 0.96f));
		Border_Card->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
}

void URogueShopWidgetBase::NativeConstruct()
{
	Super::NativeConstruct();

	if (Button_Refresh != nullptr)
	{
		Button_Refresh->OnClicked.RemoveAll(this);
		Button_Refresh->OnClicked.AddDynamic(this, &URogueShopWidgetBase::HandleRefreshButtonClicked);
	}

	if (Button_Close != nullptr)
	{
		Button_Close->OnClicked.RemoveAll(this);
		Button_Close->OnClicked.AddDynamic(this, &URogueShopWidgetBase::HandleCloseButtonClicked);
	}
}

void URogueShopWidgetBase::SetOwningRogueHUD(ARogueHUD* InOwningHUD)
{
	OwningRogueHUD = InOwningHUD;
}

void URogueShopWidgetBase::UpdateShopView(const FRogueShopViewData& InViewData)
{
	SetVisibility(InViewData.bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);

	if (UniformGridPanel_Offers != nullptr && ResolveOfferCardWidgetClass() != nullptr)
	{
		ApplyManagedShopView(InViewData);
		return;
	}

	OnShopViewUpdated(InViewData);
}

void URogueShopWidgetBase::HandleRefreshButtonClicked()
{
	RequestRefreshShop();
}

void URogueShopWidgetBase::HandleCloseButtonClicked()
{
	RequestCloseShop();
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

void URogueShopWidgetBase::ApplyManagedShopView(const FRogueShopViewData& InViewData)
{
	if (Text_Title != nullptr)
	{
		Text_Title->SetText(InViewData.TitleText);
		Text_Title->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}

	if (Text_Money != nullptr)
	{
		Text_Money->SetText(InViewData.MoneyText);
		Text_Money->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}

	if (Text_Hint != nullptr)
	{
		Text_Hint->SetText(InViewData.HintText);
		Text_Hint->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}

	if (Text_AutoRefresh != nullptr)
	{
		Text_AutoRefresh->SetText(InViewData.AutoRefreshText);
		Text_AutoRefresh->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}

	if (Text_Refresh != nullptr)
	{
		Text_Refresh->SetText(InViewData.RefreshButtonText);
		Text_Refresh->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}

	EnsureOfferCardWidgets(InViewData.Offers.Num());

	for (int32 Index = 0; Index < ManagedOfferCardWidgets.Num(); ++Index)
	{
		if (URogueShopOfferCardWidgetBase* CardWidget = ManagedOfferCardWidgets[Index])
		{
			if (InViewData.Offers.IsValidIndex(Index))
			{
				CardWidget->SetVisibility(ESlateVisibility::Visible);
				CardWidget->InitializeOfferCard(OwningRogueHUD.Get(), Index);
				CardWidget->UpdateOfferView(InViewData.Offers[Index]);
			}
			else
			{
				CardWidget->SetVisibility(ESlateVisibility::Collapsed);
			}
		}
	}
}

void URogueShopWidgetBase::EnsureOfferCardWidgets(int32 DesiredCount)
{
	if (UniformGridPanel_Offers == nullptr)
	{
		return;
	}

	if (DesiredCount <= ManagedOfferCardWidgets.Num())
	{
		return;
	}

	TSubclassOf<URogueShopOfferCardWidgetBase> WidgetClass = ResolveOfferCardWidgetClass();
	if (WidgetClass == nullptr)
	{
		return;
	}

	while (ManagedOfferCardWidgets.Num() < DesiredCount)
	{
		URogueShopOfferCardWidgetBase* CardWidget = CreateWidget<URogueShopOfferCardWidgetBase>(this, WidgetClass);
		if (CardWidget == nullptr)
		{
			break;
		}

		CardWidget->InitializeOfferCard(OwningRogueHUD.Get(), ManagedOfferCardWidgets.Num());
		UniformGridPanel_Offers->AddChild(CardWidget);

		if (UUniformGridSlot* GridSlot = Cast<UUniformGridSlot>(CardWidget->Slot))
		{
			const int32 CardIndex = ManagedOfferCardWidgets.Num();
			GridSlot->SetColumn(CardIndex % 3);
			GridSlot->SetRow(CardIndex / 3);
		}

		ManagedOfferCardWidgets.Add(CardWidget);
	}
}

TSubclassOf<URogueShopOfferCardWidgetBase> URogueShopWidgetBase::ResolveOfferCardWidgetClass()
{
	if (ManagedOfferCardWidgetClass != nullptr)
	{
		return ManagedOfferCardWidgetClass;
	}

	static TSubclassOf<URogueShopOfferCardWidgetBase> CachedWidgetClass = nullptr;
	if (CachedWidgetClass == nullptr)
	{
		CachedWidgetClass = LoadClass<URogueShopOfferCardWidgetBase>(
			nullptr,
			TEXT("/Game/UMG/Menus/WBP_ShopOfferCard.WBP_ShopOfferCard_C"));
	}

	return CachedWidgetClass;
}

void URogueUpgradeSelectionWidgetBase::SetOwningRogueHUD(ARogueHUD* InOwningHUD)
{
	OwningRogueHUD = InOwningHUD;
}

void URogueUpgradeSelectionWidgetBase::NativeConstruct()
{
	Super::NativeConstruct();
}

void URogueUpgradeSelectionWidgetBase::UpdateUpgradeSelectionView(const FRogueUpgradeSelectionViewData& InViewData)
{
	SetVisibility(InViewData.bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);

	if (HorizontalBox_Cards != nullptr && ResolveUpgradeCardWidgetClass() != nullptr)
	{
		ApplyManagedUpgradeSelectionView(InViewData);
		return;
	}

	OnUpgradeSelectionViewUpdated(InViewData);
}

void URogueUpgradeSelectionWidgetBase::ApplyManagedUpgradeSelectionView(const FRogueUpgradeSelectionViewData& InViewData)
{
	if (Text_Title != nullptr)
	{
		Text_Title->SetText(InViewData.TitleText);
		Text_Title->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}

	EnsureUpgradeCardWidgets(InViewData.Cards.Num());

	for (int32 Index = 0; Index < ManagedUpgradeCardWidgets.Num(); ++Index)
	{
		if (URogueUpgradeCardWidgetBase* CardWidget = ManagedUpgradeCardWidgets[Index])
		{
			if (InViewData.Cards.IsValidIndex(Index))
			{
				CardWidget->SetVisibility(ESlateVisibility::Visible);
				CardWidget->InitializeUpgradeCard(OwningRogueHUD.Get(), Index);
				CardWidget->UpdateUpgradeCardView(InViewData.Cards[Index]);
			}
			else
			{
				CardWidget->SetVisibility(ESlateVisibility::Collapsed);
			}
		}
	}
}

void URogueUpgradeSelectionWidgetBase::EnsureUpgradeCardWidgets(int32 DesiredCount)
{
	if (HorizontalBox_Cards == nullptr)
	{
		return;
	}

	if (DesiredCount <= ManagedUpgradeCardWidgets.Num())
	{
		return;
	}

	TSubclassOf<URogueUpgradeCardWidgetBase> WidgetClass = ResolveUpgradeCardWidgetClass();
	if (WidgetClass == nullptr)
	{
		return;
	}

	while (ManagedUpgradeCardWidgets.Num() < DesiredCount)
	{
		URogueUpgradeCardWidgetBase* CardWidget = CreateWidget<URogueUpgradeCardWidgetBase>(this, WidgetClass);
		if (CardWidget == nullptr)
		{
			break;
		}

		CardWidget->InitializeUpgradeCard(OwningRogueHUD.Get(), ManagedUpgradeCardWidgets.Num());
		HorizontalBox_Cards->AddChildToHorizontalBox(CardWidget);

		if (UHorizontalBoxSlot* HorizontalSlot = Cast<UHorizontalBoxSlot>(CardWidget->Slot))
		{
			HorizontalSlot->SetPadding(FMargin(10.0f, 0.0f));
		}

		ManagedUpgradeCardWidgets.Add(CardWidget);
	}
}

TSubclassOf<URogueUpgradeCardWidgetBase> URogueUpgradeSelectionWidgetBase::ResolveUpgradeCardWidgetClass()
{
	if (ManagedUpgradeCardWidgetClass != nullptr)
	{
		return ManagedUpgradeCardWidgetClass;
	}

	static TSubclassOf<URogueUpgradeCardWidgetBase> CachedWidgetClass = nullptr;
	if (CachedWidgetClass == nullptr)
	{
		CachedWidgetClass = LoadClass<URogueUpgradeCardWidgetBase>(
			nullptr,
			TEXT("/Game/UMG/Menus/WBP_UpgradeCard.WBP_UpgradeCard_C"));
	}

	return CachedWidgetClass;
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
