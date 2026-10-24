#include "UI/RogueHUD.h"

#include "Player/RogueCharacter.h"
#include "Enemies/RogueEnemy.h"
#include "Framework/RogueGameMode.h"
#include "Framework/RogueGameState.h"
#include "Player/RoguePlayerState.h"
#include "UI/Widgets/RogueMenuWidgetBases.h"
#include "UI/Widgets/RogueHUDViewData.h"
#include "UI/Widgets/RogueVitalsWidgetBase.h"
#include "UI/Widgets/RogueDamageFlashWidgetBase.h"
#include "UI/Widgets/RogueEnemyHealthBarWidgetBase.h"
#include "UI/Widgets/RogueMainMenuWidgetBase.h"
#include "Networking/RogueRoomSubsystem.h"
#include "UI/Widgets/RogueTutorialWidgetBase.h"
#include "UI/Widgets/RogueUpgradeTreeWidgetBase.h"
#include "Player/RogueDamageCameraShake.h"
#include "Combat/Targeting/RogueEnemyTrackerSubsystem.h"
#include "AudioDevice.h"
#include "Camera/CameraShakeBase.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/Font.h"
#include "GameFramework/GameUserSettings.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/ConfigCacheIni.h"


#include "RogueHUDWidgetFactory.h"
using namespace RogueHUDWidgetFactory;

// Main-menu widget ownership, navigation and room entry points.
void ARogueHUD::InitializeMainMenuWidgets()
{
	APlayerController* PlayerController = GetOwningPlayerController();
	if (PlayerController == nullptr)
	{
		return;
	}

	ResolveWidgetClass(MainMenuWidgetClassPath, MainMenuWidgetClass);
	ResolveWidgetClass(TutorialWidgetClassPath, TutorialWidgetClass);
	ResolveWidgetClass(SettingsMenuWidgetClassPath, SettingsMenuWidgetClass);

	// 原生子类（ARogueMainMenuHUD）的 CDO 不会读父类 [/Script/ai.RogueHUD] 段：
	// 未在自己的 Config 段里配置时，回退到 ARogueHUD 的 CDO（即 DefaultGame.ini 里的那份）。
	const ARogueHUD* BaseDefaults = GetDefault<ARogueHUD>();
	if (MainMenuWidgetClassPath.IsNull())
	{
		MainMenuWidgetClassPath = BaseDefaults->MainMenuWidgetClassPath;
	}
	if (TutorialWidgetClassPath.IsNull())
	{
		TutorialWidgetClassPath = BaseDefaults->TutorialWidgetClassPath;
	}
	if (SettingsMenuWidgetClassPath.IsNull())
	{
		SettingsMenuWidgetClassPath = BaseDefaults->SettingsMenuWidgetClassPath;
	}
	if (SinglePlayerMapPath.IsEmpty())
	{
		SinglePlayerMapPath = BaseDefaults->SinglePlayerMapPath;
	}
	ResolveWidgetClass(MainMenuWidgetClassPath, MainMenuWidgetClass);
	ResolveWidgetClass(TutorialWidgetClassPath, TutorialWidgetClass);
	ResolveWidgetClass(SettingsMenuWidgetClassPath, SettingsMenuWidgetClass);

	// 主菜单常驻最上层；设置/教程压在它之上，默认收起。
	MainMenuWidget = CreateRogueMenuWidget(PlayerController, MainMenuWidgetClass, this, 100);
	if (MainMenuWidget != nullptr)
	{
		MainMenuWidget->SetIsFocusable(true);
		MainMenuWidget->SetVisibility(ESlateVisibility::Visible);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[Rogue] 主菜单创建失败：MainMenuWidgetClass=%s（检查 DefaultGame.ini 的 MainMenuWidgetClassPath）"),
			*GetNameSafe(MainMenuWidgetClass.Get()));
	}

	SettingsMenuWidget = CreateRogueMenuWidget(PlayerController, SettingsMenuWidgetClass, this, 110);
	TutorialWidget = CreateRogueMenuWidget(PlayerController, TutorialWidgetClass, this, 120);
	if (SettingsMenuWidget != nullptr)
	{
		SettingsMenuWidget->SetIsFocusable(true);
	}
	if (TutorialWidget != nullptr)
	{
		TutorialWidget->SetIsFocusable(true);
	}

	// 玩家控制器可能在 HUD BeginPlay 之后才完成输入初始化；延后 0.2 秒再锁 UI 输入。
	MainMenuInputApplyTime = GetWorld() != nullptr ? GetWorld()->GetTimeSeconds() + 0.2f : 0.0f;

	UE_LOG(LogTemp, Log, TEXT("[Rogue] 主菜单模式 HUD 就绪：MainMenu=%s Settings=%s Tutorial=%s"),
		*GetNameSafe(MainMenuWidgetClass.Get()),
		*GetNameSafe(SettingsMenuWidgetClass.Get()),
		*GetNameSafe(TutorialWidgetClass.Get()));
}

