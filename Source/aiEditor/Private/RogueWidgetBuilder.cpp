#include "RogueWidgetBuilder.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Engine/FontFace.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/CoreStyle.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "WidgetBlueprint.h"

namespace
{
	UCanvasPanel* ResetCanvas(UWidgetBlueprint* Blueprint)
	{
		UWidgetTree* Tree = Blueprint->WidgetTree;
		Tree->Modify();

		UCanvasPanel* Root = Cast<UCanvasPanel>(Tree->RootWidget);
		if (Root == nullptr)
		{
			Root = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
			Tree->RootWidget = Root;
		}

		Root->ClearChildren();
		return Root;
	}

	UCanvasPanelSlot* AddToCanvas(UCanvasPanel* Root, UWidget* Widget, const FAnchors& Anchors, const FVector2D& Alignment, const FVector2D& Position, const FVector2D& Size)
	{
		UCanvasPanelSlot* Slot = Root->AddChildToCanvas(Widget);
		if (Slot == nullptr)
		{
			return nullptr;
		}

		Slot->SetAnchors(Anchors);
		Slot->SetAlignment(Alignment);
		Slot->SetAutoSize(false);
		Slot->SetPosition(Position);
		Slot->SetSize(Size);
		return Slot;
	}

	UProgressBar* MakeBar(UWidgetBlueprint* Blueprint, UCanvasPanel* Root, FName Name, const FLinearColor& Color, const FVector2D& Position)
	{
		UProgressBar* Bar = Blueprint->WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), Name);
		Bar->SetFillColorAndOpacity(Color);
		Bar->SetPercent(1.0f);

		// 锚定左下角：Position 是相对锚点的偏移，Y 为负表示向上。
		AddToCanvas(Root, Bar, FAnchors(0.0f, 1.0f, 0.0f, 1.0f), FVector2D(0.0f, 1.0f), Position, FVector2D(320.0f, 18.0f));
		return Bar;
	}

	UFontFace* GetChineseFontFace()
	{
		// Roboto 不含 CJK 字形，直接用引擎自带的 DroidSansFallback。
		static UFontFace* FontFace = LoadObject<UFontFace>(nullptr,
			TEXT("/Engine/EngineFonts/Faces/DroidSansFallback.DroidSansFallback"));
		return FontFace;
	}

	UTextBlock* MakeText(
		UWidgetBlueprint* Blueprint,
		UCanvasPanel* Root,
		FName Name,
		const FString& DefaultText,
		const FLinearColor& Color,
		int32 FontSize,
		const FAnchors& Anchors,
		const FVector2D& Alignment,
		const FVector2D& Position,
		const FVector2D& Size)
	{
		UTextBlock* Text = Blueprint->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		Text->SetText(FText::FromString(DefaultText));
		Text->SetColorAndOpacity(FSlateColor(Color));
		if (UFontFace* FontFace = GetChineseFontFace())
		{
			Text->SetFont(FSlateFontInfo(FontFace, static_cast<float>(FontSize)));
		}
		else
		{
			Text->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", FontSize));
		}
		Text->SetJustification(ETextJustify::Left);
		Text->SetShadowOffset(FVector2D(1.0f, 1.0f));
		Text->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.65f));
		AddToCanvas(Root, Text, Anchors, Alignment, Position, Size);
		return Text;
	}

	void Finish(UWidgetBlueprint* Blueprint)
	{
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
		FKismetEditorUtilities::CompileBlueprint(Blueprint);
		Blueprint->GetOutermost()->MarkPackageDirty();
	}
}

