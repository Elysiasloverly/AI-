#include "RogueUIThemeBuilder.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "RogueWidgetBuilder.h"
#include "UI/Widgets/RogueShopWidgetBase.h"
#include "UI/Widgets/RogueUpgradeSelectionWidgetBase.h"
#include "GameFramework/PlayerController.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/SizeBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/VerticalBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/UniformGridPanel.h"
#include "Components/EditableTextBox.h"
#include "Engine/Texture2D.h"
#include "Engine/Font.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "WidgetBlueprint.h"

namespace
{
    const FLinearColor Cream(.96f,.91f,.78f,1), Gold(.85f,.64f,.29f,1), Ink(.035f,.025f,.045f,1);
    FSlateBrush TextureBrush(UTexture2D* Texture, bool bSlice = true, FVector2D Size = FVector2D(256,256))
    {
        FSlateBrush Brush;
        Brush.SetResourceObject(Texture);
        Brush.DrawAs = ESlateBrushDrawType::Image;
        Brush.Margin = FMargin(0);
        if (bSlice) { Brush.SetUVRegion(FBox2f(FVector2f(.24f,.24f),FVector2f(.76f,.76f))); }
        Brush.SetImageSize(Size);
        Brush.TintColor = FLinearColor::White;
        return Brush;
    }
    void Place(UCanvasPanel* Parent, UWidget* Child, FVector2D Position, FVector2D Size,
        FAnchors Anchors = FAnchors(0,0), FVector2D Alignment = FVector2D::ZeroVector)
    {
        UCanvasPanelSlot* Slot = Parent->AddChildToCanvas(Child);
        Slot->SetAnchors(Anchors); Slot->SetAlignment(Alignment); Slot->SetPosition(Position); Slot->SetSize(Size);
    }
    void Stretch(UCanvasPanel* Parent, UWidget* Child, FMargin Offsets = FMargin(0), FAnchors Anchors = FAnchors(0,0,1,1))
    {
        UCanvasPanelSlot* Slot = Parent->AddChildToCanvas(Child);
        Slot->SetAnchors(Anchors); Slot->SetOffsets(Offsets);
    }
    UCanvasPanel* Reset(UWidgetBlueprint* BP)
    {
        BP->Modify(); BP->WidgetTree->Modify();
        UCanvasPanel* Root = Cast<UCanvasPanel>(BP->WidgetTree->RootWidget);
        if (!Root) { Root = BP->WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(),TEXT("RootCanvas")); BP->WidgetTree->RootWidget = Root; }
        Root->ClearChildren(); return Root;
    }
    UTextBlock* Text(UWidgetBlueprint* BP, FName Name, const TCHAR* Value, int32 Size, FLinearColor Color = Cream)
    {
        UTextBlock* Widget = BP->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),Name);
        Widget->SetText(FText::FromString(Value)); Widget->SetFont(FSlateFontInfo(URogueWidgetBuilder::GetOrCreateProjectFont(),Size));
        Widget->SetColorAndOpacity(Color); return Widget;
    }
    UProgressBar* Bar(UWidgetBlueprint* BP, UCanvasPanel* Canvas, FName Name, FVector2D Position, FVector2D Size, FLinearColor Color, bool bTransparentBackground = false)
    {
        UProgressBar* Widget = BP->WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(),Name);
        FProgressBarStyle Style = Widget->GetWidgetStyle();
        Style.BackgroundImage.TintColor = FLinearColor(.018f,.023f,.045f,bTransparentBackground ? 0 : 1);
        Style.FillImage.TintColor = FLinearColor::White;
        Widget->SetWidgetStyle(Style); Widget->SetFillColorAndOpacity(Color); Widget->SetBorderPadding(FVector2D(0));
        Widget->SetPercent(1); Place(Canvas,Widget,Position,Size); return Widget;
    }
    UBorder* Frame(UWidgetBlueprint* BP,UCanvasPanel* Canvas,FName Name)
    {
        UBorder* Widget=BP->WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(),Name);
        Widget->SetBrushColor(FLinearColor(.025f,.025f,.07f,.95f)); Stretch(Canvas,Widget); return Widget;
    }
    void Finish(UWidgetBlueprint* BP)
    {
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
        FKismetEditorUtilities::CompileBlueprint(BP); BP->MarkPackageDirty();
    }
    bool IsInsideButton(UWidget* Widget)
    {
        for (UWidget* Parent=Widget->GetParent();Parent;Parent=Parent->GetParent()) { if (Cast<UButton>(Parent)) { return true; } }
        return false;
    }
    bool IsFullScreen(UWidget* Widget)
    {
        if (const UCanvasPanelSlot* Slot=Cast<UCanvasPanelSlot>(Widget->Slot))
        {
            const FAnchors A=Slot->GetAnchors(); const FVector2D Size=Slot->GetSize();
            return (A.Maximum.X-A.Minimum.X>.9 && A.Maximum.Y-A.Minimum.Y>.9) || (Size.X>=1600 && Size.Y>=850);
        }
        return false;
    }
}

