#include "UI/RogueHUD.h"

#include "Player/RogueCharacter.h"
#include "Enemies/RogueEnemy.h"
#include "Core/RogueGameMode.h"
#include "UI/RogueMenuWidgetBases.h"
#include "Subsystems/RogueEnemyTrackerSubsystem.h"
#include "AudioDevice.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "GameFramework/GameUserSettings.h"
#include "InputCoreTypes.h"
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
}

void ARogueHUD::BeginPlay()
{
	Super::BeginPlay();

	RefreshPauseSettings();
	ApplyMasterVolume();
	InitializeMenuWidgets();
}

void ARogueHUD::DrawHUD()
{
	Super::DrawHUD();

	ARogueCharacter* PlayerCharacter = Cast<ARogueCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
	ARogueGameMode* RogueGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ARogueGameMode>() : nullptr;
	if (!IsValid(PlayerCharacter) || !IsValid(RogueGameMode) || Canvas == nullptr)
	{
		return;
	}

	UpdateShopWidget(RogueGameMode, PlayerCharacter);
	UpdateUpgradeSelectionWidget(RogueGameMode);
	UpdatePauseWidget();
	UpdateSettingsWidget();
	UpdateDeathWidget(RogueGameMode);

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

	DrawText(FString::Printf(TEXT("第 %d 波"), RogueGameMode->GetCurrentWave()), FLinearColor::White, Left, Top, Font, 1.4f, false);
	Top += 24.0f;
	DrawText(FString::Printf(TEXT("击败 %d"), RogueGameMode->GetEnemiesDefeated()), FLinearColor::White, Left, Top, Font, 1.1f, false);
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

	if (!PlayerCharacter->IsDead() && !RogueGameMode->IsAwaitingUpgradeChoice() && !RogueGameMode->IsShopOpen() && PauseMenuPage == ERoguePauseMenuPage::None)
	{
		FVector PromptWorldLocation = FVector::ZeroVector;
		if (RogueGameMode->GetShopPromptWorldLocation(PromptWorldLocation))
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

	if (RogueGameMode->IsAwaitingUpgradeChoice())
	{
		if (UpgradeSelectionWidget == nullptr)
		{
			DrawRect(FLinearColor(0.02f, 0.02f, 0.05f, 0.88f), Canvas->SizeX * 0.15f, Canvas->SizeY * 0.18f, Canvas->SizeX * 0.7f, Canvas->SizeY * 0.55f);
			DrawText(TEXT("请选择升级 - 鼠标点击卡牌"), FLinearColor(1.0f, 0.9f, 0.4f), Canvas->SizeX * 0.25f, Canvas->SizeY * 0.22f, Font, 1.6f, false);

			const TArray<FRogueUpgradeOption>& Options = RogueGameMode->GetPendingUpgrades();
			float MouseX = -1.0f;
			float MouseY = -1.0f;
			if (APlayerController* PlayerController = GetOwningPlayerController())
			{
				PlayerController->GetMousePosition(MouseX, MouseY);
			}

			for (int32 Index = 0; Index < Options.Num(); ++Index)
			{
				FVector2D CardPosition;
				FVector2D CardSize;
				if (!GetUpgradeCardRect(Index, CardPosition, CardSize))
				{
					continue;
				}

				const bool bHovered = MouseX >= CardPosition.X && MouseX <= CardPosition.X + CardSize.X && MouseY >= CardPosition.Y && MouseY <= CardPosition.Y + CardSize.Y;
				const FLinearColor CardColor = bHovered ? FLinearColor(0.18f, 0.24f, 0.32f, 0.98f) : FLinearColor(0.08f, 0.08f, 0.12f, 0.95f);
				DrawRect(CardColor, CardPosition.X, CardPosition.Y, CardSize.X, CardSize.Y);
				AddHitBox(CardPosition, CardSize, FName(*FString::Printf(TEXT("Upgrade_%d"), Index)), true, 10);
				DrawText(FString::Printf(TEXT("%d. %s"), Index + 1, *Options[Index].Title), FLinearColor::White, CardPosition.X + 26.0f, CardPosition.Y + 12.0f, Font, 1.25f, false);
				DrawText(Options[Index].Description, FLinearColor(0.75f, 0.85f, 1.0f), CardPosition.X + 26.0f, CardPosition.Y + 38.0f, Font, 0.95f, false);
			}

			if (APlayerController* PlayerController = GetOwningPlayerController())
			{
				if (PlayerController->WasInputKeyJustPressed(EKeys::LeftMouseButton))
				{
					const int32 UpgradeIndex = GetUpgradeIndexAtScreenPosition(MouseX, MouseY);
					if (UpgradeIndex != INDEX_NONE)
					{
						RogueGameMode->TrySelectUpgrade(UpgradeIndex);
					}
				}
			}
		}
	}

	if (RogueGameMode->IsShopOpen())
	{
		if (ShopWidget == nullptr)
		{
			float MouseX = -1.0f;
			float MouseY = -1.0f;
			if (APlayerController* PlayerController = GetOwningPlayerController())
			{
				PlayerController->GetMousePosition(MouseX, MouseY);
			}

			DrawShopMenu(Font, MouseX, MouseY, RogueGameMode, PlayerCharacter);
		}
	}

	if (PauseMenuPage != ERoguePauseMenuPage::None)
	{
		const bool bShouldDrawPauseFallback =
			(PauseMenuPage == ERoguePauseMenuPage::Main && PauseMenuWidget == nullptr) ||
			(PauseMenuPage == ERoguePauseMenuPage::Settings && SettingsMenuWidget == nullptr);
		if (bShouldDrawPauseFallback)
		{
			DrawPauseMenu(Font);
		}
	}

	if (PlayerCharacter->IsDead())
	{
		if (DeathScreenWidget == nullptr)
		{
			float MouseX = -1.0f;
			float MouseY = -1.0f;
			if (APlayerController* PlayerController = GetOwningPlayerController())
			{
				PlayerController->GetMousePosition(MouseX, MouseY);
			}

			DrawDeathMenu(Font, MouseX, MouseY, RogueGameMode);
		}
	}
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
	DamageNumbers.Add(DamageNumber);
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
	if (ARogueGameMode* RogueGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ARogueGameMode>() : nullptr)
	{
		RogueGameMode->TryBuyShopOffer(OfferIndex);
	}
}

void ARogueHUD::RequestRefreshFromShop()
{
	if (ARogueGameMode* RogueGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ARogueGameMode>() : nullptr)
	{
		RogueGameMode->TryRefreshShop();
	}
}

void ARogueHUD::RequestCloseFromShop()
{
	if (ARogueGameMode* RogueGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ARogueGameMode>() : nullptr)
	{
		RogueGameMode->CloseShop();
	}
}

void ARogueHUD::RequestSelectUpgradeFromWidget(int32 UpgradeIndex)
{
	if (ARogueGameMode* RogueGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ARogueGameMode>() : nullptr)
	{
		RogueGameMode->TrySelectUpgrade(UpgradeIndex);
	}
}

void ARogueHUD::RequestResumeFromPauseMenu()
{
	HandlePauseMenuAction(FName(TEXT("Pause_Resume")));
}

void ARogueHUD::RequestOpenSettingsFromPauseMenu()
{
	HandlePauseMenuAction(FName(TEXT("Pause_Settings")));
}

void ARogueHUD::RequestQuitFromPauseMenu()
{
	HandlePauseMenuAction(FName(TEXT("Pause_Quit")));
}

void ARogueHUD::RequestBackFromSettingsMenu()
{
	HandlePauseMenuAction(FName(TEXT("Settings_Back")));
}

void ARogueHUD::RequestApplyFromSettingsMenu()
{
	HandlePauseMenuAction(FName(TEXT("Settings_Apply")));
}

void ARogueHUD::RequestVolumeAdjustFromSettingsMenu(bool bIncrease)
{
	HandlePauseMenuAction(bIncrease ? FName(TEXT("Settings_VolumeUp")) : FName(TEXT("Settings_VolumeDown")));
}

void ARogueHUD::RequestQualityAdjustFromSettingsMenu(bool bIncrease)
{
	HandlePauseMenuAction(bIncrease ? FName(TEXT("Settings_QualityUp")) : FName(TEXT("Settings_QualityDown")));
}

void ARogueHUD::RequestResolutionAdjustFromSettingsMenu(bool bIncrease)
{
	HandlePauseMenuAction(bIncrease ? FName(TEXT("Settings_ResolutionUp")) : FName(TEXT("Settings_ResolutionDown")));
}

void ARogueHUD::RequestFrameLimitAdjustFromSettingsMenu(bool bIncrease)
{
	HandlePauseMenuAction(bIncrease ? FName(TEXT("Settings_FrameLimitUp")) : FName(TEXT("Settings_FrameLimitDown")));
}

void ARogueHUD::RequestToggleDisplayModeFromSettingsMenu()
{
	HandlePauseMenuAction(FName(TEXT("Settings_Fullscreen")));
}

void ARogueHUD::RequestRestartAfterDeath()
{
	HandleDeathMenuAction(FName(TEXT("Death_Restart")));
}

void ARogueHUD::RequestQuitAfterDeath()
{
	HandleDeathMenuAction(FName(TEXT("Death_Quit")));
}

int32 ARogueHUD::GetUpgradeIndexAtScreenPosition(float ScreenX, float ScreenY) const
{
	for (int32 Index = 0; Index < 3; ++Index)
	{
		FVector2D CardPosition;
		FVector2D CardSize;
		if (!GetUpgradeCardRect(Index, CardPosition, CardSize))
		{
			continue;
		}

		if (ScreenX >= CardPosition.X && ScreenX <= CardPosition.X + CardSize.X && ScreenY >= CardPosition.Y && ScreenY <= CardPosition.Y + CardSize.Y)
		{
			return Index;
		}
	}

	return INDEX_NONE;
}

bool ARogueHUD::GetUpgradeCardRect(int32 UpgradeIndex, FVector2D& OutPosition, FVector2D& OutSize) const
{
	if (Canvas == nullptr || UpgradeIndex < 0)
	{
		return false;
	}

	OutPosition = FVector2D(Canvas->SizeX * 0.23f, Canvas->SizeY * 0.30f + UpgradeIndex * 90.0f);
	OutSize = FVector2D(Canvas->SizeX * 0.54f, 72.0f);
	return true;
}

bool ARogueHUD::GetShopCardRect(int32 ShopIndex, FVector2D& OutPosition, FVector2D& OutSize) const
{
	if (Canvas == nullptr || ShopIndex < 0 || ShopIndex >= 6)
	{
		return false;
	}

	const int32 Column = ShopIndex % 3;
	const int32 Row = ShopIndex / 3;
	const float StartX = Canvas->SizeX * 0.14f;
	const float StartY = Canvas->SizeY * 0.26f;
	const float SpacingX = Canvas->SizeX * 0.24f;
	const float SpacingY = 132.0f;
	OutPosition = FVector2D(StartX + Column * SpacingX, StartY + Row * SpacingY);
	OutSize = FVector2D(Canvas->SizeX * 0.20f, 112.0f);
	return true;
}

bool ARogueHUD::GetShopButtonRect(FName ButtonName, FVector2D& OutPosition, FVector2D& OutSize) const
{
	if (Canvas == nullptr)
	{
		return false;
	}

	OutSize = FVector2D(Canvas->SizeX * 0.18f, 56.0f);
	if (ButtonName == FName(TEXT("Shop_Refresh")))
	{
		OutPosition = FVector2D(Canvas->SizeX * 0.26f, Canvas->SizeY * 0.69f);
		return true;
	}

	if (ButtonName == FName(TEXT("Shop_Close")))
	{
		OutPosition = FVector2D(Canvas->SizeX * 0.56f, Canvas->SizeY * 0.69f);
		return true;
	}

	return false;
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
		const float HealthPercent = Enemy->GetHealthPercent();

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
		const FString DamageText = FString::Printf(TEXT("%.0f"), DamageNumber.Damage);
		DrawText(DamageText, FLinearColor(0.02f, 0.02f, 0.02f, Alpha * 0.65f), DamageNumber.CachedScreenPosition.X - 1.0f, DamageNumber.CachedScreenPosition.Y - 1.0f, Font, Scale, false);
		DrawText(DamageText, TextColor, DamageNumber.CachedScreenPosition.X, DamageNumber.CachedScreenPosition.Y, Font, Scale, false);
	}
}

