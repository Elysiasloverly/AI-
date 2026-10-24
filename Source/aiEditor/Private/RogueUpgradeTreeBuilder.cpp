#include "RogueUpgradeTreeBuilder.h"

#include "RogueWidgetBuilder.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/PanelWidget.h"
#include "Components/ScaleBox.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/Font.h"
#include "Fonts/SlateFontInfo.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "WidgetBlueprint.h"

namespace
{
	const FLinearColor Ink(0.88f, 0.94f, 1.0f, 1.0f);
	const FLinearColor SubtleInk(0.58f, 0.71f, 0.82f, 1.0f);
	const FLinearColor Accent(0.37f, 0.84f, 0.89f, 1.0f);

	UCanvasPanel* NewCanvas(UWidgetBlueprint* Blueprint)
	{
		Blueprint->Modify();
		Blueprint->WidgetTree->Modify();
		UCanvasPanel* Root = Cast<UCanvasPanel>(Blueprint->WidgetTree->RootWidget);
		if (Root == nullptr)
		{
			Root = Blueprint->WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
			Blueprint->WidgetTree->RootWidget = Root;
		}
		Root->ClearChildren();
		return Root;
	}

	void Place(UCanvasPanel* Canvas, UWidget* Widget, const FVector2D& Position, const FVector2D& Size,
		const FAnchors& Anchors = FAnchors(0.0f, 0.0f), const FVector2D& Alignment = FVector2D::ZeroVector)
	{
		UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(Widget);
		Slot->SetAnchors(Anchors);
		Slot->SetAlignment(Alignment);
		Slot->SetPosition(Position);
		Slot->SetSize(Size);
	}

	void Stretch(UCanvasPanel* Canvas, UWidget* Widget, const FAnchors& Anchors = FAnchors(0.0f, 0.0f, 1.0f, 1.0f))
	{
		UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(Widget);
		Slot->SetAnchors(Anchors);
		Slot->SetOffsets(FMargin(0.0f));
	}

	UTextBlock* Text(UWidgetBlueprint* Blueprint, FName Name, const TCHAR* Value, int32 Size, const FLinearColor& Color = Ink)
	{
		UTextBlock* Result = Blueprint->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		Result->SetText(FText::FromString(Value));
		Result->SetColorAndOpacity(Color);
		UFont* Font = LoadObject<UFont>(nullptr, TEXT("/Game/UMG/Fonts/F_DroidMono_CJK.F_DroidMono_CJK"));
		if (Font == nullptr) Font = URogueWidgetBuilder::GetOrCreateProjectFont();
		Result->SetFont(FSlateFontInfo(Font, static_cast<float>(Size)));
		Result->SetAutoWrapText(true);
		return Result;
	}

	UButton* Button(UWidgetBlueprint* Blueprint, FName Name, FName LabelName, const TCHAR* Caption, int32 FontSize)
	{
		UButton* Result = Blueprint->WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		Result->SetBackgroundColor(FLinearColor(0.13f, 0.24f, 0.32f, 1.0f));
		UTextBlock* Label = Text(Blueprint, LabelName, Caption, FontSize);
		Label->SetJustification(ETextJustify::Center);
		Label->SetAutoWrapText(false);
		Result->AddChild(Label);
		return Result;
	}

	void Finish(UWidgetBlueprint* Blueprint)
	{
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
		FKismetEditorUtilities::CompileBlueprint(Blueprint);
		Blueprint->MarkPackageDirty();
	}

	UTextBlock* FindFirstText(UWidget* Widget)
	{
		if (UTextBlock* Label = Cast<UTextBlock>(Widget)) return Label;
		if (UPanelWidget* Panel = Cast<UPanelWidget>(Widget))
		{
			for (int32 Index = 0; Index < Panel->GetChildrenCount(); ++Index)
			{
				if (UTextBlock* Label = FindFirstText(Panel->GetChildAt(Index))) return Label;
			}
		}
		return nullptr;
	}
}