bool URogueUIThemeBuilder::BuildBossHealthBar(UWidgetBlueprint* BP)
{
    if (!BP || !BP->WidgetTree) { return false; }
    UCanvasPanel* Root=Reset(BP);
    UCanvasPanel* Panel=BP->WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(),TEXT("BossPanel"));
    Place(Root,Panel,FVector2D(0,26),FVector2D(840,112),FAnchors(.5f,0),FVector2D(.5f,0));
    Frame(BP,Panel,TEXT("BossFrame"));
    UTextBlock* Title=Text(BP,TEXT("BossTitle"),TEXT("B O S S"),25,Gold);
    Title->SetJustification(ETextJustify::Center); Stretch(Panel,Title,FMargin(24,8,24,34),FAnchors(0,0,1,0));
    UProgressBar* Trail=Bar(BP,Panel,TEXT("DamageBar"),FVector2D::ZeroVector,FVector2D::ZeroVector,FLinearColor(1,.76f,.38f));
    Stretch(Panel,Trail,FMargin(28,48,28,25),FAnchors(0,0,1,0));
    UProgressBar* Fill=Bar(BP,Panel,TEXT("FillBar"),FVector2D::ZeroVector,FVector2D::ZeroVector,FLinearColor(.83f,.07f,.19f),true);
    Stretch(Panel,Fill,FMargin(28,48,28,25),FAnchors(0,0,1,0));
    UTextBlock* Health=Text(BP,TEXT("HealthText"),TEXT("1000 / 1000"),17);
    Health->SetJustification(ETextJustify::Center); Place(Panel,Health,FVector2D(0,-9),FVector2D(400,24),FAnchors(.5f,1),FVector2D(.5f,1));
    Finish(BP); return true;
}

bool URogueUIThemeBuilder::BuildTouhouVitals(UWidgetBlueprint* BP)
{
    if (!BP || !BP->WidgetTree) { return false; }
    UCanvasPanel* Root=Reset(BP);
    UCanvasPanel* Status=BP->WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(),TEXT("StatusPanel"));
    Place(Root,Status,FVector2D(28,22),FVector2D(260,102)); Frame(BP,Status,TEXT("StatusFrame"));
    Place(Status,Text(BP,TEXT("WaveText"),TEXT("第 1 波"),24),FVector2D(22,12),FVector2D(220,35));
    Place(Status,Text(BP,TEXT("GoldText"),TEXT("金币 0"),19,Gold),FVector2D(22,56),FVector2D(220,28));
    UCanvasPanel* Panel=BP->WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(),TEXT("VitalsPanel"));
    Place(Root,Panel,FVector2D(28,-28),FVector2D(414,216),FAnchors(0,1),FVector2D(0,1)); Frame(BP,Panel,TEXT("VitalsFrame"));
    Place(Panel,Text(BP,TEXT("HealthText"),TEXT("生命 100 / 100"),19),FVector2D(24,17),FVector2D(366,28));
    Bar(BP,Panel,TEXT("HealthDamageBar"),FVector2D(24,51),FVector2D(366,22),FLinearColor(1,.72f,.34f));
    Bar(BP,Panel,TEXT("HealthBar"),FVector2D(24,51),FVector2D(366,22),FLinearColor(.82f,.08f,.18f),true);
    Place(Panel,Text(BP,TEXT("ArmorText"),TEXT("护甲 50 / 50"),17,FLinearColor(.64f,.87f,1)),FVector2D(24,87),FVector2D(366,25));
    Bar(BP,Panel,TEXT("ArmorDamageBar"),FVector2D(24,118),FVector2D(366,18),FLinearColor(.78f,.97f,1));
    Bar(BP,Panel,TEXT("ArmorBar"),FVector2D(24,118),FVector2D(366,18),FLinearColor(.18f,.62f,.88f),true);
    Place(Panel,Text(BP,TEXT("ExperienceText"),TEXT("经验 0 / 6"),15,Gold),FVector2D(24,153),FVector2D(366,23));
    Bar(BP,Panel,TEXT("ExperienceBar"),FVector2D(24,183),FVector2D(366,9),Gold);
    Finish(BP); return true;
}