void ARogueHUD::DrawShopMenu(UFont* Font, float MouseX, float MouseY, const ARogueGameMode* RogueGameMode, const ARogueCharacter* PlayerCharacter)
{
	if (Canvas == nullptr || RogueGameMode == nullptr || PlayerCharacter == nullptr)
	{
		return;
	}

	DrawRect(FLinearColor(0.01f, 0.02f, 0.05f, 0.88f), 0.0f, 0.0f, Canvas->SizeX, Canvas->SizeY);
	DrawRect(FLinearColor(0.05f, 0.07f, 0.11f, 0.97f), Canvas->SizeX * 0.10f, Canvas->SizeY * 0.16f, Canvas->SizeX * 0.80f, Canvas->SizeY * 0.62f);
	DrawText(TEXT("商店"), FLinearColor(1.0f, 0.92f, 0.55f), Canvas->SizeX * 0.47f, Canvas->SizeY * 0.18f, Font, 1.9f, false);
	DrawText(FString::Printf(TEXT("金币 %d"), PlayerCharacter->GetMoney()), FLinearColor(1.0f, 0.86f, 0.36f), Canvas->SizeX * 0.14f, Canvas->SizeY * 0.20f, Font, 1.15f, false);
	DrawText(TEXT("点击卡牌购买强化"), FLinearColor(0.72f, 0.84f, 0.96f), Canvas->SizeX * 0.40f, Canvas->SizeY * 0.20f, Font, 1.0f, false);
	DrawText(FString::Printf(TEXT("自动补货 %.0f 秒"), FMath::CeilToFloat(RogueGameMode->GetShopSecondsUntilRefresh())), FLinearColor(0.76f, 0.88f, 1.0f), Canvas->SizeX * 0.68f, Canvas->SizeY * 0.20f, Font, 0.95f, false);

	const TArray<FRogueShopOffer>& Offers = RogueGameMode->GetShopOffers();
	for (int32 Index = 0; Index < Offers.Num() && Index < 6; ++Index)
	{
		FVector2D CardPosition;
		FVector2D CardSize;
		if (!GetShopCardRect(Index, CardPosition, CardSize))
		{
			continue;
		}

		const bool bHovered = MouseX >= CardPosition.X && MouseX <= CardPosition.X + CardSize.X && MouseY >= CardPosition.Y && MouseY <= CardPosition.Y + CardSize.Y;
		const bool bAffordable = PlayerCharacter->GetMoney() >= Offers[Index].Cost;
		const FLinearColor CardColor = Offers[Index].bPurchased
			? FLinearColor(0.10f, 0.13f, 0.16f, 0.95f)
			: (bHovered ? FLinearColor(0.16f, 0.23f, 0.32f, 0.98f) : FLinearColor(0.08f, 0.10f, 0.14f, 0.96f));
		DrawRect(CardColor, CardPosition.X, CardPosition.Y, CardSize.X, CardSize.Y);
		AddHitBox(CardPosition, CardSize, FName(*FString::Printf(TEXT("Shop_Offer_%d"), Index)), true, 15);
		DrawText(Offers[Index].Upgrade.Title, FLinearColor::White, CardPosition.X + 18.0f, CardPosition.Y + 10.0f, Font, 1.05f, false);
		DrawText(Offers[Index].Upgrade.Description, FLinearColor(0.74f, 0.84f, 0.98f), CardPosition.X + 18.0f, CardPosition.Y + 38.0f, Font, 0.82f, false);
		const FString CostLabel = Offers[Index].bPurchased ? TEXT("已购买") : FString::Printf(TEXT("价格 %d"), Offers[Index].Cost);
		const FLinearColor CostColor = Offers[Index].bPurchased ? FLinearColor(0.60f, 0.72f, 0.82f) : (bAffordable ? FLinearColor(1.0f, 0.86f, 0.36f) : FLinearColor(1.0f, 0.42f, 0.42f));
		DrawText(CostLabel, CostColor, CardPosition.X + 18.0f, CardPosition.Y + 82.0f, Font, 0.95f, false);
	}

	const TArray<FName> Buttons =
	{
		FName(TEXT("Shop_Refresh")),
		FName(TEXT("Shop_Close"))
	};

	for (const FName& ButtonName : Buttons)
	{
		FVector2D Position;
		FVector2D Size;
		if (!GetShopButtonRect(ButtonName, Position, Size))
		{
			continue;
		}

		const bool bHovered = MouseX >= Position.X && MouseX <= Position.X + Size.X && MouseY >= Position.Y && MouseY <= Position.Y + Size.Y;
		DrawRect(bHovered ? FLinearColor(0.18f, 0.24f, 0.34f, 0.98f) : FLinearColor(0.08f, 0.10f, 0.16f, 0.95f), Position.X, Position.Y, Size.X, Size.Y);
		AddHitBox(Position, Size, ButtonName, true, 16);
		if (ButtonName == FName(TEXT("Shop_Refresh")))
		{
			DrawText(FString::Printf(TEXT("立即刷新 (%d)"), RogueGameMode->GetShopRefreshCost()), FLinearColor::White, Position.X + 18.0f, Position.Y + 14.0f, Font, 1.00f, false);
		}
		else
		{
			DrawText(TEXT("离开商店"), FLinearColor::White, Position.X + 36.0f, Position.Y + 14.0f, Font, 1.04f, false);
		}
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

void ARogueHUD::DrawPauseMenu(UFont* Font)
{
	if (Canvas == nullptr)
	{
		return;
	}

	float MouseX = -1.0f;
	float MouseY = -1.0f;
	if (APlayerController* PlayerController = GetOwningPlayerController())
	{
		PlayerController->GetMousePosition(MouseX, MouseY);
	}

	DrawRect(FLinearColor(0.01f, 0.02f, 0.05f, 0.86f), 0.0f, 0.0f, Canvas->SizeX, Canvas->SizeY);

	if (PauseMenuPage == ERoguePauseMenuPage::Main)
	{
		DrawPauseMainMenu(Font, MouseX, MouseY);
	}
	else if (PauseMenuPage == ERoguePauseMenuPage::Settings)
	{
		DrawPauseSettingsMenu(Font, MouseX, MouseY);
	}
}

void ARogueHUD::DrawPauseMainMenu(UFont* Font, float MouseX, float MouseY)
{
	DrawText(TEXT("游戏菜单"), FLinearColor(1.0f, 0.92f, 0.55f), Canvas->SizeX * 0.41f, Canvas->SizeY * 0.20f, Font, 1.9f, false);

	const TArray<FName> Buttons =
	{
		FName(TEXT("Pause_Resume")),
		FName(TEXT("Pause_Settings")),
		FName(TEXT("Pause_Quit"))
	};

	const TArray<FString> Labels =
	{
		TEXT("回到游戏"),
		TEXT("设置"),
		TEXT("退出游戏")
	};

	for (int32 Index = 0; Index < Buttons.Num(); ++Index)
	{
		FVector2D Position;
		FVector2D Size;
		if (!GetPauseMenuButtonRect(Buttons[Index], Position, Size))
		{
			continue;
		}

		const bool bHovered = MouseX >= Position.X && MouseX <= Position.X + Size.X && MouseY >= Position.Y && MouseY <= Position.Y + Size.Y;
		DrawRect(bHovered ? FLinearColor(0.18f, 0.24f, 0.34f, 0.98f) : FLinearColor(0.08f, 0.10f, 0.16f, 0.95f), Position.X, Position.Y, Size.X, Size.Y);
		AddHitBox(Position, Size, Buttons[Index], true, 20);
		DrawText(Labels[Index], FLinearColor::White, Position.X + 48.0f, Position.Y + 18.0f, Font, 1.25f, false);
	}
}

void ARogueHUD::DrawPauseSettingsMenu(UFont* Font, float MouseX, float MouseY)
{
	DrawText(TEXT("设置"), FLinearColor(1.0f, 0.92f, 0.55f), Canvas->SizeX * 0.46f, Canvas->SizeY * 0.18f, Font, 1.8f, false);

	const FVector2D PanelPosition(Canvas->SizeX * 0.23f, Canvas->SizeY * 0.26f);
	const FVector2D PanelSize(Canvas->SizeX * 0.54f, Canvas->SizeY * 0.54f);
	DrawRect(FLinearColor(0.06f, 0.08f, 0.12f, 0.96f), PanelPosition.X, PanelPosition.Y, PanelSize.X, PanelSize.Y);

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

	for (int32 RowIndex = 0; RowIndex < RowLabels.Num(); ++RowIndex)
	{
		FVector2D ValuePosition;
		FVector2D ValueSize;
		if (!GetSettingsValueRect(RowIndex, ValuePosition, ValueSize))
		{
			continue;
		}

		const float LabelX = PanelPosition.X + 34.0f;
		const float RowY = ValuePosition.Y + 10.0f;
		DrawText(RowLabels[RowIndex], FLinearColor::White, LabelX, RowY, Font, 1.08f, false);
		DrawText(RowValues[RowIndex], FLinearColor(0.65f, 0.85f, 1.0f), ValuePosition.X + 52.0f, RowY, Font, 1.0f, false);

		const bool bUseAdjustButtons = RowIndex != 3;
		if (bUseAdjustButtons)
		{
			const FVector2D MinusPosition(ValuePosition.X, ValuePosition.Y);
			const FVector2D PlusPosition(ValuePosition.X + ValueSize.X - 44.0f, ValuePosition.Y);
			const FVector2D AdjustSize(44.0f, ValueSize.Y);
			FName MinusName;
			FName PlusName;
			switch (RowIndex)
			{
			case 0:
				MinusName = FName(TEXT("Settings_VolumeDown"));
				PlusName = FName(TEXT("Settings_VolumeUp"));
				break;
			case 1:
				MinusName = FName(TEXT("Settings_QualityDown"));
				PlusName = FName(TEXT("Settings_QualityUp"));
				break;
			case 2:
				MinusName = FName(TEXT("Settings_ResolutionDown"));
				PlusName = FName(TEXT("Settings_ResolutionUp"));
				break;
			default:
				MinusName = FName(TEXT("Settings_FrameLimitDown"));
				PlusName = FName(TEXT("Settings_FrameLimitUp"));
				break;
			}

			const bool bMinusHovered = MouseX >= MinusPosition.X && MouseX <= MinusPosition.X + AdjustSize.X && MouseY >= MinusPosition.Y && MouseY <= MinusPosition.Y + AdjustSize.Y;
			const bool bPlusHovered = MouseX >= PlusPosition.X && MouseX <= PlusPosition.X + AdjustSize.X && MouseY >= PlusPosition.Y && MouseY <= PlusPosition.Y + AdjustSize.Y;

			DrawRect(bMinusHovered ? FLinearColor(0.18f, 0.24f, 0.34f, 0.98f) : FLinearColor(0.10f, 0.12f, 0.20f, 0.95f), MinusPosition.X, MinusPosition.Y, AdjustSize.X, AdjustSize.Y);
			DrawRect(bPlusHovered ? FLinearColor(0.18f, 0.24f, 0.34f, 0.98f) : FLinearColor(0.10f, 0.12f, 0.20f, 0.95f), PlusPosition.X, PlusPosition.Y, AdjustSize.X, AdjustSize.Y);
			AddHitBox(MinusPosition, AdjustSize, MinusName, true, 20);
			AddHitBox(PlusPosition, AdjustSize, PlusName, true, 20);
			DrawText(TEXT("-"), FLinearColor::White, MinusPosition.X + 16.0f, MinusPosition.Y + 4.0f, Font, 1.15f, false);
			DrawText(TEXT("+"), FLinearColor::White, PlusPosition.X + 14.0f, PlusPosition.Y + 2.0f, Font, 1.15f, false);
		}
		else
		{
			const bool bHovered = MouseX >= ValuePosition.X && MouseX <= ValuePosition.X + ValueSize.X && MouseY >= ValuePosition.Y && MouseY <= ValuePosition.Y + ValueSize.Y;
			DrawRect(bHovered ? FLinearColor(0.18f, 0.24f, 0.34f, 0.98f) : FLinearColor(0.10f, 0.12f, 0.20f, 0.95f), ValuePosition.X, ValuePosition.Y, ValueSize.X, ValueSize.Y);
			AddHitBox(ValuePosition, ValueSize, FName(TEXT("Settings_Fullscreen")), true, 20);
			DrawText(RowValues[RowIndex], FLinearColor::White, ValuePosition.X + 38.0f, ValuePosition.Y + 8.0f, Font, 1.0f, false);
		}
	}

	DrawText(
		bDisplaySettingsDirty ? TEXT("画面设置已修改，点击“应用设置”后生效") : TEXT("当前画面设置已应用"),
		bDisplaySettingsDirty ? FLinearColor(1.0f, 0.82f, 0.45f) : FLinearColor(0.70f, 0.78f, 0.84f),
		Canvas->SizeX * 0.31f,
		Canvas->SizeY * 0.77f,
		Font,
		0.95f,
		false);

	FVector2D ApplyPosition;
	FVector2D ApplySize;
	if (GetPauseMenuButtonRect(FName(TEXT("Settings_Apply")), ApplyPosition, ApplySize))
	{
		const bool bHovered = MouseX >= ApplyPosition.X && MouseX <= ApplyPosition.X + ApplySize.X && MouseY >= ApplyPosition.Y && MouseY <= ApplyPosition.Y + ApplySize.Y;
		const FLinearColor ApplyColor = bDisplaySettingsDirty
			? (bHovered ? FLinearColor(0.22f, 0.34f, 0.22f, 0.98f) : FLinearColor(0.12f, 0.22f, 0.12f, 0.95f))
			: (bHovered ? FLinearColor(0.16f, 0.20f, 0.22f, 0.96f) : FLinearColor(0.08f, 0.10f, 0.12f, 0.92f));
		DrawRect(ApplyColor, ApplyPosition.X, ApplyPosition.Y, ApplySize.X, ApplySize.Y);
		AddHitBox(ApplyPosition, ApplySize, FName(TEXT("Settings_Apply")), true, 20);
		DrawText(TEXT("应用设置"), FLinearColor::White, ApplyPosition.X + 44.0f, ApplyPosition.Y + 14.0f, Font, 1.1f, false);
	}

	FVector2D BackPosition;
	FVector2D BackSize;
	if (GetPauseMenuButtonRect(FName(TEXT("Settings_Back")), BackPosition, BackSize))
	{
		const bool bHovered = MouseX >= BackPosition.X && MouseX <= BackPosition.X + BackSize.X && MouseY >= BackPosition.Y && MouseY <= BackPosition.Y + BackSize.Y;
		DrawRect(bHovered ? FLinearColor(0.18f, 0.24f, 0.34f, 0.98f) : FLinearColor(0.08f, 0.10f, 0.16f, 0.95f), BackPosition.X, BackPosition.Y, BackSize.X, BackSize.Y);
		AddHitBox(BackPosition, BackSize, FName(TEXT("Settings_Back")), true, 20);
		DrawText(TEXT("返回上一层"), FLinearColor::White, BackPosition.X + 42.0f, BackPosition.Y + 14.0f, Font, 1.1f, false);
	}
}

void ARogueHUD::DrawDeathMenu(UFont* Font, float MouseX, float MouseY, ARogueGameMode* RogueGameMode)
{
	DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.78f), 0.0f, 0.0f, Canvas->SizeX, Canvas->SizeY);
	DrawText(TEXT("本轮结束"), FLinearColor(1.0f, 0.25f, 0.25f), Canvas->SizeX * 0.42f, Canvas->SizeY * 0.34f, Font, 2.0f, false);

	if (RogueGameMode != nullptr)
	{
		DrawText(FString::Printf(TEXT("存活 %.0f 秒  |  击败 %d 个敌人"), RogueGameMode->GetRunTimeSeconds(), RogueGameMode->GetEnemiesDefeated()), FLinearColor::White, Canvas->SizeX * 0.29f, Canvas->SizeY * 0.40f, Font, 1.2f, false);
	}

	const TArray<FName> Buttons =
	{
		FName(TEXT("Death_Restart")),
		FName(TEXT("Death_Quit"))
	};

	const TArray<FString> Labels =
	{
		TEXT("开始新的一局"),
		TEXT("退出游戏")
	};

	for (int32 Index = 0; Index < Buttons.Num(); ++Index)
	{
		FVector2D Position;
		FVector2D Size;
		if (!GetDeathMenuButtonRect(Buttons[Index], Position, Size))
		{
			continue;
		}

		const bool bHovered = MouseX >= Position.X && MouseX <= Position.X + Size.X && MouseY >= Position.Y && MouseY <= Position.Y + Size.Y;
		DrawRect(bHovered ? FLinearColor(0.18f, 0.24f, 0.34f, 0.98f) : FLinearColor(0.08f, 0.10f, 0.16f, 0.95f), Position.X, Position.Y, Size.X, Size.Y);
		AddHitBox(Position, Size, Buttons[Index], true, 30);
		DrawText(Labels[Index], FLinearColor::White, Position.X + 36.0f, Position.Y + 18.0f, Font, 1.18f, false);
	}
}

