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
	if (ARogueGameMode* RogueGameMode = GetRogueGameMode())
	{
		RogueGameMode->TryBuyShopOffer(OfferIndex);
	}
}

void ARogueHUD::RequestRefreshFromShop()
{
	if (ARogueGameMode* RogueGameMode = GetRogueGameMode())
	{
		RogueGameMode->TryRefreshShop();
	}
}

void ARogueHUD::RequestCloseFromShop()
{
	if (ARogueGameMode* RogueGameMode = GetRogueGameMode())
	{
		RogueGameMode->CloseShop();
	}
}

void ARogueHUD::RequestSelectUpgradeFromWidget(int32 UpgradeIndex)
{
	if (ARogueGameMode* RogueGameMode = GetRogueGameMode())
	{
		RogueGameMode->TrySelectUpgrade(UpgradeIndex);
	}
}

void ARogueHUD::RequestResumeFromPauseMenu()
{
	ClosePauseMenu();
	if (ARogueCharacter* PlayerCharacter = Cast<ARogueCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0)))
	{
		PlayerCharacter->SetUpgradeSelectionInput(false);
	}
	UGameplayStatics::SetGamePaused(this, false);
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
	UGameplayStatics::SetGamePaused(this, false);

	const FString CurrentLevelName = UGameplayStatics::GetCurrentLevelName(this, true);
	if (!CurrentLevelName.IsEmpty())
	{
		UGameplayStatics::OpenLevel(this, FName(*CurrentLevelName));
	}
}

void ARogueHUD::RequestQuitAfterDeath()
{
	PauseMenuPage = ERoguePauseMenuPage::None;
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
	}
}

ARogueGameMode* ARogueHUD::GetRogueGameMode() const
{
	return GetWorld() != nullptr ? GetWorld()->GetAuthGameMode<ARogueGameMode>() : nullptr;
}

void ARogueHUD::UpdateShopWidget(const ARogueGameMode* RogueGameMode, const ARogueCharacter* PlayerCharacter)
{
	if (ShopWidget == nullptr)
	{
		return;
	}

	const bool bVisible = RogueGameMode != nullptr && PlayerCharacter != nullptr && RogueGameMode->IsShopOpen();
	const int32 AutoRefreshSeconds = bVisible ? FMath::CeilToInt(RogueGameMode->GetShopSecondsUntilRefresh()) : INDEX_NONE;
	FString Signature = bVisible ? TEXT("Shop|Visible") : TEXT("Shop|Hidden");
	if (bVisible)
	{
		Signature += FString::Printf(TEXT("|Money=%d|Refresh=%d"), PlayerCharacter->GetMoney(), RogueGameMode->GetShopRefreshCost());
		const TArray<FRogueShopOffer>& Offers = RogueGameMode->GetShopOffers();
		for (const FRogueShopOffer& Offer : Offers)
		{
			const bool bAffordable = PlayerCharacter->GetMoney() >= Offer.Cost;
			Signature += FString::Printf(
				TEXT("|%d:%d:%d:%d:%s:%s"),
				static_cast<int32>(Offer.Upgrade.Type),
				Offer.Cost,
				Offer.bPurchased ? 1 : 0,
				bAffordable ? 1 : 0,
				*Offer.Upgrade.Title,
				*Offer.Upgrade.Description);
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
	if (RogueGameMode != nullptr)
	{
		ViewData.AutoRefreshText = GetAutoRefreshText(AutoRefreshSeconds);
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

	const bool bVisible = RogueGameMode != nullptr && RogueGameMode->IsAwaitingUpgradeChoice();
	FString Signature = bVisible ? TEXT("Upgrade|Visible") : TEXT("Upgrade|Hidden");
	if (bVisible)
	{
		const TArray<FRogueUpgradeOption>& Options = RogueGameMode->GetPendingUpgrades();
		for (const FRogueUpgradeOption& Option : Options)
		{
			Signature += FString::Printf(TEXT("|%d:%s:%s"), static_cast<int32>(Option.Type), *Option.Title, *Option.Description);
		}
	}

	if (!UpdateCachedSignature(LastUpgradeWidgetSignature, Signature))
	{
		return;
	}

	FRogueUpgradeSelectionViewData ViewData;
	ViewData.bVisible = bVisible;
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

void ARogueHUD::UpdateDeathWidget(ARogueGameMode* RogueGameMode)
{
	if (DeathScreenWidget == nullptr)
	{
		return;
	}

	const ARogueCharacter* PlayerCharacter = Cast<ARogueCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
	const bool bPlayerDead = PlayerCharacter != nullptr && PlayerCharacter->IsDead();
	FString Signature = bPlayerDead ? TEXT("Death|Visible") : TEXT("Death|Hidden");
	if (bPlayerDead && RogueGameMode != nullptr)
	{
		Signature += FString::Printf(TEXT("|Time=%.0f|Kills=%d"), RogueGameMode->GetRunTimeSeconds(), RogueGameMode->GetEnemiesDefeated());
	}

	if (!UpdateCachedSignature(LastDeathWidgetSignature, Signature))
	{
		return;
	}

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