bool URogueUIThemeBuilder::ApplyTouhouTheme(UWidgetBlueprint* BP,UTexture2D* Background,UTexture2D* Paper,UTexture2D* Indigo,UTexture2D* ButtonTexture)
{
    if (!BP || !BP->WidgetTree || !Background || !Paper || !Indigo || !ButtonTexture) { return false; }
    // 受伤泛红由专属动画驱动，避免把特效遮罩替换为装饰面板。
    if (BP->GetName()==TEXT("WBP_DamageFlash")) { return true; }
    BP->Modify(); BP->WidgetTree->Modify();
    const FString Name=BP->GetName();
    const bool bCard=Name==TEXT("WBP_UpgradeCard") || Name==TEXT("WBP_ShopOfferCard") || Name==TEXT("WBP_UpgradeTreeNode");
    const bool bMain=Name==TEXT("WBP_MainMenu");
    const bool bLight=Name.Contains(TEXT("Menu")) || Name.Contains(TEXT("Tree")) || Name.Contains(TEXT("Shop")) || Name.Contains(TEXT("Upgrade")) || Name.Contains(TEXT("Room")) || Name.Contains(TEXT("Lobby")) || Name.Contains(TEXT("Tutorial")) || Name.Contains(TEXT("Screen")) || Name.Contains(TEXT("NightTransition"));
    const bool bShop=Name==TEXT("WBP_Shop");
    const bool bSelection=Name==TEXT("WBP_UpgradeSelection");
    const bool bPage=bMain || Name==TEXT("WBP_UpgradeTree") || Name==TEXT("WBP_Tutorial") || Name.Contains(TEXT("RoomBrowser")) || Name.Contains(TEXT("MultiplayerMenu")) || Name==TEXT("WBP_Lobby");
    TArray<UWidget*> Widgets; BP->WidgetTree->GetAllWidgets(Widgets);
    for (UWidget* Widget:Widgets)
    {
        Widget->Modify(); const FString N=Widget->GetName();
        if (UBorder* Border=Cast<UBorder>(Widget))
        {
            if (IsFullScreen(Border))
            {
                if (bPage) { Border->SetBrush(TextureBrush(Background,false,FVector2D(1920,1080))); Border->SetBrushColor(FLinearColor::White); }
                else { Border->SetBrushColor(FLinearColor(.009f,.012f,.035f,.72f)); }
            }
            else
            {
                Border->SetBrush(TextureBrush((bLight||bCard)?Paper:Indigo));
                Border->SetBrushColor(FLinearColor::White);
            }
        }
        if (UImage* Image=Cast<UImage>(Widget))
        {
            if (bMain && (IsFullScreen(Image) || N.Contains(TEXT("Background"))))
            { Image->SetBrushFromTexture(Background); Image->SetColorAndOpacity(FLinearColor::White); }
        }
        if (UButton* Button=Cast<UButton>(Widget))
        {
            FButtonStyle Style=Button->GetStyle();
            const bool bPrimary=bMain && N==TEXT("SinglePlayerButton");
            const FLinearColor Edge=bLight?FLinearColor(.58f,.53f,.48f,1):FLinearColor(.34f,.29f,.22f,1);
            const FLinearColor Normal=bPrimary?FLinearColor(.34f,.025f,.055f,1):(bLight?FLinearColor(.96f,.94f,.90f,1):FLinearColor(.025f,.035f,.07f,.92f));
            Style.SetNormal(FSlateRoundedBoxBrush(Normal,3.f,Edge,1.f));
            Style.SetHovered(FSlateRoundedBoxBrush(bPrimary?FLinearColor(.46f,.035f,.07f,1):(bLight?FLinearColor(.98f,.85f,.82f,1):FLinearColor(.24f,.025f,.065f,1)),3.f,FLinearColor(.5f,.055f,.09f,1),1.5f));
            Style.SetPressed(FSlateRoundedBoxBrush(bLight?FLinearColor(.76f,.67f,.64f,1):FLinearColor(.11f,.014f,.035f,1),3.f,FLinearColor(.35f,.045f,.075f,1),1.f));
            Style.SetDisabled(FSlateRoundedBoxBrush(bLight?FLinearColor(.72f,.71f,.69f,.8f):FLinearColor(.025f,.03f,.05f,.65f),3.f,bLight?FLinearColor(.5f,.49f,.46f,1):FLinearColor(.17f,.18f,.22f,1),1.f));
            Style.SetNormalPadding(bCard?FMargin(6,3):FMargin(16,8)); Style.SetPressedPadding(bCard?FMargin(6,4,6,2):FMargin(16,10,16,6));
            if (UButtonSlot* ContentSlot=Cast<UButtonSlot>(Button->GetContent()?Button->GetContent()->Slot:nullptr))
            {
                ContentSlot->SetHorizontalAlignment(bCard&&N!=TEXT("PurchaseButton")?HAlign_Fill:HAlign_Center);
                ContentSlot->SetVerticalAlignment(bCard&&N!=TEXT("PurchaseButton")?VAlign_Fill:VAlign_Center);
            }
            Button->SetStyle(Style); Button->SetBackgroundColor(FLinearColor::White); Button->SetColorAndOpacity(FLinearColor::White);
        }
        if (UTextBlock* Label=Cast<UTextBlock>(Widget))
        {
            const bool bButton=IsInsideButton(Label) && (!bCard || N.Contains(TEXT("PurchaseLabel")) || N.Contains(TEXT("ButtonLabel")));
            const bool bTitle=N.Contains(TEXT("Title")) || N.Contains(TEXT("Header"));
            const bool bSubtle=N.Contains(TEXT("Hint")) || N.Contains(TEXT("Subtitle")) || N.Contains(TEXT("Footer"));
            FSlateFontInfo Font=Label->GetFont();
            if (bCard) { Font.Size=bTitle?FMath::Max(Font.Size,22):FMath::Max(Font.Size,16); }
            else if (!Name.Contains(TEXT("HealthBar")) && !Name.Contains(TEXT("Vitals"))) { Font.Size=FMath::Max(Font.Size,bButton?16:15); }
            if (bMain && bButton) { Font.Size=24; }
            if (bMain && N==TEXT("TitleText")) { Font.Size=58; }
            if (bMain && N==TEXT("SubtitleText")) { Font.Size=18; }
            Label->SetFont(Font);
            if (bButton) { Label->SetJustification(ETextJustify::Center); }
            if (bCard && (N.Contains(TEXT("Desc")) || N.Contains(TEXT("Prerequisite"))))
            {
                Label->SetAutoWrapText(true); Label->SetLineHeightPercentage(1.15f);
                if (UVerticalBoxSlot* Vertical=Cast<UVerticalBoxSlot>(Label->Slot)) { Vertical->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); Vertical->SetVerticalAlignment(VAlign_Top); }
            }
            const bool bPrimaryLabel=bMain && N==TEXT("SinglePlayerButtonLabel");
            FLinearColor Color=bPrimaryLabel?Cream:((bLight||bCard)?(bTitle?FLinearColor(.36f,.02f,.055f,1):Ink):(bButton?Cream:(bSubtle?FLinearColor(.7f,.72f,.79f,1):(bTitle?Gold:Cream))));
            Label->SetColorAndOpacity(Color);
            Label->SetShadowOffset((bLight||bCard)?FVector2D::ZeroVector:FVector2D(0,1));
            Label->SetShadowColorAndOpacity(FLinearColor(0,0,.01f,(bLight||bCard)?0:.8f));
        }
    }
    if (bCard && (Name==TEXT("WBP_UpgradeCard") || Name==TEXT("WBP_ShopOfferCard")))
    {
        if (USizeBox* Size=Cast<USizeBox>(BP->WidgetTree->RootWidget))
        { Size->SetWidthOverride(350); Size->SetHeightOverride(Name==TEXT("WBP_UpgradeCard")?320:226); }
        if (UBorder* Card=Cast<UBorder>(BP->WidgetTree->FindWidget(TEXT("Border_Card")))) { Card->SetPadding(FMargin(28,26)); }
    }
    if (bShop || bSelection)
    {
        if (UCanvasPanel* Root=Cast<UCanvasPanel>(BP->WidgetTree->RootWidget))
        {
            UBorder* Shade=BP->WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(),TEXT("TouhouModalShade"));
            Shade->SetBrushColor(FLinearColor(.005f,.007f,.025f,.76f)); Stretch(Root,Shade);
            CastChecked<UCanvasPanelSlot>(Shade->Slot)->SetZOrder(-20);
            if (UBorder* Panel=Cast<UBorder>(BP->WidgetTree->FindWidget(TEXT("Border_Background"))))
            {
                Panel->SetBrush(TextureBrush(Paper)); Panel->SetBrushColor(FLinearColor::White);
                Panel->SetPadding(FMargin(40)); Panel->SetHorizontalAlignment(HAlign_Fill); Panel->SetVerticalAlignment(VAlign_Fill);
                UCanvasPanelSlot* Slot=CastChecked<UCanvasPanelSlot>(Panel->Slot);
                Slot->SetAnchors(FAnchors(.5f,.5f)); Slot->SetAlignment(FVector2D(.5f,.5f)); Slot->SetPosition(FVector2D::ZeroVector);
                Slot->SetSize(bShop?FVector2D(1340,800):FVector2D(1280,560)); Slot->SetZOrder(0);
            }
        }
        if (UUniformGridPanel* Grid=Cast<UUniformGridPanel>(BP->WidgetTree->FindWidget(TEXT("UniformGridPanel_Offers")))) { Grid->SetSlotPadding(FMargin(10,10)); }
    }
    if (bMain)
    {
        if (UBorder* Menu=Cast<UBorder>(BP->WidgetTree->FindWidget(TEXT("MenuPanel"))))
        {
            Menu->SetPadding(FMargin(30,34));
            if (UCanvasPanelSlot* Slot=Cast<UCanvasPanelSlot>(Menu->Slot))
            {
                Slot->SetAnchors(FAnchors(0,.5f));Slot->SetAlignment(FVector2D(0,.5f));
                Slot->SetPosition(FVector2D(110,0));Slot->SetSize(FVector2D(460,650));
            }
        }
        if (UCanvasPanel* Root=Cast<UCanvasPanel>(BP->WidgetTree->RootWidget))
        {
            UImage* Art=Cast<UImage>(BP->WidgetTree->FindWidget(TEXT("TouhouBackgroundArt")));
            if (!Art) { Art=BP->WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(),TEXT("TouhouBackgroundArt")); Stretch(Root,Art); }
            Art->SetBrushFromTexture(Background); Art->SetVisibility(ESlateVisibility::HitTestInvisible);
            CastChecked<UCanvasPanelSlot>(Art->Slot)->SetZOrder(-1000);
        }
    }
    return RoundWidgetCorners(BP);
}

