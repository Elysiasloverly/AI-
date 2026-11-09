#include "RogueUIThemeBuilder.h"

#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ProgressBar.h"
#include "Components/ScaleBox.h"
#include "Components/ScaleBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "WidgetBlueprint.h"

// 仅供编辑器明确执行：样式、间距及标签保存在 WBP，运行时只更新数值。
bool URogueUIThemeBuilder::RefineCompactVitals(UWidgetBlueprint* Blueprint)
{
    if (!Blueprint || !Blueprint->WidgetTree) return false;
    auto* Tree = Blueprint->WidgetTree.Get();
    auto* Body = Cast<UCanvasPanel>(Tree->FindWidget(TEXT("CenteredVitalsBody")));
    auto* Panel = Cast<USizeBox>(Tree->FindWidget(TEXT("VitalsPanel")));
    auto* Frame = Cast<UBorder>(Tree->FindWidget(TEXT("VitalsFrame")));
    const auto* Reference = Cast<UTextBlock>(Tree->FindWidget(TEXT("HealthText")));
    if (!Body || !Panel || !Frame || !Reference) return false;
    const FSlateFontInfo BaseFont = Reference->GetFont();
    Blueprint->Modify(); Tree->Modify();

    auto Place = [](UCanvasPanel* Parent, UWidget* Widget, FVector2D Position, FVector2D Size, int32 Z = 0)
    {
        Widget->Modify(); Widget->RemoveFromParent();
        auto* CanvasSlot = Parent->AddChildToCanvas(Widget);
        CanvasSlot->SetPosition(Position); CanvasSlot->SetSize(Size); CanvasSlot->SetZOrder(Z);
        Widget->SetVisibility(ESlateVisibility::HitTestInvisible);
    };
    auto Text = [&](FName Name, int32 FontSize, FLinearColor Color, ETextJustify::Type Justification = ETextJustify::Left)
    {
        auto* Widget = FindObject<UTextBlock>(Tree, *Name.ToString());
        if (!Widget) Widget = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
        Widget->Modify();
        auto Font = BaseFont; Font.Size = FontSize; Font.OutlineSettings.OutlineSize = 0;
        Widget->SetFont(Font); Widget->SetColorAndOpacity(Color); Widget->SetJustification(Justification);
        Widget->SetAutoWrapText(false); Widget->SetShadowOffset(FVector2D::ZeroVector);
        return Widget;
    };
    auto Back = [&](FName Name, FVector2D Position, FVector2D Size, FLinearColor Color, float Radius, FLinearColor Stroke)
    {
        auto* Border = FindObject<UBorder>(Tree, *Name.ToString());
        if (!Border) Border = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), Name);
        Border->Modify(); Border->SetBrush(FSlateRoundedBoxBrush(Color, Radius, Stroke, 1.f));
        Border->SetBrushColor(FLinearColor::White); Place(Body, Border, Position, Size);
        return Border;
    };
    auto Row = [&](const TCHAR* Name, const TCHAR* CanvasName, FVector2D Position, FVector2D Size)
    {
        auto* Box = Cast<USizeBox>(Tree->FindWidget(Name));
        auto* Canvas = Cast<UCanvasPanel>(Tree->FindWidget(CanvasName));
        if (!Box || !Canvas) return static_cast<UCanvasPanel*>(nullptr);
        Box->Modify(); Box->SetWidthOverride(Size.X); Box->SetHeightOverride(Size.Y);
        Place(Body, Box, Position, Size, 2);
        return Canvas;
    };
    auto Bar = [&](const TCHAR* Name, UCanvasPanel* Canvas, FVector2D Position, FVector2D Size, FLinearColor Color, int32 Z, bool bTrack)
    {
        auto* Widget = Cast<UProgressBar>(Tree->FindWidget(Name));
        if (!Widget) return;
        Widget->Modify(); auto Style = Widget->GetWidgetStyle();
        Style.BackgroundImage = FSlateRoundedBoxBrush(FLinearColor(.008f, .017f, .029f, bTrack ? 1.f : 0.f), float(Size.Y * .5f));
        Style.FillImage = FSlateRoundedBoxBrush(FLinearColor::White, float(Size.Y * .5f));
        Widget->SetWidgetStyle(Style); Widget->SetFillColorAndOpacity(Color);
        Widget->SetBorderPadding(FVector2D::ZeroVector); Widget->SetBarFillStyle(EProgressBarFillStyle::Scale);
        Place(Canvas, Widget, Position, Size, Z);
    };
    auto Value = [&](const TCHAR* Name, const TCHAR* ScaleName, UCanvasPanel* Canvas, FVector2D Position, FVector2D Size, int32 FontSize, FLinearColor Color, bool bRight)
    {
        auto* Scale = FindObject<UScaleBox>(Tree, ScaleName);
        if (!Scale) Scale = Tree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass(), ScaleName);
        Scale->Modify(); Scale->SetStretch(EStretch::ScaleToFit); Scale->SetStretchDirection(EStretchDirection::DownOnly);
        auto* Label = Text(Name, FontSize, Color, bRight ? ETextJustify::Right : ETextJustify::Left);
        Label->RemoveFromParent(); Scale->SetContent(Label); Label->SetText(FText::FromString(TEXT("0 / 0")));
        if (auto* ContentSlot = Cast<UScaleBoxSlot>(Label->Slot))
        {
            ContentSlot->SetHorizontalAlignment(bRight ? HAlign_Right : HAlign_Left);
            ContentSlot->SetVerticalAlignment(VAlign_Center);
        }
        Place(Canvas, Scale, Position, Size, 5);
    };

    Frame->Modify(); Frame->SetPadding(FMargin(0.f));
    Frame->SetBrush(FSlateRoundedBoxBrush(FLinearColor(.017f, .03f, .049f, .97f), 18.f, FLinearColor(.22f, .37f, .46f, .72f), 1.f));
    Frame->SetBrushColor(FLinearColor::White);
    Panel->Modify(); Panel->SetWidthOverride(620.f); Panel->SetHeightOverride(144.f);
    if (auto* PanelSlot = Cast<UCanvasPanelSlot>(Panel->Slot)) PanelSlot->SetSize(FVector2D(620.f, 144.f));
    const FLinearColor White(.91f, .95f, 1.f), Muted(.56f, .69f, .78f), Cyan(.44f, .81f, .94f), Gold(.88f, .74f, .44f);
    auto* Health = Row(TEXT("HealthRow"), TEXT("HealthRowCanvas"), {132, 16}, {472, 44});
    auto* Armor = Row(TEXT("ArmorRow"), TEXT("ArmorRowCanvas"), {132, 68}, {472, 30});
    auto* Experience = Row(TEXT("ExperienceRow"), TEXT("ExperienceRowCanvas"), {16, 110}, {588, 23});
    if (!Health || !Armor || !Experience) return false;

    auto* HealthCaption = Text(TEXT("HealthText"), 14, Muted);
    HealthCaption->SetText(FText::FromString(TEXT("生命"))); Place(Health, HealthCaption, {0, 6}, {52, 21}, 5);
    Value(TEXT("HealthValueText"), TEXT("HealthValueScale"), Health, {166, 0}, {306, 30}, 22, White, true);
    Bar(TEXT("HealthDamageBar"), Health, {0, 33}, {472, 10}, {.99f, .67f, .74f, 1}, 1, true);
    Bar(TEXT("HealthBar"), Health, {0, 33}, {472, 10}, {.94f, .31f, .48f, 1}, 2, false);
    auto* ArmorCaption = Text(TEXT("ArmorText"), 12, Muted);
    ArmorCaption->SetText(FText::FromString(TEXT("护甲"))); Place(Armor, ArmorCaption, {0, 0}, {52, 20}, 5);
    Value(TEXT("ArmorValueText"), TEXT("ArmorValueScale"), Armor, {166, 0}, {306, 20}, 15, {.69f, .85f, .94f, 1}, true);
    Bar(TEXT("ArmorDamageBar"), Armor, {0, 24}, {472, 5}, {.77f, .94f, 1, 1}, 1, true);
    Bar(TEXT("ArmorBar"), Armor, {0, 24}, {472, 5}, Cyan, 2, false);
    auto* ArmorUnavailable = Text(TEXT("ArmorUnavailableText"), 12, {.37f, .48f, .56f, 1});
    ArmorUnavailable->SetText(FText::FromString(TEXT("暂无护甲")));
    Place(Body, ArmorUnavailable, {132, 72}, {472, 20}, 3);
    ArmorUnavailable->SetVisibility(ESlateVisibility::Collapsed);
    auto* ExperienceCaption = Text(TEXT("ExperienceText"), 12, Muted);
    ExperienceCaption->SetText(FText::FromString(TEXT("经验"))); Place(Experience, ExperienceCaption, {0, 0}, {44, 17}, 5);
    Value(TEXT("ExperienceValueText"), TEXT("ExperienceValueScale"), Experience, {52, 0}, {234, 17}, 12, {.72f, .76f, .78f, 1}, false);
    Bar(TEXT("ExperienceBar"), Experience, {0, 20}, {588, 3}, Gold, 2, true);

    auto* Badge = Back(TEXT("LevelBadge"), {14, 16}, {94, 88}, {.028f, .065f, .097f, 1}, 12.f, {.18f, .37f, .47f, .58f});
    auto* LevelContent = FindObject<UVerticalBox>(Tree, TEXT("LevelBadgeContent"));
    if (!LevelContent) LevelContent = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("LevelBadgeContent"));
    LevelContent->Modify(); LevelContent->ClearChildren(); LevelContent->RemoveFromParent();
    Badge->SetPadding(FMargin(8.f));
    Badge->SetHorizontalAlignment(HAlign_Center); Badge->SetVerticalAlignment(VAlign_Center);
    Badge->SetContent(LevelContent);
    // 标题与数字共用框内居中容器，不再靠两份独立的 Canvas 坐标对齐。
    auto* Caption = Text(TEXT("LevelCaption"), 13, Muted, ETextJustify::Center);
    Caption->RemoveFromParent(); Caption->SetText(FText::FromString(TEXT("等级")));
    auto* CaptionSlot = LevelContent->AddChildToVerticalBox(Caption);
    CaptionSlot->SetHorizontalAlignment(HAlign_Center); CaptionSlot->SetPadding(FMargin(0, 0, 0, 6));
    auto* Level = Text(TEXT("LevelText"), 34, {.76f, .93f, 1, 1}, ETextJustify::Center);
    Level->RemoveFromParent();
    LevelContent->AddChildToVerticalBox(Level)->SetHorizontalAlignment(HAlign_Center);
    // 待选次数只在状态栏上方提示，等级区不再重复显示 Q 和计数。
    for (const auto* Name : {TEXT("PendingUpgradesText"), TEXT("UpgradeKeyHint"), TEXT("UpgradeKeyBadge")})
        if (auto* Widget = Tree->FindWidget(Name)) { Widget->Modify(); Tree->RemoveWidget(Widget); }
    Back(TEXT("VitalsLeftDivider"), {120, 25}, {1, 70}, {.21f, .36f, .44f, .28f}, 0.f, FLinearColor::Transparent);
    Back(TEXT("VitalsTopHighlight"), {26, 1}, {568, 1}, {.45f, .7f, .81f, .2f}, 0.f, FLinearColor::Transparent);

    // 变化反馈仍在原来的行中，单独预留宽度，不与右侧常驻数值交叠。
    struct FFeedback { UCanvasPanel* Canvas; const TCHAR* Number; const TCHAR* Fill; FVector2D Size; FVector2D NumberPosition; FVector2D NumberSize; FLinearColor Color; };
    for (const auto& Item : {
        FFeedback{Health, TEXT("HealthChangeText"), TEXT("HealthFeedback"), {472, 44}, {60, 6}, {96, 20}, {1, .38f, .52f, .1f}},
        FFeedback{Armor, TEXT("ArmorChangeText"), TEXT("ArmorFeedback"), {472, 30}, {60, 0}, {96, 20}, {.35f, .78f, 1, .08f}},
        FFeedback{Experience, TEXT("ExperienceChangeText"), TEXT("ExperienceFeedback"), {588, 23}, {300, 0}, {288, 18}, {1, .78f, .36f, .06f}}})
    {
        if (auto* Fill = Cast<UBorder>(Tree->FindWidget(Item.Fill)))
        {
            Fill->Modify(); Fill->SetBrush(FSlateRoundedBoxBrush(FLinearColor::White, 7.f));
            Fill->SetBrushColor(Item.Color); Place(Item.Canvas, Fill, {-5, -3}, Item.Size + FVector2D(10, 6));
        }
        const bool bExperience = Item.Canvas == Experience;
        auto* Number = Text(Item.Number, bExperience ? 12 : 13, bExperience ? Gold : Item.Canvas == Armor ? Cyan : FLinearColor(1, .65f, .72f, 1), ETextJustify::Right);
        Place(Item.Canvas, Number, Item.NumberPosition, Item.NumberSize, 6);
    }
    if (auto* Hint = Cast<UTextBlock>(Tree->FindWidget(TEXT("PendingUpgradeHintText"))))
        if (auto* HintSlot = Cast<UCanvasPanelSlot>(Hint->Slot))
        { Hint->Modify(); HintSlot->SetPosition({0, -180}); HintSlot->SetSize({620, 28}); }
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    FKismetEditorUtilities::CompileBlueprint(Blueprint); Blueprint->MarkPackageDirty();
    return Blueprint->Status != BS_Error;
}
