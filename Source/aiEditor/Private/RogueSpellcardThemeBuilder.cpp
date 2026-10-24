#include "RogueSpellcardThemeBuilder.h"

#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Brushes/SlateColorBrush.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "WidgetBlueprint.h"

namespace RogueSpellcardTheme
{
    const FLinearColor Ink(.018f, .022f, .052f, 1);
    const FLinearColor Vermilion(.48f, .045f, .075f, 1);
    const FLinearColor Ivory(.97f, .94f, .86f, 1);
    const FLinearColor MutedInk(.20f, .17f, .22f, 1);

    bool IsViewportBackground(const UWidget* Widget, const UWidgetBlueprint* BP)
    {
        const auto* Slot = Cast<UCanvasPanelSlot>(Widget->Slot);
        if (!Slot || Widget->GetParent() != BP->WidgetTree->RootWidget) return false;
        const FAnchors A = Slot->GetAnchors();
        return A.Maximum.X - A.Minimum.X > .9f && A.Maximum.Y - A.Minimum.Y > .9f;
    }

    FSlateBrush ArtBrush(UTexture2D* Texture)
    {
        FSlateBrush Brush;
        Brush.DrawAs = ESlateBrushDrawType::Image;
        Brush.SetResourceObject(Texture);
        Brush.SetImageSize(FVector2D(1920, 1080));
        Brush.TintColor = FLinearColor::White;
        return Brush;
    }

    template <typename T>
    T* Decoration(UWidgetBlueprint* BP, FName Name)
    {
        if (auto* Existing = Cast<T>(BP->WidgetTree->FindWidget(Name))) return Existing;
        return BP->WidgetTree->ConstructWidget<T>(T::StaticClass(), Name);
    }

    void At(UCanvasPanel* Canvas, UWidget* Widget, FVector2D Position, FVector2D Size, int32 Layer)
    {
        Widget->RemoveFromParent();
        auto* Slot = Canvas->AddChildToCanvas(Widget);
        Slot->SetPosition(Position); Slot->SetSize(Size); Slot->SetZOrder(Layer);
        Widget->SetVisibility(ESlateVisibility::HitTestInvisible);
    }

    void HeaderRule(UWidgetBlueprint* BP, UCanvasPanel* Canvas, FName Name, float Y)
    {
        auto* Rule = Decoration<UBorder>(BP, Name);
        Rule->SetBrush(FSlateRoundedBoxBrush(Vermilion, 1.f));
        Rule->SetBrushColor(FLinearColor::White);
        At(Canvas, Rule, FVector2D(0, Y), FVector2D(0, 2), -2);
        auto* Slot = CastChecked<UCanvasPanelSlot>(Rule->Slot);
        Slot->SetAnchors(FAnchors(0, 0, 1, 0));
        Slot->SetOffsets(FMargin(0, Y, 0, 2));
        Rule->SetRenderOpacity(.65f);
    }

    void Watermark(UWidgetBlueprint* BP, UBorder* Frame, UTexture2D* Seal, float Opacity)
    {
        UWidget* Content = Frame->GetContent();
        if (!Content) return;
        const FName OverlayName(*FString::Printf(TEXT("Spirit_%s_Overlay"), *Frame->GetName()));
        UOverlay* Overlay = Cast<UOverlay>(Content);
        if (!Overlay || Overlay->GetFName() != OverlayName)
        {
            Content->RemoveFromParent();
            Overlay = Decoration<UOverlay>(BP, OverlayName);
            Overlay->ClearChildren();
            Frame->AddChild(Overlay);
            auto* Box = Decoration<USizeBox>(BP, *FString::Printf(TEXT("Spirit_%s_SealSize"), *Frame->GetName()));
            Box->SetWidthOverride(92); Box->SetHeightOverride(92);
            auto* Image = Decoration<UImage>(BP, *FString::Printf(TEXT("Spirit_%s_Seal"), *Frame->GetName()));
            Image->SetBrushFromTexture(Seal); Image->SetColorAndOpacity(FLinearColor::White);
            Image->SetRenderOpacity(Opacity); Image->SetVisibility(ESlateVisibility::HitTestInvisible);
            Box->ClearChildren(); Box->AddChild(Image); Box->SetVisibility(ESlateVisibility::HitTestInvisible);
            auto* MarkSlot = Overlay->AddChildToOverlay(Box);
            MarkSlot->SetHorizontalAlignment(HAlign_Right); MarkSlot->SetVerticalAlignment(VAlign_Bottom);
            auto* ContentSlot = Overlay->AddChildToOverlay(Content);
            ContentSlot->SetHorizontalAlignment(HAlign_Fill); ContentSlot->SetVerticalAlignment(VAlign_Fill);
        }
        Overlay->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    }