FString URogueUIThemeBuilder::DescribePresentation(UWidgetBlueprint* BP)
{
    if (!BP || !BP->WidgetTree) { return TEXT("invalid"); }
    FString Result;
    TArray<UWidget*> Widgets; BP->WidgetTree->GetAllWidgets(Widgets);
    for (UWidget* Widget:Widgets)
    {
        Result+=Widget->GetClass()->GetName()+TEXT(":")+Widget->GetName();
        if (const UCanvasPanelSlot* Slot=Cast<UCanvasPanelSlot>(Widget->Slot))
        { Result+=FString::Printf(TEXT(" pos=%s size=%s anchors=%s/%s"),*Slot->GetPosition().ToString(),*Slot->GetSize().ToString(),*Slot->GetAnchors().Minimum.ToString(),*Slot->GetAnchors().Maximum.ToString()); }
        if (const UTextBlock* Label=Cast<UTextBlock>(Widget)) { Result+=FString::Printf(TEXT(" font=%.1f text=%s"),static_cast<double>(Label->GetFont().Size),*Label->GetText().ToString()); }
        Result+=TEXT("\n");
    }
    for (const FDelegateEditorBinding& Binding : BP->Bindings)
    { Result+=TEXT("Binding:")+Binding.ObjectName+TEXT(".")+Binding.PropertyName.ToString()+TEXT(" -> ")+Binding.FunctionName.ToString()+TEXT("\n"); }
    TArray<UEdGraph*> Graphs; BP->GetAllGraphs(Graphs);
    for (UEdGraph* Graph:Graphs)
    {
        for (UEdGraphNode* Node:Graph->Nodes)
        {
            Result+=TEXT("GraphNode:")+Node->GetClass()->GetName()+TEXT(":")+Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString()+TEXT("\n");
        }
    }
    return Result;
}


