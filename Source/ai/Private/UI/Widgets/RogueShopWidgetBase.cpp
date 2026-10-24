#include "UI/Widgets/RogueShopWidgetBase.h"
#include "UI/RogueHUD.h"
#include "Components/Button.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"

#include "RogueMenuWidgetHelpers.h"

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
	SetVisibility(RogueMenuWidgetHelpers::GetMenuVisibility(InViewData.bVisible));

	if (UniformGridPanel_Offers != nullptr && ResolveOfferCardWidgetClass() != nullptr)
	{
		ApplyManagedShopView(InViewData);
		return;
	}

	OnShopViewUpdated(InViewData);
}

void URogueShopWidgetBase::UpdateAutoRefreshText(const FText& InAutoRefreshText)
{
	RogueMenuWidgetHelpers::SetManagedText(Text_AutoRefresh, InAutoRefreshText);
	OnAutoRefreshTextUpdated(InAutoRefreshText);
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
	RogueMenuWidgetHelpers::SetManagedText(Text_Title, InViewData.TitleText);
	RogueMenuWidgetHelpers::SetManagedText(Text_Money, InViewData.MoneyText);
	RogueMenuWidgetHelpers::SetManagedText(Text_Hint, InViewData.HintText);
	UpdateAutoRefreshText(InViewData.AutoRefreshText);
	RogueMenuWidgetHelpers::SetManagedText(Text_Refresh, InViewData.RefreshButtonText);

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

	// 类引用存入 UPROPERTY 供 GC 追踪；函数静态的普通 TSubclassOf 不构成 GC 强引用，
	// 对应类若随关卡卸载并被回收，就存在缓存失效风险。
	ManagedOfferCardWidgetClass = LoadClass<URogueShopOfferCardWidgetBase>(
		nullptr, TEXT("/Game/UMG/Menus/Shop/WBP_ShopOfferCard.WBP_ShopOfferCard_C"));
	return ManagedOfferCardWidgetClass;
}