bool URogueWidgetBuilder::BuildPlayerVitals(UWidgetBlueprint* Blueprint)
{
	if (Blueprint == nullptr || Blueprint->WidgetTree == nullptr)
	{
		return false;
	}

	UCanvasPanel* Root = ResetCanvas(Blueprint);

	// 左上角：波数 + 金币。
	MakeText(Blueprint, Root, TEXT("WaveText"), TEXT("第 0 波"), FLinearColor::White, 26,
		FAnchors(0.0f, 0.0f), FVector2D(0.0f, 0.0f), FVector2D(36.0f, 24.0f), FVector2D(280.0f, 40.0f));
	MakeText(Blueprint, Root, TEXT("GoldText"), TEXT("金币 0"), FLinearColor(1.0f, 0.84f, 0.34f), 20,
		FAnchors(0.0f, 0.0f), FVector2D(0.0f, 0.0f), FVector2D(36.0f, 64.0f), FVector2D(280.0f, 32.0f));

	// 左下角：生命 / 护甲 / 经验条 + 数值。
	MakeBar(Blueprint, Root, TEXT("HealthBar"), FLinearColor(0.85f, 0.15f, 0.15f, 0.95f), FVector2D(40.0f, -92.0f));
	MakeText(Blueprint, Root, TEXT("HealthText"), TEXT("生命 0 / 0"), FLinearColor::White, 18,
		FAnchors(0.0f, 1.0f), FVector2D(0.0f, 1.0f), FVector2D(40.0f, -116.0f), FVector2D(320.0f, 24.0f));

	MakeBar(Blueprint, Root, TEXT("ArmorBar"), FLinearColor(0.58f, 0.60f, 0.64f, 0.96f), FVector2D(40.0f, -152.0f));
	MakeText(Blueprint, Root, TEXT("ArmorText"), TEXT("护甲 0 / 0"), FLinearColor(0.78f, 0.80f, 0.84f), 18,
		FAnchors(0.0f, 1.0f), FVector2D(0.0f, 1.0f), FVector2D(40.0f, -176.0f), FVector2D(320.0f, 24.0f));

	MakeBar(Blueprint, Root, TEXT("ExperienceBar"), FLinearColor(0.2f, 0.6f, 1.0f, 0.95f), FVector2D(40.0f, -212.0f));
	MakeText(Blueprint, Root, TEXT("ExperienceText"), TEXT("经验 0 / 0"), FLinearColor(0.4f, 0.8f, 1.0f), 18,
		FAnchors(0.0f, 1.0f), FVector2D(0.0f, 1.0f), FVector2D(40.0f, -236.0f), FVector2D(320.0f, 24.0f));

	Finish(Blueprint);
	return true;
}

bool URogueWidgetBuilder::BuildEnemyHealthBar(UWidgetBlueprint* Blueprint)
{
	if (Blueprint == nullptr || Blueprint->WidgetTree == nullptr)
	{
		return false;
	}

	UCanvasPanel* Root = ResetCanvas(Blueprint);

	// 背景铺满 WBP 的 viewport 尺寸（由 HUD 的 BarSize 决定）。
	UBorder* Background = Blueprint->WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Background"));
	Background->SetBrushColor(FLinearColor(0.04f, 0.04f, 0.05f, 0.78f));
	if (UCanvasPanelSlot* BackgroundSlot = Root->AddChildToCanvas(Background))
	{
		BackgroundSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		BackgroundSlot->SetOffsets(FMargin(0.0f));
	}

	UProgressBar* Fill = Blueprint->WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("FillBar"));
	Fill->SetFillColorAndOpacity(FLinearColor(0.95f, 0.15f, 0.20f, 0.95f));
	Fill->SetPercent(1.0f);
	if (UCanvasPanelSlot* FillSlot = Root->AddChildToCanvas(Fill))
	{
		FillSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		// 留 2px 边，让背景露出一点描边。
		FillSlot->SetOffsets(FMargin(2.0f));
	}

	Finish(Blueprint);
	return true;
}

FString URogueWidgetBuilder::DescribeTextFonts(UWidgetBlueprint* Blueprint)
{
	if (Blueprint == nullptr || Blueprint->WidgetTree == nullptr)
	{
		return TEXT("<no widget tree>");
	}

	TArray<UWidget*> Widgets;
	Blueprint->WidgetTree->GetAllWidgets(Widgets);

	FString Result;
	for (const UWidget* Widget : Widgets)
	{
		const UTextBlock* Text = Cast<UTextBlock>(Widget);
		if (Text == nullptr)
		{
			continue;
		}

		const FSlateFontInfo Font = Text->GetFont();
		Result += FString::Printf(TEXT("%s -> %s (%.0f)\n"),
			*GetNameSafe(Text),
			Font.FontObject != nullptr ? *Font.FontObject->GetPathName() : TEXT("<none>"),
			Font.Size);
	}
	return Result;
}

FString URogueWidgetBuilder::DescribeWidgetTree(UWidgetBlueprint* Blueprint)
{
	if (Blueprint == nullptr || Blueprint->WidgetTree == nullptr)
	{
		return TEXT("<no widget tree>");
	}

	TArray<UWidget*> Widgets;
	Blueprint->WidgetTree->GetAllWidgets(Widgets);

	FString Result;
	for (const UWidget* Widget : Widgets)
	{
		Result += FString::Printf(TEXT("%s:%s\n"), *GetNameSafe(Widget->GetClass()), *GetNameSafe(Widget));
	}
	return Result;
}