UUserWidget* CreateCardPagePreview(APlayerController* Owner,int32 Page)
{
    if (!Owner) { return nullptr; }
    const TCHAR* Titles[]={TEXT("灵符连诵"),TEXT("炉心扩散"),TEXT("神威增幅"),TEXT("生命强化"),TEXT("灵力强化"),TEXT("吸取范围")};
    const TCHAR* Descs[]={TEXT("每次攻击额外追加一轮御札连射。连续选取可叠加连发次数。"),TEXT("八卦炉的爆炸半径增加，覆盖更广的范围。"),TEXT("御柱锁定同一敌人时，每跳伤害递增值增加 0.1。"),TEXT("提高最大生命值，并补充新增部分的生命。"),TEXT("提高武器伤害，让每次攻击更有力量。"),TEXT("扩大经验球的吸取范围，更从容地穿行于弹幕之间。")};
    if (Page==1)
    {
        UClass* Class=LoadClass<URogueShopWidgetBase>(nullptr,TEXT("/Game/UMG/Menus/Shop/WBP_Shop.WBP_Shop_C"));
        URogueShopWidgetBase* Widget=CreateWidget<URogueShopWidgetBase>(Owner,Class);if(!Widget)return nullptr;
        Widget->AddToViewport(200);
        FRogueShopViewData View;View.bVisible=true;View.TitleText=FText::FromString(TEXT("香霖堂 · 补给商店"));View.MoneyText=FText::FromString(TEXT("金币  120"));
        View.HintText=FText::FromString(TEXT("点击卡牌购买本局强化"));View.AutoRefreshText=FText::FromString(TEXT("自动补货 45 秒"));View.RefreshButtonText=FText::FromString(TEXT("刷新商品 · 14 金币"));
        for(int32 I=0;I<6;++I){FRogueShopOfferViewData Card;Card.TitleText=FText::FromString(Titles[I]);Card.DescriptionText=FText::FromString(Descs[I]);Card.CostText=FText::FromString(I==5?TEXT("金币不足 · 140 金币"):TEXT("购买 · 22 金币"));Card.bAffordable=I!=5;Card.bPurchased=I==4;if(Card.bPurchased)Card.CostText=FText::FromString(TEXT("已购买"));View.Offers.Add(Card);}
        Widget->UpdateShopView(View);return Widget;
    }
    UClass* Class=LoadClass<URogueUpgradeSelectionWidgetBase>(nullptr,TEXT("/Game/UMG/Menus/Upgrade/WBP_UpgradeSelection.WBP_UpgradeSelection_C"));
    URogueUpgradeSelectionWidgetBase* Widget=CreateWidget<URogueUpgradeSelectionWidgetBase>(Owner,Class);if(!Widget)return nullptr;
    Widget->AddToViewport(200);FRogueUpgradeSelectionViewData View;View.bVisible=true;View.TitleText=FText::FromString(TEXT("灵力觉醒 · 选择一项强化"));
    for(int32 I=0;I<3;++I){FRogueUpgradeCardViewData Card;Card.TitleText=FText::FromString(Titles[I]);Card.DescriptionText=FText::FromString(Descs[I]);View.Cards.Add(Card);}
    Widget->UpdateUpgradeSelectionView(View);return Widget;
}


