#include "RogueUIThemeBuilder.h"
#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "WidgetBlueprint.h"

bool URogueUIThemeBuilder::AddPendingUpgradeCounter(UWidgetBlueprint* BP)
{
    if(!BP||!BP->WidgetTree)return false;
    auto* Tree=BP->WidgetTree.Get();
    auto* Body=Cast<UCanvasPanel>(Tree->FindWidget(TEXT("CenteredVitalsBody")));
    auto* Badge=Cast<UBorder>(Tree->FindWidget(TEXT("LevelBadge")));
    auto* Level=Cast<UTextBlock>(Tree->FindWidget(TEXT("LevelText")));
    auto* BadgeSlot=Badge?Cast<UCanvasPanelSlot>(Badge->Slot):nullptr;
    if(!Body||!BadgeSlot||!Level)return false;
    BP->Modify();Tree->Modify();
    const FVector2D Origin=BadgeSlot->GetPosition(),Size=BadgeSlot->GetSize();
    auto Place=[&](UWidget* Widget,FVector2D Pos,FVector2D Extent)
    {
        Widget->Modify();Widget->RemoveFromParent();auto* Slot=Body->AddChildToCanvas(Widget);
        Slot->SetPosition(Pos);Slot->SetSize(Extent);Slot->SetZOrder(6);
        Widget->SetVisibility(ESlateVisibility::HitTestInvisible);
    };
    auto Font=Level->GetFont();Font.Size=28;Level->SetFont(Font);
    Place(Level,Origin+FVector2D(6,30),FVector2D(Size.X-12,40));
    if(auto* Caption=Cast<UTextBlock>(Tree->FindWidget(TEXT("LevelCaption"))))
        Place(Caption,Origin+FVector2D(6,10),FVector2D(Size.X-12,18));
    auto* Count=FindObject<UTextBlock>(Tree,TEXT("PendingUpgradesText"));
    if(!Count)Count=Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),TEXT("PendingUpgradesText"));
    Font.Size=11;Font.OutlineSettings.OutlineSize=0;Count->SetFont(Font);
    Count->SetText(FText::FromString(TEXT("待选 0")));Count->SetColorAndOpacity(FLinearColor(.95f,.78f,.45f,1));
    Count->SetJustification(ETextJustify::Center);Count->SetAutoWrapText(false);
    Place(Count,Origin+FVector2D(26,Size.Y-22),{Size.X-32,16});
    auto* Key=FindObject<UBorder>(Tree,TEXT("UpgradeKeyBadge"));
    if(!Key)Key=Tree->ConstructWidget<UBorder>(UBorder::StaticClass(),TEXT("UpgradeKeyBadge"));
    Key->SetBrush(FSlateRoundedBoxBrush(FLinearColor(.035f,.1f,.135f,1),3.f,FLinearColor(.2f,.45f,.54f,.75f),1.f));
    Key->SetPadding(FMargin(0));Key->SetHorizontalAlignment(HAlign_Center);Key->SetVerticalAlignment(VAlign_Center);
    Place(Key,Origin+FVector2D(7,Size.Y-22),{16,16});
    auto* Hint=FindObject<UTextBlock>(Tree,TEXT("UpgradeKeyHint"));
    if(!Hint)Hint=Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),TEXT("UpgradeKeyHint"));
    Font.Size=10;Hint->SetFont(Font);Hint->SetText(FText::FromString(TEXT("Q")));
    Hint->SetColorAndOpacity(FLinearColor(.7f,.9f,1,1));Key->SetContent(Hint);
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
    FKismetEditorUtilities::CompileBlueprint(BP);BP->MarkPackageDirty();return BP->Status!=BS_Error;
}
