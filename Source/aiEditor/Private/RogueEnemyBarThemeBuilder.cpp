#include "RogueEnemyBarThemeBuilder.h"

#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ProgressBar.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "UI/Widgets/RogueEnemyHealthBarWidgetBase.h"
#include "UObject/UnrealType.h"
#include "WidgetBlueprint.h"

namespace
{
	void StretchEnemyBar(UCanvasPanel* Canvas, UWidget* Widget, float Inset)
	{
		UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(Widget);
		Slot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
		Slot->SetOffsets(FMargin(Inset));
	}
	FSlateBrush CapsuleBrush(const FLinearColor& Color)
	{
		FSlateBrush Brush = FSlateRoundedBoxBrush(Color, 0.0f);
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::HalfHeightRadius;
		return Brush;
	}
	UProgressBar* MakeEnemyBar(UWidgetBlueprint* BP, UCanvasPanel* Canvas, FName Name, const FLinearColor& Color)
	{
		UProgressBar* Bar = BP->WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), Name);
		FProgressBarStyle Style;
		Style.BackgroundImage = CapsuleBrush(FLinearColor::Transparent);
		Style.FillImage = CapsuleBrush(FLinearColor::White);
		Style.MarqueeImage = Style.FillImage;
		Bar->SetWidgetStyle(Style);
		Bar->SetFillColorAndOpacity(Color);
		Bar->SetBorderPadding(FVector2D::ZeroVector);
		// Scale 让填充段自身保持胶囊端头；Mask 会把内部下降的端头裁成直角。
		Bar->SetBarFillStyle(EProgressBarFillStyle::Scale);
		Bar->SetPercent(1.0f);
		Bar->SetVisibility(ESlateVisibility::HitTestInvisible);
		StretchEnemyBar(Canvas, Bar, 1.5f);
		return Bar;
	}
}

bool URogueEnemyBarThemeBuilder::BuildRefinedEnemyBar(UWidgetBlueprint* BP)
{
	if (!BP || !BP->WidgetTree || BP->GetName() != TEXT("WBP_EnemyHealthBar")
		|| !BP->ParentClass || !BP->ParentClass->IsChildOf(URogueEnemyHealthBarWidgetBase::StaticClass()))
	{
		return false;
	}
	BP->Modify();
	BP->WidgetTree->Modify();
	UCanvasPanel* Root = Cast<UCanvasPanel>(BP->WidgetTree->RootWidget);
	if (!Root)
	{
		Root = BP->WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
		BP->WidgetTree->RootWidget = Root;
	}
	Root->ClearChildren();
	UBorder* Frame = BP->WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Background"));
	FSlateBrush FrameBrush = CapsuleBrush(FLinearColor(0.014f, 0.018f, 0.027f, 0.94f));
	FrameBrush.OutlineSettings.Color = FLinearColor(0.31f, 0.34f, 0.39f, 0.75f);
	FrameBrush.OutlineSettings.Width = 0.75f;
	Frame->SetBrush(FrameBrush);
	Frame->SetBrushColor(FLinearColor::White);
	Frame->SetVisibility(ESlateVisibility::HitTestInvisible);
	StretchEnemyBar(Root, Frame, 0.0f);
	MakeEnemyBar(BP, Root, TEXT("DamageBar"), FLinearColor(0.94f, 0.63f, 0.31f, 1.0f));
	MakeEnemyBar(BP, Root, TEXT("FillBar"), FLinearColor(0.82f, 0.105f, 0.17f, 1.0f));

	// 旧的百分比绑定会在 Slate 刷新时覆盖原生插值，改由基类接收真实数据。
	BP->Bindings.RemoveAll([](const FDelegateEditorBinding& Binding)
	{
		return (Binding.ObjectName == TEXT("FillBar") || Binding.ObjectName == TEXT("DamageBar"))
			&& (Binding.PropertyName == TEXT("Percent") || Binding.PropertyName == TEXT("FillColorAndOpacity"));
	});
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
	FKismetEditorUtilities::CompileBlueprint(BP);
	if (BP->Status == BS_Error) { return false; }
	if (FStructProperty* SizeProperty = FindFProperty<FStructProperty>(BP->GeneratedClass, TEXT("BarSize")))
	{
		*SizeProperty->ContainerPtrToValuePtr<FVector2D>(BP->GeneratedClass->GetDefaultObject()) = FVector2D(96.0f, 10.0f);
	}
	BP->MarkPackageDirty();
	return true;
}
