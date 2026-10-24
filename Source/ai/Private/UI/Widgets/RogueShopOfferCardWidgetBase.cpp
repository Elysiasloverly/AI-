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
		const FLinearColor NormalColor = NormalCardTint;
		const FLinearColor PurchasedColor = PurchasedCardTint;
		const FLinearColor UnaffordableColor = UnaffordableCardTint;
		RogueMenuWidgetHelpers::SetManagedBorderColor(Border_Card, InViewData.bPurchased ? PurchasedColor : (InViewData.bAffordable ? NormalColor : UnaffordableColor));
	}
}
