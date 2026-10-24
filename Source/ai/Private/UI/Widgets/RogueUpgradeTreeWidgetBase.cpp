#include "UI/Widgets/RogueUpgradeTreeWidgetBase.h"
#include "UI/Widgets/RogueUpgradeTreeLayout.h"
#include "UI/Widgets/RogueUpgradeTreePaintSpace.h"

#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/SizeBox.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Engine/LocalPlayer.h"
#include "Input/Reply.h"
#include "InputCoreTypes.h"
#include "Progression/Meta/RogueProgressionSubsystem.h"
#include "Progression/Meta/RogueUpgradeTreeDefinition.h"
#include "Rendering/DrawElements.h"
#include "UI/RogueHUD.h"

void URogueUpgradeTreeWidgetBase::SetOwningRogueHUD(ARogueHUD* InOwningHUD)
{
	OwningRogueHUD = InOwningHUD;
}

void URogueUpgradeTreeWidgetBase::NativeConstruct()
{
	Super::NativeConstruct();
	SetIsFocusable(true);
	if (UTextBlock* Footer = Cast<UTextBlock>(GetWidgetFromName(TEXT("FooterText"))))
	{
		Footer->SetText(FText::FromString(Footer->GetText().ToString().Replace(TEXT("滚轮可查看上方分支"), TEXT("滚轮可查看下方分支"))));
	}
	if (WeaponsTabButton != nullptr) WeaponsTabButton->OnClicked.AddUniqueDynamic(this, &URogueUpgradeTreeWidgetBase::ShowWeaponTree);
	if (AttributesTabButton != nullptr) AttributesTabButton->OnClicked.AddUniqueDynamic(this, &URogueUpgradeTreeWidgetBase::ShowAttributeTree);
	if (CloseButton != nullptr) CloseButton->OnClicked.AddUniqueDynamic(this, &URogueUpgradeTreeWidgetBase::CloseTree);
	if (PreviousPageButton != nullptr) PreviousPageButton->OnClicked.AddUniqueDynamic(this, &URogueUpgradeTreeWidgetBase::PreviousPage);
	if (NextPageButton != nullptr) NextPageButton->OnClicked.AddUniqueDynamic(this, &URogueUpgradeTreeWidgetBase::NextPage);
	RefreshTree();
	if (TreeScrollBox != nullptr) TreeScrollBox->ScrollToStart();
}

void URogueUpgradeTreeWidgetBase::NativeDestruct()
{
	if (URogueProgressionSubsystem* LocalProgression = Progression.Get())
	{
		LocalProgression->OnProgressionChanged.RemoveDynamic(this, &URogueUpgradeTreeWidgetBase::HandleProgressionChanged);
	}
	Progression.Reset();
	Super::NativeDestruct();
}

URogueProgressionSubsystem* URogueUpgradeTreeWidgetBase::ResolveProgression()
{
	ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();
	URogueProgressionSubsystem* LocalProgression = LocalPlayer != nullptr ? LocalPlayer->GetSubsystem<URogueProgressionSubsystem>() : nullptr;
	if (Progression.Get() != LocalProgression)
	{
		if (Progression.IsValid()) Progression->OnProgressionChanged.RemoveDynamic(this, &URogueUpgradeTreeWidgetBase::HandleProgressionChanged);
		Progression = LocalProgression;
		if (LocalProgression != nullptr) LocalProgression->OnProgressionChanged.AddUniqueDynamic(this, &URogueUpgradeTreeWidgetBase::HandleProgressionChanged);
	}
	return LocalProgression;
}

