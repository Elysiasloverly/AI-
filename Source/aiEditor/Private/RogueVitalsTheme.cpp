#include "RogueUIThemeBuilder.h"

#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "WidgetBlueprint.h"

namespace RogueVitalsTheme
{
    template <typename T>
    T* Find(UWidgetBlueprint* BP, FName Name)
    {
        return Cast<T>(BP->WidgetTree->FindWidget(Name));
    }

    template <typename T>
    T* ReuseOrCreate(UWidgetBlueprint* BP, FName Name)
    {
        if (T* Existing = FindObject<T>(BP->WidgetTree, *Name.ToString()))
        {
            Existing->Modify();
            Existing->RemoveFromParent();
            return Existing;
        }
        return BP->WidgetTree->ConstructWidget<T>(T::StaticClass(), Name);
    }

    void Place(UCanvasPanel* Parent, UWidget* Child, FVector2D Position, FVector2D Size, int32 ZOrder = 0)
    {
        Child->Modify();
        Child->RemoveFromParent();
        UCanvasPanelSlot* Slot = Parent->AddChildToCanvas(Child);
        Slot->SetPosition(Position);
        Slot->SetSize(Size);
        Slot->SetZOrder(ZOrder);
    }

    void RoundedFill(UProgressBar* Bar)
    {
        // Scale keeps a rounded moving end; Mask would crop it to a straight edge.
        // All designer-authored brushes, textures and colors remain unchanged.
        Bar->SetBarFillStyle(EProgressBarFillStyle::Scale);
        Bar->SetVisibility(ESlateVisibility::HitTestInvisible);
    }
}

bool URogueUIThemeBuilder::AddDashCooldownToVitals(UWidgetBlueprint* BP)
{
    using namespace RogueVitalsTheme;
    if (!BP || !BP->WidgetTree) return false;
    // Append to the saved layout; never rebuild the user's health/armor widgets.
    UVerticalBox* Body = Find<UVerticalBox>(BP, TEXT("VitalsRows"));
    UProgressBar* ReferenceBar = Find<UProgressBar>(BP, TEXT("ExperienceBar"));
    UTextBlock* ReferenceText = Find<UTextBlock>(BP, TEXT("ExperienceText"));
    if (!Body || !ReferenceBar || !ReferenceText) return false;
    if (Find<UWidget>(BP, TEXT("DashCooldownRow"))) return true;
    BP->Modify(); BP->WidgetTree->Modify(); Body->Modify();
    USizeBox* Row = BP->WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("DashCooldownRow"));
    Row->SetHeightOverride(48.f);
    Row->SetVisibility(ESlateVisibility::HitTestInvisible);
    UVerticalBoxSlot* RowSlot = Body->AddChildToVerticalBox(Row);
    RowSlot->SetPadding(FMargin(0.f, 10.f, 0.f, 0.f));
    RowSlot->SetHorizontalAlignment(HAlign_Fill);
    UCanvasPanel* Canvas = BP->WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("DashCooldownCanvas"));
    Row->AddChild(Canvas);
    auto AddLabel = [&](FName Name, const TCHAR* Text, bool bRight)
    {
        UTextBlock* Label = BP->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
        FSlateFontInfo Font = ReferenceText->GetFont(); Font.Size = 17;
        Label->SetFont(Font); Label->SetText(FText::FromString(Text));
        Label->SetColorAndOpacity(bRight ? FSlateColor(FLinearColor(.38f,.82f,.96f,1)) : ReferenceText->GetColorAndOpacity());
        Label->SetJustification(bRight ? ETextJustify::Right : ETextJustify::Left);
        UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(Label);
        Slot->SetAnchors(FAnchors(0,0,1,0));
        Slot->SetOffsets(FMargin(bRight ? 100.f : 0.f,0.f,bRight ? 0.f : 240.f,25.f));
    };
    AddLabel(TEXT("DashLabel"), TEXT("冲刺"), false);
    AddLabel(TEXT("DashCooldownText"), TEXT("冲刺就绪"), true);
    UProgressBar* Bar = BP->WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("DashCooldownBar"));
    Bar->SetWidgetStyle(ReferenceBar->GetWidgetStyle());
    Bar->SetFillColorAndOpacity(FLinearColor(.38f,.82f,.96f,1));
    Bar->SetBarFillType(EProgressBarFillType::LeftToRight);
    Bar->SetBarFillStyle(EProgressBarFillStyle::Scale);
    Bar->SetPercent(1.f);
    UCanvasPanelSlot* BarSlot = Canvas->AddChildToCanvas(Bar);
    BarSlot->SetAnchors(FAnchors(0,0,1,0));
    BarSlot->SetOffsets(FMargin(0,31.f,0,10.f));
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
    FKismetEditorUtilities::CompileBlueprint(BP);
    BP->MarkPackageDirty();
    return BP->Status != BS_Error;
}

