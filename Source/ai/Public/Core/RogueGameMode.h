#pragma once

#include "CoreMinimal.h"
#include "Core/RogueCombatPools.h"
#include "Core/RogueEnemyTracker.h"
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
class ARogueProjectile;
class ARogueEnemyProjectile;
class ARogueRocketProjectile;
class ARogueImpactEffect;
class ARogueLaserBeam;
class ARogueOrbitingBlade;
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
	void RegisterEnemy(ARogueEnemy* Enemy);
	void UnregisterEnemy(ARogueEnemy* Enemy);
	void RegisterExperiencePickup(ARogueExperiencePickup* Pickup);
	void UnregisterExperiencePickup(ARogueExperiencePickup* Pickup);
	ARogueEnemy* AcquireEnemy(TSubclassOf<ARogueEnemy> EnemyClass, AActor* OwnerActor, const FVector& SpawnLocation, const FRotator& SpawnRotation);
	ARogueProjectile* AcquirePlayerProjectile(TSubclassOf<ARogueProjectile> ProjectileClass, AActor* OwnerActor, APawn* InstigatorPawn, const FVector& SpawnLocation, const FRotator& SpawnRotation);
	ARogueEnemyProjectile* AcquireEnemyProjectile(TSubclassOf<ARogueEnemyProjectile> ProjectileClass, AActor* OwnerActor, APawn* InstigatorPawn, const FVector& SpawnLocation, const FRotator& SpawnRotation);
	ARogueRocketProjectile* AcquireRocketProjectile(TSubclassOf<ARogueRocketProjectile> ProjectileClass, AActor* OwnerActor, APawn* InstigatorPawn, const FVector& SpawnLocation, const FRotator& SpawnRotation);
	ARogueExperiencePickup* AcquireExperiencePickup(AActor* OwnerActor, const FVector& SpawnLocation);
	ARogueImpactEffect* AcquireImpactEffect(const FVector& SpawnLocation, const FRotator& SpawnRotation, AActor* OwnerActor);
	ARogueLaserBeam* AcquireLaserBeam(TSubclassOf<ARogueLaserBeam> BeamClass, AActor* OwnerActor, const FVector& SpawnLocation, const FRotator& SpawnRotation);
	ARogueOrbitingBlade* AcquireOrbitingBlade(TSubclassOf<ARogueOrbitingBlade> BladeClass, AActor* OwnerActor, const FVector& SpawnLocation, const FRotator& SpawnRotation);
	ARogueEnemy* FindNearestEnemyInRange(const FVector& Origin, float MaxRange, const TArray<TObjectPtr<ARogueEnemy>>& IgnoredEnemies);
	void CollectEnemiesInRange(const FVector& Origin, float Range, TArray<ARogueEnemy*>& OutEnemies, int32 MaxResults = 0, bool bSortByDistance = true);

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
	const TArray<TWeakObjectPtr<ARogueEnemy>>& GetActiveEnemies() const { return EnemyTracker.GetActiveEnemies(); }
	int32 GetActiveEnemyCount() const { return EnemyTracker.GetActiveEnemyCount(); }
	bool ShouldCullCombatEffects() const { return RuntimePerformanceSettings.CombatEffectsCullEnemyCount > 0 && EnemyTracker.GetActiveEnemyCount() >= RuntimePerformanceSettings.CombatEffectsCullEnemyCount; }

protected:
	virtual void BeginPlay() override;

private:
	void OptimizeLevelEnvironment();
	void DisableActorRendering(AActor* Actor) const;
	void SpawnEnemy();
	void SpawnBoss();
	ARogueEnemy* SpawnConfiguredEnemy(const FRogueEnemyProfile& EnemyProfile, float DesiredDistance, float MinimumDistance);
	void QueueUpgradeSelections(int32 Count, ARogueCharacter* Character);
	void OpenNextUpgradeSelection();
	FVector FindSpawnLocation(float DesiredDistance, float MinimumDistance) const;
	ERogueEnemyType PickEnemyTypeForCurrentWave() const;
	void PrewarmCombatPools();

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
	FRogueCombatPools CombatPools;

	UPROPERTY()
	FRogueShopSystem ShopSystem;

	UPROPERTY()
	TObjectPtr<URogueGameBalanceAsset> LoadedGameBalanceAsset;

	FRogueEnemyTracker EnemyTracker;
	TWeakObjectPtr<ARogueCharacter> CachedCharacter;
	TWeakObjectPtr<ARogueArena> SpawnedArena;
	TWeakObjectPtr<ARogueShopTerminal> SpawnedShopTerminal;
};
