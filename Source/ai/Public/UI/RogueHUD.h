#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameUserSettings.h"
#include "GameFramework/HUD.h"
#include "UI/Widgets/RogueMenuWidgetBases.h"
#include "RogueHUD.generated.h"

class ARogueGameMode;
class ARoguePlayerState;
class ARogueGameState;
class ARogueCharacter;
class ARogueEnemy;
class APlayerController;
class URoguePauseMenuWidgetBase;
class URogueShopWidgetBase;
class URogueUpgradeSelectionWidgetBase;
class URogueSettingsMenuWidgetBase;
class URogueDeathScreenWidgetBase;
class URogueReviveCountdownWidgetBase;
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

	/** 生成飘字时缓存文本，避免逐帧重复格式化；投影、位置更新和绘制仍按各自路径执行。 */
	FString CachedText;
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
	ARogueHUD();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void DrawHUD() override;
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
	friend class FRogueReviveCountdownVisibilityTest;
	friend struct FRoguePlayerLifeCycleTestAccess;

	void RefreshEnemyHealthBarCache(const URogueEnemyTrackerSubsystem* Tracker);
	void RefreshDamageNumberProjectionCache(APlayerController* PlayerController, const URogueEnemyTrackerSubsystem* Tracker);
	void DrawEnemyHealthBars(UFont* Font);
	void DrawDamageNumbers(UFont* Font);
	void InitializeMenuWidgets();
	/** UMG 状态不依赖画布绘制，濒死与复活在本机每帧同步。 */
	void RefreshMenuWidgets();

	/** 整局运行状态。与 GameMode 不同，GameState 在客户端也存在（复制副本），
	 *  所以客户端 HUD 读波次/击杀/存活时间必须走这里。 */
	const ARogueGameState* GetRogueGameState() const;

	/** 本地玩家的 PlayerState。联机下这是客户端能拿到的、与"我这个玩家"相关的状态载体
	 *  （GameMode 在客户端拿不到 —— GetAuthGameMode 返回 nullptr）。
	 *  返回非 const：HUD 需要通过它发起 ServerRPC（买/刷新/关商店/选升级）。 */
	ARoguePlayerState* GetLocalRoguePlayerState() const;
	/** 商店视图：**全部**数据来自本地 PlayerState / 本地角色（客户端可读）。
	 *  注意不要再引入 GameMode 依赖 —— 客户端拿不到它，会导致商店在客户端永远是空的。 */
	void UpdateShopWidget(const ARogueCharacter* PlayerCharacter);
	void UpdateUpgradeSelectionWidget();
	void UpdatePauseWidget();
	void UpdateSettingsWidget();
	void UpdateDeathWidget();

	/** 每帧按本地复制状态推导输入模式：
	 *  暂停菜单 / 商店 / 待选升级卡 / 濒死 → 显示光标 + GameAndUI；团灭 → UIOnly。
	 *  必须在本地机器上推导：服务端改 SetInputMode 只会改它自己那份 PlayerController，
	 *  对真正需要切光标的客户端一点用都没有（这就是"鼠标还在转视角、点不了卡片"的原因）。 */
	void UpdateInteractionInputMode(const ARogueCharacter* PlayerCharacter);
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
	FString LastShopWidgetSignature;
	FString LastUpgradeWidgetSignature;
	FString LastPauseWidgetSignature;
	FString LastSettingsWidgetSignature;
	FString LastDeathWidgetSignature;
	int32 LastShopAutoRefreshSeconds = INDEX_NONE;
	/** 每个本地 HUD 独立缓存，避免同进程 PIE / 分屏玩家互相跳过界面更新。 */
	int32 LastReviveCountdownSeconds = INDEX_NONE;
	bool bLastReviveCountdownVisible = false;

	/** 上一次应用过的输入模式（0=GameOnly，1=GameAndUI，2=团灭 UIOnly）。
	 *  初值 -1 保证第一帧应用输入模式。 */
	int32 LastAppliedUIInputState = -1;

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

	/** 濒死期间的复活界面，默认使用 WBP_ResurrectionScreen；蓝图可覆盖样式。 */
	UPROPERTY(EditDefaultsOnly, Category = "Rogue UI|Widgets")
	TSubclassOf<URogueReviveCountdownWidgetBase> ReviveCountdownWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<URogueReviveCountdownWidgetBase> ReviveCountdownWidget = nullptr;

	/** 濒死且本局未结束时可见，零秒不自行推断服务器已经复活。 */
	void UpdateReviveCountdownWidget(const ARogueCharacter* PlayerCharacter);
};