bool URogueUIThemeBuilder::BuildReadableTreeNode(UWidgetBlueprint* BP)
{
    if(!BP||!BP->WidgetTree)return false;
    BP->Modify();BP->WidgetTree->Modify();
    USizeBox* Root=BP->WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(),TEXT("ThemeNodeRoot"));
    Root->SetWidthOverride(360);Root->SetHeightOverride(320);BP->WidgetTree->RootWidget=Root;
    UBorder* Border=BP->WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(),TEXT("StateBorder"));
    Border->SetPadding(FMargin(22,20));Root->AddChild(Border);
    UVerticalBox* Body=BP->WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(),TEXT("NodeBody"));Border->AddChild(Body);
    auto AddText=[BP,Body](FName Name,const TCHAR* Value,int32 Size,FMargin Padding=FMargin(0,0,0,6))
    {
        UTextBlock* Label=Text(BP,Name,Value,Size,Ink);Label->SetAutoWrapText(true);
        UVerticalBoxSlot* Slot=Body->AddChildToVerticalBox(Label);Slot->SetPadding(Padding);return Label;
    };
    AddText(TEXT("TitleText"),TEXT("武器 / 专属升级"),22,FMargin(0,0,0,12));
    UTextBlock* Desc=AddText(TEXT("DescriptionText"),TEXT("解锁后加入自己的升级卡池。"),16);
    CastChecked<UVerticalBoxSlot>(Desc->Slot)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    AddText(TEXT("PrerequisiteText"),TEXT("需要：前置节点"),14);
    AddText(TEXT("StateText"),TEXT("前置未解锁"),14);
    AddText(TEXT("RankText"),TEXT(""),14);
    UHorizontalBox* Footer=BP->WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(),TEXT("NodeFooter"));Body->AddChildToVerticalBox(Footer);
    UTextBlock* Cost=Text(BP,TEXT("CostText"),TEXT("5 冰砖 / 次"),16,Ink);
    UHorizontalBoxSlot* CostSlot=Footer->AddChildToHorizontalBox(Cost);CostSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));CostSlot->SetVerticalAlignment(VAlign_Center);
    UButton* Buy=BP->WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(),TEXT("PurchaseButton"));
    UTextBlock* Label=Text(BP,TEXT("PurchaseLabel"),TEXT("解锁"),16,Ink);Label->SetJustification(ETextJustify::Center);Buy->AddChild(Label);Footer->AddChildToHorizontalBox(Buy);
    Finish(BP);return BP->Status!=BS_Error;
}


