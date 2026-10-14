#include "UI/Widgets/RogueShopOfferCardWidgetBase.h"
#include "UI/RogueHUD.h"
#include "Components/Button.h"

#include "RogueMenuWidgetHelpers.h"

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
	RogueMenuWidgetHelpers::SetManagedText(Text_Title, InViewData.TitleText);
	RogueMenuWidgetHelpers::SetManagedText(Text_Desc, InViewData.DescriptionText);
	RogueMenuWidgetHelpers::SetManagedText(Text_Cost, InViewData.CostText);

	if (Border_Card != nullptr)
	{
		const FLinearColor NormalColor(0.05f, 0.11f, 0.26f, 0.96f);
		const FLinearColor PurchasedColor(0.18f, 0.18f, 0.20f, 0.92f);
		const FLinearColor UnaffordableColor(0.26f, 0.08f, 0.08f, 0.92f);
		RogueMenuWidgetHelpers::SetManagedBorderColor(Border_Card, InViewData.bPurchased ? PurchasedColor : (InViewData.bAffordable ? NormalColor : UnaffordableColor));
	}
}
