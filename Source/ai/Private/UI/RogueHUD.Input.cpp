#include "UI/RogueHUD.h"
#include "RogueHUDPrivate.h"
#include "Player/RogueCharacter.h"
#include "Player/RoguePlayerState.h"
#include "UI/Widgets/RoguePauseMenuWidgetBase.h"
#include "UI/Widgets/RogueSettingsMenuWidgetBase.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SViewport.h"
#include "InputCoreTypes.h"

// 仅接管本 HUD 所在游戏视口的 Esc。点击 UMG 按钮后仍有效，退出 PIE 即注销，
// 不接管编辑器面板、其他玩家窗口或 Shift+Esc 的退出预览快捷键。
class FRoguePauseInputProcessor final : public IInputProcessor
{
public:
    explicit FRoguePauseInputProcessor(ARogueHUD* InHUD) : HUD(InHUD) {}
    virtual void Tick(float, FSlateApplication&, TSharedRef<ICursor>) override {}
    virtual bool HandleKeyDownEvent(FSlateApplication&, const FKeyEvent& Event) override
    {
        if (Event.GetKey() != EKeys::Escape || Event.IsShiftDown() || Event.IsControlDown() || Event.IsAltDown()) return false;
        auto* Owner = HUD.Get();
        UE_LOG(LogTemp,Verbose,TEXT("[RoguePause] Esc HUD=%s focus=%d"),*GetNameSafe(Owner),Owner && Owner->HasBattleInputFocus());
        if (!Owner || !Owner->HasBattleInputFocus()) return false;
        if (Event.IsRepeat()) return true;
        return Owner->HandlePauseMenuKey();
    }
private:
    TWeakObjectPtr<ARogueHUD> HUD;
};

void ARogueHUD::RegisterPauseInput()
{
    if (PauseInputProcessor || !FSlateApplication::IsInitialized()) return;
    const auto* PC = GetOwningPlayerController();
    if (!PC || !PC->IsLocalController() || !PC->GetLocalPlayer()) return;
    PauseInputProcessor = MakeShared<FRoguePauseInputProcessor>(this);
    FSlateApplication::Get().RegisterInputPreProcessor(PauseInputProcessor, 0);
}

void ARogueHUD::UnregisterPauseInput()
{
    if (PauseInputProcessor && FSlateApplication::IsInitialized())
        FSlateApplication::Get().UnregisterInputPreProcessor(PauseInputProcessor);
    PauseInputProcessor.Reset();
}

bool ARogueHUD::HasBattleInputFocus() const
{
    const auto* PC = GetOwningPlayerController();
    const auto* LocalPlayer = PC ? PC->GetLocalPlayer() : nullptr;
    const auto* Viewport = LocalPlayer ? LocalPlayer->ViewportClient.Get() : nullptr;
    if (bMainMenuMode || !Viewport) return false;
    auto HasFocus = [](const TSharedPtr<SWidget>& Widget)
    {
        return Widget && (Widget->HasKeyboardFocus() || Widget->HasFocusedDescendants());
    };
    if (HasFocus(Viewport->GetGameViewportWidget())) return true;
    return IsPauseMenuOpen() && ((PauseMenuWidget && HasFocus(PauseMenuWidget->GetCachedWidget()))
        || (SettingsMenuWidget && HasFocus(SettingsMenuWidget->GetCachedWidget())));
}

bool ARogueHUD::HandlePauseMenuKey()
{
    if (bMainMenuMode) return false;
    if (IsPauseMenuOpen())
    {
        RequestResumeFromPauseMenu();
        return true;
    }
    const auto* Player = RogueHUDPrivate::GetLocalPlayerCharacter(this);
    if (!Player || !Player->IsAlive() || RogueHUDPrivate::IsRunOver(GetRogueGameState(), Player)) return false;
    const auto* State = GetLocalRoguePlayerState();
    // Esc 先关闭当前交互界面；收起选卡保留奖励，退出选择状态后恢复战斗。
    if (State && State->IsShopOpen()) { RequestCloseFromShop(); return true; }
    if(State&&State->IsAwaitingUpgradeChoice()) {GetLocalRoguePlayerState()->Server_ToggleUpgradeSelection();return true;}
    OpenPauseMenu();
    return IsPauseMenuOpen();
}
