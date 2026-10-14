#include "UI/Widgets/RogueUpgradeCardWidgetBase.h"
#include "UI/RogueHUD.h"
#include "Components/Button.h"

#include "RogueMenuWidgetHelpers.h"

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

void URogueUpgradeCardWidgetBase::HandleSelectButtonClicked()
{
	RequestSelectUpgrade();
}

void URogueUpgradeCardWidgetBase::ApplyManagedUpgradeCardView(const FRogueUpgradeCardViewData& InViewData)
{
	RogueMenuWidgetHelpers::SetManagedText(Text_Title, InViewData.TitleText);
	RogueMenuWidgetHelpers::SetManagedText(Text_Desc, InViewData.DescriptionText);
	RogueMenuWidgetHelpers::SetManagedBorderColor(Border_Card, FLinearColor(0.05f, 0.11f, 0.26f, 0.96f));
}
