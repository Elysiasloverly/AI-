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
#include "Player/RogueDamageCameraShake.h"
#include "Combat/Targeting/RogueEnemyTrackerSubsystem.h"
#include "AudioDevice.h"
#include "Camera/CameraShakeBase.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "GameFramework/GameUserSettings.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/ConfigCacheIni.h"

namespace
{
	const TCHAR* RogueSettingsSection = TEXT("/Script/ai.RogueRuntimeSettings");
	const TCHAR* MasterVolumeKey = TEXT("MasterVolume");

	template <typename TWidget>
	void ResolveWidgetClass(const TSoftClassPtr<TWidget>& Path, TSubclassOf<TWidget>& InOutClass)
	{
		if (InOutClass == nullptr && !Path.IsNull())
		{
			InOutClass = Path.LoadSynchronous();
		}
	}
	constexpr int32 MaxDamageNumbersNormal = 42;
	constexpr int32 MaxDamageNumbersHeavy = 24;
	constexpr int32 MaxRegularHealthBarsNormal = 22;
	constexpr int32 MaxRegularHealthBarsHeavy = 12;
	constexpr float EnemyHealthBarRefreshIntervalNormal = 0.05f;
	constexpr float EnemyHealthBarRefreshIntervalHeavy = 0.10f;
	constexpr float DamageProjectionRefreshIntervalNormal = 0.05f;
	constexpr float DamageProjectionRefreshIntervalHeavy = 0.09f;

	bool IsRogueRunOver(const ARogueGameState* GameState, const ARogueCharacter* PlayerCharacter)
	{
		// Pawn 和 GameState 可按任意顺序复制；Eliminated 同样确认权威团灭终态。
		return (GameState != nullptr && GameState->IsGameOver())
			|| (PlayerCharacter != nullptr && PlayerCharacter->IsEliminated());
	}

	bool UpdateCachedSignature(FString& LastSignature, const FString& NewSignature)
	{
		if (NewSignature == LastSignature)
		{
			return false;
		}

		LastSignature = NewSignature;
		return true;
	}

	FText GetAutoRefreshText(int32 Seconds)
	{
		return FText::FromString(FString::Printf(TEXT("自动补货 %d 秒"), FMath::Max(0, Seconds)));
	}

	template <typename WidgetType>
	WidgetType* CreateRogueMenuWidget(APlayerController* PlayerController, TSubclassOf<WidgetType> WidgetClass, ARogueHUD* OwningHUD, int32 ZOrder)
	{
		if (PlayerController == nullptr || WidgetClass == nullptr || OwningHUD == nullptr)
		{
			return nullptr;
		}

		WidgetType* Widget = CreateWidget<WidgetType>(PlayerController, WidgetClass);
		if (Widget == nullptr)
		{
			return nullptr;
		}

		Widget->SetOwningRogueHUD(OwningHUD);
		Widget->AddToViewport(ZOrder);
		Widget->SetVisibility(ESlateVisibility::Collapsed);
		return Widget;
	}
}

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
	ApplyMasterVolume();
	InitializeMenuWidgets();
}

namespace
{
	/**
	 * 取"这个 HUD 所服务的那个本地玩家"的角色。
	 *
	 * 使用 HUD 的所属控制器，支持本地多玩家并避免依赖固定玩家索引。
	 * GetPlayerCharacter(..., 0) 按当前 World 查找控制器；同进程 PIE 的多个 World
	 * 并不自动共享同一个索引 0，分屏同一 World 才更直接暴露固定索引选错玩家的问题。
	 */
	ARogueCharacter* GetLocalPlayerCharacter(const AHUD* HUD)
	{
		if (HUD == nullptr)
		{
			return nullptr;
		}

		APlayerController* OwningPlayerController = HUD->GetOwningPlayerController();
		return OwningPlayerController != nullptr ? Cast<ARogueCharacter>(OwningPlayerController->GetPawn()) : nullptr;
	}
}

void ARogueHUD::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	RefreshMenuWidgets();
}

void ARogueHUD::RefreshMenuWidgets()
{
	ARogueCharacter* PlayerCharacter = GetLocalPlayerCharacter(this);
	if (!IsValid(PlayerCharacter))
	{
		PlayerCharacter = nullptr;
	}
	const ARogueGameState* RogueGameState = GetRogueGameState();
	const bool bRunOver = IsRogueRunOver(RogueGameState, PlayerCharacter);
	const bool bGameplayMenusAllowed = PlayerCharacter != nullptr && PlayerCharacter->IsAlive() && !bRunOver;
	if (!bGameplayMenusAllowed)
	{
		ClosePauseMenu();
	}

	// ⚠️ 这里**不能**把 GameMode 放进早退条件（连"取出来备用"都不要，免得后人又拿它当判据）。
	// GetWorld()->GetAuthGameMode() 在客户端恒为 nullptr（GameMode 是服务端独占对象），
	// 一旦把它当成"初始化没完成"的判据，客户端就会每帧从这里 return ——
	// 表现是客户端玩家**整个 HUD 都是死的**：商店、升级选卡、暂停菜单、设置、
	// 死亡结算、连状态条都不更新。而它偏偏在单机与主机上完全正常，所以极难发现。
	// HUD 需要的一切都从 GameState / 本地 PlayerState / 本地角色上读（它们客户端都有）。

	// 菜单/界面更新与 Canvas 无关：先把状态推给 widget。
	// 由 Tick 和 DrawHUD 入口触发，创建了 HUD/UMG 的无画布测试仍可检查菜单状态
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
	if (Canvas != nullptr)
	{
		Super::DrawHUD();
	}

	// 菜单/状态条更新与 Canvas 无关；WBP 在 RefreshMenuWidgets 里刷新。
	RefreshMenuWidgets();

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
}

void ARogueHUD::AddDamageNumber(const FVector& WorldLocation, float Damage, bool bIsBossDamage)
{
	if (GetWorld() == nullptr || Damage <= 0.0f)
	{
		return;
	}

	const URogueEnemyTrackerSubsystem* TrackerForDamage = GetWorld()->GetSubsystem<URogueEnemyTrackerSubsystem>();
	const int32 MaxDamageNumbers = TrackerForDamage != nullptr && TrackerForDamage->ShouldCullCombatEffects() ? MaxDamageNumbersHeavy : MaxDamageNumbersNormal;
	if (DamageNumbers.Num() >= MaxDamageNumbers)
	{
		const int32 OverflowCount = DamageNumbers.Num() - MaxDamageNumbers + 1;
		DamageNumbers.RemoveAt(0, OverflowCount, EAllowShrinking::No);
	}

	FRogueDamageNumber DamageNumber;
	DamageNumber.WorldLocation = WorldLocation;
	DamageNumber.Damage = Damage;
	DamageNumber.SpawnTime = GetWorld()->GetTimeSeconds();
	DamageNumber.Lifetime = bIsBossDamage ? 0.95f : 0.72f;
	DamageNumber.Color = bIsBossDamage ? FLinearColor(1.0f, 0.55f, 0.25f, 1.0f) : FLinearColor(1.0f, 0.92f, 0.55f, 1.0f);
	DamageNumber.bScreenPositionValid = false;
	// 文本在这里算一次（数值之后不会再变，见 FRogueDamageNumber::CachedText 的说明）
	DamageNumber.CachedText = FString::Printf(TEXT("%.0f"), Damage);
	DamageNumbers.Add(MoveTemp(DamageNumber));
}

void ARogueHUD::OpenPauseMenu()
{
	const ARogueCharacter* PlayerCharacter = GetLocalPlayerCharacter(this);
	const ARogueGameState* RogueGameState = GetRogueGameState();
	if (PlayerCharacter == nullptr || !PlayerCharacter->IsAlive()
		|| IsRogueRunOver(RogueGameState, PlayerCharacter))
	{
		ClosePauseMenu();
		return;
	}
	RefreshPauseSettings();
	PauseMenuPage = ERoguePauseMenuPage::Main;
}

