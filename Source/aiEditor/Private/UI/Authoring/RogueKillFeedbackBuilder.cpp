#include "RogueUIThemeBuilder.h"
#include "RogueWidgetBuilder.h"
#include "Engine/Font.h"
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"

bool URogueUIThemeBuilder::BuildKillFeedback(UWidgetBlueprint* BP)
{
    if(!BP || BP->GetName()!=TEXT("WBP_KillFeedback") || !BP->WidgetTree)return false;
    BP->Modify();BP->WidgetTree->Modify();
    UWidgetTree* T=BP->WidgetTree;
    auto* Root=T->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(),TEXT("FeedbackRoot"));
    T->RootWidget=Root;Root->SetVisibility(ESlateVisibility::HitTestInvisible);
    auto* Panel=T->ConstructWidget<UBorder>(UBorder::StaticClass(),TEXT("SpiritSeal"));
    Panel->SetBrush(FSlateRoundedBoxBrush(FLinearColor(.025f,.055f,.085f,.76f),12.f,FLinearColor(.48f,.82f,.93f,.45f),1.f));
    Panel->SetPadding(FMargin(16,8));
    auto* Slot=Root->AddChildToCanvas(Panel);Slot->SetAnchors(FAnchors(1,1));Slot->SetAlignment(FVector2D(1,1));
    Slot->SetPosition(FVector2D(-38,-125));Slot->SetSize(FVector2D(220,82));
    auto* Body=T->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(),TEXT("FeedbackBody"));Panel->AddChild(Body);
    auto Label=[&](FName Name,const TCHAR* Text,int32 FontSize,FVector2D Pos,FLinearColor Color)
    {
        auto* L=T->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),Name);
        L->SetText(FText::FromString(Text));L->SetFont(FSlateFontInfo(URogueWidgetBuilder::GetOrCreateProjectFont(),FontSize));
        L->SetColorAndOpacity(Color);auto* S=Body->AddChildToCanvas(L);S->SetPosition(Pos);S->SetSize(FVector2D(188,30));
    };
    Label(TEXT("KillText"),TEXT("退治"),24,FVector2D(0,-2),FLinearColor(.92f,.96f,1));
    Label(TEXT("ChainText"),TEXT("连锁  × 1"),17,FVector2D(0,32),FLinearColor(.52f,.84f,.93f));
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FKismetEditorUtilities::CompileBlueprint(BP);
    BP->MarkPackageDirty();return true;
}
