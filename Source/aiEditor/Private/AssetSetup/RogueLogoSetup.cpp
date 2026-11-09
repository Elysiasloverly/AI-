#include "RogueLogoSetup.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/PanelWidget.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "WidgetBlueprint.h"

bool URogueLogoSetup::SetMainMenuLogo(UWidgetBlueprint* BP, UTexture2D* Texture, float Width, float Height)
{
    if (!BP || !BP->WidgetTree || !Texture || !FMath::IsFinite(Width) || !FMath::IsFinite(Height) || Width <= 0 || Height <= 0) return false;
    UWidgetTree* Tree = BP->WidgetTree;
    UImage* Image = Cast<UImage>(Tree->FindWidget(TEXT("Image_GameLogo")));
    if (!Image)
    {
        auto* Title = Cast<UTextBlock>(Tree->FindWidget(TEXT("TitleText")));
        UPanelWidget* Parent = Title ? Title->GetParent() : nullptr;
        if (!Title || !Parent) return false;
        BP->Modify(); Tree->Modify(); Parent->Modify(); Title->Modify();
        Title->ForceLayoutPrepass();
        const float OldHeight = FMath::Max(Title->GetDesiredSize().Y, static_cast<float>(Title->GetFont().Size) * 1.25f);
        auto* Header = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("GameLogoSize"));
        Header->SetWidthOverride(Width); Header->SetHeightOverride(Height);
        Header->SetVisibility(ESlateVisibility::HitTestInvisible);
        // Reuse the existing title slot; all sibling widgets and their styles remain intact.
        if (!Parent->ReplaceChild(Title, Header)) return false;
        Title->Slot = nullptr;
        auto* Overlay = Tree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("GameLogoHeader"));
        Header->SetContent(Overlay);
        auto* Scale = Tree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass(), TEXT("GameLogoScale"));
        Scale->SetStretch(EStretch::ScaleToFit); Scale->SetStretchDirection(EStretchDirection::Both);
        Overlay->AddChildToOverlay(Scale);
        Image = Tree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Image_GameLogo"));
        Scale->SetContent(Image);
        // Keep the named text available to old graph/animation references without drawing a duplicate title.
        Overlay->AddChildToOverlay(Title); Title->SetVisibility(ESlateVisibility::Collapsed);
        if (auto* Panel = Cast<UBorder>(Tree->FindWidget(TEXT("MenuPanel"))))
        {
            if (auto* PanelSlot = Cast<UCanvasPanelSlot>(Panel->Slot))
            {
                Panel->Modify(); PanelSlot->Modify();
                FVector2D Size = PanelSlot->GetSize();
                Size.Y += FMath::Max(0.f, Height - OldHeight);
                PanelSlot->SetSize(Size);
            }
        }
    }
    // ScaleToFit must receive the whole fixed header area, even before its normalizing prepass.
    if (auto* Scale = Cast<UScaleBox>(Tree->FindWidget(TEXT("GameLogoScale"))))
    {
        if (auto* ScaleSlot = Cast<UOverlaySlot>(Scale->Slot))
        {
            ScaleSlot->SetHorizontalAlignment(HAlign_Fill);
            ScaleSlot->SetVerticalAlignment(VAlign_Fill);
        }
    }
    Image->SetBrushFromTexture(Texture, true);
    // Persist source dimensions in the brush; transient SImage overrides do not survive saving.
    FSlateBrush LogoBrush = Image->GetBrush();
    LogoBrush.ImageSize = FVector2D(Texture->Source.GetSizeX(), Texture->Source.GetSizeY());
    Image->SetBrush(LogoBrush);
    Image->SetColorAndOpacity(FLinearColor::White);
    Image->SetVisibility(ESlateVisibility::HitTestInvisible);
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
    FKismetEditorUtilities::CompileBlueprint(BP);
    return BP->Status != BS_Error;
}