void URogueUpgradeTreeWidgetBase::RefreshTree()
{
	URogueProgressionSubsystem* LocalProgression = ResolveProgression();
	const URogueUpgradeTreeDefinition* Definition = LocalProgression != nullptr ? LocalProgression->GetDefinition() : nullptr;
	if (Definition == nullptr || TreeCanvas == nullptr || NodeWidgetClass == nullptr)
	{
		if (StatusText != nullptr) StatusText->SetText(FText::FromString(TEXT("升级树暂不可用：请检查节点 WBP 和升级树配置。")));
		return;
	}

	const FRogueProgressionSnapshot Snapshot = LocalProgression->GetSnapshot();
	const int32 IceBricks = LocalProgression->GetIceBricks();
	const bool bSaveWritable = LocalProgression->IsSaveWritable();
	if (StatusText != nullptr && LocalProgression->GetPendingBossRewardCount() > 0)
	{
		StatusText->SetText(FText::FromString(FString::Printf(TEXT("有 %d 个冰砖等待保存，系统会自动重试。%s"), LocalProgression->GetPendingBossRewardCount(), *LocalProgression->GetSaveError().ToString())));
	}
	else if (!LocalProgression->GetSaveError().IsEmpty() && StatusText != nullptr) StatusText->SetText(LocalProgression->GetSaveError());
	if (IceBricksText != nullptr) IceBricksText->SetText(FText::FromString(FString::Printf(TEXT("冰砖  %d"), IceBricks)));
	if (SectionText != nullptr) SectionText->SetText(FText::FromString(bAttributeTree
		? TEXT("基础属性 · 每级 3 冰砖 · 每项最多 5 级")
		: TEXT("武器与专属卡 · 每个节点 5 冰砖 · 沿箭头逐步解锁")));
	if (WeaponsTabButton != nullptr) WeaponsTabButton->SetIsEnabled(bAttributeTree);
	if (AttributesTabButton != nullptr) AttributesTabButton->SetIsEnabled(!bAttributeTree);

	TreeCanvas->ClearChildren();
	NodeWidgets.Reset();
	NodePositions.Reset();
	NodeSizes.Reset();
	NodePrerequisites.Reset();
	PurchasedNodes.Reset();
	TArray<const FRogueUpgradeTreeNode*> VisibleNodes;
	TArray<FRogueUpgradeTreeNode> LayoutNodes;
	for (const FRogueUpgradeTreeNode& Node : Definition->GetNodes())
	{
		const bool bAttributeNode = Node.Kind == ERogueMetaNodeKind::BaseStat || Node.Kind == ERogueMetaNodeKind::ArmorUnlock;
		if (bAttributeNode != bAttributeTree) continue;
		VisibleNodes.Add(&Node);
		LayoutNodes.Add(Node);
	}

	for (const FRogueUpgradeTreeNode* Node : VisibleNodes)
	{
		URogueUpgradeTreeNodeWidgetBase* Widget = CreateWidget<URogueUpgradeTreeNodeWidgetBase>(GetOwningPlayer(), NodeWidgetClass);
		if (Widget == nullptr) continue;
		NodeWidgets.Add(Widget);
		Widget->OnNodePressed.AddUniqueDynamic(this, &URogueUpgradeTreeWidgetBase::PurchaseNode);
		if (UCanvasPanelSlot* NodeSlot = TreeCanvas->AddChildToCanvas(Widget))
		{
			NodeSlot->SetSize(NodeSize);
		}

		NodePrerequisites.Add(Node->NodeId, Node->Prerequisites);

		FRogueUpgradeTreeNodeViewData Data;
		Data.NodeId = Node->NodeId;
		Data.Title = Node->Title;
		Data.Description = Node->Description;
		Data.CurrentRank = Definition->GetNodeRank(Snapshot, Node->NodeId);
		Data.MaximumRank = Definition->GetMaximumRank(*Node);
		const FText Failure = Definition->GetPurchaseFailure(Snapshot, IceBricks, Node->NodeId);
		Data.bCanPurchase = Failure.IsEmpty() && bSaveWritable;
		bool bPrerequisitesMet = true;
		TArray<FString> MissingPrerequisites;
		for (const FName ParentId : Node->Prerequisites)
		{
			if (Definition->GetNodeRank(Snapshot, ParentId) <= 0)
			{
				bPrerequisitesMet = false;
				const FRogueUpgradeTreeNode* Parent = Definition->FindNode(ParentId);
				MissingPrerequisites.Add(Parent != nullptr ? Parent->Title.ToString() : ParentId.ToString());
			}
		}
		if (Data.CurrentRank > 0) PurchasedNodes.Add(Node->NodeId);
		if (Data.CurrentRank >= Data.MaximumRank)
		{
			Data.State = ERogueUpgradeTreeNodeState::Maxed;
			Data.StateLabel = FText::FromString(Data.MaximumRank > 1 ? TEXT("已满级") : TEXT("已解锁"));
			Data.Cost = FText::FromString(Node->bInitiallyUnlocked ? TEXT("初始开放") : TEXT("永久生效"));
		}
		else
		{
			Data.Cost = FText::FromString(FString::Printf(TEXT("%d 冰砖 / 次"), Node->Cost));
			Data.State = !bPrerequisitesMet ? ERogueUpgradeTreeNodeState::Locked
				: (Data.bCanPurchase ? (Data.CurrentRank > 0 ? ERogueUpgradeTreeNodeState::Unlocked : ERogueUpgradeTreeNodeState::Available)
					: ERogueUpgradeTreeNodeState::Unaffordable);
			Data.StateLabel = FText::FromString(!bPrerequisitesMet ? TEXT("前置未解锁")
				: (!bSaveWritable ? TEXT("存档不可写") : (Data.bCanPurchase ? (Data.CurrentRank > 0 ? TEXT("可升级") : TEXT("待解锁")) : TEXT("冰砖不足"))));
		}
		Data.Prerequisites = MissingPrerequisites.IsEmpty() ? FText::GetEmpty()
			: FText::FromString(TEXT("需要：") + FString::Join(MissingPrerequisites, TEXT("、")));
		if (Data.MaximumRank > 1)
		{
			FString Pips;
			for (int32 Rank = 0; Rank < Data.MaximumRank; ++Rank) Pips += Rank < Data.CurrentRank ? TEXT("● ") : TEXT("○ ");
			Data.Rank = FText::FromString(FString::Printf(TEXT("%s %d/%d"), *Pips, Data.CurrentRank, Data.MaximumRank));
		}
		Widget->SetViewData(Data);
		Widget->ForceLayoutPrepass();
		NodeSizes.Add(Node->NodeId, Widget->GetPreferredLayoutSize(NodeSize));
	}
	const FRogueUpgradeTreeLayout FullLayout = RogueUpgradeTreeLayout::Build(LayoutNodes, NodeSizes, NodeSize, NodeSpacing, CanvasPadding);
	int32& CurrentPage = bAttributeTree ? AttributePageIndex : WeaponPageIndex;
	const FRogueUpgradeTreePage Page = RogueUpgradeTreeLayout::Paginate(FullLayout, CurrentPage);
	CurrentPage = Page.PageIndex;
	PageCount = Page.PageCount;
	const FRogueUpgradeTreeLayout& Layout = Page.Layout;
	if (!Layout.InvalidPrerequisites.IsEmpty() && StatusText != nullptr)
	{
		StatusText->SetText(FText::FromString(TEXT("部分节点的前置不存在、不在当前分区或存在循环，请检查 DA_UpgradeTree。")));
	}
	NodePositions = Layout.Positions;
	NodeSizes = Layout.Sizes;
	for (int32 Index = NodeWidgets.Num() - 1; Index >= 0; --Index)
	{
		URogueUpgradeTreeNodeWidgetBase* Widget = NodeWidgets[Index];
		if (!NodePositions.Contains(Widget->ViewData.NodeId))
		{
			TreeCanvas->RemoveChild(Widget);
			NodeWidgets.RemoveAt(Index);
			continue;
		}
		if (UCanvasPanelSlot* NodeSlot = Cast<UCanvasPanelSlot>(Widget->Slot))
		{
			NodeSlot->SetPosition(NodePositions.FindRef(Widget->ViewData.NodeId));
			NodeSlot->SetSize(NodeSizes.FindRef(Widget->ViewData.NodeId));
		}
	}
	if (TreeContentSize != nullptr)
	{
		TreeContentSize->SetWidthOverride(FMath::Max(Layout.ContentSize.X, 1.0));
		TreeContentSize->SetHeightOverride(FMath::Max(Layout.ContentSize.Y, 1.0));
	}
	UpdatePageControls();
	OnTreeRefreshed(IceBricks, bAttributeTree);
}

