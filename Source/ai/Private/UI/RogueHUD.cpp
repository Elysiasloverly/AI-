#include "UI/RogueHUD.h"

#include "Player/RogueCharacter.h"
#include "Enemies/RogueEnemy.h"
#include "Core/RogueGameMode.h"
#include "Core/RogueGameState.h"
#include "Player/RoguePlayerState.h"
#include "UI/RogueMenuWidgetBases.h"
#include "Subsystems/RogueEnemyTrackerSubsystem.h"
#include "AudioDevice.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "GameFramework/GameUserSettings.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/App.h"

namespace
{
	const TCHAR* RogueSettingsSection = TEXT("/Script/ai.RogueRuntimeSettings");
	const TCHAR* MasterVolumeKey = TEXT("MasterVolume");
	constexpr int32 MaxDamageNumbersNormal = 42;
	constexpr int32 MaxDamageNumbersHeavy = 24;
	constexpr int32 MaxRegularHealthBarsNormal = 22;
	constexpr int32 MaxRegularHealthBarsHeavy = 12;
	constexpr float EnemyHealthBarRefreshIntervalNormal = 0.05f;
	constexpr float EnemyHealthBarRefreshIntervalHeavy = 0.10f;
	constexpr float DamageProjectionRefreshIntervalNormal = 0.05f;
	constexpr float DamageProjectionRefreshIntervalHeavy = 0.09f;

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
	 * ⚠️ 绝对不能用 UGameplayStatics::GetPlayerCharacter(this, 0)：
	 * 索引 0 是**这个进程里的第一个本地玩家**。在 PIE 的 2 人模式下（默认 Run Under One Process）
	 * 或者分屏下，两个玩家共用一个进程 —— 于是第二个窗口的 HUD 会拿到**主机**的角色，
	 * 表现是"UI 显示的是别人的数据"，甚至因为判断不到自己的角色而整个 HUD 不刷新。
	 * 只有"每个玩家一个独立进程"（真正的联机、打包后多开）时索引 0 才恰好等于自己。
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

void ARogueHUD::DrawHUD()
{
	Super::DrawHUD();

	ARogueCharacter* PlayerCharacter = GetLocalPlayerCharacter(this);
	if (!IsValid(PlayerCharacter))
	{
		return;
	}

	// ⚠️ 这里**不能**把 GameMode 放进早退条件（连"取出来备用"都不要，免得后人又拿它当判据）。
	// GetWorld()->GetAuthGameMode() 在客户端恒为 nullptr（GameMode 是服务端独占对象），
	// 一旦把它当成"初始化没完成"的判据，客户端就会每帧从这里 return ——
	// 表现是客户端玩家**整个 HUD 都是死的**：商店、升级选卡、暂停菜单、设置、
	// 死亡结算、连血条/FPS 都不画。而它偏偏在单机与主机上完全正常，所以极难发现。
	// HUD 需要的一切都从 GameState / 本地 PlayerState / 本地角色上读（它们客户端都有）。

	// 菜单/界面更新与 Canvas 无关：先把状态推给 widget。
	// 顺序上放在 Canvas 判空之前，这样无头/nullrhi 运行时菜单逻辑仍然是活的
	// （否则"界面不刷新"的 bug 会被无头测试掩盖掉）。
	UpdateShopWidget(PlayerCharacter);
	UpdateUpgradeSelectionWidget();
	UpdatePauseWidget();
	UpdateSettingsWidget();
	UpdateDeathWidget();

	// 输入模式（光标/UI 捕获）：按本地可见状态推导，见函数注释
	UpdateInteractionInputMode(PlayerCharacter);

	// 复活倒计时界面（倒地等待时可见；不抢输入，见 widget 基类的说明）
	UpdateReviveCountdownWidget(PlayerCharacter);

	// 以下是纯画布绘制（血条、伤害数字、FPS 文本…）。
	// 没有 Canvas（无头运行、专用服务器）就没有可画的地方，直接结束。
	if (Canvas == nullptr)
	{
		return;
	}

	UFont* Font = GEngine != nullptr ? GEngine->GetSmallFont() : nullptr;
	UFont* AccentFont = GEngine != nullptr ? GEngine->GetMediumFont() : Font;
	const float InstantFramesPerSecond = FApp::GetDeltaTime() > KINDA_SMALL_NUMBER ? 1.0f / FApp::GetDeltaTime() : 0.0f;
	if (InstantFramesPerSecond > 0.0f)
	{
		SmoothedFramesPerSecond = SmoothedFramesPerSecond <= KINDA_SMALL_NUMBER
			? InstantFramesPerSecond
			: FMath::Lerp(SmoothedFramesPerSecond, InstantFramesPerSecond, 0.14f);
	}

	const FLinearColor FramesPerSecondColor =
		SmoothedFramesPerSecond >= 110.0f ? FLinearColor(0.60f, 1.0f, 0.72f) :
		(SmoothedFramesPerSecond >= 60.0f ? FLinearColor(0.96f, 0.92f, 0.58f) : FLinearColor(1.0f, 0.48f, 0.40f));
	DrawText(FString::Printf(TEXT("FPS %.0f"), SmoothedFramesPerSecond), FramesPerSecondColor, Canvas->SizeX - 150.0f, 26.0f, Font, 1.05f, false);

	const float Left = 40.0f;
	float Top = 30.0f;

	DrawEnemyHealthBars(Font);
	DrawDamageNumbers(Font);

	// 整局状态从 **GameState** 读，而不是 GameMode。
	// GameMode 是服务端独占对象：客户端调 GetAuthGameMode() 得到 nullptr，
	// 于是客户端 HUD 画不出波次/击败数。GameState 会复制到所有客户端。
	const ARogueGameState* RogueGameState = GetRogueGameState();
	const int32 DisplayWave = RogueGameState != nullptr ? RogueGameState->GetCurrentWave() : 0;
	const int32 DisplayKills = RogueGameState != nullptr ? RogueGameState->GetEnemiesDefeated() : 0;

	DrawText(FString::Printf(TEXT("第 %d 波"), DisplayWave), FLinearColor::White, Left, Top, Font, 1.4f, false);
	Top += 24.0f;
	DrawText(FString::Printf(TEXT("击败 %d"), DisplayKills), FLinearColor::White, Left, Top, Font, 1.1f, false);
	Top += 24.0f;
	DrawText(FString::Printf(TEXT("等级 %d"), PlayerCharacter->GetPlayerLevel()), FLinearColor::White, Left, Top, Font, 1.1f, false);
	Top += 24.0f;
	DrawText(FString::Printf(TEXT("金币 %d"), PlayerCharacter->GetMoney()), FLinearColor(1.0f, 0.86f, 0.36f), Left, Top, Font, 1.1f, false);
	Top += 24.0f;
	DrawText(FString::Printf(TEXT("生命 %.0f / %.0f"), PlayerCharacter->GetCurrentHealth(), PlayerCharacter->GetMaxHealth()), FLinearColor::Green, Left, Top, Font, 1.1f, false);
	if (PlayerCharacter->HasArmor())
	{
		Top += 24.0f;
		DrawText(FString::Printf(TEXT("护甲 %.0f / %.0f"), PlayerCharacter->GetCurrentArmor(), PlayerCharacter->GetMaxArmor()), FLinearColor(0.78f, 0.80f, 0.84f), Left, Top, Font, 1.05f, false);
	}
	Top += 24.0f;
	DrawText(FString::Printf(TEXT("经验 %d / %d"), PlayerCharacter->GetCurrentExperience(), PlayerCharacter->GetExperienceToNextLevel()), FLinearColor(0.4f, 0.8f, 1.0f), Left, Top, Font, 1.1f, false);
	Top += 24.0f;
	DrawText(FString::Printf(TEXT("伤害 %.0f  攻速 %.2f 秒"), PlayerCharacter->GetAttackDamage(), PlayerCharacter->GetAttackInterval()), FLinearColor::White, Left, Top, Font, 1.0f, false);
	Top += 22.0f;
	DrawText(FString::Printf(TEXT("弹体 %d  |  镰刀 %d"), PlayerCharacter->GetEffectiveProjectileCount(), PlayerCharacter->GetEffectiveScytheCount()), FLinearColor::White, Left, Top, Font, 0.95f, false);
	Top += 20.0f;
	DrawText(FString::Printf(TEXT("火箭 %d  |  激光 %d"), PlayerCharacter->GetEffectiveRocketLauncherCount(), PlayerCharacter->GetEffectiveLaserCannonCount()), FLinearColor::White, Left, Top, Font, 0.95f, false);
	Top += 20.0f;
	DrawText(FString::Printf(TEXT("迫击炮 %d  |  地狱塔 %d"), PlayerCharacter->GetEffectiveMortarCount(), PlayerCharacter->GetEffectiveHellTowerCount()), FLinearColor::White, Left, Top, Font, 0.95f, false);
	Top += 20.0f;
	DrawText(FString::Printf(TEXT("激光折射 %d"), PlayerCharacter->GetLaserRefractionCount()), FLinearColor(1.0f, 0.48f, 0.38f), Left, Top, Font, 0.95f, false);
	Top += 20.0f;
	const FString DashStatusText = PlayerCharacter->IsDashReady() ? TEXT("冲刺 就绪") : FString::Printf(TEXT("冲刺 %.1f 秒"), PlayerCharacter->GetDashCooldownRemaining());
	DrawText(DashStatusText, PlayerCharacter->IsDashReady() ? FLinearColor(0.60f, 1.0f, 0.75f) : FLinearColor(1.0f, 0.78f, 0.40f), Left, Top, Font, 0.95f, false);
	Top += 20.0f;
	DrawText(TEXT("移动 WASD  |  冲刺 Shift  |  旋转 鼠标  |  缩放 滚轮"), FLinearColor(0.8f, 0.85f, 0.9f), Left, Top, Font, 0.9f, false);

	// "是否正在选升级"改读本地玩家的 PlayerState（每玩家一份）。
	// 原来读 GameMode：客户端拿不到 GameMode，而且联机下这个状态本来就是每玩家独立的。
	const ARoguePlayerState* LocalPlayerState = GetLocalRoguePlayerState();
	const bool bAwaitingUpgrade = LocalPlayerState != nullptr && LocalPlayerState->IsAwaitingUpgradeChoice();

	// 商店是否打开读本地 PlayerState（客户端可读；GameMode 在客户端拿不到）
	const bool bShopOpen = LocalPlayerState != nullptr && LocalPlayerState->IsShopOpen();

	if (!PlayerCharacter->IsDead() && !bAwaitingUpgrade && !bShopOpen && PauseMenuPage == ERoguePauseMenuPage::None)
	{
		FVector PromptWorldLocation = FVector::ZeroVector;
		// 靠近商店的交互提示：直接问角色（基于商店终端的重叠检测，终端已复制到客户端）。
		// 原来走 GameMode，客户端拿不到 → 提示永远不显示。
		if (PlayerCharacter->GetShopPromptWorldLocation(PromptWorldLocation))
		{
			FVector2D PromptScreenPosition = FVector2D(Canvas->SizeX * 0.43f, Canvas->SizeY * 0.84f);
			if (APlayerController* PlayerController = GetOwningPlayerController();
				PlayerController != nullptr && PlayerController->ProjectWorldLocationToScreen(PromptWorldLocation, PromptScreenPosition, true))
			{
				PromptScreenPosition.X -= 92.0f;
				PromptScreenPosition.Y -= 44.0f;
			}

			DrawText(TEXT("SHOP"), FLinearColor(1.0f, 0.88f, 0.30f, 0.98f), PromptScreenPosition.X - 8.0f, PromptScreenPosition.Y - 6.0f, AccentFont, 1.7f, false);
			if (PlayerCharacter->CanInteractWithShop())
			{
				DrawText(TEXT("按 E 打开商店"), FLinearColor(1.0f, 0.95f, 0.72f), PromptScreenPosition.X, PromptScreenPosition.Y + 28.0f, Font, 1.1f, false);
			}
		}
	}

	float BarTop = Canvas->SizeY - 90.0f;
	if (PlayerCharacter->HasArmor())
	{
		DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.55f), Left, BarTop - 32.0f, 320.0f, 20.0f);
		DrawRect(FLinearColor(0.58f, 0.60f, 0.64f, 0.96f), Left + 2.0f, BarTop - 30.0f, 316.0f * PlayerCharacter->GetArmorPercent(), 16.0f);
	}

	DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.55f), Left, BarTop, 320.0f, 20.0f);
	DrawRect(FLinearColor(0.85f, 0.15f, 0.15f, 0.95f), Left + 2.0f, BarTop + 2.0f, 316.0f * PlayerCharacter->GetHealthPercent(), 16.0f);

	DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.55f), Left, BarTop + 32.0f, 320.0f, 20.0f);
	DrawRect(FLinearColor(0.2f, 0.6f, 1.0f, 0.95f), Left + 2.0f, BarTop + 34.0f, 316.0f * PlayerCharacter->GetExperiencePercent(), 16.0f);
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
	// 这两处 SetGamePaused(false) 是**离开关卡前的防御性清理**，不是"恢复游戏"。
	// 万一世界因为别的原因处于暂停（控制台 pause、将来某个功能），
	// 带着暂停状态切关卡/退出会留下坏状态。保留它们成本为零。
	UGameplayStatics::SetGamePaused(this, false);

	// 重开必须由**服务端**执行：客户端自己 OpenLevel 只会加载出一份单机关卡副本，
	// 表现为"点了重开之后队友全没了"。所以这里只发一个请求，
	// 服务端会校验"本局是否已结束"再决定要不要 OpenLevel（server travel 把所有人一起带走）。
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
	const bool bHeavyCombat = Tracker->ShouldCullCombatEffects() || Tracker->GetActiveEnemyCount() >= 40;
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
		// 钳制后再乘宽度。
		// 为什么必须钳：只要比例是 NaN（MaxHealth 还没复制到客户端 → 除零）或大于 1
		// （当前值暂时超过上限、镜像未同步），`BarWidth * HealthPercent` 就会变成一个
		// 巨长甚至非法的宽度 —— 表现正是"怪物血条突然变得很长"。
		// 这里只保证"画出来的东西永远合法"，根因（比例来源）另外处理。
		const float RawHealthPercent = Enemy->GetHealthPercent();
		const float HealthPercent = FMath::IsFinite(RawHealthPercent)
			? FMath::Clamp(RawHealthPercent, 0.0f, 1.0f)
			: 0.0f;

		DrawRect(FLinearColor(0.04f, 0.04f, 0.05f, 0.70f), BarX - 2.0f, BarY - 2.0f, BarWidth + 4.0f, BarHeight + 4.0f);
		DrawRect(FLinearColor(0.10f, 0.10f, 0.12f, 0.90f), BarX, BarY, BarWidth, BarHeight);
		DrawRect(FillColor, BarX, BarY, BarWidth * HealthPercent, BarHeight);

		if (CachedBar.bBoss)
		{
			DrawText(TEXT("首领"), FLinearColor(1.0f, 0.8f, 0.45f, 0.95f), BarX + 44.0f, BarY - 18.0f, Font, 0.9f, false);
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
	const bool bHeavyCombat = Tracker != nullptr && (Tracker->ShouldCullCombatEffects() || Tracker->GetActiveEnemyCount() >= 40);
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
	if (CurrentTime >= NextDamageNumberProjectionRefreshTime)
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
	if (Canvas == nullptr || Tracker == nullptr)
	{
		return;
	}

	const APlayerController* PlayerController = GetOwningPlayerController();
	const APawn* PlayerPawn = PlayerController != nullptr ? PlayerController->GetPawn() : nullptr;
	const FVector PlayerLocation = PlayerPawn != nullptr ? PlayerPawn->GetActorLocation() : FVector::ZeroVector;
	const bool bHeavyCombat = Tracker->ShouldCullCombatEffects() || Tracker->GetActiveEnemyCount() >= 40;
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
	const bool bHeavyCombat = Tracker != nullptr && (Tracker->ShouldCullCombatEffects() || Tracker->GetActiveEnemyCount() >= 40);
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
		UpgradeSelectionWidget = CreateRogueMenuWidget(PlayerController, UpgradeSelectionWidgetClass, this, 63);
		ShopWidget = CreateRogueMenuWidget(PlayerController, ShopWidgetClass, this, 64);
		PauseMenuWidget = CreateRogueMenuWidget(PlayerController, PauseMenuWidgetClass, this, 65);
		SettingsMenuWidget = CreateRogueMenuWidget(PlayerController, SettingsMenuWidgetClass, this, 66);
		DeathScreenWidget = CreateRogueMenuWidget(PlayerController, DeathScreenWidgetClass, this, 70);

		// 复活倒计时（Z 序介于其它菜单之下：它是"等待中"的提示，不应该盖住暂停/商店界面）
		ReviveCountdownWidget = CreateRogueMenuWidget(PlayerController, ReviveCountdownWidgetClass, this, 60);
	}
}

