#include "UI/Widgets/RogueUpgradeCardWidgetBase.h"
#include "Audio/RogueMusicSubsystem.h"
#include "UI/RogueHUD.h"
#include "Components/Button.h"
#include "UObject/UnrealType.h"

#include "RogueMenuWidgetHelpers.h"

void URogueUpgradeCardWidgetBase::InitializeUpgradeCard(ARogueHUD* InOwningHUD, int32 InUpgradeIndex)
{
	OwningRogueHUD = InOwningHUD;
	UpgradeIndex = InUpgradeIndex;
}

TSharedRef<SWidget> URogueUpgradeCardWidgetBase::RebuildWidget()
{
	// 点击战斗卡片不能把键盘焦点从视口拿走，否则视口会清掉正在按住的方向键。
	// 数字键 1–3 仍由角色的 Enhanced Input 处理。
	Initialize();
	SetIsFocusable(false);
	if (Button_Select)
	{
		// UE 5.5 通过这个属性创建 Slate 按钮，UButton 只在构造阶段提供受保护的 setter。
		const FBoolProperty* Focusable = FindFProperty<FBoolProperty>(UButton::StaticClass(), TEXT("IsFocusable"));
		if (Focusable)
		{
			Focusable->SetPropertyValue_InContainer(Button_Select.Get(), false);
		}
	}
	return Super::RebuildWidget();
}

void URogueUpgradeCardWidgetBase::NativeConstruct()
{
	Super::NativeConstruct();
	URogueMusicSubsystem::BindWidgetButtonSounds(this);

	if (Button_Select != nullptr)
	{
		Button_Select->OnClicked.RemoveAll(this);
		Button_Select->OnClicked.AddDynamic(this, &URogueUpgradeCardWidgetBase::HandleSelectButtonClicked);
	}
}

void URogueUpgradeCardWidgetBase::UpdateUpgradeCardView(const FRogueUpgradeCardViewData& InViewData)
{
	SelectionId = InViewData.SelectionId;
	bCanSelect = InViewData.bCanSelect;
	if (Button_Select) Button_Select->SetIsEnabled(bCanSelect);
	ApplyManagedUpgradeCardView(InViewData);
	OnUpgradeCardViewUpdated(InViewData);
}

void URogueUpgradeCardWidgetBase::RequestSelectUpgrade()
{
	if (!bCanSelect || !SelectionId.IsValid()) return;
	if (ARogueHUD* RogueHUD = OwningRogueHUD.Get())
	{
		RogueHUD->RequestSelectUpgradeFromWidget(UpgradeIndex, SelectionId);
	}
}

void URogueUpgradeCardWidgetBase::HandleSelectButtonClicked()
{
	RequestSelectUpgrade();
}

void URogueUpgradeCardWidgetBase::ApplyManagedUpgradeCardView(const FRogueUpgradeCardViewData& InViewData)
{
    ApplyCardIcon(InViewData.IconType);
    RogueMenuWidgetHelpers::SetCardSeal(this, InViewData.bCanSelect ? 1.f : 0.f);
	RogueMenuWidgetHelpers::SetManagedText(Text_Title, InViewData.TitleText);
	RogueMenuWidgetHelpers::SetManagedText(Text_Desc, InViewData.DescriptionText);
    if (UButton* Button = GetMotionButton()) Button->SetToolTipText(InViewData.DescriptionText);
	RogueMenuWidgetHelpers::SetManagedBorderColor(Border_Card, CardTint);
}
