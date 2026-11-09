// 战斗菜单：把复制状态转换为视图，并将玩家操作交回控制/权威入口。
#include "UI/RogueHUD.h"
#include "UI/Widgets/RogueVictoryScreenWidgetBase.h"
#include "UI/Widgets/RogueOwnedItemsWidget.h"
#include "UI/RoguePresentationTheme.h"
#include "Components/HorizontalBox.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "UI/Widgets/RogueMenuWidgetBases.h"
#include "UI/Widgets/RogueVitalsWidgetBase.h"
#include "UI/Widgets/RogueDamageFlashWidgetBase.h"
#include "UI/Widgets/RogueEnemyHealthBarWidgetBase.h"
#include "Camera/CameraShakeBase.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/GameInstance.h"
#include "Player/RogueCharacter.h"
#include "Framework/RogueGameState.h"
#include "Player/RoguePlayerState.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "RogueHUDWidgetFactory.h"
#include "RogueHUDPrivate.h"
using namespace RogueHUDWidgetFactory;

void ARogueHUD::RequestSelectOfferFromShop(int32 OfferIndex, const FGuid &QuoteId, int32 QuotedCost)
{
	// 走本地玩家 PlayerState 的 ServerRPC —— 服务端与客户端都可用
	if (ARoguePlayerState *RoguePlayerState = GetLocalRoguePlayerState())
	{
		// 旧蓝图的按索引入口也必须使用最后一次实际呈现的报价，不能临时读取新快照。
        if (!PresentedShopOffers.IsValidIndex(OfferIndex)) return;
        const FRogueShopOffer& Offer = PresentedShopOffers[OfferIndex];
        const FGuid ReceiptId = QuoteId.IsValid() ? QuoteId : Offer.QuoteId;
        const int32 Cost = QuoteId.IsValid() ? QuotedCost : Offer.GetCost();
        if (OwnedItemsWidget && ShopWidget)
            OwnedItemsWidget->RememberCardOrigin(ReceiptId, Offer.GetUpgrade().Type, ShopWidget->GetOfferCard(OfferIndex));
        RoguePlayerState->Server_RequestBuyShopOffer(OfferIndex, ReceiptId, Cost);
	}
}

void ARogueHUD::RequestRefreshFromShop()
{
	if (ARoguePlayerState *RoguePlayerState = GetLocalRoguePlayerState())
	{
		RoguePlayerState->Server_RequestRefreshShop();
	}
}

void ARogueHUD::RequestCloseFromShop()
{
	if (ARoguePlayerState *RoguePlayerState = GetLocalRoguePlayerState())
	{
		RoguePlayerState->Server_RequestCloseShop();
	}
}

bool ARogueHUD::HandleUpgradeSelectionKey()
{
    auto* State=GetLocalRoguePlayerState();
    const auto* Player=RogueHUDPrivate::GetLocalPlayerCharacter(this);
    if(bMainMenuMode||!Player||!Player->IsAlive()||IsPauseMenuOpen()||!State||State->IsShopOpen()
        ||RogueHUDPrivate::IsRunOver(GetRogueGameState(),Player))return false;
    if(State->GetPendingUpgradeCount()<=0&&!State->IsAwaitingUpgradeChoice())return false;
    State->Server_ToggleUpgradeSelection();
    return true;
}

void ARogueHUD::RequestSelectUpgradeFromWidget(int32 UpgradeIndex, const FGuid &SelectionId)
{
	// 走本地玩家 PlayerState 的 ServerRPC —— 这样服务端与客户端都能正确请求。
	// 旧写法调 GetRogueGameMode()->TrySelectUpgrade(...)，而 GetRogueGameMode() 在客户端是 nullptr。
	if (APlayerController *OwningPlayerController = GetOwningPlayerController())
	{
		if (ARoguePlayerState *RoguePlayerState = OwningPlayerController->GetPlayerState<ARoguePlayerState>())
		{
            const FGuid ReceiptId = SelectionId.IsValid() ? SelectionId : PresentedUpgradeSelectionId;
            const auto& Options = RoguePlayerState->GetPendingUpgrades();
            if (OwnedItemsWidget && UpgradeSelectionWidget && Options.IsValidIndex(UpgradeIndex))
                OwnedItemsWidget->RememberCardOrigin(ReceiptId, Options[UpgradeIndex].Type,
                    UpgradeSelectionWidget->GetUpgradeCard(UpgradeIndex));
            RoguePlayerState->SubmitUpgradeSelection(UpgradeIndex, ReceiptId);
		}
	}
}

