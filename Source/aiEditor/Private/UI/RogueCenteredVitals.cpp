#include "RogueUIThemeBuilder.h"
#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "WidgetBlueprint.h"

// 手动执行的设计器排版工具。绑定控件沿用原名，正式运行不重建或覆盖 WBP 样式。
bool URogueUIThemeBuilder::CenterCombatVitals(UWidgetBlueprint* BP)
{
    if (!BP || !BP->WidgetTree || BP->GetName()!=TEXT("WBP_PlayerVitals")) return false;
    auto* Tree=BP->WidgetTree.Get();
    auto* Root=Cast<UCanvasPanel>(Tree->RootWidget);
    auto* Panel=Cast<USizeBox>(Tree->FindWidget(TEXT("VitalsPanel")));
    auto* Frame=Cast<UBorder>(Tree->FindWidget(TEXT("VitalsFrame")));
    auto* Reference=Cast<UTextBlock>(Tree->FindWidget(TEXT("HealthText")));
    if (!Root || !Panel || !Frame || !Reference) return false;
    const FSlateFontInfo BaseFont=Reference->GetFont();
    BP->Modify();Tree->Modify();
    auto Place=[](UCanvasPanel* Parent,UWidget* Child,FVector2D Position,FVector2D Size,int32 Z=0)
    {
        Child->Modify();Child->RemoveFromParent();
        auto* Slot=Parent->AddChildToCanvas(Child);
        Slot->SetPosition(Position);Slot->SetSize(Size);Slot->SetZOrder(Z);
        Child->SetVisibility(ESlateVisibility::HitTestInvisible);
        return Slot;
    };
    auto Canvas=[&](FName Name)
    {
        if(auto* Existing=FindObject<UCanvasPanel>(Tree,*Name.ToString()))return Existing;
        return Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(),Name);
    };
    auto* Body=Canvas(TEXT("CenteredVitalsBody"));Body->ClearChildren();
    Frame->SetContent(Body);Frame->SetPadding(FMargin(0));
    Frame->SetBrush(FSlateRoundedBoxBrush(FLinearColor(.018f,.033f,.055f,.96f),16.f,FLinearColor(.22f,.38f,.49f,.65f),1.f));
    Frame->SetBrushColor(FLinearColor::White);
    Panel->SetWidthOverride(760);Panel->SetHeightOverride(132);
    auto* PanelSlot=Cast<UCanvasPanelSlot>(Panel->Slot);
    if(!PanelSlot)return false;
    PanelSlot->SetAnchors(FAnchors(.5f,1.f));PanelSlot->SetAlignment({.5f,1});
    PanelSlot->SetPosition({0,-24});PanelSlot->SetSize({760,132});PanelSlot->SetAutoSize(false);
    Panel->SetVisibility(ESlateVisibility::HitTestInvisible);

    auto Text=[&](FName Name,UCanvasPanel* Parent,FVector2D Pos,FVector2D Size,int32 FontSize,FLinearColor Color)
    {
        auto* Label=FindObject<UTextBlock>(Tree,*Name.ToString());
        if(!Label)Label=Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),Name);
        auto Font=BaseFont;Font.Size=FontSize;Font.OutlineSettings.OutlineSize=0;
        Label->SetFont(Font);Label->SetColorAndOpacity(Color);Label->SetAutoWrapText(false);
        Label->SetJustification(ETextJustify::Left);Label->SetShadowOffset({0,0});
        Place(Parent,Label,Pos,Size,5);return Label;
    };
    auto Back=[&](FName Name,FVector2D Pos,FVector2D Size,FLinearColor Color,float Radius,FLinearColor Stroke)
    {
        auto* Border=FindObject<UBorder>(Tree,*Name.ToString());
        if(!Border)Border=Tree->ConstructWidget<UBorder>(UBorder::StaticClass(),Name);
        Border->SetBrush(FSlateRoundedBoxBrush(Color,Radius,Stroke,1.f));Border->SetBrushColor(FLinearColor::White);
        Place(Body,Border,Pos,Size);return Border;
    };
    auto Bar=[&](FName Name,UCanvasPanel* Parent,FVector2D Pos,FVector2D Size,FLinearColor Fill,int32 Z,bool Track)
    {
        auto* Widget=FindObject<UProgressBar>(Tree,*Name.ToString());if(!Widget)return;
        auto Style=Widget->GetWidgetStyle();
        Style.BackgroundImage=FSlateRoundedBoxBrush(FLinearColor(.01f,.022f,.035f,Track?1.f:0.f),float(Size.Y*.5));
        Style.FillImage=FSlateRoundedBoxBrush(FLinearColor::White,float(Size.Y*.5));
        Widget->SetWidgetStyle(Style);Widget->SetFillColorAndOpacity(Fill);Widget->SetBorderPadding({0,0});
        Widget->SetBarFillStyle(EProgressBarFillStyle::Scale);Place(Parent,Widget,Pos,Size,Z);
    };
    auto Row=[&](FName Name,FName CanvasName,FVector2D Pos,FVector2D Size)
    {
        auto* Box=FindObject<USizeBox>(Tree,*Name.ToString());auto* Inner=Canvas(CanvasName);
        if(!Box)return static_cast<UCanvasPanel*>(nullptr);
        for(auto* Child:Inner->GetAllChildren())Child->SetVisibility(ESlateVisibility::Collapsed);
        Box->SetContent(Inner);Box->SetWidthOverride(Size.X);Box->SetHeightOverride(Size.Y);
        Place(Body,Box,Pos,Size,1);return Inner;
    };
    auto* Health=Row(TEXT("HealthRow"),TEXT("HealthRowCanvas"),{132,17},{416,42});
    auto* Armor=Row(TEXT("ArmorRow"),TEXT("ArmorRowCanvas"),{132,64},{416,32});
    auto* Exp=Row(TEXT("ExperienceRow"),TEXT("ExperienceRowCanvas"),{132,102},{416,22});
    auto* Dash=Row(TEXT("DashCooldownRow"),TEXT("DashCooldownCanvas"),{580,17},{164,98});
    if(!Health||!Armor||!Exp||!Dash)return false;
    const FLinearColor White(.9f,.95f,1,1),Muted(.58f,.7f,.79f,1),Cyan(.4f,.83f,.96f,1),Gold(.91f,.75f,.43f,1);
    Text(TEXT("HealthText"),Health,{0,0},{292,24},18,White);
    Bar(TEXT("HealthDamageBar"),Health,{0,28},{416,11},{.98f,.66f,.73f,1},1,true);
    Bar(TEXT("HealthBar"),Health,{0,28},{416,11},{.94f,.28f,.48f,1},2,false);
    Text(TEXT("ArmorText"),Armor,{0,0},{292,20},14,{.65f,.81f,.89f,1});
    Bar(TEXT("ArmorDamageBar"),Armor,{0,23},{416,6},{.77f,.94f,1,1},1,true);
    Bar(TEXT("ArmorBar"),Armor,{0,23},{416,6},Cyan,2,false);
    Text(TEXT("ExperienceText"),Exp,{0,0},{204,17},12,Muted);
    Bar(TEXT("ExperienceBar"),Exp,{0,19},{416,3},Gold,2,true);

    Back(TEXT("LevelBadge"),{14,17},{80,98},{.033f,.078f,.115f,1},12,{.15f,.33f,.43f,.6f});
    Text(TEXT("LevelCaption"),Body,{20,30},{68,19},13,Muted)->SetText(FText::FromString(TEXT("等级")));
    auto* Caption=FindObject<UTextBlock>(Tree,TEXT("LevelCaption"));Caption->SetJustification(ETextJustify::Center);
    auto* Level=Text(TEXT("LevelText"),Body,{20,55},{68,43},30,{.73f,.93f,1,1});
    Level->SetText(FText::FromString(TEXT("1")));Level->SetJustification(ETextJustify::Center);
    Back(TEXT("VitalsLeftDivider"),{111,26},{1,80},{.2f,.35f,.44f,.45f},0,FLinearColor::Transparent);
    Back(TEXT("VitalsRightDivider"),{565,26},{1,80},{.2f,.35f,.44f,.45f},0,FLinearColor::Transparent);
    Back(TEXT("DashSectionFrame"),{580,17},{164,98},{.03f,.065f,.09f,.9f},12,{.16f,.32f,.39f,.5f});
    Text(TEXT("DashLabel"),Dash,{14,9},{136,19},13,Muted)->SetText(FText::FromString(TEXT("回避")));
    auto* DashText=Text(TEXT("DashCooldownText"),Dash,{14,34},{136,25},17,Cyan);
    DashText->SetJustification(ETextJustify::Center);
    Bar(TEXT("DashCooldownBar"),Dash,{14,76},{136,7},Cyan,2,true);

    // 变化数字与常驻文字分列；上浮只在各自行内发生，经验升级长文案也有足够宽度。
    struct FFeedback{UCanvasPanel* Row;const TCHAR* Number;const TCHAR* Fill;FVector2D Size;FLinearColor Color;};
    for(const auto& Item:{FFeedback{Health,TEXT("HealthChangeText"),TEXT("HealthFeedback"),{416,42},{1,.38f,.52f,.12f}},
        FFeedback{Armor,TEXT("ArmorChangeText"),TEXT("ArmorFeedback"),{416,32},{.35f,.78f,1,.1f}},
        FFeedback{Exp,TEXT("ExperienceChangeText"),TEXT("ExperienceFeedback"),{416,22},{1,.78f,.36f,.08f}}})
    {
        if(auto* Fill=FindObject<UBorder>(Tree,Item.Fill))
        {
            Fill->SetBrush(FSlateRoundedBoxBrush(FLinearColor::White,6.f));Fill->SetBrushColor(Item.Color);
            Place(Item.Row,Fill,{-6,-3},Item.Size+FVector2D(12,6),0);
        }
        const bool IsExp=Item.Row==Exp;
        Text(Item.Number,Item.Row,{IsExp?216.f:300.f,0},{IsExp?200.f:116.f,20},IsExp?12:13,
            IsExp?Gold:(Item.Row==Armor?Cyan:FLinearColor(1,.66f,.72f,1)))->SetJustification(ETextJustify::Right);
    }
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
    FKismetEditorUtilities::CompileBlueprint(BP);BP->MarkPackageDirty();
    return BP->Status!=BS_Error;
}
