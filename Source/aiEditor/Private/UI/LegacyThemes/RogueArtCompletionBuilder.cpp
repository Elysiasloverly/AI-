// 历史风格生成器：仅供显式重建/研究；不会在启动或当前UI维护入口自动运行。
#include "RogueArtCompletionBuilder.h"
#include "RogueWidgetBuilder.h"
#include "Engine/Font.h"
#include "UI/RoguePresentationTheme.h"
#include "UI/Widgets/RogueOwnedItemsWidget.h"
#include "UI/Widgets/RogueCardMotionWidgetBase.h"
#include "UI/Widgets/RogueUpgradeTreeNodeWidgetBase.h"
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/SizeBox.h"
#include "Components/ScaleBox.h"
#include "Engine/Texture2D.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"

namespace RogueArtBuilder
{
    template<typename T>T* Make(UWidgetBlueprint* BP,const TCHAR* Name)
    {auto* Found=Cast<T>(BP->WidgetTree->FindWidget(FName(Name)));return Found?Found:BP->WidgetTree->ConstructWidget<T>(T::StaticClass(),FName(Name));}
    void Place(UCanvasPanel* Canvas,UWidget* W,FVector2D Pos,FVector2D Size,FAnchors Anchors=FAnchors(0))
    {auto* Slot=Cast<UCanvasPanelSlot>(W->Slot);if(!Slot)Slot=Canvas->AddChildToCanvas(W);Slot->SetAnchors(Anchors);Slot->SetPosition(Pos);Slot->SetSize(Size);}
    bool Finish(UWidgetBlueprint* BP)
    {FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FKismetEditorUtilities::CompileBlueprint(BP);BP->MarkPackageDirty();return BP->Status!=BS_Error;}
    UTextBlock* Text(UWidgetBlueprint* BP,const TCHAR* Name,const TCHAR* Value,int Size,FLinearColor Color)
    {auto* T=Make<UTextBlock>(BP,Name);T->SetText(FText::FromString(Value));T->SetFont(FSlateFontInfo(URogueWidgetBuilder::GetOrCreateProjectFont(),Size));T->SetColorAndOpacity(Color);T->SetVisibility(ESlateVisibility::HitTestInvisible);return T;}
}
int32 URogueArtCompletionBuilder::RoundImages(UWidgetBlueprint* BP, float Radius)
{
    if (!BP || !BP->WidgetTree || !FMath::IsFinite(Radius)) return -1;
    BP->Modify();
    BP->WidgetTree->Modify();
    TArray<UWidget*> Widgets;
    BP->WidgetTree->GetAllWidgets(Widgets);
    int32 Changed = 0;
    for (UWidget* Widget : Widgets)
    {
        auto* Image = Cast<UImage>(Widget);
        if (!Image || Image->GetBrush().DrawAs == ESlateBrushDrawType::NoDrawType) continue;
        FSlateBrush Brush = Image->GetBrush();
        FVector2D Size(Brush.ImageSize);
        if (const auto* Slot = Cast<UCanvasPanelSlot>(Image->Slot))
        {
            const auto Anchors = Slot->GetAnchors();
            const auto SlotSize = Slot->GetSize();
            if (Anchors.Minimum.X == Anchors.Maximum.X && SlotSize.X > 0) Size.X = SlotSize.X;
            if (Anchors.Minimum.Y == Anchors.Maximum.Y && SlotSize.Y > 0) Size.Y = SlotSize.Y;
        }
        // 拉伸布局及自适应图片的设计期尺寸可能为 0：它表示尚未布局，不是实际宽高。
        // 只用已知的正尺寸约束圆角，避免把结算大插画误压成 0.5 单位的小圆角。
        float EffectiveRadius = FMath::Max(.5f, Radius);
        if (Size.X > 0.f) EffectiveRadius = FMath::Min(EffectiveRadius, FMath::Max(.5f, static_cast<float>(Size.X) * .5f));
        if (Size.Y > 0.f) EffectiveRadius = FMath::Min(EffectiveRadius, FMath::Max(.5f, static_cast<float>(Size.Y) * .5f));
        const auto& Corners = Brush.OutlineSettings.CornerRadii;
        const bool bLegacyTinyRadius = FMath::IsNearlyEqual(Corners.X, .5) && FMath::IsNearlyEqual(Corners.Y, .5) &&
            FMath::IsNearlyEqual(Corners.Z, .5) && FMath::IsNearlyEqual(Corners.W, .5) && EffectiveRadius > .5f;
        if (Brush.DrawAs == ESlateBrushDrawType::RoundedBox && Corners.X > 0.f && !bLegacyTinyRadius) continue;
        Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
        Brush.OutlineSettings = FSlateBrushOutlineSettings(EffectiveRadius);
        Image->Modify();
        Image->SetBrush(Brush);
        ++Changed;
    }
    return Changed > 0 && !RogueArtBuilder::Finish(BP) ? -1 : Changed;
}