ARogueGameMode* ARogueHUD::GetRogueGameMode() const
{
	return GetWorld() != nullptr ? GetWorld()->GetAuthGameMode<ARogueGameMode>() : nullptr;
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
	if (PlayerController == nullptr || PlayerCharacter == nullptr)
	{
		return;
	}

	// 这些状态的真相源全部是**本地可读的复制状态**（暂停菜单是本机的 HUD 状态）：
	//   · PlayerState->IsShopOpen() / IsAwaitingUpgradeChoice()  —— COND_OwnerOnly 复制给拥有者
	//   · PlayerCharacter->IsDead()                              —— 由复制的 State.Dead 推导
	//   · PauseMenuPage                                          —— 纯本地
	// 所以不需要任何 RPC，每台机器自己就能得出正确结论。
	const ARoguePlayerState* LocalPlayerState = GetLocalRoguePlayerState();
	const bool bWantUIInput = PauseMenuPage != ERoguePauseMenuPage::None
		|| PlayerCharacter->IsDead()
		|| (LocalPlayerState != nullptr && (LocalPlayerState->IsShopOpen() || LocalPlayerState->IsAwaitingUpgradeChoice()));

	const int32 NewState = bWantUIInput ? 1 : 0;
	if (NewState == LastAppliedUIInputState)
	{
		return;
	}
	LastAppliedUIInputState = NewState;

	PlayerController->bShowMouseCursor = bWantUIInput;
	PlayerController->bEnableClickEvents = bWantUIInput;
	PlayerController->bEnableMouseOverEvents = bWantUIInput;

	if (bWantUIInput)
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

	const bool bVisible = PlayerCharacter != nullptr && LocalPlayerState != nullptr && LocalPlayerState->IsShopOpen();
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
	// 用静态空数组而不是临时对象：下面按引用使用，且生命周期要跨过函数体
	static const TArray<FRogueUpgradeOption> EmptyUpgradeOptions;
	const TArray<FRogueUpgradeOption>& Options =
		LocalPlayerState != nullptr ? LocalPlayerState->GetPendingUpgrades() : EmptyUpgradeOptions;

	const bool bVisible = LocalPlayerState != nullptr && LocalPlayerState->IsAwaitingUpgradeChoice();

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

	const FString Signature = PauseMenuPage == ERoguePauseMenuPage::Main ? TEXT("Pause|Main") : TEXT("Pause|Hidden");
	if (!UpdateCachedSignature(LastPauseWidgetSignature, Signature))
	{
		return;
	}

	FRoguePauseMenuViewData ViewData;
	ViewData.bVisible = PauseMenuPage == ERoguePauseMenuPage::Main;
	ViewData.TitleText = FText::FromString(TEXT("暂停菜单"));
	PauseMenuWidget->UpdatePauseView(ViewData);
}