void URogueUpgradeTreeWidgetBase::UpdatePageControls()
{
	const int32 Index = GetCurrentPageNumber() - 1;
	if (PreviousPageButton) PreviousPageButton->SetIsEnabled(Index > 0);
	if (NextPageButton) NextPageButton->SetIsEnabled(Index + 1 < PageCount);
	if (PageNumberText) PageNumberText->SetText(FText::FromString(FString::Printf(TEXT("%d / %d"), Index + 1, PageCount)));
}

void URogueUpgradeTreeWidgetBase::PreviousPage() { ChangePage(-1); }
void URogueUpgradeTreeWidgetBase::NextPage() { ChangePage(1); }

void URogueUpgradeTreeWidgetBase::ChangePage(int32 Delta)
{
	int32& CurrentPage = bAttributeTree ? AttributePageIndex : WeaponPageIndex;
	const int32 NewPage = FMath::Clamp(CurrentPage + Delta, 0, FMath::Max(0, PageCount - 1));
	if (NewPage == CurrentPage) { return; }
	CurrentPage = NewPage;
	if (StatusText) StatusText->SetText(FText::GetEmpty());
	RefreshTree();
	if (TreeScrollBox) TreeScrollBox->ScrollToStart();
	SetKeyboardFocus();
}

void URogueUpgradeTreeWidgetBase::ShowWeaponTree()
{
	bAttributeTree = false;
	if (StatusText != nullptr) StatusText->SetText(FText::GetEmpty());
	RefreshTree();
	if (TreeScrollBox != nullptr) TreeScrollBox->ScrollToStart();
}

void URogueUpgradeTreeWidgetBase::ShowAttributeTree()
{
	bAttributeTree = true;
	if (StatusText != nullptr) StatusText->SetText(FText::GetEmpty());
	RefreshTree();
	if (TreeScrollBox != nullptr) TreeScrollBox->ScrollToStart();
}