bool URogueArtCompletionBuilder::AddCardIllustrations(UWidgetBlueprint* BP,URoguePresentationTheme* Theme)
{
    using namespace RogueArtBuilder;if(!BP||!BP->WidgetTree||!Theme)return false;BP->Modify();BP->WidgetTree->Modify();
    auto* Icon=Make<UImage>(BP,TEXT("ArtIcon"));Icon->SetBrushFromMaterial(Theme->GetCardIcon(ERogueUpgradeType::ProjectileCount));Icon->SetVisibility(ESlateVisibility::HitTestInvisible);
    if(auto* Body=Cast<UCanvasPanel>(BP->WidgetTree->FindWidget(TEXT("CardBody"))))
    {
        Place(Body,Icon,FVector2D(0,0),FVector2D(58,58));
        auto Rect=[&](const TCHAR* N,FVector2D P,FVector2D S){if(auto* W=BP->WidgetTree->FindWidget(N))Place(Body,W,P,S);};
        Rect(TEXT("Text_Title"),FVector2D(70,4),FVector2D(212,54));
        if(auto* Title=Cast<UTextBlock>(BP->WidgetTree->FindWidget(TEXT("Text_Title")))){auto F=Title->GetFont();F.Size=20;Title->SetFont(F);Title->SetAutoWrapText(true);}
        Rect(TEXT("TitleHairline"),FVector2D(0,69),FVector2D(282,1));
        const bool Shop=BP->GetName()==TEXT("WBP_ShopOfferCard");
        Rect(TEXT("Text_Desc"),FVector2D(0,80),FVector2D(282,Shop?94:132));
        if(Shop){Rect(TEXT("Text_Cost"),FVector2D(0,192),FVector2D(282,28));Rect(TEXT("CostHairline"),FVector2D(0,181),FVector2D(282,1));}
        if(auto* Old=BP->WidgetTree->FindWidget(TEXT("Spirit_CardSeal")))Old->SetRenderOpacity(.035f);
    }
    else if(auto* NodeBody=Cast<UVerticalBox>(BP->WidgetTree->FindWidget(TEXT("NodeBody"))))
    {
        if(auto* RootSize=Cast<USizeBox>(BP->WidgetTree->RootWidget))
        {RootSize->ClearHeightOverride();RootSize->SetMinDesiredHeight(320.f);}
        auto* Header=Make<UHorizontalBox>(BP,TEXT("ArtNodeHeader"));
        auto* Title=BP->WidgetTree->FindWidget(TEXT("TitleText"));if(!Title)return false;
        if(Header->GetParent()!=NodeBody){Title->RemoveFromParent();NodeBody->InsertChildAt(0,Header);}
        auto* Box=Make<USizeBox>(BP,TEXT("ArtNodeIconSize"));Box->SetWidthOverride(48);Box->SetHeightOverride(48);Box->AddChild(Icon);
        auto* I=Header->AddChildToHorizontalBox(Box);I->SetPadding(FMargin(0,0,12,5));
        auto* S=Header->AddChildToHorizontalBox(Title);S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));S->SetVerticalAlignment(VAlign_Center);
        if(auto* Desc=Cast<UTextBlock>(BP->WidgetTree->FindWidget(TEXT("DescriptionText"))))
        {
            // Explicit wrapping also gives the prepass a reliable text height
            // before the canvas has assigned the final node geometry.
            Desc->SetWrapTextAt(278.f);Desc->SetAutoWrapText(true);
            auto* DescriptionSize=Make<USizeBox>(BP,TEXT("ArtNodeDescriptionSize"));DescriptionSize->SetMinDesiredHeight(112.f);
            if(Desc->GetParent()!=DescriptionSize){Desc->RemoveFromParent();NodeBody->InsertChildAt(1,DescriptionSize);DescriptionSize->AddChild(Desc);}
            if(auto* DescSlot=Cast<UVerticalBoxSlot>(DescriptionSize->Slot)){DescSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));DescSlot->SetPadding(FMargin(0,0,0,10));}
        }
    }
    else return false;
    if(!Finish(BP))return false;
    if(auto* CDO=Cast<URogueCardMotionWidgetBase>(BP->GeneratedClass->GetDefaultObject()))CDO->PresentationTheme=Theme;
    if(auto* CDO=Cast<URogueUpgradeTreeNodeWidgetBase>(BP->GeneratedClass->GetDefaultObject()))CDO->PresentationTheme=Theme;
    BP->MarkPackageDirty();return true;
}
bool URogueArtCompletionBuilder::AddCardStateSeal(UWidgetBlueprint* BP, UMaterialInterface* Material)
{
    using namespace RogueArtBuilder;
    if (!BP || !BP->WidgetTree || !Material) return false;
    BP->Modify();
    BP->WidgetTree->Modify();
    auto* Seal = Make<UImage>(BP, TEXT("CardStateSeal"));
    Seal->SetBrushFromMaterial(Material);
    Seal->SetVisibility(ESlateVisibility::HitTestInvisible);
    if (auto* Body = Cast<UCanvasPanel>(BP->WidgetTree->FindWidget(TEXT("CardBody"))))
    {
        const bool bShop = BP->GetName() == TEXT("WBP_ShopOfferCard");
        Place(Body, Seal, FVector2D(251, bShop ? 185 : 231), FVector2D(32, 32));
        if (bShop)
            if (auto* Cost = BP->WidgetTree->FindWidget(TEXT("Text_Cost")))
                if (auto* Slot = Cast<UCanvasPanelSlot>(Cost->Slot)) Slot->SetSize(FVector2D(245, Slot->GetSize().Y));
    }
    else if (auto* Header = Cast<UHorizontalBox>(BP->WidgetTree->FindWidget(TEXT("ArtNodeHeader"))))
    {
        auto* Size = Make<USizeBox>(BP, TEXT("StateSealSize"));
        Size->SetWidthOverride(27);
        Size->SetHeightOverride(27);
        if (Size->GetContent() != Seal) Size->SetContent(Seal);
        if (Size->GetParent() != Header)
        {
            auto* Slot = Header->AddChildToHorizontalBox(Size);
            Slot->SetVerticalAlignment(VAlign_Center);
            Slot->SetPadding(FMargin(5, 0, 0, 0));
        }
        Size->SetVisibility(ESlateVisibility::HitTestInvisible);
    }
    else return false;
    return Finish(BP);
}