bool URogueUpgradeTreeBuilder::BuildUpgradeTree(UWidgetBlueprint* Blueprint)
{
	if (Blueprint == nullptr || Blueprint->WidgetTree == nullptr) return false;
	UCanvasPanel* Root = NewCanvas(Blueprint);
	UBorder* Background = Blueprint->WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Background"));
	Background->SetBrushColor(FLinearColor(0.025f, 0.035f, 0.065f, 1.0f));
	Stretch(Root, Background);

	UTextBlock* Title = Text(Blueprint, TEXT("PageTitle"), TEXT("永久升级树"), 32, Ink);
	Place(Root, Title, FVector2D(54.0f, 26.0f), FVector2D(600.0f, 54.0f));
	UTextBlock* Currency = Text(Blueprint, TEXT("IceBricksText"), TEXT("冰砖  0"), 26, Accent);
	Currency->SetJustification(ETextJustify::Right);
	Place(Root, Currency, FVector2D(-54.0f, 28.0f), FVector2D(330.0f, 52.0f), FAnchors(1.0f, 0.0f), FVector2D(1.0f, 0.0f));
	UTextBlock* Subtitle = Text(Blueprint, TEXT("PageSubtitle"), TEXT("每击败一位 Boss，本局每位玩家获得 1 冰砖。解锁进度保存在自己的本地存档。"), 15, SubtleInk);
	Place(Root, Subtitle, FVector2D(56.0f, 82.0f), FVector2D(1350.0f, 40.0f));
	Place(Root, Button(Blueprint, TEXT("WeaponsTabButton"), TEXT("WeaponsTabLabel"), TEXT("武器与专属卡"), 18), FVector2D(56.0f, 131.0f), FVector2D(230.0f, 46.0f));
	Place(Root, Button(Blueprint, TEXT("AttributesTabButton"), TEXT("AttributesTabLabel"), TEXT("基础属性"), 18), FVector2D(300.0f, 131.0f), FVector2D(200.0f, 46.0f));
	Place(Root, Button(Blueprint, TEXT("CloseButton"), TEXT("CloseLabel"), TEXT("返回主菜单  Esc"), 16), FVector2D(-54.0f, 131.0f), FVector2D(230.0f, 46.0f), FAnchors(1.0f, 0.0f), FVector2D(1.0f, 0.0f));
	UTextBlock* Section = Text(Blueprint, TEXT("SectionText"), TEXT("武器与专属卡 · 沿箭头逐步解锁"), 15, Accent);
	Place(Root, Section, FVector2D(56.0f, 191.0f), FVector2D(1400.0f, 32.0f));

	UBorder* TreeBackground = Blueprint->WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("TreeBackground"));
	TreeBackground->SetBrushColor(FLinearColor(0.045f, 0.06f, 0.10f, 1.0f));
	Stretch(Root, TreeBackground, FAnchors(0.027f, 0.23f, 0.973f, 0.89f));
	UScrollBox* Scroll = Blueprint->WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("TreeScrollBox"));
	Scroll->SetConsumeMouseWheel(EConsumeMouseWheel::Always);
	Scroll->SetAllowOverscroll(false);
	Stretch(Root, Scroll, FAnchors(0.031f, 0.235f, 0.969f, 0.885f));
	UScaleBox* Scale = Blueprint->WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass(), TEXT("TreeScaleBox"));
	Scale->SetStretch(EStretch::ScaleToFitX);
	Scale->SetStretchDirection(EStretchDirection::DownOnly);
	Scroll->AddChild(Scale);
	USizeBox* ContentSize = Blueprint->WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("TreeContentSize"));
	ContentSize->SetWidthOverride(1980.0f);
	ContentSize->SetHeightOverride(1260.0f);
	Scale->AddChild(ContentSize);
	UCanvasPanel* Canvas = Blueprint->WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("TreeCanvas"));
	ContentSize->AddChild(Canvas);
	UTextBlock* Status = Text(Blueprint, TEXT("StatusText"), TEXT(""), 16, Accent);
	Place(Root, Status, FVector2D(56.0f, -78.0f), FVector2D(1500.0f, 32.0f), FAnchors(0.0f, 1.0f));
	UTextBlock* Footer = Text(Blueprint, TEXT("FooterText"), TEXT("御札与其数量卡初始开放。护甲需要先解锁；基础属性每项最多升级 5 次。滚轮可查看上方分支。"), 13, SubtleInk);
	Place(Root, Footer, FVector2D(56.0f, -44.0f), FVector2D(1550.0f, 30.0f), FAnchors(0.0f, 1.0f));
	Finish(Blueprint);
	return true;
}

