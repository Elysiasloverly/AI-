#include "RogueEditorAssetUtils.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "UI/RogueHUD.h"
#include "Player/RoguePlayerState.h"
#include "UI/Widgets/RoguePauseMenuWidgetBase.h"
#include "UI/Widgets/RogueSettingsMenuWidgetBase.h"
#include "Widgets/SViewport.h"

bool URogueEditorAssetUtils::SendPreviewEscape(APlayerController* PC, bool bFocusMenuButton)
{
    if (!PC || !PC->GetWorld() || PC->GetWorld()->WorldType != EWorldType::PIE || !FSlateApplication::IsInitialized()) return false;
    const auto* Local=PC->GetLocalPlayer();
    if (!Local || !Local->ViewportClient) return false;
    auto& Slate=FSlateApplication::Get();
    bool bFocused=false;
    if (bFocusMenuButton)
    {
        for (TObjectIterator<UUserWidget> It;It;++It)
        {
            if (It->GetOwningPlayer()!=PC || !It->IsInViewport() || !It->IsVisible() || !It->WidgetTree
                || (!It->IsA<URoguePauseMenuWidgetBase>() && !It->IsA<URogueSettingsMenuWidgetBase>())) continue;
            TArray<UWidget*> Children;It->WidgetTree->GetAllWidgets(Children);
            for (auto* Child:Children)
                if (auto* Button=Cast<UButton>(Child);Button && Button->GetIsEnabled())
                {
                    Slate.SetKeyboardFocus(Button->TakeWidget(),EFocusCause::SetDirectly);
                    bFocused=Button->GetCachedWidget()->HasKeyboardFocus();
                    if (bFocused) break;
                }
            if (bFocused) break;
        }
        if (!bFocused) return false;
    }
    else if (auto Viewport=Local->ViewportClient->GetGameViewportWidget())
        Slate.SetKeyboardFocus(Viewport,EFocusCause::SetDirectly);
    else return false;
    // Python enables editor-only local script execution. Real input must retain normal RPC routing.
    TGuardValue<bool> ScriptExecution(GAllowActorScriptExecutionInEditor,false);
    const FKeyEvent Escape(EKeys::Escape,FModifierKeysState(),0,false,0,0);
    auto* HUD=Cast<ARogueHUD>(PC->GetHUD());
    const bool Before=HUD && HUD->IsPauseMenuOpen();
    const bool Handled=Slate.ProcessKeyDownEvent(Escape);
    UE_LOG(LogTemp,Display,TEXT("[RoguePausePreview] PC=%s HUD=%s MainMenu=%d Before=%d After=%d Handled=%d Focus=%s"),
        *PC->GetName(),*GetNameSafe(HUD),HUD && HUD->IsMainMenuMode(),Before,HUD && HUD->IsPauseMenuOpen(),Handled,
        Slate.GetKeyboardFocusedWidget() ? *Slate.GetKeyboardFocusedWidget()->GetTypeAsString() : TEXT("none"));
    if (auto* PS=PC->GetPlayerState<ARoguePlayerState>()) UE_LOG(LogTemp,Display,TEXT("[RoguePausePreview] State=%s Pawn=%s Menu=%d Role=%d"),*PS->GetName(),*GetNameSafe(PS->GetPawn()),PS->IsPauseMenuOpen(),int32(PS->GetLocalRole()));
    Slate.ProcessKeyUpEvent(Escape);
    return true;
}

#include "Framework/RogueGameState.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Widgets/SWindow.h"

bool URogueEditorAssetUtils::RefreshPreviewEnemyLocations(APlayerController* PC)
{
    if (!PC || !PC->HasAuthority() || PC->GetWorld()->WorldType != EWorldType::PIE) return false;
    if (auto* State=PC->GetWorld()->GetGameState<ARogueGameState>()) {State->RefreshEnemyLocatorView();return true;}
    return false;
}

bool URogueEditorAssetUtils::CapturePreviewHUD(APlayerController* PC,const FString& Filename)
{
    if (!PC || !PC->GetWorld() || PC->GetWorld()->WorldType != EWorldType::PIE || !FSlateApplication::IsInitialized()) return false;
    const auto* Local=PC->GetLocalPlayer();
    if (!Local || !Local->ViewportClient) return false;
    auto Window=Local->ViewportClient->GetWindow();
    if (!Window) return false;
    TArray<FColor> Pixels;FIntVector Size;
    if (!FSlateApplication::Get().TakeScreenshot(Window.ToSharedRef(),Pixels,Size)) return false;
    TArray<uint8> Png;FImageUtils::CompressImageArray(Size.X,Size.Y,Pixels,Png);
    return FFileHelper::SaveArrayToFile(Png,*Filename);
}