bool ARogueHUD::GetPauseMenuButtonRect(FName ButtonName, FVector2D& OutPosition, FVector2D& OutSize) const
{
	if (Canvas == nullptr)
	{
		return false;
	}

	OutSize = FVector2D(Canvas->SizeX * 0.28f, 62.0f);

	if (ButtonName == FName(TEXT("Pause_Resume")))
	{
		OutPosition = FVector2D(Canvas->SizeX * 0.36f, Canvas->SizeY * 0.32f);
		return true;
	}

	if (ButtonName == FName(TEXT("Pause_Settings")))
	{
		OutPosition = FVector2D(Canvas->SizeX * 0.36f, Canvas->SizeY * 0.43f);
		return true;
	}

	if (ButtonName == FName(TEXT("Pause_Quit")))
	{
		OutPosition = FVector2D(Canvas->SizeX * 0.36f, Canvas->SizeY * 0.54f);
		return true;
	}

	if (ButtonName == FName(TEXT("Settings_Back")))
	{
		OutPosition = FVector2D(Canvas->SizeX * 0.39f, Canvas->SizeY * 0.82f);
		OutSize = FVector2D(Canvas->SizeX * 0.22f, 56.0f);
		return true;
	}

	if (ButtonName == FName(TEXT("Settings_Apply")))
	{
		OutPosition = FVector2D(Canvas->SizeX * 0.17f, Canvas->SizeY * 0.82f);
		OutSize = FVector2D(Canvas->SizeX * 0.22f, 56.0f);
		return true;
	}

	return false;
}