void ARogueHUD::ClosePauseMenu()
{
	PauseMenuPage = ERoguePauseMenuPage::None;
}

void ARogueHUD::RequestSelectOfferFromShop(int32 OfferIndex)
{
	// 走本地玩家 PlayerState 的 ServerRPC —— 服务端与客户端都可用
	if (ARoguePlayerState* RoguePlayerState = GetLocalRoguePlayerState())
	{
		RoguePlayerState->Server_RequestBuyShopOffer(OfferIndex);
	}
}

void ARogueHUD::RequestRefreshFromShop()
{
	if (ARoguePlayerState* RoguePlayerState = GetLocalRoguePlayerState())
	{
		RoguePlayerState->Server_RequestRefreshShop();
	}
}

void ARogueHUD::RequestCloseFromShop()
{
	if (ARoguePlayerState* RoguePlayerState = GetLocalRoguePlayerState())
	{
		RoguePlayerState->Server_RequestCloseShop();
	}
}

void ARogueHUD::RequestSelectUpgradeFromWidget(int32 UpgradeIndex)
{
	// 走本地玩家 PlayerState 的 ServerRPC —— 这样服务端与客户端都能正确请求。
	// 旧写法调 GetRogueGameMode()->TrySelectUpgrade(...)，而 GetRogueGameMode() 在客户端是 nullptr。
	if (APlayerController* OwningPlayerController = GetOwningPlayerController())
	{
		if (ARoguePlayerState* RoguePlayerState = OwningPlayerController->GetPlayerState<ARoguePlayerState>())
		{
			RoguePlayerState->Server_RequestUpgradeSelection(UpgradeIndex);
		}
	}
}