bool URogueArtCompletionBuilder::BuildBattlePresentation(UWidgetBlueprint* BP,UMaterialInterface* Seal,UMaterialInterface* Veil)
{
    using namespace RogueArtBuilder;if(!BP||!BP->WidgetTree)return false;BP->Modify();BP->WidgetTree->Modify();
    auto* Root=Make<UCanvasPanel>(BP,TEXT("PresentationRoot"));BP->WidgetTree->RootWidget=Root;Root->SetVisibility(ESlateVisibility::HitTestInvisible);
    auto* Curtain=Make<UImage>(BP,TEXT("EntryVeil"));Curtain->SetBrushFromMaterial(Veil);Curtain->SetVisibility(ESlateVisibility::HitTestInvisible);Place(Root,Curtain,FVector2D::ZeroVector,FVector2D::ZeroVector,FAnchors(0,0,1,1));
    auto* Panel=Make<UBorder>(BP,TEXT("CuePanel"));Panel->SetBrush(FSlateRoundedBoxBrush(FLinearColor(.035f,.045f,.08f,.82f),14.f,FLinearColor(.75f,.60f,.33f,.55f),1.f));Panel->SetPadding(FMargin(0));Panel->SetVisibility(ESlateVisibility::HitTestInvisible);
    Place(Root,Panel,FVector2D(-310,134),FVector2D(620,122),FAnchors(.5f,0));
    auto* Body=Make<UCanvasPanel>(BP,TEXT("CueBody"));Panel->AddChild(Body);
    auto* Rune=Make<UImage>(BP,TEXT("CueSeal"));Rune->SetBrushFromMaterial(Seal);Rune->SetRenderOpacity(.65f);Rune->SetRenderTransformPivot(FVector2D(.5f,.5f));Rune->SetVisibility(ESlateVisibility::HitTestInvisible);Place(Body,Rune,FVector2D(18,17),FVector2D(88,88));
    Place(Body,Text(BP,TEXT("CueTitle"),TEXT("异变显现"),30,FLinearColor(.98f,.90f,.70f,1)),FVector2D(122,18),FVector2D(475,45));
    Place(Body,Text(BP,TEXT("CueSubtitle"),TEXT("妖力凝聚 · 留意符阵与弹幕"),17,FLinearColor(.78f,.83f,.91f,1)),FVector2D(122,75),FVector2D(475,30));
    return Finish(BP);
}
bool URogueArtCompletionBuilder::AddBakaStoryArt(UWidgetBlueprint* Battle, UWidgetBlueprint* Victory, UTexture2D* Departure, UTexture2D* Ending)
{
    using namespace RogueArtBuilder;
    if (!Battle || !Victory || !Departure || !Ending || !Battle->WidgetTree || !Victory->WidgetTree) return false;

    auto* BattleRoot = Cast<UCanvasPanel>(Battle->WidgetTree->RootWidget);
    auto* VictoryRoot = Cast<UCanvasPanel>(Victory->WidgetTree->RootWidget);
    auto* VictoryPanel = Cast<UBorder>(Victory->WidgetTree->FindWidget(TEXT("VictoryPanel")));
    auto* VictorySlot = VictoryPanel ? Cast<UCanvasPanelSlot>(VictoryPanel->Slot) : nullptr;
    auto* Title = Cast<UTextBlock>(Victory->WidgetTree->FindWidget(TEXT("TitleText")));
    auto* Summary = Cast<UTextBlock>(Victory->WidgetTree->FindWidget(TEXT("SummaryText")));
    if (!BattleRoot || !VictoryRoot || !VictorySlot || !Title || !Summary) return false;

    auto* Opening = Make<UImage>(Battle, TEXT("DepartureArt"));
    Opening->SetBrushFromTexture(Departure, true);
    Opening->SetVisibility(ESlateVisibility::Collapsed);
    Place(BattleRoot, Opening, FVector2D(-450, 126), FVector2D(400, 225), FAnchors(1.f, 0.f));

    auto* Illustration = Make<UImage>(Victory, TEXT("VictoryArt"));
    if (Illustration->GetParent() != VictoryRoot) Illustration->RemoveFromParent();
    if (auto* OldFrame = Victory->WidgetTree->FindWidget(TEXT("VictoryArtFrame"))) OldFrame->RemoveFromParent();
    Illustration->SetBrushFromTexture(Ending, true);
    Illustration->SetVisibility(ESlateVisibility::HitTestInvisible);
    Place(VictoryRoot, Illustration, FVector2D::ZeroVector, FVector2D::ZeroVector, FAnchors(.50f, .265f, .97f, .735f));
    CastChecked<UCanvasPanelSlot>(Illustration->Slot)->SetZOrder(2);

    VictorySlot->SetAnchors(FAnchors(.28f, .5f));
    VictorySlot->SetSize(FVector2D(850, 720));
    VictorySlot->SetZOrder(3);
    if (auto* ReturnLabel = Cast<UTextBlock>(Victory->WidgetTree->FindWidget(TEXT("MainMenuButtonLabel"))))
        ReturnLabel->SetAutoWrapText(false);
    Title->SetText(FText::FromString(TEXT("幻想乡挑战达成")));
    Summary->SetText(FText::FromString(TEXT("Baka 打完了这一圈。天亮了，下次再来！")));
    return Finish(Battle) && Finish(Victory);
}

