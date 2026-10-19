#pragma once

#include "CoreMinimal.h"
#include "Framework/RogueGameModeConfig.h"
#include "Shared/RogueTypes.h"

class ARogueEnemy;
class ARogueExperiencePickup;

/** 活跃对象注册表与二维格子缓存。重建遍历活跃敌人及已有桶；
 *  查询遍历覆盖格子及其中候选，密集或大范围查询可能接近全表扫描。
 *  格子按配置间隔刷新；期间对象移动/新注册可造成候选遗漏，并非实时物理查询。 */
struct AI_API FRogueEnemyTracker
{
public:
	void TickMaintenance(float DeltaSeconds, const FRogueRuntimePerformanceSettings& Settings);
	void RegisterEnemy(ARogueEnemy* Enemy);
	void UnregisterEnemy(ARogueEnemy* Enemy);
	void RegisterPickup(ARogueExperiencePickup* Pickup);
	void UnregisterPickup(ARogueExperiencePickup* Pickup);
	ARogueEnemy* FindNearestEnemyInRange(const FVector& Origin, float MaxRange, const TArray<TObjectPtr<ARogueEnemy>>& IgnoredEnemies, const FRogueRuntimePerformanceSettings& Settings);
	void CollectEnemiesInRange(const FVector& Origin, float Range, TArray<ARogueEnemy*>& OutEnemies, int32 MaxResults, bool bSortByDistance, const FRogueRuntimePerformanceSettings& Settings);
	ARogueExperiencePickup* FindPickupMergeTarget(const FVector& Origin, float MaxDistance) const;
	void ForceSpatialRefresh(const FRogueRuntimePerformanceSettings& Settings);

	const TArray<TWeakObjectPtr<ARogueEnemy>>& GetActiveEnemies() const { return ActiveEnemies; }
	int32 GetActiveEnemyCount() const { return ActiveEnemyCount; }

private:
	void CompactEnemyRegistry();
	void CompactPickupRegistry();
	void RebuildEnemySpatialHash(float CellSize);
	void EnsureEnemySpatialHashUpToDate(const FRogueRuntimePerformanceSettings& Settings);
	static FIntPoint GetEnemySpatialCell(const FVector& Location, float CellSize);

	TArray<TWeakObjectPtr<ARogueEnemy>> ActiveEnemies;
	TArray<TWeakObjectPtr<ARogueExperiencePickup>> ActiveExperiencePickups;
	TMap<FIntPoint, TArray<TWeakObjectPtr<ARogueEnemy>>> EnemySpatialHash;
	float EnemyRegistryCleanupTimer = 0.0f;
	float EnemySpatialHashRefreshTimer = 0.0f;
	float PickupRegistryCleanupTimer = 0.0f;
	int32 ActiveEnemyCount = 0;
};