bool URogueUIThemeBuilder::BuildReadableCardPage(UWidgetBlueprint* BP)
{
    if(!BP||!BP->WidgetTree)return false;
    const FString Name=BP->GetName();
    const bool bCard=Name==TEXT("WBP_UpgradeCard")||Name==TEXT("WBP_ShopOfferCard");
    const bool bShop=Name==TEXT("WBP_Shop")||Name==TEXT("WBP_ShopOfferCard");
    BP->Modify();BP->WidgetTree->Modify();
    if(bCard)
    {
        USizeBox* Root=BP->WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(),TEXT("CardRoot"));Root->SetWidthOverride(350);Root->SetHeightOverride(bShop?226:320);BP->WidgetTree->RootWidget=Root;
        UButton* Click=BP->WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(),bShop?TEXT("Button_Offer"):TEXT("Button_Select"));Root->AddChild(Click);
        UBorder* Paper=BP->WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(),TEXT("Border_Card"));Paper->SetPadding(FMargin(28,26));Click->AddChild(Paper);
        UCanvasPanel* Body=BP->WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(),TEXT("CardBody"));Paper->AddChild(Body);
        UTextBlock* Title=Text(BP,TEXT("Text_Title"),TEXT("升级卡"),22,Ink);Place(Body,Title,FVector2D::ZeroVector,FVector2D(282,32));
        UTextBlock* Desc=Text(BP,TEXT("Text_Desc"),TEXT("卡牌效果说明"),16,Ink);Desc->SetAutoWrapText(true);Desc->SetLineHeightPercentage(1.15f);
        Place(Body,Desc,FVector2D(0,44),FVector2D(282,bShop?88:150));
        if(bShop)Place(Body,Text(BP,TEXT("Text_Cost"),TEXT("22 金币"),18,Ink),FVector2D(0,140),FVector2D(282,28));
        else
        {
            UTextBlock* Hint=Text(BP,TEXT("ChoiceHint"),TEXT("点击选择"),15,FLinearColor(.26f,.21f,.24f,1));Hint->SetJustification(ETextJustify::Right);
            Place(Body,Hint,FVector2D(0,226),FVector2D(282,24));
        }
        BP->Bindings.RemoveAll([](const FDelegateEditorBinding& B){return B.ObjectName==TEXT("Button_Offer")&&(B.PropertyName==TEXT("BackgroundColor")||B.PropertyName==TEXT("ColorAndOpacity"));});
        if (bShop)
        {
            // 旧卡牌在视图事件里直接写蓝色按钮底色，会覆盖设计器的新样式。
            // 只移除 Button_Offer 的旧底色赋值，保留文本事件和购买回调。
            TArray<UEdGraph*> Graphs; BP->GetAllGraphs(Graphs);
            for (UEdGraph* Graph : Graphs)
            {
                const TArray<TObjectPtr<UEdGraphNode>> Nodes=Graph->Nodes;
                for (UEdGraphNode* Node : Nodes)
                {
                    UK2Node_VariableSet* Set=Cast<UK2Node_VariableSet>(Node);
                    if (!Set || Set->GetVarName()!=TEXT("BackgroundColor")) continue;
                    UEdGraphPin* Target=Set->FindPin(TEXT("self"));
                    bool bOffer=false;
                    if(Target)for(UEdGraphPin* Link:Target->LinkedTo)
                    {
                        const UK2Node_VariableGet* Get=Cast<UK2Node_VariableGet>(Link->GetOwningNode());
                        bOffer|=Get && Get->GetVarName()==TEXT("Button_Offer");
                    }
                    if(!bOffer)continue;
                    UEdGraphPin* In=Set->GetExecPin(); UEdGraphPin* Out=Set->GetThenPin();
                    const TArray<UEdGraphPin*> Previous=In?In->LinkedTo:TArray<UEdGraphPin*>();
                    const TArray<UEdGraphPin*> Next=Out?Out->LinkedTo:TArray<UEdGraphPin*>();
                    FBlueprintEditorUtils::RemoveNode(BP,Set,true);
                    for(UEdGraphPin* A:Previous)for(UEdGraphPin* B:Next)A->MakeLinkTo(B);
                }
            }
        }
    }
    else
    {
        UCanvasPanel* Root=Reset(BP);
        UBorder* Shade=BP->WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(),TEXT("TouhouModalShade"));Shade->SetBrushColor(FLinearColor(.01f,.012f,.03f,.72f));Stretch(Root,Shade);
        UBorder* Panel=BP->WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(),TEXT("Border_Background"));Panel->SetPadding(FMargin(40));
        Place(Root,Panel,FVector2D::ZeroVector,bShop?FVector2D(1340,800):FVector2D(1280,560),FAnchors(.5f,.5f),FVector2D(.5f,.5f));
        UCanvasPanel* Body=BP->WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(),TEXT("PageContent"));Panel->AddChild(Body);
        UTextBlock* Title=Text(BP,TEXT("Text_Title"),bShop?TEXT("补给商店"):TEXT("请选择升级"),30,Ink);
        Place(Body,Title,FVector2D::ZeroVector,bShop?FVector2D(820,48):FVector2D(1200,48));
        if(bShop)
        {
            UTextBlock* Money=Text(BP,TEXT("Text_Money"),TEXT("金币 0"),22,Ink);Money->SetJustification(ETextJustify::Right);Place(Body,Money,FVector2D(930,6),FVector2D(300,34));
            Place(Body,Text(BP,TEXT("Text_Hint"),TEXT("点击卡牌购买本局强化"),16,Ink),FVector2D(0,62),FVector2D(820,28));
            UTextBlock* Refresh=Text(BP,TEXT("Text_AutoRefresh"),TEXT("自动补货 60 秒"),16,Ink);Refresh->SetJustification(ETextJustify::Right);Place(Body,Refresh,FVector2D(930,62),FVector2D(300,28));
            UUniformGridPanel* Grid=BP->WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass(),TEXT("UniformGridPanel_Offers"));Grid->SetSlotPadding(FMargin(10,8));Place(Body,Grid,FVector2D(25,112),FVector2D(1200,500));
            auto Action=[BP,Body](FName ButtonName,FName TextName,const TCHAR* Caption,FVector2D Position,FVector2D Size)
            {
                UButton* Button=BP->WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(),ButtonName);UTextBlock* Label=Text(BP,TextName,Caption,18,Ink);Label->SetJustification(ETextJustify::Center);Button->AddChild(Label);Place(Body,Button,Position,Size);
            };
            Action(TEXT("Button_Refresh"),TEXT("Text_Refresh"),TEXT("刷新商品"),FVector2D(670,634),FVector2D(300,48));
            Action(TEXT("Button_Close"),TEXT("CloseLabel"),TEXT("退出商店"),FVector2D(995,634),FVector2D(230,48));
        }
        else
        {
            Title->SetJustification(ETextJustify::Center);
            UTextBlock* Hint=Text(BP,TEXT("PageHint"),TEXT("每次选择一项强化"),16,Ink);Hint->SetJustification(ETextJustify::Center);Place(Body,Hint,FVector2D(0,62),FVector2D(1200,28));
            UHorizontalBox* Cards=BP->WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(),TEXT("HorizontalBox_Cards"));Place(Body,Cards,FVector2D(45,118),FVector2D(1110,320));
        }
    }
    Finish(BP);return BP->Status!=BS_Error;
}