void ARogueHUD::RequestResumeFromPauseMenu()
{
	ClosePauseMenu();
	if (ARogueCharacter *PlayerCharacter = RogueHUDPrivate::GetLocalPlayerCharacter(this))
	{
		PlayerCharacter->SetUpgradeSelectionInput(false);
	}
	// 不再调 SetGamePaused —— 暂停菜单本身就不暂停世界（见 ARogueCharacter::TogglePauseMenu）
}

void ARogueHUD::RequestOpenSettingsFromPauseMenu()
{
	PauseMenuPage = ERoguePauseMenuPage::Settings;
}

void ARogueHUD::RequestQuitFromPauseMenu()
{
	ClosePauseMenu();
	UKismetSystemLibrary::QuitGame(this, GetOwningPlayerController(), EQuitPreference::Quit, false);
}

void ARogueHUD::RequestRestartAfterDeath()
{
	ClosePauseMenu();
	// 请求清除当前世界的暂停状态；是否允许由权威 GameMode/控制器规则决定。
	UGameplayStatics::SetGamePaused(this, false);

	// 重开必须由**服务端**执行：客户端自己 OpenLevel 只会加载出一份单机关卡副本，
	// 表现为"点了重开之后队友全没了"。所以这里只发一个请求，
	// 服务端校验本局已结束后，联机使用 ServerTravel；单机路径使用 OpenLevel。
	if (ARoguePlayerState *LocalPlayerState = GetLocalRoguePlayerState())
	{
		LocalPlayerState->Server_RequestRestartRun();
	}
}

void ARogueHUD::RequestQuitAfterDeath()
{
	ClosePauseMenu();
	// 同 RequestRestartAfterDeath：离开前的防御性清理
	UGameplayStatics::SetGamePaused(this, false);
	UKismetSystemLibrary::QuitGame(this, GetOwningPlayerController(), EQuitPreference::Quit, false);
}