void ARogueHUD::UpdateSettingsWidget()
{
	if (SettingsMenuWidget == nullptr)
	{
		return;
	}

	const bool bVisible = PauseMenuPage == ERoguePauseMenuPage::Settings;
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

	// 可见条件：本地玩家处于"倒地等待"——已倒下、且复活还没到期、且本局尚未结束。
	// 本局结束（全员同时倒下）时改由死亡结算界面接管，两个界面不会同时出现。
	const ARoguePlayerState* LocalPlayerState = GetLocalRoguePlayerState();
	const ARogueGameState* RogueGameState = GetRogueGameState();

	const bool bDowned = PlayerCharacter != nullptr && PlayerCharacter->IsDead();
	const bool bRunOver = RogueGameState != nullptr && RogueGameState->IsGameOver();
	const int32 SecondsRemaining = LocalPlayerState != nullptr ? FMath::CeilToInt(LocalPlayerState->GetReviveSecondsRemaining()) : 0;

	const bool bVisible = bDowned && !bRunOver && SecondsRemaining > 0;

	// 只在内容变化时推送（秒数每秒变一次，代价可忽略；避免每帧刷 UI）
	static int32 LastPushedSeconds = INDEX_NONE;
	static bool bLastPushedVisible = false;
	if (bVisible == bLastPushedVisible && SecondsRemaining == LastPushedSeconds)
	{
		return;
	}
	LastPushedSeconds = SecondsRemaining;
	bLastPushedVisible = bVisible;

	// 只在状态真正变化时打一行（每秒最多一条）：下次联机自测时，
	// "主机/客户端到底有没有拿到倒计时界面"可以直接从日志判断，不用靠肉眼盯。
	UE_LOG(LogTemp, Log, TEXT("[Rogue] 复活倒计时界面：%s（剩余 %d 秒，本机角色 IsDead=%d）"),
		bVisible ? TEXT("显示") : TEXT("隐藏"), SecondsRemaining,
		(PlayerCharacter != nullptr && PlayerCharacter->IsDead()) ? 1 : 0);

	ReviveCountdownWidget->UpdateReviveCountdown(SecondsRemaining, bVisible);
}