bool URogueUpgradeTreeBuilder::BuildUpgradeTreeNode(UWidgetBlueprint* Blueprint)
{
	if (Blueprint == nullptr || Blueprint->WidgetTree == nullptr) return false;
	UCanvasPanel* Root = NewCanvas(Blueprint);
	UBorder* State = Blueprint->WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("StateBorder"));
	State->SetBrushColor(FLinearColor(0.10f, 0.12f, 0.19f, 1.0f));
	State->SetPadding(FMargin(0.0f));
	Stretch(Root, State);
	UCanvasPanel* Card = Blueprint->WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("CardCanvas"));
	State->AddChild(Card);
	Place(Card, Text(Blueprint, TEXT("TitleText"), TEXT("武器 / 专属升级"), 17), FVector2D(14.0f, 10.0f), FVector2D(260.0f, 28.0f));
	Place(Card, Text(Blueprint, TEXT("DescriptionText"), TEXT("解锁后加入自己的升级卡池。"), 12, SubtleInk), FVector2D(14.0f, 42.0f), FVector2D(260.0f, 48.0f));
	Place(Card, Text(Blueprint, TEXT("PrerequisiteText"), TEXT("需要：前置节点"), 10, SubtleInk), FVector2D(14.0f, 94.0f), FVector2D(260.0f, 28.0f));
	Place(Card, Text(Blueprint, TEXT("StateText"), TEXT("前置未解锁"), 12, Accent), FVector2D(14.0f, 126.0f), FVector2D(260.0f, 20.0f));
	Place(Card, Text(Blueprint, TEXT("RankText"), TEXT(""), 11, Accent), FVector2D(14.0f, 148.0f), FVector2D(260.0f, 18.0f));
	Place(Card, Text(Blueprint, TEXT("CostText"), TEXT("5 冰砖 / 次"), 12), FVector2D(14.0f, 174.0f), FVector2D(175.0f, 20.0f));
	Place(Card, Button(Blueprint, TEXT("PurchaseButton"), TEXT("PurchaseLabel"), TEXT("解锁"), 12), FVector2D(196.0f, 164.0f), FVector2D(78.0f, 30.0f));
	Finish(Blueprint);
	return true;
}

bool URogueUpgradeTreeBuilder::AddPaginationControls(UWidgetBlueprint* Blueprint)
{
	if (!Blueprint || !Blueprint->WidgetTree) { return false; }
	UCanvasPanel* Root = Cast<UCanvasPanel>(Blueprint->WidgetTree->RootWidget);
	if (!Root) { return false; }
	Blueprint->Modify();
	Blueprint->WidgetTree->Modify();
	if (UScaleBox* Scale = Cast<UScaleBox>(Blueprint->WidgetTree->FindWidget(TEXT("TreeScaleBox"))))
	{
		Scale->Modify();
		Scale->SetStretch(EStretch::ScaleToFitX);
		Scale->SetStretchDirection(EStretchDirection::Both);
	}
	if (!Blueprint->WidgetTree->FindWidget(TEXT("PaginationCanvas")))
	{
		UCanvasPanel* Pager = Blueprint->WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("PaginationCanvas"));
		Pager->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		Place(Root, Pager, FVector2D(-54, -18), FVector2D(400, 46), FAnchors(1, 1), FVector2D(1, 1));
		Place(Pager, Button(Blueprint, TEXT("PreviousPageButton"), TEXT("PreviousPageLabel"), TEXT("上一页"), 18), FVector2D(0, 0), FVector2D(124, 46));
		UTextBlock* PageNumber = Text(Blueprint, TEXT("PageNumberText"), TEXT("1 / 1"), 18, Accent);
		PageNumber->SetJustification(ETextJustify::Center);
		PageNumber->SetAutoWrapText(false);
		Place(Pager, PageNumber, FVector2D(136, 10), FVector2D(128, 30));
		Place(Pager, Button(Blueprint, TEXT("NextPageButton"), TEXT("NextPageLabel"), TEXT("下一页"), 18), FVector2D(276, 0), FVector2D(124, 46));
	}
	if (UTextBlock* Footer = Cast<UTextBlock>(Blueprint->WidgetTree->FindWidget(TEXT("FooterText"))))
	{
		Footer->Modify();
		Footer->SetAutoWrapText(true);
		if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(Footer->Slot))
		{
			Slot->Modify();
			Slot->SetAnchors(FAnchors(0, 1, 1, 1));
			Slot->SetOffsets(FMargin(56, -54, 500, 42));
		}
	}
	Finish(Blueprint);
	return true;
}

bool URogueUpgradeTreeBuilder::UpdateMainMenuEntry(UWidgetBlueprint* Blueprint)
{
	if (Blueprint == nullptr || Blueprint->WidgetTree == nullptr) return false;
	UWidget* Entry = Blueprint->WidgetTree->FindWidget(TEXT("SkillTreeButton"));
	UTextBlock* Label = FindFirstText(Entry);
	if (Label == nullptr) return false;
	Blueprint->Modify();
	Label->Modify();
	Label->SetText(FText::FromString(TEXT("升级树")));
	Finish(Blueprint);
	return true;
}
