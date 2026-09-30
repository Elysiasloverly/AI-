#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameUserSettings.h"
#include "GameFramework/HUD.h"
#include "UI/RogueMenuWidgetBases.h"
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

	/** 飘字文本，**生成时算一次**。
	 *  为什么不每帧 Printf：数值生成后就固定不变，而屏幕上最多同时有 42 条飘字，
	 *  每帧拼 42 个 FString = 每秒 2500 次堆分配（重火力下敌人多时更明显）。
	 *  缓存后每帧只剩下两次 DrawText。 */
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
	virtual void BeginPlay() override;
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
	void RefreshEnemyHealthBarCache(const URogueEnemyTrackerSubsystem* Tracker);
	void RefreshDamageNumberProjectionCache(APlayerController* PlayerController, const URogueEnemyTrackerSubsystem* Tracker);
	void DrawEnemyHealthBars(UFont* Font);
	void DrawDamageNumbers(UFont* Font);
	void InitializeMenuWidgets();

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

	/** 每帧按**本地可见的复制状态**推导输入模式：
	 *  暂停菜单 / 商店 / 待选升级卡 / 已阵亡 → 显示光标 + GameAndUI；其余 → GameOnly。
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

	/** 上一次应用过的输入模式（0=GameOnly，1=UI 可见光标）。
	 *  三态：初值 -1 表示"还没应用过"，保证第一帧一定落一次。 */
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

	/** 复活倒计时界面（倒地等待时显示；与死亡结算界面分开）。
	 *  在 BP_RogueHUD 的 Class Defaults 里指定你的 WBP 即可。 */
	UPROPERTY(EditDefaultsOnly, Category = "Rogue UI|Widgets")
	TSubclassOf<URogueReviveCountdownWidgetBase> ReviveCountdownWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<URogueReviveCountdownWidgetBase> ReviveCountdownWidget = nullptr;

	/** 每帧更新复活倒计时界面：倒下且复活未到期时可见。 */
	void UpdateReviveCountdownWidget(const ARogueCharacter* PlayerCharacter);
};