bool URogueArtCompletionBuilder::AddJourneyMoments(UWidgetBlueprint* Battle, UWidgetBlueprint* Bar,
    UWidgetBlueprint* Victory, UWidgetBlueprint* Defeat, UTexture2D* Dusk, UTexture2D* Dawn, UMaterialInterface* IceBrick)
{
    using namespace RogueArtBuilder;
    if (!Battle || !Bar || !Victory || !Defeat || !Dusk || !Dawn || !IceBrick ||
        !Battle->WidgetTree || !Bar->WidgetTree || !Victory->WidgetTree || !Defeat->WidgetTree) return false;
    auto* BattleRoot = Cast<UCanvasPanel>(Battle->WidgetTree->RootWidget);
    auto* Items = Cast<UCanvasPanel>(Bar->WidgetTree->FindWidget(TEXT("ItemsCanvas")));
    auto* Win = Cast<UVerticalBox>(Victory->WidgetTree->FindWidget(TEXT("VictoryContent")));
    auto* Lose = Cast<UCanvasPanel>(Defeat->WidgetTree->FindWidget(TEXT("RestyledBody")));
    if (!BattleRoot || !Items || !Win || !Lose) return false;

    for (auto* BP : {Battle, Bar, Victory, Defeat})
    {
        BP->Modify();
        BP->WidgetTree->Modify();
    }
    const auto AddChapter = [&](const TCHAR* FrameName, const TCHAR* ImageName, UTexture2D* Texture)
    {
        auto* Frame = Make<UScaleBox>(Battle, FrameName);
        Frame->SetStretch(EStretch::ScaleToFit);
        Frame->SetVisibility(ESlateVisibility::HitTestInvisible);
        Place(BattleRoot, Frame, FVector2D::ZeroVector, FVector2D::ZeroVector, FAnchors(0, 0, 1, 1));
        CastChecked<UCanvasPanelSlot>(Frame->Slot)->SetZOrder(1);
        auto* Art = Make<UImage>(Battle, ImageName);
        Art->SetBrushFromTexture(Texture, true);
        Art->SetVisibility(ESlateVisibility::Collapsed);
        Frame->SetContent(Art);
    };
    AddChapter(TEXT("DuskFrame"), TEXT("DuskArt"), Dusk);
    AddChapter(TEXT("DawnFrame"), TEXT("DawnArt"), Dawn);
    if (auto* Cue = Battle->WidgetTree->FindWidget(TEXT("CuePanel")))
        if (auto* Slot = Cast<UCanvasPanelSlot>(Cue->Slot)) Slot->SetZOrder(2);

    const FLinearColor Ink(.13f, .22f, .36f, 1.f);
    auto* Reward = Make<UBorder>(Battle, TEXT("RewardPanel"));
    Reward->SetBrush(FSlateRoundedBoxBrush(FLinearColor(.032f, .066f, .12f, .94f), 14.f,
        FLinearColor(.53f, .83f, .94f, .82f), 1.f));
    Reward->SetPadding(FMargin(0));
    Reward->SetVisibility(ESlateVisibility::Collapsed);
    Place(BattleRoot, Reward, FVector2D(-306, 276), FVector2D(265, 72), FAnchors(1.f, 0.f));
    CastChecked<UCanvasPanelSlot>(Reward->Slot)->SetZOrder(3);
    auto* RewardBody = Make<UCanvasPanel>(Battle, TEXT("RewardBody"));
    Reward->SetContent(RewardBody);
    auto* BrickIcon = Make<UImage>(Battle, TEXT("IceBrickIcon"));
    BrickIcon->SetBrushFromMaterial(IceBrick);
    BrickIcon->SetVisibility(ESlateVisibility::HitTestInvisible);
    Place(RewardBody, BrickIcon, FVector2D(9, 8), FVector2D(56, 56));
    Place(RewardBody, Text(Battle, TEXT("RewardAmount"), TEXT("冰砖 +1"), 26, FLinearColor(.82f, .97f, 1.f, 1.f)),
        FVector2D(73, 19), FVector2D(187, 38));

    auto* FlyIcon = Make<UImage>(Bar, TEXT("AcquisitionIcon"));
    FlyIcon->SetVisibility(ESlateVisibility::Collapsed);
    Place(Items, FlyIcon, FVector2D::ZeroVector, FVector2D(106), FAnchors(1.f, 1.f));
    auto* FlySlot = CastChecked<UCanvasPanelSlot>(FlyIcon->Slot);
    FlySlot->SetAlignment(FVector2D(.5f));
    FlySlot->SetZOrder(10);

    for (auto* BP : {Victory, Defeat})
    {
        auto* Caption = Text(BP, TEXT("PersonalRunReward"), TEXT("本局获得 0 冰砖   ·   本局武器"), 20, Ink);
        auto* Loadout = Make<UHorizontalBox>(BP, TEXT("PersonalRunLoadout"));
        if (BP == Victory)
        {
            auto* Hint = BP->WidgetTree->FindWidget(TEXT("SaveHintText"));
            const int32 Index = Hint ? Win->GetChildIndex(Hint) : Win->GetChildrenCount() - 1;
            if (Caption->GetParent() != Win) Win->InsertChildAt(Index, Caption);
            if (Loadout->GetParent() != Win)
                if (auto* Slot = Cast<UVerticalBoxSlot>(Win->InsertChildAt(Win->GetChildIndex(Caption) + 1, Loadout)))
                    Slot->SetPadding(FMargin(0, 1, 0, 12));
        }
        else
        {
            Place(Lose, Caption, FVector2D(18, 282), FVector2D(690, 30));
            Place(Lose, Loadout, FVector2D(18, 322), FVector2D(690, 64));
        }
        for (int32 Index = 0; Index < 8; ++Index)
        {
            auto* Icon = Make<UImage>(BP, *FString::Printf(TEXT("PersonalWeapon%d"), Index));
            FSlateBrush WeaponBrush = Icon->GetBrush();
            WeaponBrush.ImageSize = FVector2D(60.f);
            Icon->SetBrush(WeaponBrush);
            Icon->SetVisibility(ESlateVisibility::Collapsed);
            if (Icon->GetParent() != Loadout)
                Loadout->AddChildToHorizontalBox(Icon)->SetPadding(FMargin(0, 0, 7, 0));
        }
    }
    if (auto* Panel = Victory->WidgetTree->FindWidget(TEXT("VictoryPanel")))
        if (auto* Slot = Cast<UCanvasPanelSlot>(Panel->Slot)) Slot->SetSize(FVector2D(850, 820));
    if (auto* Panel = Defeat->WidgetTree->FindWidget(TEXT("RestyledPanel")))
        if (auto* Slot = Cast<UCanvasPanelSlot>(Panel->Slot)) Slot->SetSize(FVector2D(820, 620));
    const TCHAR* Actions[] = {TEXT("Button_Restart"), TEXT("MainMenuButton"), TEXT("Button_Quit")};
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(Actions); ++Index)
        if (auto* Button = Defeat->WidgetTree->FindWidget(FName(Actions[Index])))
            Place(Lose, Button, FVector2D(18 + 232 * Index, 422), FVector2D(213, 60));

    const bool bBattle = Finish(Battle), bBar = Finish(Bar), bVictory = Finish(Victory), bDefeat = Finish(Defeat);
    return bBattle && bBar && bVictory && bDefeat;
}