void ARogueHUD::UpdateDeathWidget()
{
	if (DeathScreenWidget == nullptr)
	{
		return;
	}

	const ARogueCharacter* PlayerCharacter = GetLocalPlayerCharacter(this);
	const bool bPlayerDead = PlayerCharacter != nullptr && PlayerCharacter->IsDead();

	// 结算数据也来自 GameState（客户端可读）。
	// 注意"玩家死了"与"整局结束"是两件事：联机下别人还活着时这里仍要显示本地玩家的结算。
	const ARogueGameState* RogueGameState = GetRogueGameState();
	const float SummaryRunTime = RogueGameState != nullptr ? RogueGameState->GetRunTimeSeconds() : 0.0f;
	const int32 SummaryKills = RogueGameState != nullptr ? RogueGameState->GetEnemiesDefeated() : 0;

	// 整局是否已经结束（= 全员阵亡）。这决定结算界面是"本轮结束"还是"你已阵亡，队友还在打"。
	// 服务端只在全员阵亡时才把它置真（见 ARogueGameMode::NotifyPlayerDied）。
	const bool bRunOver = RogueGameState != nullptr && RogueGameState->IsGameOver();

	// 复活倒计时（联机合作：死亡 = 倒地等待，而不是出局）。
	// 读的是本地 PlayerState 上复制的"复活截止时刻"，各端自己换算剩余秒数。
	const ARoguePlayerState* LocalStateForRevive = GetLocalRoguePlayerState();
	const int32 ReviveSeconds = LocalStateForRevive != nullptr ? FMath::CeilToInt(LocalStateForRevive->GetReviveSecondsRemaining()) : 0;

	// 结算界面 = "本局结束"的界面。
	//
	// 触发条件**只有 bRunOver**（= 全员同时倒下过一次，见 ARogueGameMode::NotifyPlayerDied），
	// 不再额外要求"本机此刻还躺着"：
	//   规则 A 下全员倒地只意味着本局结束（不再刷怪），10 秒后每个人仍然会回出生点复活。
	//   如果界面跟着"复活"消失，玩家就会停在一个"没有怪、也没有重开按钮"的空场里
	//   （实测反馈正是"复活了但不会再刷怪"）。所以它一直留到点"重开"为止。
	//
	// 单人倒地（队友还在打）依然由独立的倒计时界面负责，不会弹结算界面。
	const bool bVisible = bRunOver;

	FString Signature = bVisible ? TEXT("Death|Visible") : TEXT("Death|Hidden");
	if (bVisible || bPlayerDead)
	{
		// 签名里必须带上 bPlayerDead / 复活倒计时：本局从"队友还在打"变成"全员阵亡"、
		// 或倒计时每秒变化时，界面都要跟着刷新。
		Signature += FString::Printf(TEXT("|Time=%.0f|Kills=%d|Dead=%d|Revive=%d"), SummaryRunTime, SummaryKills, bPlayerDead ? 1 : 0, ReviveSeconds);
	}

	if (!UpdateCachedSignature(LastDeathWidgetSignature, Signature))
	{
		return;
	}

	FRogueDeathViewData ViewData;
	ViewData.bVisible = bVisible;
	if (bVisible || bPlayerDead)
	{
		const FString RunSummary = FString::Printf(TEXT("存活 %.0f 秒  |  击败 %d 个敌人"), SummaryRunTime, SummaryKills);
		if (bVisible)
		{
			ViewData.TitleText = FText::FromString(TEXT("本轮结束"));
			if (bPlayerDead && ReviveSeconds > 0)
			{
				// 已经团灭但个人还在倒地等待：把"还会复活"说清楚，免得以为要一直躺着。
				ViewData.SummaryText = FText::FromString(FString::Printf(
					TEXT("%s\n%d 秒后回到出生点复活（本局已结束，可随时重开）"), *RunSummary, ReviveSeconds));
			}
			else
			{
				ViewData.SummaryText = FText::FromString(FString::Printf(
					TEXT("%s\n本局已结束 —— 点\"重开\"开始新一轮"), *RunSummary));
			}
		}
		else
		{
			// 还没团灭：这是"倒地等待复活"，把倒计时明确告诉玩家
			//（否则界面看起来像卡死，而玩家不知道还要等多久、会回到哪里）。
			ViewData.TitleText = FText::FromString(TEXT("你已阵亡"));
			if (ReviveSeconds > 0)
			{
				ViewData.SummaryText = FText::FromString(FString::Printf(
					TEXT("%s\n%d 秒后回到出生点复活（队友仍在战斗）"), *RunSummary, ReviveSeconds));
			}
			else
			{
				ViewData.SummaryText = FText::FromString(FString::Printf(
					TEXT("%s\n等待队友 —— 全员阵亡后才会结束本轮"), *RunSummary));
			}
		}
	}

	DeathScreenWidget->UpdateDeathView(ViewData);
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