void ARogueHUD::RequestResumeFromPauseMenu()
{
	ClosePauseMenu();
	if (ARogueCharacter* PlayerCharacter = GetLocalPlayerCharacter(this))
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

void ARogueHUD::RequestBackFromSettingsMenu()
{
	PauseMenuPage = ERoguePauseMenuPage::Main;
}

void ARogueHUD::RequestApplyFromSettingsMenu()
{
	ApplyDisplaySettings();
}

void ARogueHUD::RequestVolumeAdjustFromSettingsMenu(bool bIncrease)
{
	const float Direction = bIncrease ? 1.0f : -1.0f;
	MasterVolume = FMath::Clamp(MasterVolume + Direction * 0.1f, 0.0f, 1.0f);
	ApplyMasterVolume();
}

void ARogueHUD::RequestQualityAdjustFromSettingsMenu(bool bIncrease)
{
	GraphicsQualityLevel = FMath::Clamp(GraphicsQualityLevel + (bIncrease ? 1 : -1), 0, 4);
	bGraphicsQualityCustom = false;
	bDisplaySettingsDirty = true;
}

void ARogueHUD::RequestResolutionAdjustFromSettingsMenu(bool bIncrease)
{
	StepResolution(bIncrease ? 1 : -1);
	bDisplaySettingsDirty = true;
}

void ARogueHUD::RequestFrameLimitAdjustFromSettingsMenu(bool bIncrease)
{
	StepFrameRateLimit(bIncrease ? 1 : -1);
	bDisplaySettingsDirty = true;
}

void ARogueHUD::RequestToggleDisplayModeFromSettingsMenu()
{
	CycleDisplayMode();
	bDisplaySettingsDirty = true;
}

void ARogueHUD::RequestRestartAfterDeath()
{
	PauseMenuPage = ERoguePauseMenuPage::None;
	// 请求清除当前世界的暂停状态；是否允许由权威 GameMode/控制器规则决定。
	UGameplayStatics::SetGamePaused(this, false);

	// 重开必须由**服务端**执行：客户端自己 OpenLevel 只会加载出一份单机关卡副本，
	// 表现为"点了重开之后队友全没了"。所以这里只发一个请求，
	// 服务端校验本局已结束后，联机使用 ServerTravel；单机路径使用 OpenLevel。
	if (ARoguePlayerState* LocalPlayerState = GetLocalRoguePlayerState())
	{
		LocalPlayerState->Server_RequestRestartRun();
	}
}

void ARogueHUD::RequestQuitAfterDeath()
{
	PauseMenuPage = ERoguePauseMenuPage::None;
	// 同 RequestRestartAfterDeath：离开前的防御性清理
	UGameplayStatics::SetGamePaused(this, false);
	UKismetSystemLibrary::QuitGame(this, GetOwningPlayerController(), EQuitPreference::Quit, false);
}

void ARogueHUD::DrawEnemyHealthBars(UFont* Font)
{
	APlayerController* PlayerController = GetOwningPlayerController();
	URogueEnemyTrackerSubsystem* Tracker = GetWorld() ? GetWorld()->GetSubsystem<URogueEnemyTrackerSubsystem>() : nullptr;
	if (Canvas == nullptr || PlayerController == nullptr || Tracker == nullptr || GetWorld() == nullptr)
	{
		return;
	}

	const float CurrentTime = GetWorld()->GetTimeSeconds();
	const bool bHeavyCombat = Tracker->ShouldCullCombatEffects();
	const float RefreshInterval = bHeavyCombat ? EnemyHealthBarRefreshIntervalHeavy : EnemyHealthBarRefreshIntervalNormal;
	if (CurrentTime >= NextHealthBarCacheRefreshTime)
	{
		RefreshEnemyHealthBarCache(Tracker);
		NextHealthBarCacheRefreshTime = CurrentTime + RefreshInterval;
	}

	for (const FRogueCachedEnemyHealthBar& CachedBar : CachedEnemyHealthBars)
	{
		ARogueEnemy* Enemy = CachedBar.Enemy.Get();
		if (!IsValid(Enemy) || Enemy->IsDead())
		{
			continue;
		}

		FVector2D ScreenPosition;
		if (!PlayerController->ProjectWorldLocationToScreen(Enemy->GetHealthBarWorldLocation(), ScreenPosition, true))
		{
			continue;
		}

		if (ScreenPosition.X < -120.0f || ScreenPosition.X > Canvas->SizeX + 120.0f || ScreenPosition.Y < -80.0f || ScreenPosition.Y > Canvas->SizeY + 80.0f)
		{
			continue;
		}

		const float BarWidth = CachedBar.bBoss ? 136.0f : 62.0f;
		const float BarHeight = CachedBar.bBoss ? 10.0f : 6.0f;
		const float BarX = ScreenPosition.X - BarWidth * 0.5f;
		const float BarY = ScreenPosition.Y;
		const FLinearColor FillColor = CachedBar.bBoss ? FLinearColor(0.95f, 0.30f, 0.20f, 0.95f) : FLinearColor(0.95f, 0.15f, 0.20f, 0.90f);
		const float RawHealthPercent = Enemy->GetHealthPercent();
		const float HealthPercent = FMath::IsFinite(RawHealthPercent)
			? FMath::Clamp(RawHealthPercent, 0.0f, 1.0f)
			: 0.0f;

		DrawRect(FLinearColor(0.04f, 0.04f, 0.05f, 0.70f), BarX - 2.0f, BarY - 2.0f, BarWidth + 4.0f, BarHeight + 4.0f);
		DrawRect(FLinearColor(0.10f, 0.10f, 0.12f, 0.90f), BarX, BarY, BarWidth, BarHeight);
		DrawRect(FillColor, BarX, BarY, BarWidth * HealthPercent, BarHeight);

		if (CachedBar.bBoss)
		{
			DrawText(TEXT("BOSS"), FLinearColor(1.0f, 0.8f, 0.45f, 0.95f), BarX + 44.0f, BarY - 18.0f, Font, 0.9f, false);
		}
	}
}

void ARogueHUD::UpdateVitalsWidget(const ARogueCharacter* PlayerCharacter)
{
	if (VitalsWidget == nullptr)
	{
		return;
	}

	auto SafePercent = [](float Value)
	{
		return FMath::IsFinite(Value) ? FMath::Clamp(Value, 0.0f, 1.0f) : 0.0f;
	};

	FRoguePlayerVitalsView View;
	if (PlayerCharacter != nullptr)
	{
		View.bVisible = true;
		View.bHasArmor = PlayerCharacter->HasArmor();
		View.bDowned = PlayerCharacter->IsDowned();
		View.HealthPercent = SafePercent(PlayerCharacter->GetHealthPercent());
		View.ArmorPercent = SafePercent(PlayerCharacter->GetArmorPercent());
		View.ExperiencePercent = SafePercent(PlayerCharacter->GetExperiencePercent());
		View.CurrentHealth = PlayerCharacter->GetCurrentHealth();
		View.MaxHealth = PlayerCharacter->GetMaxHealth();
		View.CurrentArmor = PlayerCharacter->GetCurrentArmor();
		View.MaxArmor = PlayerCharacter->GetMaxArmor();
		View.Level = PlayerCharacter->GetPlayerLevel();
		View.Experience = PlayerCharacter->GetCurrentExperience();
		View.ExperienceToNextLevel = PlayerCharacter->GetExperienceToNextLevel();
		View.Money = PlayerCharacter->GetMoney();
		if (const ARogueGameState* RogueGameState = GetRogueGameState())
		{
			View.CurrentWave = RogueGameState->GetCurrentWave();
		}
	}

	VitalsWidget->UpdateVitals(View);
}

void ARogueHUD::UpdateLocalDamageFeedback(ARogueCharacter* PlayerCharacter)
{
	// 泛红淡出由 HUD 按帧推进：即使这一帧没有受伤，也要继续把上一次的残影收干净。
	if (DamageFlashWidget != nullptr)
	{
		const UWorld* World = GetWorld();
		DamageFlashWidget->AdvanceFlash(World != nullptr ? World->GetDeltaSeconds() : 0.0f);
	}

	if (PlayerCharacter == nullptr)
	{
		// 观战 / 团灭 / 尚未 Possess：没有"我的血量"可比，清掉基线。
		// 否则下一个角色上线时会把它的满血当成"从 0 涨回来"，或者把残血当成一次重击。
		LastObservedHealthPawn = nullptr;
		LastObservedHealth = -1.0f;
		return;
	}

	if (LastObservedHealthPawn.Get() != PlayerCharacter)
	{
		// 换 Pawn（重生、重连、分屏另一个玩家）：只建立基线，不触发反馈。
		LastObservedHealthPawn = PlayerCharacter;
		LastObservedHealth = PlayerCharacter->GetCurrentHealth();
		return;
	}

	const float CurrentHealth = PlayerCharacter->GetCurrentHealth();
	const float PreviousHealth = LastObservedHealth;
	LastObservedHealth = CurrentHealth;

	// 只在血量**下降**时触发。0.5 的容差用来吸收浮点与护甲/再生同帧抵消带来的抖动；
	// 治疗、上限提升（HandleMaxHealthChanged 补满增量）都走上升方向，不会误触发。
	const float DamageTaken = PreviousHealth - CurrentHealth;
	if (DamageTaken <= 0.5f)
	{
		return;
	}

	const float MaxHealth = FMath::Max(PlayerCharacter->GetMaxHealth(), 1.0f);
	TriggerDamageFeedback(DamageTaken / MaxHealth);
}

void ARogueHUD::TriggerDamageFeedback(float Intensity)
{
	const float Normalized = FMath::Clamp(Intensity, 0.0f, 1.0f);

	// 泛红交给 WBP：这里只传强度，颜色/边缘宽度/动画都由蓝图决定。
	if (DamageFlashWidget != nullptr)
	{
		DamageFlashWidget->PlayDamageFlash(Normalized);
	}

	APlayerController* PlayerController = GetOwningPlayerController();
	if (PlayerController == nullptr
		|| PlayerController->PlayerCameraManager == nullptr
		|| DamageCameraShakeClass == nullptr)
	{
		return;
	}

	// 连续受击（多段伤害、多个敌人同帧命中）会叠加震动实例，抖成一片。
	// 用最小间隔压成一次，但下一次仍会按新伤害重新评估强度。
	const UWorld* World = GetWorld();
	const float Now = World != nullptr ? World->GetTimeSeconds() : 0.0f;
	if (Now - LastCameraShakeTime < CameraShakeMinInterval)
	{
		return;
	}
	LastCameraShakeTime = Now;

	// 小伤害也保留可感知的震动：缩放不低于 0.55，避免"掉了血但画面完全没反应"。
	const float Scale = FMath::Lerp(0.55f, 1.0f, Normalized);
	PlayerController->PlayerCameraManager->StartCameraShake(DamageCameraShakeClass, Scale);
}

void ARogueHUD::UpdateEnemyHealthBarWidgets()
{
	APlayerController* PlayerController = GetOwningPlayerController();
	UWorld* World = GetWorld();
	URogueEnemyTrackerSubsystem* Tracker = World != nullptr ? World->GetSubsystem<URogueEnemyTrackerSubsystem>() : nullptr;
	if (PlayerController == nullptr || World == nullptr || Tracker == nullptr || EnemyHealthBarWidgetClass == nullptr)
	{
		HideAllEnemyHealthBarWidgets();
		return;
	}

	int32 ViewportSizeX = 0;
	int32 ViewportSizeY = 0;
	PlayerController->GetViewportSize(ViewportSizeX, ViewportSizeY);
	if (ViewportSizeX <= 0 || ViewportSizeY <= 0)
	{
		HideAllEnemyHealthBarWidgets();
		return;
	}

	const float CurrentTime = World->GetTimeSeconds();
	const bool bHeavyCombat = Tracker->ShouldCullCombatEffects();
	const float RefreshInterval = bHeavyCombat ? EnemyHealthBarRefreshIntervalHeavy : EnemyHealthBarRefreshIntervalNormal;
	if (CurrentTime >= NextHealthBarCacheRefreshTime)
	{
		RefreshEnemyHealthBarCache(Tracker);
		NextHealthBarCacheRefreshTime = CurrentTime + RefreshInterval;
	}

	int32 UsedWidgets = 0;
	for (const FRogueCachedEnemyHealthBar& CachedBar : CachedEnemyHealthBars)
	{
		ARogueEnemy* Enemy = CachedBar.Enemy.Get();
		if (!IsValid(Enemy) || Enemy->IsDead())
		{
			continue;
		}

		FVector2D ScreenPosition;
		if (!PlayerController->ProjectWorldLocationToScreen(Enemy->GetHealthBarWorldLocation(), ScreenPosition, true))
		{
			continue;
		}

		if (ScreenPosition.X < -120.0f || ScreenPosition.X > ViewportSizeX + 120.0f
			|| ScreenPosition.Y < -80.0f || ScreenPosition.Y > ViewportSizeY + 80.0f)
		{
			continue;
		}

		URogueEnemyHealthBarWidgetBase* Widget = AcquireEnemyHealthBarWidget(UsedWidgets);
		if (Widget == nullptr)
		{
			break;
		}

		// 显式排除非有限比例，再钳到 [0,1]，防止异常比例传给 WBP 的进度条。
		const float RawHealthPercent = Enemy->GetHealthPercent();

		FRogueEnemyHealthBarView View;
		View.bVisible = true;
		View.bBoss = CachedBar.bBoss;
		View.HealthPercent = FMath::IsFinite(RawHealthPercent)
			? FMath::Clamp(RawHealthPercent, 0.0f, 1.0f)
			: 0.0f;
		View.ScreenPosition = ScreenPosition;
		Widget->UpdateBar(View);

		++UsedWidgets;
	}

	for (int32 Index = UsedWidgets; Index < EnemyHealthBarWidgets.Num(); ++Index)
	{
		if (EnemyHealthBarWidgets[Index] != nullptr)
		{
			EnemyHealthBarWidgets[Index]->UpdateBar(FRogueEnemyHealthBarView());
		}
	}
}

URogueEnemyHealthBarWidgetBase* ARogueHUD::AcquireEnemyHealthBarWidget(int32 Index)
{
	while (EnemyHealthBarWidgets.Num() <= Index)
	{
		URogueEnemyHealthBarWidgetBase* Widget = CreateRogueMenuWidget(
			GetOwningPlayerController(), EnemyHealthBarWidgetClass, this, 6);
		if (Widget == nullptr)
		{
			return nullptr;
		}

		// 血条是屏幕空间的小控件：先按 WBP 的 BarSize 固定尺寸，再以顶部中心对齐。
		Widget->SetDesiredSizeInViewport(Widget->GetBarSize());
		Widget->SetAlignmentInViewport(FVector2D(0.5f, 0.0f));
		EnemyHealthBarWidgets.Add(Widget);
	}

	return EnemyHealthBarWidgets[Index];
}

void ARogueHUD::HideAllEnemyHealthBarWidgets()
{
	for (TObjectPtr<URogueEnemyHealthBarWidgetBase>& Widget : EnemyHealthBarWidgets)
	{
		if (Widget != nullptr)
		{
			Widget->UpdateBar(FRogueEnemyHealthBarView());
		}
	}
}

void ARogueHUD::DrawDamageNumbers(UFont* Font)
{
	APlayerController* PlayerController = GetOwningPlayerController();
	URogueEnemyTrackerSubsystem* Tracker = GetWorld() ? GetWorld()->GetSubsystem<URogueEnemyTrackerSubsystem>() : nullptr;
	if (Canvas == nullptr || PlayerController == nullptr || GetWorld() == nullptr)
	{
		return;
	}

	const float CurrentTime = GetWorld()->GetTimeSeconds();
	const bool bHeavyCombat = Tracker != nullptr && Tracker->ShouldCullCombatEffects();
	const int32 MaxDamageNumbers = bHeavyCombat ? MaxDamageNumbersHeavy : MaxDamageNumbersNormal;
	if (DamageNumbers.Num() > MaxDamageNumbers)
	{
		DamageNumbers.RemoveAt(0, DamageNumbers.Num() - MaxDamageNumbers, EAllowShrinking::No);
	}

	for (int32 Index = DamageNumbers.Num() - 1; Index >= 0; --Index)
	{
		const float Age = CurrentTime - DamageNumbers[Index].SpawnTime;
		if (Age >= DamageNumbers[Index].Lifetime)
		{
			DamageNumbers.RemoveAtSwap(Index, 1, EAllowShrinking::No);
		}
	}

	const float RefreshInterval = bHeavyCombat ? DamageProjectionRefreshIntervalHeavy : DamageProjectionRefreshIntervalNormal;

	// 飘字为空时跳过投影缓存更新；有数据时按设定间隔更新屏幕位置。
	// 是否为 HUD 性能瓶颈需要在具体场景测量，不能仅由元素数推断。
	if (DamageNumbers.Num() > 0 && CurrentTime >= NextDamageNumberProjectionRefreshTime)
	{
		RefreshDamageNumberProjectionCache(PlayerController, Tracker);
		NextDamageNumberProjectionRefreshTime = CurrentTime + RefreshInterval;
	}

	for (const FRogueDamageNumber& DamageNumber : DamageNumbers)
	{
		const float Age = CurrentTime - DamageNumber.SpawnTime;
		if (Age >= DamageNumber.Lifetime || !DamageNumber.bScreenPositionValid)
		{
			continue;
		}
		const float Alpha = 1.0f - FMath::Clamp(Age / DamageNumber.Lifetime, 0.0f, 1.0f);
		FLinearColor TextColor = DamageNumber.Color;
		TextColor.A = Alpha;
		const float Scale = FMath::Lerp(1.25f, 0.9f, Age / DamageNumber.Lifetime);
		const FString& DamageText = DamageNumber.CachedText;
		DrawText(DamageText, FLinearColor(0.02f, 0.02f, 0.02f, Alpha * 0.65f), DamageNumber.CachedScreenPosition.X - 1.0f, DamageNumber.CachedScreenPosition.Y - 1.0f, Font, Scale, false);
		DrawText(DamageText, TextColor, DamageNumber.CachedScreenPosition.X, DamageNumber.CachedScreenPosition.Y, Font, Scale, false);
	}
}

void ARogueHUD::RefreshEnemyHealthBarCache(const URogueEnemyTrackerSubsystem* Tracker)
{
	CachedEnemyHealthBars.Reset();
	if (Tracker == nullptr)
	{
		return;
	}

	const APlayerController* PlayerController = GetOwningPlayerController();
	const APawn* PlayerPawn = PlayerController != nullptr ? PlayerController->GetPawn() : nullptr;
	const FVector PlayerLocation = PlayerPawn != nullptr ? PlayerPawn->GetActorLocation() : FVector::ZeroVector;
	const bool bHeavyCombat = Tracker->ShouldCullCombatEffects();
	const float MaxBarDistance = bHeavyCombat ? 1300.0f : 1800.0f;
	const int32 MaxRegularBars = bHeavyCombat ? MaxRegularHealthBarsHeavy : MaxRegularHealthBarsNormal;
	struct FCandidateBar
	{
		ARogueEnemy* Enemy = nullptr;
		float DistanceSquared = 0.0f;
		bool bBoss = false;
	};

	TArray<FCandidateBar> RegularCandidates;
	RegularCandidates.Reserve(MaxRegularBars + 12);

	int32 DrawnRegularBars = 0;

	for (const TWeakObjectPtr<ARogueEnemy>& EnemyPtr : Tracker->GetActiveEnemies())
	{
		ARogueEnemy* Enemy = EnemyPtr.Get();
		if (!IsValid(Enemy) || Enemy->IsDead())
		{
			continue;
		}

		const bool bBoss = Enemy->IsBoss();
		const float DistanceSquared = FVector::DistSquared2D(PlayerLocation, Enemy->GetActorLocation());
		if (!bBoss && DistanceSquared > FMath::Square(MaxBarDistance))
		{
			continue;
		}

		if (bBoss)
		{
			FRogueCachedEnemyHealthBar& CachedBar = CachedEnemyHealthBars.AddDefaulted_GetRef();
			CachedBar.Enemy = Enemy;
			CachedBar.bBoss = true;
			continue;
		}

		FCandidateBar& Candidate = RegularCandidates.AddDefaulted_GetRef();
		Candidate.Enemy = Enemy;
		Candidate.DistanceSquared = DistanceSquared;
		Candidate.bBoss = false;
	}

	RegularCandidates.Sort([](const FCandidateBar& Left, const FCandidateBar& Right)
	{
		return Left.DistanceSquared < Right.DistanceSquared;
	});

	for (const FCandidateBar& Candidate : RegularCandidates)
	{
		if (DrawnRegularBars >= MaxRegularBars)
		{
			break;
		}

		FRogueCachedEnemyHealthBar& CachedBar = CachedEnemyHealthBars.AddDefaulted_GetRef();
		CachedBar.Enemy = Candidate.Enemy;
		CachedBar.bBoss = false;
		++DrawnRegularBars;
	}
}

void ARogueHUD::RefreshDamageNumberProjectionCache(APlayerController* PlayerController, const URogueEnemyTrackerSubsystem* Tracker)
{
	if (PlayerController == nullptr || GetWorld() == nullptr)
	{
		return;
	}

	const float CurrentTime = GetWorld()->GetTimeSeconds();
	const bool bHeavyCombat = Tracker != nullptr && Tracker->ShouldCullCombatEffects();
	const float PopupRiseSpeed = bHeavyCombat ? 72.0f : 90.0f;

	for (FRogueDamageNumber& DamageNumber : DamageNumbers)
	{
		const float Age = CurrentTime - DamageNumber.SpawnTime;
		DamageNumber.bScreenPositionValid = false;
		if (Age >= DamageNumber.Lifetime)
		{
			continue;
		}

		const FVector PopupLocation = DamageNumber.WorldLocation + FVector(0.0f, 0.0f, Age * PopupRiseSpeed);
		DamageNumber.bScreenPositionValid = PlayerController->ProjectWorldLocationToScreen(PopupLocation, DamageNumber.CachedScreenPosition, true);
	}
}

void ARogueHUD::InitializeMenuWidgets()
{
	if (APlayerController* PlayerController = GetOwningPlayerController())
	{
		// 兼容旧 HUD 蓝图将默认属性保存为 None 的情况；默认路径来自 DefaultGame.ini。
		ResolveWidgetClass(DeathScreenWidgetClassPath, DeathScreenWidgetClass);
		ResolveWidgetClass(ReviveCountdownWidgetClassPath, ReviveCountdownWidgetClass);
		ResolveWidgetClass(VitalsWidgetClassPath, VitalsWidgetClass);
		ResolveWidgetClass(EnemyHealthBarWidgetClassPath, EnemyHealthBarWidgetClass);
		UpgradeSelectionWidget = CreateRogueMenuWidget(PlayerController, UpgradeSelectionWidgetClass, this, 63);
		ShopWidget = CreateRogueMenuWidget(PlayerController, ShopWidgetClass, this, 64);
		PauseMenuWidget = CreateRogueMenuWidget(PlayerController, PauseMenuWidgetClass, this, 65);
		SettingsMenuWidget = CreateRogueMenuWidget(PlayerController, SettingsMenuWidgetClass, this, 66);
		DeathScreenWidget = CreateRogueMenuWidget(PlayerController, DeathScreenWidgetClass, this, 70);

		ReviveCountdownWidget = CreateRogueMenuWidget(PlayerController, ReviveCountdownWidgetClass, this, 69);

		// 常驻状态条：ZOrder 5，位于所有菜单之下；敌人血条 6，由池按需创建。
		VitalsWidget = CreateRogueMenuWidget(PlayerController, VitalsWidgetClass, this, 5);
		if (VitalsWidgetClass != nullptr && VitalsWidget == nullptr)
		{
			UE_LOG(LogTemp, Warning, TEXT("[Rogue] 玩家状态条创建失败：Vitals=%s"), *GetNameSafe(VitalsWidgetClass.Get()));
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
			UE_LOG(LogTemp, Warning, TEXT("[Rogue] 未配置受伤泛红 WBP：DamageFlashWidgetClass 为空，受击时只会有镜头震动。"));
		}

		if (EnemyHealthBarWidgetClass == nullptr)
		{
			UE_LOG(LogTemp, Warning, TEXT("[Rogue] 未配置敌人血条 WBP：EnemyHealthBarWidgetClass 为空，敌人血条将不显示。"));
		}
		if (DeathScreenWidget == nullptr || ReviveCountdownWidget == nullptr)
		{
			UE_LOG(LogTemp, Error, TEXT("[Rogue] 复活/团灭界面创建失败：Resurrection=%s Death=%s"),
				*GetNameSafe(ReviveCountdownWidgetClass.Get()), *GetNameSafe(DeathScreenWidgetClass.Get()));
		}
	}
}

const ARogueGameState* ARogueHUD::GetRogueGameState() const
{
	// 与 GameMode 不同，GameState 在服务端与客户端**都存在**（客户端拿到的是复制副本），
	// 所以它是客户端 HUD 读取整局状态的正确入口。
	return GetWorld() != nullptr ? GetWorld()->GetGameState<ARogueGameState>() : nullptr;
}

void ARogueHUD::UpdateInteractionInputMode(const ARogueCharacter* PlayerCharacter)
{
	APlayerController* PlayerController = GetOwningPlayerController();
	if (PlayerController == nullptr)
	{
		return;
	}

	// 团灭状态不依赖 Pawn，等待持有/重新复制期间仍能点击结算按钮。
	const ARoguePlayerState* LocalPlayerState = GetLocalRoguePlayerState();
	const ARogueGameState* RogueGameState = GetRogueGameState();
	const bool bRunOver = IsRogueRunOver(RogueGameState, PlayerCharacter);
	const bool bAlive = PlayerCharacter != nullptr && PlayerCharacter->IsAlive();
	const bool bWantUIInput = bRunOver
		|| (PlayerCharacter != nullptr && PlayerCharacter->IsDowned())
		|| (bAlive && (PauseMenuPage != ERoguePauseMenuPage::None
			|| (LocalPlayerState != nullptr && (LocalPlayerState->IsShopOpen() || LocalPlayerState->IsAwaitingUpgradeChoice()))));

	const int32 NewState = bRunOver ? 2 : (bWantUIInput ? 1 : 0);

	// 除状态缓存外，还核对光标/点击/悬停三个标志，纠正其他入口对这些标志的改动。
	// 本检查不读取当前 Slate 输入模式或焦点；若仅这些状态被改动，缓存仍可能提前返回。
	const bool bAppliedStateMatches = PlayerController->bShowMouseCursor == bWantUIInput
		&& PlayerController->bEnableClickEvents == bWantUIInput
		&& PlayerController->bEnableMouseOverEvents == bWantUIInput;
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
		if (DeathScreenWidget != nullptr)
		{
			DeathScreenWidget->SetIsFocusable(true);
			InputMode.SetWidgetToFocus(DeathScreenWidget->TakeWidget());
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

void ARogueHUD::UpdateShopWidget(const ARogueCharacter* PlayerCharacter)
{
	if (ShopWidget == nullptr)
	{
		return;
	}

	// 商店的全部读数据都来自本地 PlayerState（客户端可读）
	const ARoguePlayerState* LocalPlayerState = GetLocalRoguePlayerState();

	const ARogueGameState* RogueGameState = GetRogueGameState();
	const bool bVisible = PlayerCharacter != nullptr && PlayerCharacter->IsAlive()
		&& !IsRogueRunOver(RogueGameState, PlayerCharacter)
		&& LocalPlayerState != nullptr && LocalPlayerState->IsShopOpen();
	const int32 AutoRefreshSeconds = bVisible ? FMath::CeilToInt(LocalPlayerState->GetShopSecondsUntilRefresh()) : INDEX_NONE;
	FString Signature = bVisible ? TEXT("Shop|Visible") : TEXT("Shop|Hidden");
	if (bVisible)
	{
		Signature += FString::Printf(TEXT("|Money=%d|Refresh=%d"), PlayerCharacter->GetMoney(), LocalPlayerState->GetShopRefreshCost());
		const TArray<FRogueShopOffer>& Offers = LocalPlayerState->GetShopOffers();
		for (const FRogueShopOffer& Offer : Offers)
		{
			const bool bAffordable = PlayerCharacter->GetMoney() >= Offer.GetCost();
			Signature += FString::Printf(
				TEXT("|%d:%d:%d:%d:%s:%s"),
				static_cast<int32>(Offer.GetUpgrade().Type),
				Offer.GetCost(),
				Offer.IsPurchased() ? 1 : 0,
				bAffordable ? 1 : 0,
				*Offer.GetUpgrade().Title,
				*Offer.GetUpgrade().Description);
		}
	}

	if (Signature == LastShopWidgetSignature)
	{
		if (bVisible && AutoRefreshSeconds != LastShopAutoRefreshSeconds)
		{
			LastShopAutoRefreshSeconds = AutoRefreshSeconds;
			ShopWidget->UpdateAutoRefreshText(GetAutoRefreshText(AutoRefreshSeconds));
		}
		return;
	}
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
		ViewData.AutoRefreshText = GetAutoRefreshText(AutoRefreshSeconds);
		ViewData.RefreshButtonText = FText::FromString(FString::Printf(TEXT("立即刷新 (%d)"), LocalPlayerState->GetShopRefreshCost()));

		const TArray<FRogueShopOffer>& Offers = LocalPlayerState->GetShopOffers();
		for (const FRogueShopOffer& Offer : Offers)
		{
			FRogueShopOfferViewData& OfferView = ViewData.Offers.AddDefaulted_GetRef();
			OfferView.TitleText = FText::FromString(Offer.GetUpgrade().Title);
			OfferView.DescriptionText = FText::FromString(Offer.GetUpgrade().Description);
			OfferView.bPurchased = Offer.IsPurchased();
			OfferView.bAffordable = PlayerCharacter != nullptr && PlayerCharacter->GetMoney() >= Offer.GetCost();
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
	const ARoguePlayerState* LocalPlayerState = GetLocalRoguePlayerState();
	// 无 PlayerState 时提供静态空数组作为只读回退；此处引用仅在当前调用中使用。
	static const TArray<FRogueUpgradeOption> EmptyUpgradeOptions;
	const TArray<FRogueUpgradeOption>& Options =
		LocalPlayerState != nullptr ? LocalPlayerState->GetPendingUpgrades() : EmptyUpgradeOptions;

	const ARogueCharacter* PlayerCharacter = GetLocalPlayerCharacter(this);
	const ARogueGameState* RogueGameState = GetRogueGameState();
	const bool bVisible = PlayerCharacter != nullptr && PlayerCharacter->IsAlive()
		&& !IsRogueRunOver(RogueGameState, PlayerCharacter)
		&& LocalPlayerState != nullptr && LocalPlayerState->IsAwaitingUpgradeChoice();

	FString Signature = bVisible ? TEXT("Upgrade|Visible") : TEXT("Upgrade|Hidden");
	for (const FRogueUpgradeOption& Option : Options)
	{
		Signature += FString::Printf(TEXT("|%d:%s:%s"), static_cast<int32>(Option.Type), *Option.Title, *Option.Description);
	}

	if (!UpdateCachedSignature(LastUpgradeWidgetSignature, Signature))
	{
		return;
	}

	FRogueUpgradeSelectionViewData ViewData;
	ViewData.bVisible = bVisible;
	ViewData.TitleText = FText::FromString(TEXT("请选择升级"));
	for (const FRogueUpgradeOption& Option : Options)
	{
		FRogueUpgradeCardViewData& Card = ViewData.Cards.AddDefaulted_GetRef();
		Card.TitleText = FText::FromString(Option.Title);
		Card.DescriptionText = FText::FromString(Option.Description);
	}

	UpgradeSelectionWidget->UpdateUpgradeSelectionView(ViewData);
}

ARoguePlayerState* ARogueHUD::GetLocalRoguePlayerState() const
{
	APlayerController* OwningPlayerController = GetOwningPlayerController();
	return OwningPlayerController != nullptr ? OwningPlayerController->GetPlayerState<ARoguePlayerState>() : nullptr;
}

void ARogueHUD::UpdatePauseWidget()
{
	if (PauseMenuWidget == nullptr)
	{
		return;
	}

	const ARogueCharacter* PlayerCharacter = GetLocalPlayerCharacter(this);
	const ARogueGameState* RogueGameState = GetRogueGameState();
	const bool bVisible = PauseMenuPage == ERoguePauseMenuPage::Main
		&& PlayerCharacter != nullptr && PlayerCharacter->IsAlive()
		&& !IsRogueRunOver(RogueGameState, PlayerCharacter);
	const FString Signature = bVisible ? TEXT("Pause|Main") : TEXT("Pause|Hidden");
	if (!UpdateCachedSignature(LastPauseWidgetSignature, Signature))
	{
		return;
	}

	FRoguePauseMenuViewData ViewData;
	ViewData.bVisible = bVisible;
	ViewData.TitleText = FText::FromString(TEXT("暂停菜单"));
	PauseMenuWidget->UpdatePauseView(ViewData);
}

void ARogueHUD::UpdateSettingsWidget()
{
	if (SettingsMenuWidget == nullptr)
	{
		return;
	}

	const ARogueCharacter* PlayerCharacter = GetLocalPlayerCharacter(this);
	const ARogueGameState* RogueGameState = GetRogueGameState();
	const bool bVisible = PauseMenuPage == ERoguePauseMenuPage::Settings
		&& PlayerCharacter != nullptr && PlayerCharacter->IsAlive()
		&& !IsRogueRunOver(RogueGameState, PlayerCharacter);
	FString Signature = bVisible ? TEXT("Settings|Visible") : TEXT("Settings|Hidden");
	if (bVisible)
	{
		Signature += FString::Printf(
			TEXT("|Vol=%.2f|Quality=%d|Res=%d|Mode=%d|Frame=%d|FrameValue=%.1f|QualityCustom=%d|FrameCustom=%d|Dirty=%d"),
			MasterVolume,
			GraphicsQualityLevel,
			ResolutionOptionIndex,
			static_cast<int32>(DisplayWindowMode),
			FrameRateLimitOptionIndex,
			FrameRateLimitValue,
			bGraphicsQualityCustom ? 1 : 0,
			bFrameRateLimitCustom ? 1 : 0,
			bDisplaySettingsDirty ? 1 : 0);
	}

	if (!UpdateCachedSignature(LastSettingsWidgetSignature, Signature))
	{
		return;
	}

	FRogueSettingsMenuViewData ViewData;
	ViewData.bVisible = bVisible;
	ViewData.TitleText = FText::FromString(TEXT("设置"));
	ViewData.bCanApply = bDisplaySettingsDirty;
	ViewData.StatusText = FText::FromString(
		bDisplaySettingsDirty ? TEXT("画面设置已修改，点击“应用设置”后生效") : TEXT("当前画面设置已应用"));

	const TArray<FString> RowLabels =
	{
		TEXT("主音量"),
		TEXT("画质等级"),
		TEXT("分辨率"),
		TEXT("显示模式"),
		TEXT("限制帧率")
	};

	const TArray<FString> RowValues =
	{
		FString::Printf(TEXT("%d%%"), FMath::RoundToInt(MasterVolume * 100.0f)),
		GetGraphicsQualityLabel(),
		GetResolutionLabel(),
		GetDisplayModeLabel(),
		GetFrameRateLimitLabel()
	};

	for (int32 RowIndex = 0; RowIndex < RowLabels.Num() && RowIndex < RowValues.Num(); ++RowIndex)
	{
		FRogueSettingsRowViewData& Row = ViewData.Rows.AddDefaulted_GetRef();
		Row.LabelText = FText::FromString(RowLabels[RowIndex]);
		Row.ValueText = FText::FromString(RowValues[RowIndex]);
		Row.bUseAdjustButtons = RowIndex != 3;
	}

	SettingsMenuWidget->UpdateSettingsView(ViewData);
}

void ARogueHUD::UpdateReviveCountdownWidget(const ARogueCharacter* PlayerCharacter)
{
	if (ReviveCountdownWidget == nullptr)
	{
		return;
	}

	// 服务器复制的生命状态决定可见性，剩余时间只决定文字，零秒仍等待复活确认。
	const ARogueGameState* RogueGameState = GetRogueGameState();

	const bool bDowned = PlayerCharacter != nullptr && PlayerCharacter->IsDowned();
	const bool bRunOver = IsRogueRunOver(RogueGameState, PlayerCharacter);
	const bool bVisible = bDowned && !bRunOver;
	const int32 SecondsRemaining = bVisible
		? FMath::Max(0, FMath::CeilToInt(PlayerCharacter->GetReviveSecondsRemaining())) : 0;

	// 缓存必须属于当前 HUD：函数 static 会让同进程的多个玩家共用最后一次状态。
	// 队友先推送"隐藏 / 0 秒"时，刚复活的玩家也会命中这个缓存，自己的界面便一直不隐藏。
	// 同时核对实际可见性，确保数据未变但 widget 状态漂移时仍能重新应用。
	const ESlateVisibility DesiredVisibility = bVisible ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed;
	const bool bVisibilityMatches = ReviveCountdownWidget->GetVisibility() == DesiredVisibility;
	if (bVisible == bLastReviveCountdownVisible && SecondsRemaining == LastReviveCountdownSeconds && bVisibilityMatches)
	{
		return;
	}
	LastReviveCountdownSeconds = SecondsRemaining;
	bLastReviveCountdownVisible = bVisible;

	// 秒数、可见性或实际 widget 可见性不匹配时输出；状态切换可在同一秒发生多次。
	// 日志记录数据推送，不等于已验证实际渲染、样式或所有蓝图异步行为。
	UE_LOG(LogTemp, Log, TEXT("[Rogue] 复活倒计时界面：%s（剩余 %d 秒，本机角色 IsDowned=%d）"),
		bVisible ? TEXT("显示") : TEXT("隐藏"), SecondsRemaining,
		bDowned ? 1 : 0);

	ReviveCountdownWidget->UpdateReviveCountdown(SecondsRemaining, bVisible);
	UE_LOG(LogTemp, Log, TEXT("[RogueLife] UI Player=%s Resurrection=%d Seconds=%d Phase=%d"),
		GetLocalRoguePlayerState() != nullptr ? *GetLocalRoguePlayerState()->GetPlayerName() : TEXT("NoPlayerState"),
		bVisible ? 1 : 0, SecondsRemaining, PlayerCharacter != nullptr ? static_cast<int32>(PlayerCharacter->GetLifePhase()) : -1);
}

void ARogueHUD::UpdateDeathWidget()
{
	if (DeathScreenWidget == nullptr)
	{
		return;
	}

	// GameOver 或本机 Eliminated 都能确认团灭；无 Pawn 时仍由 GameState 提供状态。
	const ARogueCharacter* PlayerCharacter = GetLocalPlayerCharacter(this);
	const ARogueGameState* RogueGameState = GetRogueGameState();
	const float SummaryRunTime = RogueGameState != nullptr ? RogueGameState->GetRunTimeSeconds() : 0.0f;
	const int32 SummaryKills = RogueGameState != nullptr ? RogueGameState->GetEnemiesDefeated() : 0;
	const bool bVisible = IsRogueRunOver(RogueGameState, PlayerCharacter);

	FString Signature = bVisible ? TEXT("Death|Visible") : TEXT("Death|Hidden");
	if (bVisible)
	{
		Signature += FString::Printf(TEXT("|Time=%.0f|Kills=%d"), SummaryRunTime, SummaryKills);
	}

	const bool bSignatureChanged = UpdateCachedSignature(LastDeathWidgetSignature, Signature);
	const ESlateVisibility DesiredVisibility = bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;
	if (!bSignatureChanged && DeathScreenWidget->GetVisibility() == DesiredVisibility)
	{
		return;
	}

	FRogueDeathViewData ViewData;
	ViewData.bVisible = bVisible;
	if (bVisible)
	{
		ViewData.TitleText = FText::FromString(TEXT("本轮结束"));
		ViewData.SummaryText = FText::FromString(FString::Printf(
			TEXT("存活 %.0f 秒  |  击败 %d 个敌人\n全员进入濒死状态，点击\"重开\"开始新一轮"),
			SummaryRunTime, SummaryKills));
	}

	DeathScreenWidget->UpdateDeathView(ViewData);
	UE_LOG(LogTemp, Log, TEXT("[RogueLife] UI Player=%s Death=%d Phase=%d"),
		GetLocalRoguePlayerState() != nullptr ? *GetLocalRoguePlayerState()->GetPlayerName() : TEXT("NoPlayerState"),
		bVisible ? 1 : 0, PlayerCharacter != nullptr ? static_cast<int32>(PlayerCharacter->GetLifePhase()) : -1);
}

void ARogueHUD::RefreshPauseSettings()
{
	if (GEngine == nullptr)
	{
		return;
	}

	if (UGameUserSettings* UserSettings = GEngine->GetGameUserSettings())
	{
		UserSettings->LoadSettings(false);
		GraphicsQualityLevel = ResolveGraphicsQualityLevel(UserSettings);
		DisplayWindowMode = UserSettings->GetFullscreenMode();
		DesktopResolution = UserSettings->GetDesktopResolution();
		RefreshResolutionOptions(UserSettings->GetScreenResolution(), DesktopResolution);
		RefreshFrameRateLimitOptions(UserSettings->GetFrameRateLimit());
		bDisplaySettingsDirty = false;
	}

	if (GConfig != nullptr)
	{
		float SavedVolume = MasterVolume;
		if (GConfig->GetFloat(RogueSettingsSection, MasterVolumeKey, SavedVolume, GGameUserSettingsIni))
		{
			MasterVolume = FMath::Clamp(SavedVolume, 0.0f, 1.0f);
		}
	}
}

void ARogueHUD::RefreshResolutionOptions(const FIntPoint& CurrentResolution, const FIntPoint& InDesktopResolution)
{
	DesktopResolution = InDesktopResolution.X > 0 && InDesktopResolution.Y > 0 ? InDesktopResolution : FIntPoint(1920, 1080);
	ResolutionOptions.Reset();

	const auto AddResolution = [this](const FIntPoint& Resolution)
	{
		if (Resolution.X > 0 && Resolution.Y > 0 && !ResolutionOptions.Contains(Resolution))
		{
			ResolutionOptions.Add(Resolution);
		}
	};

	const auto AddFilteredResolution = [this, &AddResolution](const FIntPoint& Resolution)
	{
		if (Resolution.X <= DesktopResolution.X && Resolution.Y <= DesktopResolution.Y)
		{
			AddResolution(Resolution);
		}
	};

	AddResolution(CurrentResolution);
	AddFilteredResolution(FIntPoint(960, 540));
	AddFilteredResolution(FIntPoint(1280, 720));
	AddFilteredResolution(FIntPoint(1366, 768));
	AddFilteredResolution(FIntPoint(1600, 900));
	AddFilteredResolution(FIntPoint(1920, 1080));
	AddFilteredResolution(FIntPoint(2560, 1440));
	AddFilteredResolution(FIntPoint(3840, 2160));
	AddResolution(DesktopResolution);

	ResolutionOptions.Sort([](const FIntPoint& Left, const FIntPoint& Right)
	{
		return Left.X * Left.Y < Right.X * Right.Y;
	});

	if (ResolutionOptions.Num() == 0)
	{
		ResolutionOptions.Add(DesktopResolution);
	}

	ResolutionOptionIndex = ResolutionOptions.IndexOfByKey(CurrentResolution);
	if (ResolutionOptionIndex == INDEX_NONE)
	{
		ResolutionOptionIndex = ResolutionOptions.IndexOfByKey(DesktopResolution);
	}
	if (ResolutionOptionIndex == INDEX_NONE)
	{
		ResolutionOptionIndex = 0;
	}
}

void ARogueHUD::StepResolution(int32 Direction)
{
	if (ResolutionOptions.Num() == 0)
	{
		return;
	}

	ResolutionOptionIndex = (ResolutionOptionIndex + Direction + ResolutionOptions.Num()) % ResolutionOptions.Num();
}

void ARogueHUD::RefreshFrameRateLimitOptions(float CurrentFrameRateLimit)
{
	if (FrameRateLimitOptions.Num() == 0)
	{
		FrameRateLimitOptions = { 0.0f, 30.0f, 60.0f, 90.0f, 120.0f, 144.0f, 165.0f, 240.0f };
	}

	FrameRateLimitValue = FMath::Max(0.0f, CurrentFrameRateLimit);
	if (FrameRateLimitValue <= KINDA_SMALL_NUMBER)
	{
		FrameRateLimitOptionIndex = 0;
		bFrameRateLimitCustom = false;
		return;
	}

	const int32 ExactIndex = FrameRateLimitOptions.IndexOfByPredicate([this](float Option)
	{
		return FMath::IsNearlyEqual(Option, FrameRateLimitValue, 0.5f);
	});

	if (ExactIndex != INDEX_NONE)
	{
		FrameRateLimitOptionIndex = ExactIndex;
		bFrameRateLimitCustom = false;
		return;
	}

	float BestDifference = FLT_MAX;
	int32 BestIndex = 0;
	for (int32 Index = 0; Index < FrameRateLimitOptions.Num(); ++Index)
	{
		const float Difference = FMath::Abs(FrameRateLimitOptions[Index] - FrameRateLimitValue);
		if (Difference < BestDifference)
		{
			BestDifference = Difference;
			BestIndex = Index;
		}
	}

	FrameRateLimitOptionIndex = BestIndex;
	bFrameRateLimitCustom = true;
}

void ARogueHUD::StepFrameRateLimit(int32 Direction)
{
	if (FrameRateLimitOptions.Num() == 0)
	{
		RefreshFrameRateLimitOptions(FrameRateLimitValue);
	}

	if (FrameRateLimitOptions.Num() == 0)
	{
		return;
	}

	FrameRateLimitOptionIndex = (FrameRateLimitOptionIndex + Direction + FrameRateLimitOptions.Num()) % FrameRateLimitOptions.Num();
	FrameRateLimitValue = FrameRateLimitOptions[FrameRateLimitOptionIndex];
	bFrameRateLimitCustom = false;
}

void ARogueHUD::CycleDisplayMode()
{
	switch (DisplayWindowMode)
	{
	case EWindowMode::Windowed:
		DisplayWindowMode = EWindowMode::Fullscreen;
		break;
	case EWindowMode::Fullscreen:
		DisplayWindowMode = EWindowMode::WindowedFullscreen;
		break;
	default:
		DisplayWindowMode = EWindowMode::Windowed;
		break;
	}
}

void ARogueHUD::ApplyMasterVolume()
{
	if (GetWorld() == nullptr)
	{
		return;
	}

	if (FAudioDevice* AudioDevice = GetWorld()->GetAudioDeviceRaw())
	{
		AudioDevice->SetTransientPrimaryVolume(MasterVolume);
	}

	if (GConfig != nullptr)
	{
		GConfig->SetFloat(RogueSettingsSection, MasterVolumeKey, MasterVolume, GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}
}

void ARogueHUD::ApplyDisplaySettings()
{
	if (GEngine == nullptr)
	{
		return;
	}

	if (UGameUserSettings* UserSettings = GEngine->GetGameUserSettings())
	{
		UserSettings->SetOverallScalabilityLevel(GraphicsQualityLevel);
		if (ResolutionOptions.IsValidIndex(ResolutionOptionIndex))
		{
			UserSettings->SetScreenResolution(ResolutionOptions[ResolutionOptionIndex]);
		}
		UserSettings->SetFullscreenMode(DisplayWindowMode);
		UserSettings->SetFrameRateLimit(FrameRateLimitValue);
		UserSettings->ApplySettings(false);
		UserSettings->ConfirmVideoMode();
		UserSettings->SaveSettings();

		const FString FrameLimitCommand = FrameRateLimitValue <= KINDA_SMALL_NUMBER
			? TEXT("t.MaxFPS 0")
			: FString::Printf(TEXT("t.MaxFPS %.0f"), FrameRateLimitValue);
		UKismetSystemLibrary::ExecuteConsoleCommand(this, FrameLimitCommand, GetOwningPlayerController());

		RefreshPauseSettings();
	}
}

int32 ARogueHUD::ResolveGraphicsQualityLevel(const UGameUserSettings* UserSettings)
{
	if (UserSettings == nullptr)
	{
		bGraphicsQualityCustom = false;
		return 2;
	}

	const int32 OverallQualityLevel = UserSettings->GetOverallScalabilityLevel();
	if (OverallQualityLevel >= 0)
	{
		bGraphicsQualityCustom = false;
		return FMath::Clamp(OverallQualityLevel, 0, 4);
	}

	const TArray<int32> QualityLevels =
	{
		UserSettings->GetViewDistanceQuality(),
		UserSettings->GetAntiAliasingQuality(),
		UserSettings->GetShadowQuality(),
		UserSettings->GetGlobalIlluminationQuality(),
		UserSettings->GetReflectionQuality(),
		UserSettings->GetPostProcessingQuality(),
		UserSettings->GetTextureQuality(),
		UserSettings->GetVisualEffectQuality(),
		UserSettings->GetFoliageQuality(),
		UserSettings->GetShadingQuality()
	};

	int32 QualitySum = 0;
	int32 QualityCount = 0;
	for (const int32 QualityLevel : QualityLevels)
	{
		if (QualityLevel >= 0)
		{
			QualitySum += FMath::Clamp(QualityLevel, 0, 4);
			++QualityCount;
		}
	}

	bGraphicsQualityCustom = true;
	return QualityCount > 0 ? FMath::Clamp(FMath::RoundToInt(static_cast<float>(QualitySum) / QualityCount), 0, 4) : 2;
}

FString ARogueHUD::GetGraphicsQualityLabel() const
{
	static const TCHAR* Labels[] =
	{
		TEXT("低"),
		TEXT("中"),
		TEXT("高"),
		TEXT("史诗"),
		TEXT("电影")
	};

	const FString BaseLabel = Labels[FMath::Clamp(GraphicsQualityLevel, 0, 4)];
	return bGraphicsQualityCustom ? FString::Printf(TEXT("自定义(%s)"), *BaseLabel) : BaseLabel;
}

FString ARogueHUD::GetResolutionLabel() const
{
	if (!ResolutionOptions.IsValidIndex(ResolutionOptionIndex))
	{
		return TEXT("未设置");
	}

	const FIntPoint Resolution = ResolutionOptions[ResolutionOptionIndex];
	return FString::Printf(TEXT("%d x %d"), Resolution.X, Resolution.Y);
}

FString ARogueHUD::GetDisplayModeLabel() const
{
	switch (DisplayWindowMode)
	{
	case EWindowMode::Fullscreen:
		return TEXT("全屏");
	case EWindowMode::WindowedFullscreen:
		return TEXT("全屏窗口");
	default:
		return TEXT("窗口");
	}
}

FString ARogueHUD::GetFrameRateLimitLabel() const
{
	if (FrameRateLimitValue <= KINDA_SMALL_NUMBER)
	{
		return TEXT("无限制");
	}

	const FString BaseLabel = FString::Printf(TEXT("%.0f FPS"), FrameRateLimitValue);
	return bFrameRateLimitCustom ? FString::Printf(TEXT("自定义(%s)"), *BaseLabel) : BaseLabel;
}
