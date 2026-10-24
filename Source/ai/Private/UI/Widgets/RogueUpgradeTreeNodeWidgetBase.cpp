#include "UI/Widgets/RogueUpgradeTreeNodeWidgetBase.h"

#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#if WITH_EDITOR
#include "UObject/ObjectSaveContext.h"
#endif

void URogueUpgradeTreeNodeWidgetBase::NativeConstruct()
{
	Super::NativeConstruct();
	if (PurchaseButton != nullptr)
	{
		PurchaseButton->OnClicked.AddUniqueDynamic(this, &URogueUpgradeTreeNodeWidgetBase::HandlePurchaseClicked);
	}
	ApplyViewData();
}

void URogueUpgradeTreeNodeWidgetBase::SetViewData(const FRogueUpgradeTreeNodeViewData& InViewData)
{
	ViewData = InViewData;
	ApplyViewData();
}

void URogueUpgradeTreeNodeWidgetBase::ApplyViewData()
{
	if (TitleText != nullptr) TitleText->SetText(ViewData.Title);
	if (DescriptionText != nullptr) DescriptionText->SetText(ViewData.Description);
	if (CostText != nullptr) CostText->SetText(ViewData.Cost);
	if (RankText != nullptr) RankText->SetText(ViewData.Rank);
	if (StateText != nullptr) StateText->SetText(ViewData.StateLabel);
	if (PrerequisiteText != nullptr) PrerequisiteText->SetText(ViewData.Prerequisites);
	if (PurchaseButton != nullptr) PurchaseButton->SetIsEnabled(ViewData.bCanPurchase);
	if (PurchaseLabel != nullptr)
	{
		PurchaseLabel->SetText(FText::FromString(ViewData.State == ERogueUpgradeTreeNodeState::Maxed
			? TEXT("已完成") : (ViewData.CurrentRank > 0 ? TEXT("升级") : TEXT("解锁"))));
	}
	if (StateBorder != nullptr && !bBlueprintHandlesStateStyle)
	{
		FLinearColor Color = LockedColor;
		switch (ViewData.State)
		{
		case ERogueUpgradeTreeNodeState::Available: Color = AvailableColor; break;
		case ERogueUpgradeTreeNodeState::Unaffordable: Color = UnaffordableColor; break;
		case ERogueUpgradeTreeNodeState::Unlocked: Color = UnlockedColor; break;
		case ERogueUpgradeTreeNodeState::Maxed: Color = CompletedColor; break;
		default: break;
		}
		StateBorder->SetBrushColor(Color);
	}
	OnViewDataChanged(ViewData);
}

void URogueUpgradeTreeNodeWidgetBase::HandlePurchaseClicked()
{
	if (ViewData.bCanPurchase)
	{
		OnNodePressed.Broadcast(ViewData.NodeId);
	}
}


FVector2D URogueUpgradeTreeNodeWidgetBase::GetPreferredLayoutSize(FVector2D FallbackSize) const
{
	FVector2D Size = GetDesiredSize();
	FVector2D DesignerSize = SavedDesignerSize;
#if WITH_EDITORONLY_DATA
	if (DesignSizeMode == EDesignPreviewSizeMode::Custom || DesignSizeMode == EDesignPreviewSizeMode::CustomOnScreen)
	{
		DesignerSize = DesignTimeSize;
	}
#endif
	if (DesignerSize.X > 0 && DesignerSize.Y > 0)
	{
		Size.X = FMath::Max(Size.X, DesignerSize.X);
		Size.Y = FMath::Max(Size.Y, DesignerSize.Y);
	}
	if (LayoutSizeOverride.X > 0) Size.X = LayoutSizeOverride.X;
	if (LayoutSizeOverride.Y > 0) Size.Y = LayoutSizeOverride.Y;
	if (!FMath::IsFinite(Size.X) || Size.X <= 0) Size.X = FMath::Max(1.0, FallbackSize.X);
	if (!FMath::IsFinite(Size.Y) || Size.Y <= 0) Size.Y = FMath::Max(1.0, FallbackSize.Y);
	return Size;
}

void URogueUpgradeTreeNodeWidgetBase::CaptureDesignerSize()
{
#if WITH_EDITORONLY_DATA
	SavedDesignerSize = (DesignSizeMode == EDesignPreviewSizeMode::Custom || DesignSizeMode == EDesignPreviewSizeMode::CustomOnScreen)
		? DesignTimeSize : FVector2D::ZeroVector;
#endif
}

#if WITH_EDITOR
void URogueUpgradeTreeNodeWidgetBase::PreSave(FObjectPreSaveContext SaveContext)
{
	CaptureDesignerSize();
	Super::PreSave(SaveContext);
}
#endif