    void CardDetails(UWidgetBlueprint* BP, UTexture2D* Seal)
    {
        auto* Body = Cast<UCanvasPanel>(BP->WidgetTree->FindWidget(TEXT("CardBody")));
        if (!Body) return;
        auto* Mark = Decoration<UBorder>(BP, TEXT("Spirit_CardTitleMark"));
        Mark->SetBrush(FSlateRoundedBoxBrush(Vermilion, 1.5f)); Mark->SetBrushColor(FLinearColor::White);
        At(Body, Mark, FVector2D(-13, 1), FVector2D(3, 29), -1);
        auto* Image = Decoration<UImage>(BP, TEXT("Spirit_CardSeal"));
        Image->SetBrushFromTexture(Seal); Image->SetColorAndOpacity(FLinearColor::White);
        Image->SetRenderOpacity(.065f);
        const bool bShop = BP->GetName() == TEXT("WBP_ShopOfferCard");
        At(Body, Image, FVector2D(181, bShop ? 77 : 147), FVector2D(94, 94), -3);
    }
}

bool URogueSpellcardThemeBuilder::ApplyRestrainedSpellcardTheme(UWidgetBlueprint* BP, UTexture2D* Background, UTexture2D* Seal)
{
    using namespace RogueSpellcardTheme;
    if (!BP || !BP->WidgetTree || !Background || !Seal) return false;
    const FString Name = BP->GetName();
    // 保留受伤遮罩及敌人胶囊血条的专属动画和画刷。
    if (Name == TEXT("WBP_DamageFlash") || Name == TEXT("WBP_EnemyHealthBar")) return true;
    const bool bMain = Name == TEXT("WBP_MainMenu");
    const bool bTree = Name == TEXT("WBP_UpgradeTree");
    const bool bCard = Name == TEXT("WBP_UpgradeCard") || Name == TEXT("WBP_ShopOfferCard");
    const bool bNode = Name == TEXT("WBP_UpgradeTreeNode");
    const bool bVitals = Name == TEXT("WBP_PlayerVitals");
    const bool bBoss = Name == TEXT("WBP_BossHealthBar");
    const bool bNotice = Name == TEXT("WBP_NightTransition") || Name == TEXT("WBP_ResurrectionScreen");
    const bool bDark = bMain || bTree || bVitals || bBoss || bNotice;
    const bool bPage = bMain || bTree || Name == TEXT("WBP_Tutorial") || Name == TEXT("WBP_MultiplayerMenu")
        || Name == TEXT("WBP_RoomBrowser") || Name == TEXT("WBP_Lobby");
    BP->Modify(); BP->WidgetTree->Modify();
    TArray<UWidget*> Widgets; BP->WidgetTree->GetAllWidgets(Widgets);
    for (UWidget* Widget : Widgets)
    {
        Widget->Modify();
        const FString N = Widget->GetName();
        if (auto* Border = Cast<UBorder>(Widget))
        {
            // 这些边光由状态反馈基类按事件控制，保留原样及初始透明度。
            if (N.EndsWith(TEXT("Feedback"))) continue;
            if (IsViewportBackground(Border, BP))
            {
                if (bPage)
                {
                    Border->SetBrush(ArtBrush(Background)); Border->SetBrushColor(FLinearColor::White);
                    CastChecked<UCanvasPanelSlot>(Border->Slot)->SetZOrder(-200);
                }
                else
                {
                    Border->SetBrush(FSlateColorBrush(FLinearColor(.012f, .016f, .04f, bNotice ? .36f : .70f)));
                    Border->SetBrushColor(FLinearColor::White);
                }
                continue;
            }
            const bool bDarkFrame = bDark && N != TEXT("TreeBackground");
            if (bDarkFrame)
            {
                Border->SetBrush(FSlateRoundedBoxBrush(FLinearColor(Ink.R, Ink.G, Ink.B, .94f), 16.f,
                    FLinearColor(.48f, .14f, .16f, .72f), 1.f));
            }
            else
            {
                // 沿用和纸纹理，去掉原先近乎无色的边缘，使用克制的朱红细线。
                FSlateBrush Brush = Border->Background;
                Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
                Brush.OutlineSettings.CornerRadii = FVector4(14, 14, 14, 14);
                Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
                Brush.OutlineSettings.Color = FLinearColor(.44f, .16f, .17f, .58f);
                Brush.OutlineSettings.Width = 1.f;
                Border->SetBrush(Brush);
            }
            Border->SetBrushColor(FLinearColor::White);
        }
        if (auto* Image = Cast<UImage>(Widget))
        {
            if (bMain && N.Contains(TEXT("Background")))
            {
                Image->SetBrushFromTexture(Background); Image->SetColorAndOpacity(FLinearColor::White);
            }
        }
        if (auto* Button = Cast<UButton>(Widget))
        {
            FButtonStyle Style = Button->GetStyle();
            const bool bPrimary = bMain && N == TEXT("SinglePlayerButton");
            const bool bCardButton = bCard && (N == TEXT("Button_Select") || N == TEXT("Button_Offer"));
            const float Radius = bCardButton ? 14.f : 10.f;
            const FLinearColor Normal = bPrimary ? Vermilion : bDark ? FLinearColor(.03f, .035f, .075f, .94f) : Ivory;
            const FLinearColor Edge = bDark ? FLinearColor(.58f, .28f, .29f, .8f) : FLinearColor(.48f, .25f, .25f, .7f);
            Style.Normal = FSlateRoundedBoxBrush(Normal, Radius, Edge, 1.f);
            Style.Hovered = FSlateRoundedBoxBrush(bDark ? FLinearColor(.38f, .045f, .075f, 1) : FLinearColor(1.f, .90f, .83f, 1), Radius, Vermilion, 2.f);
            Style.Pressed = FSlateRoundedBoxBrush(bDark ? FLinearColor(.22f, .025f, .045f, 1) : FLinearColor(.86f, .75f, .69f, 1), Radius, Vermilion, 1.f);
            Style.Disabled = FSlateRoundedBoxBrush(bDark ? FLinearColor(.07f, .07f, .11f, .8f) : FLinearColor(.78f, .77f, .74f, .8f), Radius,
                FLinearColor(.38f, .34f, .36f, .65f), 1.f);
            Button->SetStyle(Style); Button->SetBackgroundColor(FLinearColor::White); Button->SetColorAndOpacity(FLinearColor::White);
        }
        if (auto* Label = Cast<UTextBlock>(Widget))
        {
            if (N.EndsWith(TEXT("ChangeText"))) continue;
            const bool bTitle = N.Contains(TEXT("Title")) || N.Contains(TEXT("Header"));
            const bool bSubtle = N.Contains(TEXT("Hint")) || N.Contains(TEXT("Subtitle")) || N.Contains(TEXT("Footer"))
                || N.Contains(TEXT("Description")) || N.Contains(TEXT("Prerequisite"));
            FLinearColor Color = bDark ? Ivory : (bTitle ? Vermilion : bSubtle ? MutedInk : FLinearColor(.055f, .035f, .06f, 1));
            if (bTree && (N == TEXT("IceBricksText") || N == TEXT("SectionText"))) Color = FLinearColor(.62f, .85f, .89f, 1);
            if (bVitals && N == TEXT("ArmorText")) Color = FLinearColor(.67f, .89f, .95f, 1);
            if (bVitals && (N == TEXT("ExperienceText") || N == TEXT("GoldText"))) Color = FLinearColor(.91f, .76f, .46f, 1);
            if (bMain && N == TEXT("SubtitleText")) Color = FLinearColor(.83f, .72f, .68f, 1);
            Label->SetColorAndOpacity(Color);
            Label->SetShadowOffset(bDark ? FVector2D(0, 1) : FVector2D::ZeroVector);
            Label->SetShadowColorAndOpacity(FLinearColor(0, 0, .01f, bDark ? .65f : 0));
            if (bMain && N == TEXT("TitleText"))
            {
                FSlateFontInfo Font = Label->GetFont(); Font.Size = 62; Label->SetFont(Font);
            }
        }
        if (auto* Input = Cast<UEditableTextBox>(Widget))
        {
            auto& Style = Input->WidgetStyle;
            Style.BackgroundImageNormal = FSlateRoundedBoxBrush(Ivory, 10.f, FLinearColor(.48f, .25f, .25f, .7f), 1.f);
            Style.BackgroundImageHovered = FSlateRoundedBoxBrush(FLinearColor(1, .97f, .9f, 1), 10.f, Vermilion, 1.5f);
            Style.BackgroundImageFocused = Style.BackgroundImageHovered;
            Style.BackgroundImageReadOnly = Style.BackgroundImageNormal;
            Style.SetForegroundColor(FLinearColor(.055f, .035f, .06f, 1));
            Style.SetFocusedForegroundColor(FLinearColor(.055f, .035f, .06f, 1));
        }
    }
    if (bPage)
    {
        if (auto* Root = Cast<UCanvasPanel>(BP->WidgetTree->RootWidget))
        {
            auto* Veil = Decoration<UBorder>(BP, TEXT("Spirit_Atmosphere"));
            Veil->SetBrush(FSlateColorBrush(FLinearColor(.008f, .01f, .035f, bMain ? .14f : .48f)));
            Veil->SetBrushColor(FLinearColor::White);
            At(Root, Veil, FVector2D::ZeroVector, FVector2D::ZeroVector, -100);
            auto* Slot = CastChecked<UCanvasPanelSlot>(Veil->Slot);
            Slot->SetAnchors(FAnchors(0, 0, 1, 1)); Slot->SetOffsets(FMargin(0));
        }
    }
    if (bCard) CardDetails(BP, Seal);
    else if (bNode)
    {
        if (auto* Frame = Cast<UBorder>(BP->WidgetTree->FindWidget(TEXT("StateBorder")))) Watermark(BP, Frame, Seal, .045f);
    }
    else if (!bDark)
    {
        for (const TCHAR* FrameName : {TEXT("RestyledPanel"), TEXT("Border_Background")})
            if (auto* Frame = Cast<UBorder>(BP->WidgetTree->FindWidget(FrameName))) Watermark(BP, Frame, Seal, .05f);
    }
    for (const TCHAR* BodyName : {TEXT("PageContent"), TEXT("RestyledBody")})
        if (auto* Body = Cast<UCanvasPanel>(BP->WidgetTree->FindWidget(BodyName))) HeaderRule(BP, Body, TEXT("Spirit_HeaderRule"), -14);
    if (bBoss)
    {
        if (auto* Title = Cast<UTextBlock>(BP->WidgetTree->FindWidget(TEXT("BossTitle")))) Title->SetText(FText::FromString(TEXT("异变 · B O S S")));
    }
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
    FKismetEditorUtilities::CompileBlueprint(BP);
    BP->MarkPackageDirty();
    return BP->Status != BS_Error;
}