void ARogueHUD::UpdateMainMenuWidgets()
{
	if (APlayerController* PlayerController = GetOwningPlayerController())
	{
		// 主菜单是纯 UI：只应用一次 UIOnly，并把键盘焦点给主菜单。
		// 不要用 bEnableClickEvents/bEnableMouseOverEvents 当“已应用”的判据：
		// 两者默认 false，且不由 FInputModeUIOnly 设置；拿它们判断会每帧重设输入模式，
		// 按钮的按下/抬起被拆到两帧，表现就是“点了没反应”。
		const float NowSeconds = GetWorld() != nullptr ? GetWorld()->GetTimeSeconds() : 0.0f;
		if ((!bMainMenuInputApplied && NowSeconds >= MainMenuInputApplyTime)
			|| (bMainMenuInputApplied && !PlayerController->bShowMouseCursor))
		{
			FInputModeUIOnly InputMode;
			InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			UUserWidget* FocusWidget = MainMenuWidget;
			if (bMainMenuSettingsOpen && SettingsMenuWidget != nullptr) FocusWidget = SettingsMenuWidget;
			if (bMainMenuTutorialOpen && TutorialWidget != nullptr) FocusWidget = TutorialWidget;
			if (bMainMenuUpgradeTreeOpen && UpgradeTreeWidget != nullptr) FocusWidget = UpgradeTreeWidget;
			if (FocusWidget != nullptr) InputMode.SetWidgetToFocus(FocusWidget->TakeWidget());
			PlayerController->SetInputMode(InputMode);
			PlayerController->bShowMouseCursor = true;
			PlayerController->SetIgnoreMoveInput(true);
			PlayerController->SetIgnoreLookInput(true);
			bMainMenuInputApplied = true;
		}
	}

	// 复用游戏内设置面板的数据推送；bMainMenuMode 在 UpdateSettingsWidget 里切换可见条件。
	UpdateSettingsWidget();
}

void ARogueHUD::SetMainMenuMode(bool bEnabled)
{
	bMainMenuMode = bEnabled;
}