void URogueUpgradeTreeWidgetBase::HandleProgressionChanged()
{
	// Recompute save status after a retry commits; do not leave the old pending/error text visible.
	if (StatusText != nullptr) { StatusText->SetText(FText::GetEmpty()); }
	RefreshTree();
}

void URogueUpgradeTreeWidgetBase::PurchaseNode(FName NodeId)
{
	FText Message;
	URogueProgressionSubsystem* LocalProgression = ResolveProgression();
	const bool bSucceeded = LocalProgression != nullptr && LocalProgression->TryPurchaseNode(NodeId, Message);
	if (LocalProgression == nullptr) Message = FText::FromString(TEXT("本地玩家存档尚未就绪。"));
	if (bSucceeded) Message = FText::FromString(TEXT("升级成功，进度已保存在本机。"));
	if (StatusText != nullptr) StatusText->SetText(Message);
	RefreshTree();
	SetKeyboardFocus();
	OnPurchaseResult(NodeId, bSucceeded, Message);
}

void URogueUpgradeTreeWidgetBase::CloseTree()
{
	if (ARogueHUD* HUD = OwningRogueHUD.Get()) HUD->CloseUpgradeTree();
}

FReply URogueUpgradeTreeWidgetBase::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		CloseTree();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

int32 URogueUpgradeTreeWidgetBase::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId,
	const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const int32 BaseLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	if (TreeCanvas == nullptr || !TreeCanvas->IsVisible()) return BaseLayer;
	// Paint geometries use window coordinates. Cached/tick geometries include the
	// desktop window offset, which must never be mixed into NativePaint coordinates.
	const FRogueUpgradeTreePaintSpace PaintSpace = RogueUpgradeTreePaintSpace::Resolve(*TreeCanvas, TreeScrollBox, MyCullingRect);
	const FGeometry& CanvasGeometry = PaintSpace.CanvasGeometry;
	if (CanvasGeometry.GetLocalSize().IsNearlyZero()) return BaseLayer;
	OutDrawElements.PushClip(FSlateClippingZone(PaintSpace.Clip));
	const FPaintGeometry ConnectionGeometry = CanvasGeometry.ToPaintGeometry();
	for (const TPair<FName, TArray<FName>>& Entry : NodePrerequisites)
	{
		const FVector2D* Child = NodePositions.Find(Entry.Key);
		if (Child == nullptr) continue;
		for (const FName ParentId : Entry.Value)
		{
			const FVector2D* Parent = NodePositions.Find(ParentId);
			if (Parent == nullptr) continue;
			const bool bUpward = Child->Y <= Parent->Y;
			const FVector2D ParentSize = NodeSizes.FindRef(ParentId);
			const FVector2D ChildSize = NodeSizes.FindRef(Entry.Key);
			const FVector2D Start = *Parent + FVector2D(ParentSize.X * 0.5f, bUpward ? -6.0f : ParentSize.Y + 6.0f);
			const FVector2D End = *Child + FVector2D(ChildSize.X * 0.5f, bUpward ? ChildSize.Y + 8.0f : -8.0f);
			const float MiddleY = (Start.Y + End.Y) * 0.5f;
			const FLinearColor Color = (PurchasedNodes.Contains(ParentId) ? UnlockedConnectionColor : LockedConnectionColor)
				* InWidgetStyle.GetColorAndOpacityTint();
			TArray<FVector2D> Line;
			Line = { Start, FVector2D(Start.X, MiddleY), FVector2D(End.X, MiddleY), End };
			FSlateDrawElement::MakeLines(OutDrawElements, BaseLayer + 1, ConnectionGeometry, Line,
				ESlateDrawEffect::None, Color, true, ConnectionThickness);
			const float ArrowDirection = bUpward ? 1.0f : -1.0f;
			TArray<FVector2D> Arrow = { End + FVector2D(-6.0f, ArrowDirection * 8.0f), End,
				End + FVector2D(6.0f, ArrowDirection * 8.0f) };
			FSlateDrawElement::MakeLines(OutDrawElements, BaseLayer + 1, ConnectionGeometry, Arrow,
				ESlateDrawEffect::None, Color, true, ConnectionThickness);
		}
	}
	OutDrawElements.PopClip();
	return BaseLayer + 1;
}


bool URogueUpgradeTreeWidgetBase::GetNodeLayoutBounds(FName NodeId, FVector2D& Position, FVector2D& Size) const
{
	const FVector2D* FoundPosition = NodePositions.Find(NodeId);
	const FVector2D* FoundSize = NodeSizes.Find(NodeId);
	Position = FoundPosition != nullptr ? *FoundPosition : FVector2D::ZeroVector;
	Size = FoundSize != nullptr ? *FoundSize : FVector2D::ZeroVector;
	return FoundPosition != nullptr && FoundSize != nullptr;
}