bool ARogueHUD::GetDeathMenuButtonRect(FName ButtonName, FVector2D& OutPosition, FVector2D& OutSize) const
{
	if (Canvas == nullptr)
	{
		return false;
	}

	OutSize = FVector2D(Canvas->SizeX * 0.24f, 60.0f);

	if (ButtonName == FName(TEXT("Death_Restart")))
	{
		OutPosition = FVector2D(Canvas->SizeX * 0.38f, Canvas->SizeY * 0.50f);
		return true;
	}

	if (ButtonName == FName(TEXT("Death_Quit")))
	{
		OutPosition = FVector2D(Canvas->SizeX * 0.38f, Canvas->SizeY * 0.61f);
		return true;
	}

	return false;
}

bool ARogueHUD::GetSettingsValueRect(int32 RowIndex, FVector2D& OutPosition, FVector2D& OutSize) const
{
	if (Canvas == nullptr || RowIndex < 0)
	{
		return false;
	}

	OutPosition = FVector2D(Canvas->SizeX * 0.47f, Canvas->SizeY * 0.31f + RowIndex * 66.0f);
	OutSize = FVector2D(Canvas->SizeX * 0.21f, 42.0f);
	return true;
}

void ARogueHUD::HandlePauseMenuAction(FName BoxName)
{
	APlayerController* PlayerController = GetOwningPlayerController();
	ARogueCharacter* PlayerCharacter = Cast<ARogueCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));

	if (BoxName == FName(TEXT("Pause_Resume")))
	{
		ClosePauseMenu();
		if (PlayerCharacter != nullptr)
		{
			PlayerCharacter->SetUpgradeSelectionInput(false);
		}
		UGameplayStatics::SetGamePaused(this, false);
		return;
	}

	if (BoxName == FName(TEXT("Pause_Settings")))
	{
		PauseMenuPage = ERoguePauseMenuPage::Settings;
		return;
	}

	if (BoxName == FName(TEXT("Pause_Quit")))
	{
		ClosePauseMenu();
		UKismetSystemLibrary::QuitGame(this, PlayerController, EQuitPreference::Quit, false);
		return;
	}

	if (BoxName == FName(TEXT("Settings_Back")))
	{
		PauseMenuPage = ERoguePauseMenuPage::Main;
		return;
	}

	if (BoxName == FName(TEXT("Settings_Apply")))
	{
		ApplyDisplaySettings();
		return;
	}

	if (BoxName == FName(TEXT("Settings_VolumeDown")))
	{
		MasterVolume = FMath::Clamp(MasterVolume - 0.1f, 0.0f, 1.0f);
		ApplyMasterVolume();
		return;
	}

	if (BoxName == FName(TEXT("Settings_VolumeUp")))
	{
		MasterVolume = FMath::Clamp(MasterVolume + 0.1f, 0.0f, 1.0f);
		ApplyMasterVolume();
		return;
	}

	if (BoxName == FName(TEXT("Settings_QualityDown")))
	{
		GraphicsQualityLevel = FMath::Clamp(GraphicsQualityLevel - 1, 0, 4);
		bGraphicsQualityCustom = false;
		bDisplaySettingsDirty = true;
		return;
	}

	if (BoxName == FName(TEXT("Settings_QualityUp")))
	{
		GraphicsQualityLevel = FMath::Clamp(GraphicsQualityLevel + 1, 0, 4);
		bGraphicsQualityCustom = false;
		bDisplaySettingsDirty = true;
		return;
	}

	if (BoxName == FName(TEXT("Settings_ResolutionDown")))
	{
		StepResolution(-1);
		bDisplaySettingsDirty = true;
		return;
	}

	if (BoxName == FName(TEXT("Settings_ResolutionUp")))
	{
		StepResolution(1);
		bDisplaySettingsDirty = true;
		return;
	}

	if (BoxName == FName(TEXT("Settings_FrameLimitDown")))
	{
		StepFrameRateLimit(-1);
		bDisplaySettingsDirty = true;
		return;
	}

	if (BoxName == FName(TEXT("Settings_FrameLimitUp")))
	{
		StepFrameRateLimit(1);
		bDisplaySettingsDirty = true;
		return;
	}

	if (BoxName == FName(TEXT("Settings_Fullscreen")))
	{
		CycleDisplayMode();
		bDisplaySettingsDirty = true;
	}
}