void ARogueHUD::StartSinglePlayerGame()
{
	UE_LOG(LogTemp, Log, TEXT("[Rogue] 主菜单点击：单人游戏"));
	if (SinglePlayerMapPath.IsEmpty())
	{
		ShowMainMenuHint(FText::FromString(TEXT("未配置单人游戏关卡")));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[Rogue] 主菜单：开始单人游戏 -> %s"), *SinglePlayerMapPath);
	UGameplayStatics::OpenLevel(this, FName(*SinglePlayerMapPath));
}

void ARogueHUD::OpenMainMenuSettings()
{
	UE_LOG(LogTemp, Log, TEXT("[Rogue] 主菜单点击：设置"));
	CloseUpgradeTree();
	CloseMainMenuTutorial();
	bMainMenuSettingsOpen = true;
	UpdateSettingsWidget();
	if (SettingsMenuWidget != nullptr)
	{
		SettingsMenuWidget->SetKeyboardFocus();
	}
}

void ARogueHUD::CloseMainMenuSettings()
{
	bMainMenuSettingsOpen = false;
	UpdateSettingsWidget();
}

void ARogueHUD::OpenMainMenuTutorial()
{
	UE_LOG(LogTemp, Log, TEXT("[Rogue] 主菜单点击：教程"));
	CloseUpgradeTree();
	CloseMainMenuSettings();
	bMainMenuTutorialOpen = true;
	if (TutorialWidget != nullptr)
	{
		TutorialWidget->SetVisibility(ESlateVisibility::Visible);
		TutorialWidget->SetKeyboardFocus();
	}
}

void ARogueHUD::CloseMainMenuTutorial()
{
	bMainMenuTutorialOpen = false;
	if (TutorialWidget != nullptr)
	{
		TutorialWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void ARogueHUD::OpenUpgradeTree()
{
	if (!bMainMenuMode) return;
	if (UpgradeTreeWidget == nullptr)
	{
		if (UpgradeTreeWidgetClassPath.IsNull())
		{
			UpgradeTreeWidgetClassPath = GetDefault<ARogueHUD>()->UpgradeTreeWidgetClassPath;
		}
		ResolveWidgetClass(UpgradeTreeWidgetClassPath, UpgradeTreeWidgetClass);
		UpgradeTreeWidget = CreateRogueMenuWidget(GetOwningPlayerController(), UpgradeTreeWidgetClass, this, 130);
	}
	if (UpgradeTreeWidget == nullptr)
	{
		ShowMainMenuHint(FText::FromString(TEXT("升级树界面未配置，请检查 UpgradeTreeWidgetClassPath。")));
		return;
	}
	CloseMainMenuSettings();
	CloseMainMenuTutorial();
	bMainMenuUpgradeTreeOpen = true;
	if (MainMenuWidget != nullptr) MainMenuWidget->SetVisibility(ESlateVisibility::Collapsed);
	UpgradeTreeWidget->RefreshTree();
	UpgradeTreeWidget->SetVisibility(ESlateVisibility::Visible);
	UpgradeTreeWidget->SetKeyboardFocus();
	UE_LOG(LogTemp, Log, TEXT("[RogueProgression] OpenUpgradeTree LocalPlayer=%s"), *GetNameSafe(GetOwningPlayerController()));
}

void ARogueHUD::CloseUpgradeTree()
{
	const bool bWasOpen = bMainMenuUpgradeTreeOpen;
	bMainMenuUpgradeTreeOpen = false;
	if (UpgradeTreeWidget != nullptr) UpgradeTreeWidget->SetVisibility(ESlateVisibility::Collapsed);
	if (bWasOpen && MainMenuWidget != nullptr)
	{
		MainMenuWidget->SetVisibility(ESlateVisibility::Visible);
		MainMenuWidget->SetKeyboardFocus();
	}
}

void ARogueHUD::ShowMainMenuHint(const FText& Hint)
{
	if (MainMenuWidget != nullptr)
	{
		MainMenuWidget->SetHint(Hint);
	}
}

void ARogueHUD::QuitToDesktop()
{
	UE_LOG(LogTemp, Log, TEXT("[Rogue] 主菜单点击：退出游戏"));
	UKismetSystemLibrary::QuitGame(this, GetOwningPlayerController(), EQuitPreference::Quit, false);
}

const ARogueGameState* ARogueHUD::GetRogueGameState() const
{
	// 与 GameMode 不同，GameState 在服务端与客户端**都存在**（客户端拿到的是复制副本），
	// 所以它是客户端 HUD 读取整局状态的正确入口。
	return GetWorld() != nullptr ? GetWorld()->GetGameState<ARogueGameState>() : nullptr;
}

void ARogueHUD::RequestReturnToMainMenuAfterDeath()
{
	APlayerController* Controller = GetOwningPlayerController();
	if (Controller == nullptr || !Controller->IsLocalController()) return;
	PauseMenuPage = ERoguePauseMenuPage::None;
	UGameplayStatics::SetGamePaused(this, false);
	// The engine leaves the current network session and returns to GameDefaultMap.
	// A joining client leaves locally; a departing listen host closes its hosted game.
	if (UGameInstance* Instance = GetGameInstance()) Instance->ReturnToMainMenu();
}

void ARogueHUD::OpenMultiplayerMenu()
{
	if (!bMainMenuMode) return;
	CloseUpgradeTree(); CloseMainMenuSettings(); CloseMainMenuTutorial();
	if (MainMenuWidget) MainMenuWidget->SetVisibility(ESlateVisibility::Collapsed);
	if (URogueRoomSubsystem* Rooms=URogueRoomSubsystem::GetForPlayer(GetOwningPlayerController())) Rooms->ShowMenu(GetOwningPlayerController());
}
void ARogueHUD::CloseMultiplayerMenu()
{
	if (URogueRoomSubsystem* Rooms=URogueRoomSubsystem::GetForPlayer(GetOwningPlayerController())) Rooms->CloseMenu();
	if (MainMenuWidget) { MainMenuWidget->SetVisibility(ESlateVisibility::Visible); MainMenuWidget->SetKeyboardFocus(); }
}
