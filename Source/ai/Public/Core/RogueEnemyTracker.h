#pragma once

#include "CoreMinimal.h"
#include "Core/RogueGameModeConfig.h"
#include "Core/RogueTypes.h"

class ARogueEnemy;
class ARogueExperiencePickup;

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
	bool bEnemySpatialHashDirty = true;
};