bool URogueUIThemeBuilder::BuildAnimatedVitals(UWidgetBlueprint* BP)
{
    using namespace RogueVitalsTheme;
    if (!BP || !BP->WidgetTree || BP->GetName() != TEXT("WBP_PlayerVitals"))
    {
        return false;
    }

    UCanvasPanel* Root = Cast<UCanvasPanel>(BP->WidgetTree->RootWidget);
    UWidget* OldPanel = BP->WidgetTree->FindWidget(TEXT("VitalsPanel"));
    UBorder* Frame = Find<UBorder>(BP, TEXT("VitalsFrame"));
    UTextBlock* HealthText = Find<UTextBlock>(BP, TEXT("HealthText"));
    UTextBlock* ArmorText = Find<UTextBlock>(BP, TEXT("ArmorText"));
    UTextBlock* ExperienceText = Find<UTextBlock>(BP, TEXT("ExperienceText"));
    UProgressBar* Health = Find<UProgressBar>(BP, TEXT("HealthBar"));
    UProgressBar* HealthTrail = Find<UProgressBar>(BP, TEXT("HealthDamageBar"));
    UProgressBar* Armor = Find<UProgressBar>(BP, TEXT("ArmorBar"));
    UProgressBar* ArmorTrail = Find<UProgressBar>(BP, TEXT("ArmorDamageBar"));
    UProgressBar* Experience = Find<UProgressBar>(BP, TEXT("ExperienceBar"));
    if (!Root || !OldPanel || !Frame || !HealthText || !ArmorText || !ExperienceText
        || !Health || !HealthTrail || !Armor || !ArmorTrail || !Experience)
    {
        return false;
    }

    BP->Modify();
    BP->WidgetTree->Modify();
    Root->Modify();

    // Reparent the existing presentation widgets rather than regenerate them.
    // Their names, font choices, paper brush, rounded corners and colors survive.
    UWidget* Preserved[] = {Frame, HealthText, ArmorText, ExperienceText, Health,
        HealthTrail, Armor, ArmorTrail, Experience};
    for (UWidget* Widget : Preserved)
    {
        Widget->Modify();
        Widget->RemoveFromParent();
    }
    OldPanel->Modify();
    OldPanel->RemoveFromParent();
    USizeBox* Panel = Cast<USizeBox>(OldPanel);
    if (!Panel)
    {
        // The previous layout used a CanvasPanel with this binding name.
        OldPanel->Rename(*MakeUniqueObjectName(BP->WidgetTree, OldPanel->GetClass(), TEXT("LegacyVitalsPanel")).ToString(),
            BP->WidgetTree, REN_DontCreateRedirectors | REN_NonTransactional);
        Panel = BP->WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("VitalsPanel"));
    }
    Panel->ClearChildren();
    Panel->SetWidthOverride(414.f);
    Panel->ClearHeightOverride();
    Panel->SetRenderTransformPivot(FVector2D(.5f, 1.f));
    UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(Panel);
    PanelSlot->SetAnchors(FAnchors(0.f, 1.f));
    PanelSlot->SetAlignment(FVector2D(0.f, 1.f));
    PanelSlot->SetPosition(FVector2D(28.f, -28.f));
    PanelSlot->SetAutoSize(true);

    Frame->ClearChildren();
    Frame->SetPadding(FMargin(24.f, 20.f));
    Frame->SetHorizontalAlignment(HAlign_Fill);
    Frame->SetVerticalAlignment(VAlign_Fill);
    Panel->AddChild(Frame);
    UVerticalBox* Body = ReuseOrCreate<UVerticalBox>(BP, TEXT("VitalsRows"));
    Body->ClearChildren();
    Frame->AddChild(Body);

    auto MakeRow = [BP, Body](FName RowName, FName CanvasName, float Height, float BottomGap)
    {
        USizeBox* Row = ReuseOrCreate<USizeBox>(BP, RowName);
        Row->ClearChildren();
        Row->SetWidthOverride(366.f);
        Row->SetHeightOverride(Height);
        Row->SetRenderTransformPivot(FVector2D(.5f, .5f));
        Row->SetVisibility(ESlateVisibility::HitTestInvisible);
        UVerticalBoxSlot* RowSlot = Body->AddChildToVerticalBox(Row);
        RowSlot->SetHorizontalAlignment(HAlign_Fill);
        RowSlot->SetPadding(FMargin(0.f, 0.f, 0.f, BottomGap));
        UCanvasPanel* Canvas = ReuseOrCreate<UCanvasPanel>(BP, CanvasName);
        Canvas->ClearChildren();
        Row->AddChild(Canvas);
        return Canvas;
    };

    UCanvasPanel* HealthCanvas = MakeRow(TEXT("HealthRow"), TEXT("HealthRowCanvas"), 64.f, 8.f);
    UCanvasPanel* ArmorCanvas = MakeRow(TEXT("ArmorRow"), TEXT("ArmorRowCanvas"), 64.f, 8.f);
    UCanvasPanel* ExperienceCanvas = MakeRow(TEXT("ExperienceRow"), TEXT("ExperienceRowCanvas"), 50.f, 0.f);

    auto AddFeedback = [BP](UCanvasPanel* Canvas, FName Name, FLinearColor Color, float BarY, float BarHeight)
    {
        UBorder* Feedback = ReuseOrCreate<UBorder>(BP, Name);
        Feedback->ClearChildren();
        const FLinearColor Fill(Color.R, Color.G, Color.B, .07f);
        Feedback->SetBrush(FSlateRoundedBoxBrush(Fill, (BarHeight + 8.f) * .5f, Color, 1.2f));
        Feedback->SetBrushColor(FLinearColor::White);
        Feedback->SetRenderOpacity(0.f);
        Feedback->SetVisibility(ESlateVisibility::HitTestInvisible);
        Place(Canvas, Feedback, FVector2D(-4.f, BarY - 4.f), FVector2D(374.f, BarHeight + 8.f));
    };
    AddFeedback(HealthCanvas, TEXT("HealthFeedback"), FLinearColor(1.f, .61f, .57f, .9f), 33.f, 22.f);
    AddFeedback(ArmorCanvas, TEXT("ArmorFeedback"), FLinearColor(.67f, .94f, 1.f, .95f), 33.f, 18.f);
    AddFeedback(ExperienceCanvas, TEXT("ExperienceFeedback"), FLinearColor(1.f, .85f, .51f, .9f), 31.f, 9.f);

    auto AddChangeText = [BP](UCanvasPanel* Canvas, FName Name, UTextBlock* Reference, float Width, float FontSize, FLinearColor Color)
    {
        UTextBlock* Label = ReuseOrCreate<UTextBlock>(BP, Name);
        FSlateFontInfo Font = Reference->GetFont();
        Font.Size = FontSize;
        Label->SetFont(Font);
        Label->SetColorAndOpacity(Color);
        Label->SetText(FText::GetEmpty());
        Label->SetJustification(ETextJustify::Right);
        Label->SetRenderOpacity(0.f);
        Label->SetVisibility(ESlateVisibility::HitTestInvisible);
        Place(Canvas, Label, FVector2D(366.f - Width, 1.f), FVector2D(Width, 26.f), 4);
    };

    // Leave a dedicated right-aligned feedback column so changes cannot cover values.
    Place(HealthCanvas, HealthText, FVector2D::ZeroVector, FVector2D(248.f, 28.f), 3);
    Place(ArmorCanvas, ArmorText, FVector2D::ZeroVector, FVector2D(248.f, 25.f), 3);
    Place(ExperienceCanvas, ExperienceText, FVector2D::ZeroVector, FVector2D(174.f, 23.f), 3);
    Place(HealthCanvas, HealthTrail, FVector2D(0.f, 33.f), FVector2D(366.f, 22.f), 1);
    Place(HealthCanvas, Health, FVector2D(0.f, 33.f), FVector2D(366.f, 22.f), 2);
    Place(ArmorCanvas, ArmorTrail, FVector2D(0.f, 33.f), FVector2D(366.f, 18.f), 1);
    Place(ArmorCanvas, Armor, FVector2D(0.f, 33.f), FVector2D(366.f, 18.f), 2);
    Place(ExperienceCanvas, Experience, FVector2D(0.f, 31.f), FVector2D(366.f, 9.f), 2);
    for (UProgressBar* Bar : {Health, HealthTrail, Armor, ArmorTrail, Experience})
    {
        RoundedFill(Bar);
    }
    AddChangeText(HealthCanvas, TEXT("HealthChangeText"), HealthText, 110.f, 16.f, FLinearColor(1.f, .73f, .67f));
    AddChangeText(ArmorCanvas, TEXT("ArmorChangeText"), ArmorText, 110.f, 16.f, FLinearColor(.72f, .95f, 1.f));
    AddChangeText(ExperienceCanvas, TEXT("ExperienceChangeText"), ExperienceText, 190.f, 15.f, FLinearColor(1.f, .87f, .59f));

    // This editor action changes layout only; the existing event graphs are retained.
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
    FKismetEditorUtilities::CompileBlueprint(BP);
    BP->MarkPackageDirty();
    return BP->Status != BS_Error;
}