bool URogueUIThemeBuilder::RoundWidgetCorners(UWidgetBlueprint* BP)
{
    if(!BP||!BP->WidgetTree||BP->GetName()==TEXT("WBP_DamageFlash"))return false;
    BP->Modify();BP->WidgetTree->Modify();
    auto Rounded=[](FSlateBrush& Brush,float Radius)
    {
        Brush.DrawAs=ESlateBrushDrawType::RoundedBox;
        Brush.OutlineSettings.CornerRadii=FVector4(Radius,Radius,Radius,Radius);
        Brush.OutlineSettings.RoundingType=ESlateBrushRoundingType::FixedRadius;
    };
    TArray<UWidget*> Widgets;BP->WidgetTree->GetAllWidgets(Widgets);
    const bool bCard=BP->GetName().Contains(TEXT("Card"))||BP->GetName().Contains(TEXT("TreeNode"));
    for(UWidget* Widget:Widgets)
    {
        Widget->Modify();
        if(UBorder* Border=Cast<UBorder>(Widget))
        {
            const bool bViewportBackground=IsFullScreen(Border) && Border->GetParent()==BP->WidgetTree->RootWidget && BP->GetName()!=TEXT("WBP_EnemyHealthBar");
            if(!bViewportBackground)
            {
                FSlateBrush Brush=Border->Background;Rounded(Brush,bCard?14.f:16.f);Border->SetBrush(Brush);
            }
        }
        if(UButton* Button=Cast<UButton>(Widget))
        {
            FButtonStyle Style=Button->GetStyle();
            const float Radius=bCard&&Button->GetName()!=TEXT("PurchaseButton")?16.f:10.f;
            Rounded(Style.Normal,Radius);Rounded(Style.Hovered,Radius);Rounded(Style.Pressed,Radius);Rounded(Style.Disabled,Radius);Button->SetStyle(Style);
        }
        if(UEditableTextBox* Input=Cast<UEditableTextBox>(Widget))
        {
            Rounded(Input->WidgetStyle.BackgroundImageNormal,10);Rounded(Input->WidgetStyle.BackgroundImageHovered,10);
            Rounded(Input->WidgetStyle.BackgroundImageFocused,10);Rounded(Input->WidgetStyle.BackgroundImageReadOnly,10);
        }
        if(UProgressBar* Bar=Cast<UProgressBar>(Widget))
        {
            FProgressBarStyle Style=Bar->GetWidgetStyle();
            Rounded(Style.BackgroundImage,5);Rounded(Style.FillImage,5);Rounded(Style.MarqueeImage,5);Bar->SetWidgetStyle(Style);
        }
    }
    Finish(BP);return BP->Status!=BS_Error;
}
