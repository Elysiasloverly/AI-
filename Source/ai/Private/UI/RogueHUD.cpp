#include "UI/RogueHUD.h"
#include "UI/Widgets/RogueKillFeedbackWidgetBase.h"
#include "UI/Widgets/RogueVictoryScreenWidgetBase.h"
#include "UI/Widgets/RogueMenuWidgetBases.h"
#include "UI/Widgets/RogueVitalsWidgetBase.h"
#include "Camera/CameraShakeBase.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Engine/World.h"
#include "UI/Widgets/RogueBossHealthBarWidgetBase.h"
#include "UI/Widgets/RogueNightTransitionWidgetBase.h"
#include "UI/Widgets/RogueBattlePresentationWidget.h"
#include "UI/Widgets/RogueOwnedItemsWidget.h"

#include "Player/RogueCharacter.h"
#include "Player/RoguePlayerState.h"
#include "Enemies/RogueEnemy.h"
#include "Combat/Targeting/RogueEnemyTrackerSubsystem.h"
#include "Framework/RogueGameState.h"
#include "UI/Widgets/RogueEnemyHealthBarWidgetBase.h"
#include "Player/RogueDamageCameraShake.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "GameFramework/PlayerController.h"

#include "RogueHUDWidgetFactory.h"
#include "RogueHUDPrivate.h"
using namespace RogueHUDWidgetFactory;


ARogueHUD::ARogueHUD()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	// 原生震动类做默认值：不配 BP 也有手感；想要不同风格就继承它做个 BP 再覆盖这个字段。
	DamageCameraShakeClass = URogueDamageCameraShake::StaticClass();
}

void ARogueHUD::BeginPlay()
{
	Super::BeginPlay();

	RefreshPauseSettings();
	RefreshAudioVolumes();

	if (bMainMenuMode)
	{
		InitializeMainMenuWidgets();
		UpdateMainMenuWidgets();
		return;
	}

	InitializeMenuWidgets();
	RegisterPauseInput();
    if(GetOwningPlayerController()&&!OwnedItemsWidgetClassPath.IsNull())
    {OwnedItemsWidget=CreateWidget<URogueOwnedItemsWidget>(GetOwningPlayerController(),OwnedItemsWidgetClassPath.LoadSynchronous());if(OwnedItemsWidget)OwnedItemsWidget->AddToViewport(67);}
    if(GetOwningPlayerController()&&!BattlePresentationWidgetClassPath.IsNull())
    {BattlePresentationWidget=CreateWidget<URogueBattlePresentationWidget>(GetOwningPlayerController(),BattlePresentationWidgetClassPath.LoadSynchronous());if(BattlePresentationWidget)BattlePresentationWidget->AddToViewport(9);}
    if (GetOwningPlayerController() && !BossHealthBarWidgetClassPath.IsNull())
    {
        BossHealthBarWidget = CreateWidget<URogueBossHealthBarWidgetBase>(GetOwningPlayerController(), BossHealthBarWidgetClassPath.LoadSynchronous());
        if (BossHealthBarWidget) { BossHealthBarWidget->AddToViewport(8); BossHealthBarWidget->UpdateBossHealth({}); }
    }
    if (GetOwningPlayerController() && !NightTransitionWidgetClassPath.IsNull())
    {
        NightTransitionWidget = CreateWidget<URogueNightTransitionWidgetBase>(GetOwningPlayerController(), NightTransitionWidgetClassPath.LoadSynchronous());
        if (NightTransitionWidget)
        {
            NightTransitionWidget->AddToViewport(60);
            NightTransitionWidget->RefreshCountdown();
        }
    }
}


void ARogueHUD::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bMainMenuMode)
	{
		UpdateMainMenuWidgets();
		return;
	}
	RegisterPauseInput();
	RefreshMenuWidgets();
    if (BossHealthBarWidget)
    {
        const ARogueGameState* State = GetRogueGameState();
        BossHealthBarWidget->UpdateBossHealth(State && !State->IsGameOver() ? State->GetBossHealthView() : FRogueBossHealthView());
    }
	if (NightTransitionWidget) { NightTransitionWidget->RefreshCountdown(); }
    if(BattlePresentationWidget)BattlePresentationWidget->RefreshPresentation();
    if((OwnedItemsRefreshAge+=DeltaSeconds)>=.1f)
    {
        OwnedItemsRefreshAge=0;
        if(OwnedItemsWidget){OwnedItemsWidget->RefreshItems();if(KillFeedbackWidget)KillFeedbackWidget->SetBottomInset(OwnedItemsWidget->GetOccupiedHeight());}
    }
}

