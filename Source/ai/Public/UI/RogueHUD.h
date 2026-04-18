#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameUserSettings.h"
#include "GameFramework/HUD.h"
#include "UI/RogueMenuWidgetBases.h"
#include "RogueHUD.generated.h"

class ARogueGameMode;
class ARogueCharacter;
class ARogueEnemy;
class APlayerController;
class URoguePauseMenuWidgetBase;
class URogueShopWidgetBase;
class URogueUpgradeSelectionWidgetBase;
class URogueSettingsMenuWidgetBase;
class URogueDeathScreenWidgetBase;
class URogueEnemyTrackerSubsystem;

enum class ERoguePauseMenuPage : uint8
{
	None,
	Main,
	Settings
};

struct FRogueDamageNumber
{
	FVector WorldLocation = FVector::ZeroVector;
	float Damage = 0.0f;
	float SpawnTime = 0.0f;
	float Lifetime = 0.75f;
	FLinearColor Color = FLinearColor::White;
	FVector2D CachedScreenPosition = FVector2D::ZeroVector;
	bool bScreenPositionValid = false;
};

struct FRogueCachedEnemyHealthBar
{
	TWeakObjectPtr<ARogueEnemy> Enemy;
	bool bBoss = false;
};

UCLASS()
class AI_API ARogueHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
	virtual void DrawHUD() override;
	virtual void NotifyHitBoxClick(FName BoxName) override;
	int32 GetUpgradeIndexAtScreenPosition(float ScreenX, float ScreenY) const;
	void AddDamageNumber(const FVector& WorldLocation, float Damage, bool bIsBossDamage);
	void OpenPauseMenu();
	void ClosePauseMenu();
	void RequestSelectOfferFromShop(int32 OfferIndex);
	void RequestRefreshFromShop();
	void RequestCloseFromShop();
	void RequestSelectUpgradeFromWidget(int32 UpgradeIndex);
	void RequestResumeFromPauseMenu();
	void RequestOpenSettingsFromPauseMenu();
	void RequestQuitFromPauseMenu();
	void RequestBackFromSettingsMenu();
	void RequestApplyFromSettingsMenu();
	void RequestVolumeAdjustFromSettingsMenu(bool bIncrease);
	void RequestQualityAdjustFromSettingsMenu(bool bIncrease);
	void RequestResolutionAdjustFromSettingsMenu(bool bIncrease);
	void RequestFrameLimitAdjustFromSettingsMenu(bool bIncrease);
	void RequestToggleDisplayModeFromSettingsMenu();
	void RequestRestartAfterDeath();
	void RequestQuitAfterDeath();

	bool IsPauseMenuOpen() const { return PauseMenuPage != ERoguePauseMenuPage::None; }