void ARogueHUD::InitializeMenuWidgets()
{
	if (APlayerController *PlayerController = GetOwningPlayerController())
	{
		// 兼容旧 HUD 蓝图将默认属性保存为 None 的情况；默认路径来自 DefaultGame.ini。
		ResolveWidgetClass(DeathScreenWidgetClassPath, DeathScreenWidgetClass);
		ResolveWidgetClass(VictoryScreenWidgetClassPath, VictoryScreenWidgetClass);
		ResolveWidgetClass(ReviveCountdownWidgetClassPath, ReviveCountdownWidgetClass);
		ResolveWidgetClass(VitalsWidgetClassPath, VitalsWidgetClass);
		ResolveWidgetClass(EnemyHealthBarWidgetClassPath, EnemyHealthBarWidgetClass);
		ResolveWidgetClass(SettingsMenuWidgetClassPath, SettingsMenuWidgetClass);
        ResolveWidgetClass(PauseMenuWidgetClassPath, PauseMenuWidgetClass);
		UpgradeSelectionWidget =
			CreateRogueMenuWidget(PlayerController, UpgradeSelectionWidgetClass, this, 63);
		ShopWidget = CreateRogueMenuWidget(PlayerController, ShopWidgetClass, this, 64);
		PauseMenuWidget = CreateRogueMenuWidget(PlayerController, PauseMenuWidgetClass, this, 65);
		SettingsMenuWidget = CreateRogueMenuWidget(PlayerController, SettingsMenuWidgetClass, this, 66);
		DeathScreenWidget = CreateRogueMenuWidget(PlayerController, DeathScreenWidgetClass, this, 70);
		VictoryScreenWidget = CreateRogueMenuWidget(PlayerController, VictoryScreenWidgetClass, this, 71);

		ReviveCountdownWidget = CreateRogueMenuWidget(PlayerController, ReviveCountdownWidgetClass, this, 69);

		// 常驻状态条：ZOrder 5，位于所有菜单之下；敌人血条 6，由池按需创建。
		VitalsWidget = CreateRogueMenuWidget(PlayerController, VitalsWidgetClass, this, 5);
		if (VitalsWidgetClass != nullptr && VitalsWidget == nullptr)
		{
			UE_LOG(LogTemp, Warning, TEXT("[Rogue] 玩家状态条创建失败：Vitals=%s"),
				   *GetNameSafe(VitalsWidgetClass.Get()));
		}

		ResolveWidgetClass(DamageFlashWidgetClassPath, DamageFlashWidgetClass);
		// 受伤泛红：ZOrder 3，压在所有菜单与状态条之下 —— 泛红是背景层，
		// 盖在血条/文字上会让本来就要读的信息被染色。
		DamageFlashWidget = CreateRogueMenuWidget(PlayerController, DamageFlashWidgetClass, this, 3);
		if (DamageFlashWidget != nullptr)
		{
			// CreateRogueMenuWidget 按菜单语义把它设成了 Collapsed；泛红层要一直可见才会 tick。
			DamageFlashWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		if (DamageFlashWidgetClass == nullptr)
		{
			UE_LOG(LogTemp, Warning,
				   TEXT("[Rogue] 未配置受伤泛红 WBP：DamageFlashWidgetClass 为空，受击时只会有镜头震动。"));
		}

		if (EnemyHealthBarWidgetClass == nullptr)
		{
			UE_LOG(LogTemp, Warning,
				   TEXT("[Rogue] 未配置敌人血条 WBP：EnemyHealthBarWidgetClass 为空，敌人血条将不显示。"));
		}
		if (DeathScreenWidget == nullptr || ReviveCountdownWidget == nullptr)
		{
			UE_LOG(LogTemp, Error, TEXT("[Rogue] 复活/团灭界面创建失败：Resurrection=%s Death=%s"),
				   *GetNameSafe(ReviveCountdownWidgetClass.Get()),
				   *GetNameSafe(DeathScreenWidgetClass.Get()));
		}
	}
}

// 输入模式由当前实际打开的界面合成，避免一个面板关闭后覆盖另一个面板的鼠标需求。
void ARogueHUD::UpdateInteractionInputMode(const ARogueCharacter *PlayerCharacter)
{
	APlayerController *PlayerController = GetOwningPlayerController();
	if (PlayerController == nullptr)
	{
		return;
	}

	// 团灭状态不依赖 Pawn，等待持有/重新复制期间仍能点击结算按钮。
	const ARoguePlayerState *LocalPlayerState = GetLocalRoguePlayerState();
	const ARogueGameState *RogueGameState = GetRogueGameState();
	const bool bRunOver = RogueHUDPrivate::IsRunOver(RogueGameState, PlayerCharacter);
	const bool bAlive = PlayerCharacter != nullptr && PlayerCharacter->IsAlive();
	const bool bWantUIInput =
		bRunOver || (PlayerCharacter != nullptr && PlayerCharacter->IsDowned()) ||
		(bAlive && (PauseMenuPage != ERoguePauseMenuPage::None ||
					(LocalPlayerState != nullptr &&
					 (LocalPlayerState->IsShopOpen() || LocalPlayerState->IsAwaitingUpgradeChoice()))));

	const int32 NewState = bRunOver ? 2 : (bWantUIInput ? 1 : 0);

	// 除状态缓存外，还核对光标/点击/悬停三个标志，纠正其他入口对这些标志的改动。
	// 本检查不读取当前 Slate 输入模式或焦点；若仅这些状态被改动，缓存仍可能提前返回。
	const bool bAppliedStateMatches = PlayerController->bShowMouseCursor == bWantUIInput &&
									  PlayerController->bEnableClickEvents == bWantUIInput &&
									  PlayerController->bEnableMouseOverEvents == bWantUIInput;
	if (NewState == LastAppliedUIInputState && bAppliedStateMatches)
	{
		return;
	}
	LastAppliedUIInputState = NewState;

	PlayerController->bShowMouseCursor = bWantUIInput;
	PlayerController->bEnableClickEvents = bWantUIInput;
	PlayerController->bEnableMouseOverEvents = bWantUIInput;

	if (bRunOver)
	{
		FInputModeUIOnly InputMode;
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		UUserWidget *ResultWidget = RogueGameState && RogueGameState->IsVictory()
										? static_cast<UUserWidget *>(VictoryScreenWidget.Get())
										: static_cast<UUserWidget *>(DeathScreenWidget.Get());
		if (ResultWidget)
		{
			ResultWidget->SetIsFocusable(true);
			InputMode.SetWidgetToFocus(ResultWidget->TakeWidget());
		}
		PlayerController->SetInputMode(InputMode);
	}
	else if (bWantUIInput)
	{
		// GameAndUI：既能点卡片，也不至于把角色完全冻住（开商店时游戏本来就不暂停）。
		FInputModeGameAndUI InputMode;
		InputMode.SetHideCursorDuringCapture(false);
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PlayerController->SetInputMode(InputMode);
	}
	else
	{
		PlayerController->SetInputMode(FInputModeGameOnly());
	}
}

void ARogueHUD::UpdateShopWidget(const ARogueCharacter *PlayerCharacter)
{
	if (ShopWidget == nullptr)
	{
		return;
	}

	// 商店的全部读数据都来自本地 PlayerState（客户端可读）
	const ARoguePlayerState *LocalPlayerState = GetLocalRoguePlayerState();

	const ARogueGameState *RogueGameState = GetRogueGameState();
	const bool bVisible = PlayerCharacter != nullptr && PlayerCharacter->IsAlive() &&
						  !RogueHUDPrivate::IsRunOver(RogueGameState, PlayerCharacter) &&
						  LocalPlayerState != nullptr && LocalPlayerState->IsShopOpen();
	const int32 AutoRefreshSeconds =
		bVisible ? FMath::CeilToInt(LocalPlayerState->GetShopSecondsUntilRefresh()) : INDEX_NONE;
	FString Signature = bVisible ? TEXT("Shop|Visible") : TEXT("Shop|Hidden");
	if (bVisible)
	{
		Signature += FString::Printf(TEXT("|Money=%d|Refresh=%d"), PlayerCharacter->GetMoney(),
									 LocalPlayerState->GetShopRefreshCost());
		const TArray<FRogueShopOffer> &Offers = LocalPlayerState->GetShopOffers();
		for (const FRogueShopOffer &Offer : Offers)
		{
			const bool bAffordable = PlayerCharacter->GetMoney() >= Offer.GetCost();
			Signature +=
				FString::Printf(TEXT("|%d:%d:%d:%d:%s:%s"), static_cast<int32>(Offer.GetUpgrade().Type),
								Offer.GetCost(), Offer.IsPurchased() ? 1 : 0, bAffordable ? 1 : 0,
								*Offer.GetUpgrade().Title, *Offer.GetUpgrade().Description);
			Signature += TEXT("|") + Offer.QuoteId.ToString();
		}
	}

	if (Signature == LastShopWidgetSignature)
	{
		if (bVisible && AutoRefreshSeconds != LastShopAutoRefreshSeconds)
		{
			LastShopAutoRefreshSeconds = AutoRefreshSeconds;
			ShopWidget->UpdateAutoRefreshText(RogueHUDPrivate::AutoRefreshText(AutoRefreshSeconds));
		}
		return;
	}
	PresentedShopOffers = bVisible ? LocalPlayerState->GetShopOffers() : TArray<FRogueShopOffer>();
	LastShopWidgetSignature = Signature;
	LastShopAutoRefreshSeconds = AutoRefreshSeconds;

	FRogueShopViewData ViewData;
	ViewData.bVisible = bVisible;
	ViewData.TitleText = FText::FromString(TEXT("商店"));
	if (PlayerCharacter != nullptr)
	{
		ViewData.MoneyText = FText::FromString(FString::Printf(TEXT("金币 %d"), PlayerCharacter->GetMoney()));
	}
	ViewData.HintText = FText::FromString(TEXT("点击卡牌购买强化"));

	// 注意这里**不能**再写 `if (RogueGameMode != nullptr)`：
	// 客户端恒为 nullptr，那样商品列表、刷新按钮、倒计时在客户端全是空的
	// ——表现就是"客户端打开商店只有一个空框"，而主机一切正常。
	// 下面用到的每个字段都在本地 PlayerState 上。
	if (bVisible && LocalPlayerState != nullptr)
	{
		ViewData.AutoRefreshText = RogueHUDPrivate::AutoRefreshText(AutoRefreshSeconds);
		ViewData.RefreshButtonText =
			FText::FromString(FString::Printf(TEXT("立即刷新 (%d)"), LocalPlayerState->GetShopRefreshCost()));

		const TArray<FRogueShopOffer> &Offers = LocalPlayerState->GetShopOffers();
		for (const FRogueShopOffer &Offer : Offers)
		{
			FRogueShopOfferViewData &OfferView = ViewData.Offers.AddDefaulted_GetRef();
			OfferView.QuoteId = Offer.QuoteId;
			OfferView.IconType = Offer.GetUpgrade().Type;
			OfferView.QuotedCost = Offer.GetCost();
			OfferView.TitleText = FText::FromString(Offer.GetUpgrade().Title);
			OfferView.DescriptionText = FText::FromString(Offer.GetUpgrade().Description);
			OfferView.bPurchased = Offer.IsPurchased();
			OfferView.bAffordable =
				PlayerCharacter != nullptr && PlayerCharacter->GetMoney() >= Offer.GetCost();
			OfferView.CostText = FText::FromString(
				Offer.IsPurchased() ? TEXT("已购买") : FString::Printf(TEXT("价格 %d"), Offer.GetCost()));
		}
	}

	ShopWidget->UpdateShopView(ViewData);
}

void ARogueHUD::UpdateUpgradeSelectionWidget()
{
	if (UpgradeSelectionWidget == nullptr)
	{
		return;
	}

	// 待选卡牌改从**本地玩家的 PlayerState** 读，而不是 GameMode。
	// 原因：GameMode 是服务端独占对象，客户端 `GetAuthGameMode()` 返回 nullptr，
	// 结果就是"客户端玩家看不到升级界面"（迁移前的真实故障）。
	// PlayerState 会被复制到拥有者客户端，而且每个玩家有自己的卡牌。
	const ARoguePlayerState *LocalPlayerState = GetLocalRoguePlayerState();
	// 无 PlayerState 时提供静态空数组作为只读回退；此处引用仅在当前调用中使用。
	static const TArray<FRogueUpgradeOption> EmptyUpgradeOptions;
	const TArray<FRogueUpgradeOption> &Options =
		LocalPlayerState != nullptr ? LocalPlayerState->GetPendingUpgrades() : EmptyUpgradeOptions;

	const ARogueCharacter *PlayerCharacter = RogueHUDPrivate::GetLocalPlayerCharacter(this);
	const ARogueGameState *RogueGameState = GetRogueGameState();
	const bool bVisible = PlayerCharacter != nullptr && PlayerCharacter->IsAlive() &&
						  !RogueHUDPrivate::IsRunOver(RogueGameState, PlayerCharacter) &&
						  LocalPlayerState != nullptr && LocalPlayerState->IsAwaitingUpgradeChoice();

	FString Signature=FString::Printf(TEXT("Upgrade|%d|%d"),bVisible,LocalPlayerState?LocalPlayerState->GetPendingUpgradeCount():0);
	for (const FRogueUpgradeOption &Option : Options)
	{
		Signature += FString::Printf(TEXT("|%d:%s:%s:%s:%d"), static_cast<int32>(Option.Type), *Option.Title,
									 *Option.Description, *Option.SelectionId.ToString(),
									 LocalPlayerState &&
										 LocalPlayerState->IsUpgradeSubmissionPending(Option.SelectionId));
	}

	if (!RogueHUDPrivate::UpdateSignature(LastUpgradeWidgetSignature, Signature))
	{
		return;
	}

	PresentedUpgradeSelectionId = bVisible && Options.Num() ? Options[0].SelectionId : FGuid();
	FRogueUpgradeSelectionViewData ViewData;
	ViewData.bVisible = bVisible;
	ViewData.TitleText = FText::FromString(FString::Printf(TEXT("请选择升级 · 剩余 %d 次"),LocalPlayerState?LocalPlayerState->GetPendingUpgradeCount():0));
	for (const FRogueUpgradeOption &Option : Options)
	{
		FRogueUpgradeCardViewData &Card = ViewData.Cards.AddDefaulted_GetRef();
		Card.SelectionId = Option.SelectionId;
		Card.IconType = Option.Type;
		Card.bCanSelect = bVisible && Option.SelectionId.IsValid() &&
						  !LocalPlayerState->IsUpgradeSubmissionPending(Option.SelectionId);
		Card.TitleText = FText::FromString(Option.Title);
		Card.DescriptionText = FText::FromString(Option.Description);
	}

	UpgradeSelectionWidget->UpdateUpgradeSelectionView(ViewData);
}

ARoguePlayerState *ARogueHUD::GetLocalRoguePlayerState() const
{
	APlayerController *OwningPlayerController = GetOwningPlayerController();
	return OwningPlayerController != nullptr ? OwningPlayerController->GetPlayerState<ARoguePlayerState>()
											 : nullptr;
}

void ARogueHUD::UpdatePauseWidget()
{
	if (PauseMenuWidget == nullptr)
	{
		return;
	}

	const ARogueCharacter *PlayerCharacter = RogueHUDPrivate::GetLocalPlayerCharacter(this);
	const ARogueGameState *RogueGameState = GetRogueGameState();
	const bool bVisible = PauseMenuPage == ERoguePauseMenuPage::Main && PlayerCharacter != nullptr &&
						  PlayerCharacter->IsAlive() &&
						  !RogueHUDPrivate::IsRunOver(RogueGameState, PlayerCharacter);
	const FString Signature = bVisible ? TEXT("Pause|Main") : TEXT("Pause|Hidden");
	if (!RogueHUDPrivate::UpdateSignature(LastPauseWidgetSignature, Signature))
	{
		return;
	}

	FRoguePauseMenuViewData ViewData;
	ViewData.bVisible = bVisible;
	ViewData.TitleText = FText::FromString(TEXT("暂停菜单"));
	PauseMenuWidget->UpdatePauseView(ViewData);
}

void ARogueHUD::UpdateReviveCountdownWidget(const ARogueCharacter *PlayerCharacter)
{
	if (ReviveCountdownWidget == nullptr)
	{
		return;
	}

	// 服务器复制的生命状态决定可见性，剩余时间只决定文字，零秒仍等待复活确认。
	const ARogueGameState *RogueGameState = GetRogueGameState();

	const bool bDowned = PlayerCharacter != nullptr && PlayerCharacter->IsDowned();
	const bool bRunOver = RogueHUDPrivate::IsRunOver(RogueGameState, PlayerCharacter);
	const bool bVisible = bDowned && !bRunOver;
	const float Remaining = bVisible ? PlayerCharacter->GetReviveSecondsRemaining() : 0.f;
	const int32 SecondsRemaining = FMath::Max(0, FMath::CeilToInt(Remaining));
	if (bVisible)
	{
		ReviveCountdownWidget->SetFrostSealProgress(1.f - Remaining / PlayerCharacter->GetReviveWaitDuration());
	}

	// 缓存必须属于当前 HUD：函数 static 会让同进程的多个玩家共用最后一次状态。
	// 队友先推送"隐藏 / 0 秒"时，刚复活的玩家也会命中这个缓存，自己的界面便一直不隐藏。
	// 同时核对实际可见性，确保数据未变但 widget 状态漂移时仍能重新应用。
	const ESlateVisibility DesiredVisibility =
		bVisible ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed;
	const bool bVisibilityMatches = ReviveCountdownWidget->GetVisibility() == DesiredVisibility;
	if (bVisible == bLastReviveCountdownVisible && SecondsRemaining == LastReviveCountdownSeconds &&
		bVisibilityMatches)
	{
		return;
	}
	LastReviveCountdownSeconds = SecondsRemaining;
	bLastReviveCountdownVisible = bVisible;

	// 秒数、可见性或实际 widget 可见性不匹配时输出；状态切换可在同一秒发生多次。
	// 日志记录数据推送，不等于已验证实际渲染、样式或所有蓝图异步行为。
	UE_LOG(LogTemp, Log, TEXT("[Rogue] 复活倒计时界面：%s（剩余 %d 秒，本机角色 IsDowned=%d）"),
		   bVisible ? TEXT("显示") : TEXT("隐藏"), SecondsRemaining, bDowned ? 1 : 0);

	ReviveCountdownWidget->UpdateReviveCountdown(SecondsRemaining, bVisible);
	UE_LOG(LogTemp, Log, TEXT("[RogueLife] UI Player=%s Resurrection=%d Seconds=%d Phase=%d"),
		   GetLocalRoguePlayerState() != nullptr ? *GetLocalRoguePlayerState()->GetPlayerName()
												 : TEXT("NoPlayerState"),
		   bVisible ? 1 : 0, SecondsRemaining,
		   PlayerCharacter != nullptr ? static_cast<int32>(PlayerCharacter->GetLifePhase()) : -1);
}

namespace
{
    void UpdatePersonalRunSummary(UUserWidget& Widget, const ARoguePlayerState* PlayerState,
        const URogueOwnedItemsWidget* Bar)
    {
        auto* Reward = Cast<UTextBlock>(Widget.GetWidgetFromName(TEXT("PersonalRunReward")));
        auto* Loadout = Cast<UHorizontalBox>(Widget.GetWidgetFromName(TEXT("PersonalRunLoadout")));
        if (!PlayerState || !Reward || !Loadout) return;

        const auto Caption = FText::Format(NSLOCTEXT("RogueBattle", "RunSummary", "本局获得 {0} 冰砖   ·   本局武器"),
            FText::AsNumber(PlayerState->GetBossIceBricksEarnedThisRun()));
        if (!Reward->GetText().EqualTo(Caption)) Reward->SetText(Caption);

        if (!Bar) return;
        const auto& Items = Bar->GetItems();
        const auto* Theme = Bar->PresentationTheme.Get();
        int32 Index = 0;
        for (const auto& Item : Items)
        {
            if (!Item.bWeapon) continue;
            auto* Image = Cast<UImage>(Loadout->GetChildAt(Index++));
            if (!Image) break;
            if (auto* Material = Theme ? Theme->GetWeaponIcon(Item.Type) : nullptr)
            {
                if (Image->GetBrush().GetResourceObject() != Material)
                {
                    Image->SetBrushFromMaterial(Material);
                    Image->SetToolTipText(Item.Name);
                }
                Image->SetVisibility(ESlateVisibility::Visible);
            }
            else Image->SetVisibility(ESlateVisibility::Collapsed);
        }
        while (Index < Loadout->GetChildrenCount())
            if (auto* Slot = Loadout->GetChildAt(Index++)) Slot->SetVisibility(ESlateVisibility::Collapsed);
    }
}

bool ARogueHUD::IsCoveringBattleFeedback() const
{
    return IsPauseMenuOpen() || (ShopWidget && ShopWidget->IsVisible()) ||
        (UpgradeSelectionWidget && UpgradeSelectionWidget->IsVisible());
}

void ARogueHUD::UpdateVictoryWidget()
{
	if (!VictoryScreenWidget)
		return;
	const auto *State = GetRogueGameState();
	const FRogueRunResult Result = State ? State->GetRunResult() : FRogueRunResult();
	const bool bVisible = Result.Outcome == ERogueRunOutcome::Victory;
    if (bVisible) UpdatePersonalRunSummary(*VictoryScreenWidget, GetLocalRoguePlayerState(), OwnedItemsWidget);
	const FString Signature = FString::Printf(TEXT("%d|%d|%d|%.2f"), static_cast<int32>(Result.Outcome),
											  Result.Wave, Result.EnemiesDefeated, Result.RunTimeSeconds);
	const bool bChanged = RogueHUDPrivate::UpdateSignature(LastVictoryWidgetSignature, Signature);
	if (bChanged || VictoryScreenWidget->IsVisible() != bVisible)
	{
		VictoryScreenWidget->UpdateVictoryView(Result);
		if (bVisible)
			UE_LOG(LogTemp, Log, TEXT("[RogueVictory] UI Player=%s Wave=%d"),
				   *GetNameSafe(GetOwningPlayerController()), Result.Wave);
	}
}

void ARogueHUD::UpdateDeathWidget()
{
	if (DeathScreenWidget == nullptr)
	{
		return;
	}

	// GameOver 或本机 Eliminated 都能确认团灭；无 Pawn 时仍由 GameState 提供状态。
	const ARogueCharacter *PlayerCharacter = RogueHUDPrivate::GetLocalPlayerCharacter(this);
	const ARogueGameState *RogueGameState = GetRogueGameState();
	const float SummaryRunTime = RogueGameState != nullptr ? RogueGameState->GetRunTimeSeconds() : 0.0f;
	const int32 SummaryKills = RogueGameState != nullptr ? RogueGameState->GetEnemiesDefeated() : 0;
	const bool bVisible = !(RogueGameState && RogueGameState->IsVictory()) &&
						  RogueHUDPrivate::IsRunOver(RogueGameState, PlayerCharacter);

	FString Signature = bVisible ? TEXT("Death|Visible") : TEXT("Death|Hidden");
	if (bVisible)
	{
		Signature += FString::Printf(TEXT("|Time=%.0f|Kills=%d"), SummaryRunTime, SummaryKills);
        UpdatePersonalRunSummary(*DeathScreenWidget, GetLocalRoguePlayerState(), OwnedItemsWidget);
	}

	const bool bSignatureChanged = RogueHUDPrivate::UpdateSignature(LastDeathWidgetSignature, Signature);
	const ESlateVisibility DesiredVisibility =
		bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;
	if (!bSignatureChanged && DeathScreenWidget->GetVisibility() == DesiredVisibility)
	{
		return;
	}

	FRogueDeathViewData ViewData;
	ViewData.bVisible = bVisible;
	if (bVisible)
	{
		ViewData.TitleText = FText::FromString(TEXT("本轮挑战结束"));
		ViewData.SummaryText = FText::FromString(
			FString::Printf(TEXT("Baka 先歇一会儿，下次再战！\n坚持 %.0f 秒  ·  全队击败 %d 个敌人"),
							SummaryRunTime, SummaryKills));
	}

	DeathScreenWidget->UpdateDeathView(ViewData);
	UE_LOG(LogTemp, Log, TEXT("[RogueLife] UI Player=%s Death=%d Phase=%d"),
		   GetLocalRoguePlayerState() != nullptr ? *GetLocalRoguePlayerState()->GetPlayerName()
												 : TEXT("NoPlayerState"),
		   bVisible ? 1 : 0,
		   PlayerCharacter != nullptr ? static_cast<int32>(PlayerCharacter->GetLifePhase()) : -1);
}
