#include "RogueUIThemeBuilder.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "UI/Widgets/RogueDashRingWidget.h"
#include "WidgetBlueprint.h"

// 只在明确执行时迁移保存的 WBP，运行时不重排用户在设计器里的布局。
bool URogueUIThemeBuilder::UsePlayerSideDashRing(UWidgetBlueprint* Blueprint)
{
    if (!Blueprint || !Blueprint->WidgetTree) return false;
    auto* Tree = Blueprint->WidgetTree.Get();
    auto* Root = Cast<UCanvasPanel>(Tree->RootWidget);
    auto* Panel = Cast<USizeBox>(Tree->FindWidget(TEXT("VitalsPanel")));
    auto* Reference = Cast<UTextBlock>(Tree->FindWidget(TEXT("HealthText")));
    if (!Root || !Panel || !Reference) return false;
    Blueprint->Modify();
    Tree->Modify();

    // 原状态栏只收掉右侧冲刺分区，其余生命、护甲、经验和等级排版不变。
    for (const auto* Name : {TEXT("DashCooldownRow"), TEXT("DashSectionFrame"), TEXT("VitalsRightDivider")})
        if (auto* Widget = Tree->FindWidget(Name)) { Widget->Modify(); Widget->SetVisibility(ESlateVisibility::Collapsed); }
    Panel->Modify();
    Panel->SetWidthOverride(568.f);
    if (auto* Slot = Cast<UCanvasPanelSlot>(Panel->Slot)) Slot->SetSize(FVector2D(568.f, 132.f));

    auto Place = [&](UWidget* Widget, FVector2D Position, FVector2D Size, FAnchors Anchors, FVector2D Alignment, int32 Z)
    {
        Widget->Modify();
        Widget->RemoveFromParent();
        auto* Slot = Root->AddChildToCanvas(Widget);
        Slot->SetPosition(Position); Slot->SetSize(Size);
        Slot->SetAnchors(Anchors); Slot->SetAlignment(Alignment); Slot->SetZOrder(Z);
    };
    auto* Hint = FindObject<UTextBlock>(Tree, TEXT("PendingUpgradeHintText"));
    if (!Hint) Hint = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("PendingUpgradeHintText"));
    auto Font = Reference->GetFont();
    Font.Size = 18; Font.OutlineSettings.OutlineSize = 1;
    Font.OutlineSettings.OutlineColor = FLinearColor(.015f, .04f, .06f, .8f);
    Hint->SetFont(Font);
    Hint->SetColorAndOpacity(FLinearColor(.55f, .85f, 1.f, 1.f));
    Hint->SetShadowColorAndOpacity(FLinearColor(.005f, .02f, .035f, .8f));
    Hint->SetShadowOffset(FVector2D(0.f, 1.f));
    Hint->SetJustification(ETextJustify::Center);
    Hint->SetText(FText::FromString(TEXT("有升级可选 · 按 Q 打开")));
    Hint->SetAutoWrapText(false);
    Place(Hint, FVector2D(0.f, -166.f), FVector2D(568.f, 30.f), FAnchors(.5f, 1.f), FVector2D(.5f, 1.f), 8);
    Hint->SetVisibility(ESlateVisibility::Collapsed);

    auto* Ring = FindObject<URogueDashRingWidget>(Tree, TEXT("PlayerDashRing"));
    if (!Ring) Ring = Tree->ConstructWidget<URogueDashRingWidget>(URogueDashRingWidget::StaticClass(), TEXT("PlayerDashRing"));
    Place(Ring, FVector2D::ZeroVector, FVector2D(108.f), FAnchors(0.f), FVector2D::ZeroVector, 2);
    if (auto* RingSlot = Cast<UCanvasPanelSlot>(Ring->Slot)) RingSlot->SetAutoSize(true);
    Ring->SetVisibility(ESlateVisibility::Collapsed);
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    FKismetEditorUtilities::CompileBlueprint(Blueprint);
    Blueprint->MarkPackageDirty();
    return Blueprint->Status != BS_Error;
}