private:
	bool GetUpgradeCardRect(int32 UpgradeIndex, FVector2D& OutPosition, FVector2D& OutSize) const;
	bool GetShopCardRect(int32 ShopIndex, FVector2D& OutPosition, FVector2D& OutSize) const;
	bool GetShopButtonRect(FName ButtonName, FVector2D& OutPosition, FVector2D& OutSize) const;
	void RefreshEnemyHealthBarCache(const URogueEnemyTrackerSubsystem* Tracker);
	void RefreshDamageNumberProjectionCache(APlayerController* PlayerController, const URogueEnemyTrackerSubsystem* Tracker);
	void DrawEnemyHealthBars(UFont* Font);
	void DrawDamageNumbers(UFont* Font);
	void DrawShopMenu(UFont* Font, float MouseX, float MouseY, const ARogueGameMode* RogueGameMode, const ARogueCharacter* PlayerCharacter);
	void DrawPauseMenu(UFont* Font);
	void DrawPauseMainMenu(UFont* Font, float MouseX, float MouseY);
	void DrawPauseSettingsMenu(UFont* Font, float MouseX, float MouseY);
	void DrawDeathMenu(UFont* Font, float MouseX, float MouseY, ARogueGameMode* RogueGameMode);
	bool GetPauseMenuButtonRect(FName ButtonName, FVector2D& OutPosition, FVector2D& OutSize) const;
	bool GetDeathMenuButtonRect(FName ButtonName, FVector2D& OutPosition, FVector2D& OutSize) const;
	bool GetSettingsValueRect(int32 RowIndex, FVector2D& OutPosition, FVector2D& OutSize) const;
	void HandlePauseMenuAction(FName BoxName);
	void HandleShopAction(FName BoxName);
	void HandleDeathMenuAction(FName BoxName);
	void InitializeMenuWidgets();
	void UpdateShopWidget(const ARogueGameMode* RogueGameMode, const ARogueCharacter* PlayerCharacter);
	void UpdateUpgradeSelectionWidget(const ARogueGameMode* RogueGameMode);
	void UpdatePauseWidget();
	void UpdateSettingsWidget();
	void UpdateDeathWidget(ARogueGameMode* RogueGameMode);
	void RefreshPauseSettings();
	void RefreshResolutionOptions(const FIntPoint& CurrentResolution, const FIntPoint& InDesktopResolution);
	void RefreshFrameRateLimitOptions(float CurrentFrameRateLimit);
	void StepResolution(int32 Direction);
	void StepFrameRateLimit(int32 Direction);
	void CycleDisplayMode();
	void ApplyMasterVolume();
	void ApplyDisplaySettings();
	int32 ResolveGraphicsQualityLevel(const UGameUserSettings* UserSettings);
	FString GetGraphicsQualityLabel() const;
	FString GetResolutionLabel() const;
	FString GetDisplayModeLabel() const;
	FString GetFrameRateLimitLabel() const;

	TArray<FRogueDamageNumber> DamageNumbers;
	TArray<FRogueCachedEnemyHealthBar> CachedEnemyHealthBars;
	TArray<FIntPoint> ResolutionOptions;
	TArray<float> FrameRateLimitOptions;
	ERoguePauseMenuPage PauseMenuPage = ERoguePauseMenuPage::None;
	float MasterVolume = 1.0f;
	float FrameRateLimitValue = 0.0f;
	float SmoothedFramesPerSecond = 0.0f;
	float NextHealthBarCacheRefreshTime = 0.0f;
	float NextDamageNumberProjectionRefreshTime = 0.0f;
	int32 GraphicsQualityLevel = 2;
	int32 ResolutionOptionIndex = 0;
	int32 FrameRateLimitOptionIndex = 0;
	FIntPoint DesktopResolution = FIntPoint(1920, 1080);
	EWindowMode::Type DisplayWindowMode = EWindowMode::Windowed;
	bool bGraphicsQualityCustom = false;
	bool bFrameRateLimitCustom = false;
	bool bDisplaySettingsDirty = false;

	UPROPERTY(EditDefaultsOnly, Category = "Rogue UI|Widgets")
	TSubclassOf<URoguePauseMenuWidgetBase> PauseMenuWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<URoguePauseMenuWidgetBase> PauseMenuWidget = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Rogue UI|Widgets")
	TSubclassOf<URogueShopWidgetBase> ShopWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<URogueShopWidgetBase> ShopWidget = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Rogue UI|Widgets")
	TSubclassOf<URogueUpgradeSelectionWidgetBase> UpgradeSelectionWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<URogueUpgradeSelectionWidgetBase> UpgradeSelectionWidget = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Rogue UI|Widgets")
	TSubclassOf<URogueSettingsMenuWidgetBase> SettingsMenuWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<URogueSettingsMenuWidgetBase> SettingsMenuWidget = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Rogue UI|Widgets")
	TSubclassOf<URogueDeathScreenWidgetBase> DeathScreenWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<URogueDeathScreenWidgetBase> DeathScreenWidget = nullptr;
};
