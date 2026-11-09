#include "RogueEditorAssetUtils.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/PanelWidget.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "UI/Widgets/RogueUpgradeSelectionWidgetBase.h"
#include "Widgets/SViewport.h"

bool URogueEditorAssetUtils::SendPreviewKey(APlayerController* Owner,FName KeyName,bool bPressed,bool bFocusUpgradeButton)
{
    if(!Owner||Owner->GetWorld()->WorldType!=EWorldType::PIE||!FSlateApplication::IsInitialized())return false;
    const auto* Local=Owner->GetLocalPlayer();
    if(!Local||!Local->ViewportClient)return false;
    auto& Slate=FSlateApplication::Get();bool Focused=false;
    if(bFocusUpgradeButton)
    {
        for(TObjectIterator<URogueUpgradeSelectionWidgetBase> It;It;++It)
        {
            if(It->GetOwningPlayer()!=Owner||!It->IsVisible()||!It->WidgetTree)continue;
            // 卡片是嵌套 UserWidget，外层 WidgetTree 不包含其内部的按钮。
            // 按真实控件层级递归，确保测试覆盖“点击卡片后焦点停在按钮上”的输入路径。
            TFunction<bool(UWidget*)> FocusFirstButton;
            FocusFirstButton = [&Slate, &FocusFirstButton](UWidget* Widget)
            {
                if (!Widget) return false;
                if (auto* Button = Cast<UButton>(Widget); Button && Button->GetIsEnabled())
                {
                    Slate.SetKeyboardFocus(Button->TakeWidget(), EFocusCause::SetDirectly);
                    return true;
                }
                if (const auto* Nested = Cast<UUserWidget>(Widget); Nested && Nested->WidgetTree)
                    return FocusFirstButton(Nested->WidgetTree->RootWidget);
                if (const auto* Panel = Cast<UPanelWidget>(Widget))
                    for (auto* Child : Panel->GetAllChildren())
                        if (FocusFirstButton(Child)) return true;
                return false;
            };
            Focused = FocusFirstButton(It->WidgetTree->RootWidget);
            if(Focused)break;
        }
        if(!Focused)return false;
    }
    else if(auto Viewport=Local->ViewportClient->GetGameViewportWidget())Slate.SetKeyboardFocus(Viewport,EFocusCause::SetDirectly);
    else return false;
    const FKey Key(KeyName);if(!Key.IsValid())return false;
    TGuardValue<bool> ScriptExecution(GAllowActorScriptExecutionInEditor,false);
    const FKeyEvent Event(Key,FModifierKeysState(),0,false,0,0);
    if(bPressed)Slate.ProcessKeyDownEvent(Event);else Slate.ProcessKeyUpEvent(Event);
    return true;
}

#include "Player/RogueCharacter.h"
#include "Player/RoguePlayerState.h"
#include "UI/Widgets/RogueVitalsWidgetBase.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/TextBlock.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/SizeBox.h"
#include "UI/Widgets/RogueDashRingWidget.h"

int32 URogueEditorAssetUtils::GrantPreviewLevels(ARogueCharacter* Player,int32 Count)
{
    if(!Player||!Player->HasAuthority()||Player->GetWorld()->WorldType!=EWorldType::PIE)return -1;
    for(int32 I=0;I<FMath::Clamp(Count,0,10);++I)Player->AddExperience(Player->GetExperienceToNextLevel());
    const auto* State=Player->GetPlayerState<ARoguePlayerState>();
    return State?State->GetPendingUpgradeCount():-1;
}

FString URogueEditorAssetUtils::InspectUpgradeSelectionPreview(APlayerController* Owner)
{
    const auto* State=Owner?Owner->GetPlayerState<ARoguePlayerState>():nullptr;
    if(!State||Owner->GetWorld()->WorldType!=EWorldType::PIE)return TEXT("{}");
    FString Types,Counter;FGuid Receipt;
    for(const auto& Card:State->GetPendingUpgrades())
    {
        if(!Types.IsEmpty())Types+=TEXT(",");Types+=FString::FromInt(int32(Card.Type));Receipt=Card.SelectionId;
    }
    bool Visible=false;
    TArray<UUserWidget*> Menus,Vitals;
    UWidgetBlueprintLibrary::GetAllWidgetsOfClass(Owner,Menus,URogueUpgradeSelectionWidgetBase::StaticClass(),false);
    UWidgetBlueprintLibrary::GetAllWidgetsOfClass(Owner,Vitals,URogueVitalsWidgetBase::StaticClass(),false);
    for(auto* Widget:Menus)if(Widget->GetOwningPlayer()==Owner)Visible|=Widget->IsVisible();
    for(auto* Widget:Vitals)if(Widget->GetOwningPlayer()==Owner)
        if(auto* Label=Cast<UTextBlock>(Widget->GetWidgetFromName(TEXT("PendingUpgradesText"))))Counter=Label->GetText().ToString();
    FString RingDetails;
    for (auto* Widget : Vitals)
    {
        if (Widget->GetOwningPlayer() != Owner) continue;
        const auto* Ring = Cast<URogueDashRingWidget>(Widget->GetWidgetFromName(TEXT("PlayerDashRing")));
        const auto* Hint = Cast<UTextBlock>(Widget->GetWidgetFromName(TEXT("PendingUpgradeHintText")));
        const auto* Panel = Cast<USizeBox>(Widget->GetWidgetFromName(TEXT("VitalsPanel")));
        const auto* OldDash = Widget->GetWidgetFromName(TEXT("DashCooldownRow"));
        const auto* RingSlot = Ring ? Cast<UCanvasPanelSlot>(Ring->Slot) : nullptr;
        if (!RingSlot || !Hint || !Panel || !OldDash) continue;
        const FVector2D Position = RingSlot->GetPosition();
        RingDetails = FString::Printf(TEXT(",\"ring_visible\":%s,\"ring_position\":[%.3f,%.3f],\"ring_percent\":%.3f,\"hint_visible\":%s,\"hint_opacity\":%.3f,\"hint_text\":\"%s\",\"panel_width\":%.1f,\"legacy_dash_visible\":%s"),
            Ring->IsVisible() ? TEXT("true") : TEXT("false"), Position.X, Position.Y, Ring->GetCooldownPercent(),
            Hint->IsVisible() ? TEXT("true") : TEXT("false"), Hint->GetRenderOpacity(), *Hint->GetText().ToString(),
            Panel->GetWidthOverride(), OldDash->IsVisible() ? TEXT("true") : TEXT("false"));
        break;
    }
    return FString::Printf(TEXT("{\"count\":%d,\"queued\":%d,\"open\":%s,\"visible\":%s,\"ready\":%s,\"cursor\":%s,\"receipt\":\"%s\",\"types\":[%s],\"counter\":\"%s\"%s}"),
        State->GetPendingUpgradeCount(),State->GetQueuedUpgradeSelections(),State->IsAwaitingUpgradeChoice()?TEXT("true"):TEXT("false"),
        Visible?TEXT("true"):TEXT("false"),State->IsProgressionReady()?TEXT("true"):TEXT("false"),Owner->bShowMouseCursor?TEXT("true"):TEXT("false"),
        *Receipt.ToString(),*Types,*Counter,*RingDetails);
}