void ARogueHUD::HandleDeathMenuAction(FName BoxName)
{
	APlayerController* PlayerController = GetOwningPlayerController();

	if (BoxName == FName(TEXT("Death_Restart")))
	{
		PauseMenuPage = ERoguePauseMenuPage::None;
		UGameplayStatics::SetGamePaused(this, false);
		const FString CurrentLevelName = UGameplayStatics::GetCurrentLevelName(this, true);
		if (!CurrentLevelName.IsEmpty())
		{
			UGameplayStatics::OpenLevel(this, FName(*CurrentLevelName));
		}
		return;
	}

	if (BoxName == FName(TEXT("Death_Quit")))
	{
		PauseMenuPage = ERoguePauseMenuPage::None;
		UGameplayStatics::SetGamePaused(this, false);
		UKismetSystemLibrary::QuitGame(this, PlayerController, EQuitPreference::Quit, false);
	}
}

void ARogueHUD::InitializeMenuWidgets()
{
	if (APlayerController* PlayerController = GetOwningPlayerController())
	{
		if (ShopWidgetClass != nullptr)
		{
			ShopWidget = CreateWidget<URogueShopWidgetBase>(PlayerController, ShopWidgetClass);
			if (ShopWidget != nullptr)
			{
				ShopWidget->SetOwningRogueHUD(this);
				ShopWidget->AddToViewport(64);
				ShopWidget->SetVisibility(ESlateVisibility::Collapsed);
			}
		}

		if (UpgradeSelectionWidgetClass != nullptr)
		{
			UpgradeSelectionWidget = CreateWidget<URogueUpgradeSelectionWidgetBase>(PlayerController, UpgradeSelectionWidgetClass);
			if (UpgradeSelectionWidget != nullptr)
			{
				UpgradeSelectionWidget->SetOwningRogueHUD(this);
				UpgradeSelectionWidget->AddToViewport(63);
				UpgradeSelectionWidget->SetVisibility(ESlateVisibility::Collapsed);
			}
		}

		if (PauseMenuWidgetClass != nullptr)
		{
			PauseMenuWidget = CreateWidget<URoguePauseMenuWidgetBase>(PlayerController, PauseMenuWidgetClass);
			if (PauseMenuWidget != nullptr)
			{
				PauseMenuWidget->SetOwningRogueHUD(this);
				PauseMenuWidget->AddToViewport(65);
				PauseMenuWidget->SetVisibility(ESlateVisibility::Collapsed);
			}
		}

		if (SettingsMenuWidgetClass != nullptr)
		{
			SettingsMenuWidget = CreateWidget<URogueSettingsMenuWidgetBase>(PlayerController, SettingsMenuWidgetClass);
			if (SettingsMenuWidget != nullptr)
			{
				SettingsMenuWidget->SetOwningRogueHUD(this);
				SettingsMenuWidget->AddToViewport(66);
				SettingsMenuWidget->SetVisibility(ESlateVisibility::Collapsed);
			}
		}
	}

	if (DeathScreenWidgetClass == nullptr)
	{
		return;
	}

	if (APlayerController* PlayerController = GetOwningPlayerController())
	{
		DeathScreenWidget = CreateWidget<URogueDeathScreenWidgetBase>(PlayerController, DeathScreenWidgetClass);
		if (DeathScreenWidget != nullptr)
		{
			DeathScreenWidget->SetOwningRogueHUD(this);
			DeathScreenWidget->AddToViewport(70);
			DeathScreenWidget->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void ARogueHUD::UpdateShopWidget(const ARogueGameMode* RogueGameMode, const ARogueCharacter* PlayerCharacter)
{
	if (ShopWidget == nullptr)
	{
		return;
	}

	FRogueShopViewData ViewData;
	ViewData.bVisible = RogueGameMode != nullptr && PlayerCharacter != nullptr && RogueGameMode->IsShopOpen();
	ViewData.TitleText = FText::FromString(TEXT("商店"));
	if (PlayerCharacter != nullptr)
	{
		ViewData.MoneyText = FText::FromString(FString::Printf(TEXT("金币 %d"), PlayerCharacter->GetMoney()));
	}
	ViewData.HintText = FText::FromString(TEXT("点击卡牌购买强化"));
	if (RogueGameMode != nullptr)
	{
		ViewData.AutoRefreshText = FText::FromString(FString::Printf(TEXT("自动补货 %.0f 秒"), FMath::CeilToFloat(RogueGameMode->GetShopSecondsUntilRefresh())));
		ViewData.RefreshButtonText = FText::FromString(FString::Printf(TEXT("立即刷新 (%d)"), RogueGameMode->GetShopRefreshCost()));

		const TArray<FRogueShopOffer>& Offers = RogueGameMode->GetShopOffers();
		for (const FRogueShopOffer& Offer : Offers)
		{
			FRogueShopOfferViewData& OfferView = ViewData.Offers.AddDefaulted_GetRef();
			OfferView.TitleText = FText::FromString(Offer.Upgrade.Title);
			OfferView.DescriptionText = FText::FromString(Offer.Upgrade.Description);
			OfferView.bPurchased = Offer.bPurchased;
			OfferView.bAffordable = PlayerCharacter != nullptr && PlayerCharacter->GetMoney() >= Offer.Cost;
			OfferView.CostText = FText::FromString(
				Offer.bPurchased ? TEXT("已购买") : FString::Printf(TEXT("价格 %d"), Offer.Cost));
		}
	}

	ShopWidget->UpdateShopView(ViewData);
}

void ARogueHUD::UpdateUpgradeSelectionWidget(const ARogueGameMode* RogueGameMode)
{
	if (UpgradeSelectionWidget == nullptr)
	{
		return;
	}

	FRogueUpgradeSelectionViewData ViewData;
	ViewData.bVisible = RogueGameMode != nullptr && RogueGameMode->IsAwaitingUpgradeChoice();
	ViewData.TitleText = FText::FromString(TEXT("请选择升级"));
	if (RogueGameMode != nullptr)
	{
		const TArray<FRogueUpgradeOption>& Options = RogueGameMode->GetPendingUpgrades();
		for (const FRogueUpgradeOption& Option : Options)
		{
			FRogueUpgradeCardViewData& Card = ViewData.Cards.AddDefaulted_GetRef();
			Card.TitleText = FText::FromString(Option.Title);
			Card.DescriptionText = FText::FromString(Option.Description);
		}
	}

	UpgradeSelectionWidget->UpdateUpgradeSelectionView(ViewData);
}

void ARogueHUD::UpdatePauseWidget()
{
	if (PauseMenuWidget == nullptr)
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

	FRogueSettingsMenuViewData ViewData;
	ViewData.bVisible = PauseMenuPage == ERoguePauseMenuPage::Settings;
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

void ARogueHUD::UpdateDeathWidget(ARogueGameMode* RogueGameMode)
{
	if (DeathScreenWidget == nullptr)
	{
		return;
	}

	const ARogueCharacter* PlayerCharacter = Cast<ARogueCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
	const bool bPlayerDead = PlayerCharacter != nullptr && PlayerCharacter->IsDead();

	FRogueDeathViewData ViewData;
	ViewData.bVisible = bPlayerDead;
	if (bPlayerDead)
	{
		ViewData.TitleText = FText::FromString(TEXT("本轮结束"));
		if (RogueGameMode != nullptr)
		{
			ViewData.SummaryText = FText::FromString(
				FString::Printf(TEXT("存活 %.0f 秒  |  击败 %d 个敌人"), RogueGameMode->GetRunTimeSeconds(), RogueGameMode->GetEnemiesDefeated()));
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

void ARogueHUD::NotifyHitBoxClick(FName BoxName)
{
	Super::NotifyHitBoxClick(BoxName);

	const FString BoxNameString = BoxName.ToString();
	if (BoxNameString.StartsWith(TEXT("Shop_")))
	{
		HandleShopAction(BoxName);
		return;
	}

	if (BoxNameString.StartsWith(TEXT("Pause_")) || BoxNameString.StartsWith(TEXT("Settings_")))
	{
		HandlePauseMenuAction(BoxName);
		return;
	}

	if (BoxNameString.StartsWith(TEXT("Death_")))
	{
		HandleDeathMenuAction(BoxName);
		return;
	}

	if (!BoxNameString.StartsWith(TEXT("Upgrade_")))
	{
		return;
	}

	const FString IndexString = BoxNameString.RightChop(8);
	const int32 UpgradeIndex = FCString::Atoi(*IndexString);
	if (ARogueGameMode* RogueGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ARogueGameMode>() : nullptr)
	{
		RogueGameMode->TrySelectUpgrade(UpgradeIndex);
	}
}

void ARogueHUD::HandleShopAction(FName BoxName)
{
	ARogueGameMode* RogueGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ARogueGameMode>() : nullptr;
	if (RogueGameMode == nullptr)
	{
		return;
	}

	if (BoxName == FName(TEXT("Shop_Refresh")))
	{
		RogueGameMode->TryRefreshShop();
		return;
	}

	if (BoxName == FName(TEXT("Shop_Close")))
	{
		RogueGameMode->CloseShop();
		return;
	}

	const FString BoxNameString = BoxName.ToString();
	if (!BoxNameString.StartsWith(TEXT("Shop_Offer_")))
	{
		return;
	}

	const int32 OfferIndex = FCString::Atoi(*BoxNameString.RightChop(11));
	RogueGameMode->TryBuyShopOffer(OfferIndex);
}