bool URogueArtCompletionBuilder::AddBakaRecoveryArt(UWidgetBlueprint* Defeat, UWidgetBlueprint* Revival,
    UTexture2D* Illustration, UMaterialInterface* CountdownSeal)
{
    using namespace RogueArtBuilder;
    if (!Defeat || !Revival || !Defeat->WidgetTree || !Revival->WidgetTree || !Illustration || !CountdownSeal) return false;

    auto* Root = Cast<UCanvasPanel>(Defeat->WidgetTree->RootWidget);
    auto* Panel = Cast<UBorder>(Defeat->WidgetTree->FindWidget(TEXT("RestyledPanel")));
    auto* Body = Cast<UCanvasPanel>(Defeat->WidgetTree->FindWidget(TEXT("RestyledBody")));
    auto* Slot = Panel ? Cast<UCanvasPanelSlot>(Panel->Slot) : nullptr;
    auto* Title = Cast<UTextBlock>(Defeat->WidgetTree->FindWidget(TEXT("Text_Title")));
    auto* Summary = Cast<UTextBlock>(Defeat->WidgetTree->FindWidget(TEXT("Text_Summary")));
    auto* ReviveBody = Cast<UCanvasPanel>(Revival->WidgetTree->FindWidget(TEXT("RestyledBody")));
    auto* RevivePanel = Cast<UBorder>(Revival->WidgetTree->FindWidget(TEXT("RestyledPanel")));
    auto* ReviveSlot = RevivePanel ? Cast<UCanvasPanelSlot>(RevivePanel->Slot) : nullptr;
    auto* ReviveTitle = Cast<UTextBlock>(Revival->WidgetTree->FindWidget(TEXT("TextBlock_845")));
    auto* ReviveHint = Cast<UTextBlock>(Revival->WidgetTree->FindWidget(TEXT("TextBlock_920")));
    auto* Count = Cast<UTextBlock>(Revival->WidgetTree->FindWidget(TEXT("TextBlock")));
    auto* Footer = Cast<UTextBlock>(Revival->WidgetTree->FindWidget(TEXT("ReviveFooter")));
    const TCHAR* Actions[] = {TEXT("Button_Restart"), TEXT("MainMenuButton"), TEXT("Button_Quit")};
    if (!Root || !Body || !Slot || !Title || !Summary || !ReviveBody || !ReviveSlot || !ReviveTitle || !ReviveHint || !Count || !Footer) return false;
    for (const TCHAR* Name : Actions)
        if (!Cast<UButton>(Defeat->WidgetTree->FindWidget(FName(Name)))) return false;

    Defeat->Modify(); Defeat->WidgetTree->Modify(); Revival->Modify(); Revival->WidgetTree->Modify();
    auto* Picture = Make<UImage>(Defeat, TEXT("DefeatIllustration"));
    Picture->SetBrushFromTexture(Illustration, true);
    Picture->SetVisibility(ESlateVisibility::HitTestInvisible);
    Place(Root, Picture, FVector2D::ZeroVector, FVector2D::ZeroVector, FAnchors(.50f, .265f, .97f, .735f));
    CastChecked<UCanvasPanelSlot>(Picture->Slot)->SetZOrder(2);
    Slot->SetAnchors(FAnchors(.28f, .5f));
    Slot->SetSize(FVector2D(820.f, 510.f));
    Slot->SetZOrder(3);
    Title->SetJustification(ETextJustify::Left);
    Summary->SetJustification(ETextJustify::Left);
    auto* Caption = Text(Defeat, TEXT("DefeatChapter"), TEXT("夜色未尽  /  RETRY"), 20, FLinearColor(.48f, .14f, .25f, 1.f));
    Place(Body, Caption, FVector2D(18, 20), FVector2D(690, 38));
    Place(Body, Title, FVector2D(18, 76), FVector2D(690, 72));
    Place(Body, Summary, FVector2D(18, 170), FVector2D(690, 126));
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(Actions); ++Index)
    {
        auto* Button = CastChecked<UButton>(Defeat->WidgetTree->FindWidget(FName(Actions[Index])));
        if (auto* Label = Cast<UTextBlock>(Button->GetContent())) Label->SetAutoWrapText(false);
        Place(Body, Button, FVector2D(18 + 232 * Index, 334), FVector2D(213, 60));
    }

    RevivePanel->SetPadding(FMargin(28.f));
    ReviveSlot->SetAnchors(FAnchors(.5f, .76f));
    ReviveSlot->SetSize(FVector2D(690.f, 260.f));
    auto* Seal = Make<UImage>(Revival, TEXT("RevivalSeal"));
    Seal->SetBrushFromMaterial(CountdownSeal);
    Seal->SetVisibility(ESlateVisibility::HitTestInvisible);
    Place(ReviveBody, Seal, FVector2D(5, 10), FVector2D(180, 180));
    CastChecked<UCanvasPanelSlot>(Seal->Slot)->SetZOrder(1);
    const FLinearColor Frost(.64f, .87f, 1.f, 1.f);
    ReviveTitle->SetText(FText::FromString(TEXT("冰符重聚")));
    ReviveTitle->SetColorAndOpacity(Frost);
    ReviveHint->SetText(FText::FromString(TEXT("复活倒计时")));
    Count->SetColorAndOpacity(FLinearColor(.9f, .97f, 1.f, 1.f));
    Footer->SetText(FText::FromString(TEXT("队友仍在战斗，等待冰符重聚。")));
    for (auto* Label : {ReviveTitle, ReviveHint, Count, Footer})
        Label->SetJustification(ETextJustify::Left);
    Place(ReviveBody, ReviveTitle, FVector2D(209, 10), FVector2D(411, 52));
    Place(ReviveBody, ReviveHint, FVector2D(211, 87), FVector2D(178, 35));
    Place(ReviveBody, Count, FVector2D(395, 61), FVector2D(213, 77));
    Place(ReviveBody, Footer, FVector2D(211, 158), FVector2D(409, 38));
    return Finish(Defeat) && Finish(Revival);
}

