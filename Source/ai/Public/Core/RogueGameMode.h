#pragma once

#include "CoreMinimal.h"
#include "Core/RogueShopSystem.h"
#include "Core/RogueUpgradeSystem.h"
#include "GameFramework/GameModeBase.h"
#include "Core/RogueGameModeConfig.h"
#include "Core/RogueTypes.h"
#include "RogueGameMode.generated.h"

class ARogueCharacter;
class ARogueArena;
class ARogueEnemy;
class ARogueExperiencePickup;
class ARogueShopTerminal;
class APawn;
class URogueGameBalanceAsset;
class URogueUpgradeDefinitionAsset;
class URogueUpgradeRuleAsset;

UCLASS()
class AI_API ARogueGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ARogueGameMode();

	virtual void Tick(float DeltaSeconds) override;

	void HandleEnemyKilled(ARogueEnemy* Enemy, AActor* Killer);
	void StartUpgradeSelection(ARogueCharacter* Character);
	void TrySelectUpgrade(int32 UpgradeIndex);
	void TryOpenShop(ARogueCharacter* Character);
	void CloseShop();
	void TryBuyShopOffer(int32 OfferIndex);
	void TryRefreshShop();
	void NotifyPlayerDied();

	UFUNCTION(BlueprintPure)
	int32 GetCurrentWave() const { return RunState.CurrentWave; }

	UFUNCTION(BlueprintPure)
	int32 GetEnemiesDefeated() const { return RunState.EnemiesDefeated; }

	UFUNCTION(BlueprintPure)
	float GetRunTimeSeconds() const { return RunState.RunTimeSeconds; }

	UFUNCTION(BlueprintPure)
	bool IsAwaitingUpgradeChoice() const { return UpgradeSystem.IsAwaitingChoice(); }
	bool IsShopOpen() const { return ShopSystem.IsOpen(); }

	const TArray<FRogueUpgradeOption>& GetPendingUpgrades() const { return UpgradeSystem.GetPendingUpgrades(); }
	const TArray<FRogueShopOffer>& GetShopOffers() const { return ShopSystem.GetOffers(); }
	int32 GetShopRefreshCost() const { return ShopSystem.GetRefreshCost(); }
	float GetShopSecondsUntilRefresh() const { return ShopSystem.GetSecondsUntilAutoRefresh(); }
	bool GetShopPromptWorldLocation(FVector& OutLocation) const;

protected:
	virtual void BeginPlay() override;

private:
	void QueueUpgradeSelections(int32 Count, ARogueCharacter* Character);
	void OpenNextUpgradeSelection();

	UPROPERTY(EditDefaultsOnly, Category = "Spawning")
	TSubclassOf<ARogueEnemy> EnemyClass;

	UPROPERTY(EditDefaultsOnly, Category = "Spawning")
	TSubclassOf<ARogueExperiencePickup> ExperiencePickupClass;

	UPROPERTY(EditDefaultsOnly, Category = "Arena")
	TSubclassOf<ARogueArena> ArenaClass;

	UPROPERTY(EditDefaultsOnly, Category = "Arena")
	TSubclassOf<ARogueShopTerminal> ShopTerminalClass;

	UPROPERTY(EditDefaultsOnly, Category = "Config")
	FRogueSceneOptimizationSettings SceneOptimizationSettings;

	UPROPERTY(EditDefaultsOnly, Category = "Config")
	FRogueRuntimePerformanceSettings RuntimePerformanceSettings;

	UPROPERTY(EditDefaultsOnly, Category = "Config")
	FRoguePoolSettings PoolSettings;

	UPROPERTY(EditDefaultsOnly, Category = "Config")
	FRogueSpawnSettings SpawnSettings;

	UPROPERTY(EditDefaultsOnly, Category = "Config")
	FRogueBossSettings BossSettings;

	UPROPERTY(EditDefaultsOnly, Category = "Config")
	TSoftObjectPtr<URogueGameBalanceAsset> GameBalanceAsset;

	UPROPERTY(EditDefaultsOnly, Category = "Config")
	TSoftObjectPtr<URogueUpgradeDefinitionAsset> UpgradeDefinitionAsset;

	UPROPERTY(EditDefaultsOnly, Category = "Config")
	TSoftObjectPtr<URogueUpgradeRuleAsset> UpgradeRuleAsset;

	UPROPERTY(VisibleInstanceOnly, Category = "Run")
	FRogueRunState RunState;

	UPROPERTY()
	FRogueUpgradeSystem UpgradeSystem;

	UPROPERTY()
	FRogueShopSystem ShopSystem;

	UPROPERTY()
	TObjectPtr<URogueGameBalanceAsset> LoadedGameBalanceAsset;

	TWeakObjectPtr<ARogueCharacter> CachedCharacter;
	TWeakObjectPtr<ARogueArena> SpawnedArena;
	TWeakObjectPtr<ARogueShopTerminal> SpawnedShopTerminal;
};