void ARogueHUD::RefreshMenuWidgets()
{
	++MenuRefreshCount;
	ARogueCharacter* PlayerCharacter = RogueHUDPrivate::GetLocalPlayerCharacter(this);
	if (!IsValid(PlayerCharacter))
	{
		PlayerCharacter = nullptr;
	}
	const ARogueGameState* RogueGameState = GetRogueGameState();
	const bool bRunOver = RogueHUDPrivate::IsRunOver(RogueGameState, PlayerCharacter);
	const bool bGameplayMenusAllowed = PlayerCharacter != nullptr && PlayerCharacter->IsAlive() && !bRunOver;
	if (!bGameplayMenusAllowed)
	{
		ClosePauseMenu();
	}

    // 客户端没有 GameMode。HUD 从复制的 GameState / 所属 PlayerState / Pawn 读取状态，
    // 因而不能用 GetAuthGameMode() 是否有效决定是否刷新界面。

	// 菜单/界面更新与 Canvas 无关：先把状态推给 widget。
	// 仅由 Tick 调度；无画布测试也通过 Tick 检查菜单状态
	// （否则"界面不刷新"的 bug 会被无头测试掩盖掉）。
	UpdateShopWidget(PlayerCharacter);
	UpdateUpgradeSelectionWidget();
	UpdatePauseWidget();
	UpdateSettingsWidget();
	if (!bGameplayMenusAllowed)
	{
		// 即使蓝图回调或尚未结束的菜单动画改了可见性，也只能留下当前生命状态的界面。
		UUserWidget* GameplayMenus[] = {ShopWidget.Get(), UpgradeSelectionWidget.Get(), PauseMenuWidget.Get(), SettingsMenuWidget.Get()};
		for (UUserWidget* Menu : GameplayMenus)
		{
			if (Menu != nullptr)
			{
				Menu->StopAllAnimations();
				Menu->SetVisibility(ESlateVisibility::Collapsed);
			}
		}
	}
	// 先收起等待界面，再交由团灭结算接管，避免同一帧两种界面同时可见。
	UpdateReviveCountdownWidget(PlayerCharacter);
	UpdateDeathWidget();
    UpdateVictoryWidget();

	// 常驻状态条与敌人血条：与 Canvas 无关，无画布测试也能检查数据是否推到位。
	UpdateVitalsWidget(PlayerCharacter);

	// 掉血反馈：血量是普通复制，没有 RepNotify，只能按帧比较。
	UpdateLocalDamageFeedback(PlayerCharacter);

	UpdateEnemyHealthBarWidgets();

	// 输入模式（光标/UI 捕获）：按本地可见状态推导，见函数注释
	UpdateInteractionInputMode(PlayerCharacter);

}