bool URogueArtCompletionBuilder::DecorateFlowPage(UWidgetBlueprint* BP,UMaterialInterface* Seal)
{
    using namespace RogueArtBuilder;if(!BP||!BP->WidgetTree)return false;
    TArray<UWidget*> Widgets;BP->WidgetTree->GetAllWidgets(Widgets);
    UCanvasPanel* Canvas=nullptr;for(auto* W:Widgets)if(auto* C=Cast<UCanvasPanel>(W)){Canvas=C;break;}
    if(!Canvas)return false;BP->Modify();BP->WidgetTree->Modify();
    auto* Icon=Make<UImage>(BP,TEXT("ArtFlowSeal"));Icon->SetBrushFromMaterial(Seal);Icon->SetColorAndOpacity(FLinearColor(.72f,.46f,.24f,.10f));Icon->SetVisibility(ESlateVisibility::HitTestInvisible);
    Place(Canvas,Icon,FVector2D(-145,-145),FVector2D(290,290),FAnchors(.5f,.5f));
    CastChecked<UCanvasPanelSlot>(Icon->Slot)->SetZOrder(-1);
    return Finish(BP);
}

bool URogueArtCompletionBuilder::BuildOwnedItems(UWidgetBlueprint* Bar,UWidgetBlueprint* Tile,URoguePresentationTheme* Theme)
{
    using namespace RogueArtBuilder;if(!Bar||!Tile||!Theme)return false;
    Tile->Modify();Tile->WidgetTree->Modify();
    auto* Root=Make<UCanvasPanel>(Tile,TEXT("OwnedTileRoot"));Tile->WidgetTree->RootWidget=Root;
    auto* Frame=Make<UBorder>(Tile,TEXT("ItemFrame"));Frame->SetBrush(FSlateRoundedBoxBrush(FLinearColor(.035f,.052f,.085f,.82f),9.f,FLinearColor(.67f,.69f,.74f,.58f),1.f));Frame->SetPadding(FMargin(0));Frame->SetVisibility(ESlateVisibility::HitTestInvisible);
    Place(Root,Frame,FVector2D::ZeroVector,FVector2D::ZeroVector,FAnchors(0,0,1,1));
    auto* Body=Make<UCanvasPanel>(Tile,TEXT("TileBody"));Frame->AddChild(Body);
    auto* Image=Make<UImage>(Tile,TEXT("ItemIcon"));Image->SetVisibility(ESlateVisibility::HitTestInvisible);Image->SetRenderTransformPivot(FVector2D(.5f,.5f));Image->SetBrushFromMaterial(Theme->GetWeaponIcon(ERogueUpgradeType::ProjectileCount));
    Place(Body,Image,FVector2D::ZeroVector,FVector2D::ZeroVector,FAnchors(0,0,1,1));CastChecked<UCanvasPanelSlot>(Image->Slot)->SetOffsets(FMargin(5,3,5,5));
    auto* Badge=Make<UBorder>(Tile,TEXT("StackBadge"));Badge->SetBrush(FSlateRoundedBoxBrush(FLinearColor(.035f,.045f,.07f,.98f),5.f,FLinearColor(.76f,.62f,.34f,.9f),1.f));Badge->SetPadding(FMargin(1,0));Badge->SetVisibility(ESlateVisibility::HitTestInvisible);
    Place(Body,Badge,FVector2D(2,3),FVector2D(25,20),FAnchors(1,1));CastChecked<UCanvasPanelSlot>(Badge->Slot)->SetAlignment(FVector2D(1,1));
    auto* Count=Text(Tile,TEXT("StackText"),TEXT("1"),14,FLinearColor(.99f,.95f,.82f,1));Count->SetJustification(ETextJustify::Center);Count->SetRenderTransformPivot(FVector2D(.5f,.5f));Badge->AddChild(Count);
    if(!Finish(Tile))return false;
    Bar->Modify();Bar->WidgetTree->Modify();
    auto* BarRoot=Make<UCanvasPanel>(Bar,TEXT("OwnedBarRoot"));Bar->WidgetTree->RootWidget=BarRoot;BarRoot->SetVisibility(ESlateVisibility::HitTestInvisible);
    auto* Canvas=Make<UCanvasPanel>(Bar,TEXT("ItemsCanvas"));Canvas->SetVisibility(ESlateVisibility::HitTestInvisible);Place(BarRoot,Canvas,FVector2D::ZeroVector,FVector2D::ZeroVector,FAnchors(0,0,1,1));
    if(!Finish(Bar))return false;
    auto* CDO=Cast<URogueOwnedItemsWidget>(Bar->GeneratedClass->GetDefaultObject());if(!CDO)return false;
    CDO->PresentationTheme=Theme;CDO->ItemWidgetClass=Tile->GeneratedClass.Get();Bar->MarkPackageDirty();return true;
}
