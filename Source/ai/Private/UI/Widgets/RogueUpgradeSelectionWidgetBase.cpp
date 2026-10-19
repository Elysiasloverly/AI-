#include "UI/Widgets/RogueUpgradeSelectionWidgetBase.h"
#include "UI/RogueHUD.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"

#include "RogueMenuWidgetHelpers.h"

void URogueUpgradeSelectionWidgetBase::SetOwningRogueHUD(ARogueHUD* InOwningHUD)
{
	OwningRogueHUD = InOwningHUD;
}

void URogueUpgradeSelectionWidgetBase::UpdateUpgradeSelectionView(const FRogueUpgradeSelectionViewData& InViewData)
{
	SetVisibility(RogueMenuWidgetHelpers::GetMenuVisibility(InViewData.bVisible));

	if (HorizontalBox_Cards != nullptr && ResolveUpgradeCardWidgetClass() != nullptr)
	{
		ApplyManagedUpgradeSelectionView(InViewData);
		return;
	}

	OnUpgradeSelectionViewUpdated(InViewData);
}

void URogueUpgradeSelectionWidgetBase::ApplyManagedUpgradeSelectionView(const FRogueUpgradeSelectionViewData& InViewData)
{
	RogueMenuWidgetHelpers::SetManagedText(Text_Title, InViewData.TitleText);

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

	// 类引用存入 UPROPERTY 供 GC 追踪；函数静态的普通 TSubclassOf 不构成 GC 强引用，
	// 对应类若随关卡卸载并被回收，就存在缓存失效风险。
	ManagedUpgradeCardWidgetClass = LoadClass<URogueUpgradeCardWidgetBase>(
		nullptr, TEXT("/Game/UMG/Menus/WBP_UpgradeCard.WBP_UpgradeCard_C"));
	return ManagedUpgradeCardWidgetClass;
}