void ARogueHUD::DrawHUD()
{
	// 主菜单只有 UMG，不需要 Canvas 绘制（也没有角色/敌人数据可画）。
	if (bMainMenuMode)
	{
		return;
	}

	if (Canvas != nullptr)
	{
		Super::DrawHUD();
	}

	// UMG is refreshed once by Tick; DrawHUD only renders Canvas feedback.

	if (Canvas == nullptr)
	{
		return;
	}

	UFont* Font = GEngine != nullptr ? GEngine->GetSmallFont() : nullptr;

	// 玩家状态/波数/金币全部在 WBP_PlayerVitals 里；Canvas 只保留战斗反馈。
	if (EnemyHealthBarWidgetClass == nullptr)
	{
		DrawEnemyHealthBars(Font);
	}
	DrawDamageNumbers(Font);
	DrawRemainingEnemyLocations();

	const auto* Tracker = GetWorld() ? GetWorld()->GetSubsystem<URogueEnemyTrackerSubsystem>() : nullptr;
	const auto* Player = RogueHUDPrivate::GetLocalPlayerCharacter(this);
	if (!Tracker || !Tracker->ShouldCullCombatEffects() || !Player || !Player->IsAlive() ||
		RogueHUDPrivate::IsRunOver(GetRogueGameState(), Player)) return;

	// Keep the marker just below the local character so the center of the dodge lane stays clear.
	const FVector Point = Canvas->Project(Player->GetActorLocation());
	if (Point.Z <= 0.f || Point.ContainsNaN()) return;
	const float Scale = FMath::Clamp(float(Canvas->SizeY) / 810.f, .75f, 1.55f);
	const float X = Point.X;
	const float Y = Point.Y + 29.f * Scale;
	const float Wing = 8.f * Scale;
	const float Height = 5.f * Scale;
	const FLinearColor Ink(.018f, .035f, .07f, .90f);
	const FLinearColor Frost(.56f, .89f, 1.f, .96f);
	DrawLine(X - Wing, Y + Height, X, Y, Ink, 4.f * Scale);
	DrawLine(X, Y, X + Wing, Y + Height, Ink, 4.f * Scale);
	DrawLine(X - Wing, Y + Height, X, Y, Frost, 2.f * Scale);
	DrawLine(X, Y, X + Wing, Y + Height, Frost, 2.f * Scale);
}


void ARogueHUD::OpenPauseMenu()
{
	const ARogueCharacter* PlayerCharacter = RogueHUDPrivate::GetLocalPlayerCharacter(this);
	const ARogueGameState* RogueGameState = GetRogueGameState();
	if (PlayerCharacter == nullptr || !PlayerCharacter->IsAlive()
		|| RogueHUDPrivate::IsRunOver(RogueGameState, PlayerCharacter))
	{
		ClosePauseMenu();
		return;
	}
	RefreshPauseSettings();
	PauseMenuPage = ERoguePauseMenuPage::Main;
	if (ARoguePlayerState* State = GetLocalRoguePlayerState()) State->Server_SetPauseMenuOpen(true);
	UpdateInteractionInputMode(PlayerCharacter);
}

void ARogueHUD::ClosePauseMenu()
{
	const bool bWasOpen = IsPauseMenuOpen();
	PauseMenuPage = ERoguePauseMenuPage::None;
	if (bWasOpen)
	{
		if (ARoguePlayerState* State = GetLocalRoguePlayerState()) State->Server_SetPauseMenuOpen(false);
		UpdateInteractionInputMode(RogueHUDPrivate::GetLocalPlayerCharacter(this));
	}
}


void ARogueHUD::EndPlay(const EEndPlayReason::Type Reason)
{
    UnregisterPauseInput();
    ClosePauseMenu();
    if (KillFeedbackWidget) { KillFeedbackWidget->RemoveFromParent(); KillFeedbackWidget=nullptr; }
    if (VictoryScreenWidget) { VictoryScreenWidget->RemoveFromParent(); VictoryScreenWidget=nullptr; }
    if (BossHealthBarWidget) { BossHealthBarWidget->RemoveFromParent(); BossHealthBarWidget = nullptr; }
    if (NightTransitionWidget) { NightTransitionWidget->RemoveFromParent(); NightTransitionWidget = nullptr; }
    if(BattlePresentationWidget){BattlePresentationWidget->RemoveFromParent();BattlePresentationWidget=nullptr;}
    if(OwnedItemsWidget){OwnedItemsWidget->RemoveFromParent();OwnedItemsWidget=nullptr;}
    // 只释放本 HUD 持有的状态条，避免跨地图留下旧反馈；不影响其他系统的 UI。
    if (VitalsWidget) { VitalsWidget->RemoveFromParent(); VitalsWidget = nullptr; }
    for (URogueEnemyHealthBarWidgetBase* Bar : EnemyHealthBarWidgets) { if (Bar) Bar->RemoveFromParent(); }
    EnemyHealthBarWidgets.Reset();
    Super::EndPlay(Reason);
}

void ARogueHUD::NotifyWeaponAttack(ERogueUpgradeType Type)
{
    const auto* PC=GetOwningPlayerController();
    if(!bMainMenuMode&&PC&&PC->IsLocalController()&&OwnedItemsWidget)OwnedItemsWidget->NotifyWeaponAttack(Type);
}
