// 敌人追踪子系统 —— 管理敌人注册、空间查询与性能判断
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Core/RogueEnemyTracker.h"
#include "Core/RogueGameModeConfig.h"
#include "RogueEnemyTrackerSubsystem.generated.h"

class ARogueEnemy;
class ARogueExperiencePickup;

UCLASS()
class AI_API URogueEnemyTrackerSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	// UTickableWorldSubsystem 接口
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	/** 配置性能参数（由 GameMode 在 BeginPlay 时调用） */
	void Configure(const FRogueRuntimePerformanceSettings& InSettings);

	// ---- 注册/注销 ----
	void RegisterEnemy(ARogueEnemy* Enemy);
	void UnregisterEnemy(ARogueEnemy* Enemy);
	void RegisterExperiencePickup(ARogueExperiencePickup* Pickup);
	void UnregisterExperiencePickup(ARogueExperiencePickup* Pickup);

	// ---- 空间查询 ----
	ARogueEnemy* FindNearestEnemyInRange(const FVector& Origin, float MaxRange, const TArray<TObjectPtr<ARogueEnemy>>& IgnoredEnemies);
	void CollectEnemiesInRange(const FVector& Origin, float Range, TArray<ARogueEnemy*>& OutEnemies, int32 MaxResults = 0, bool bSortByDistance = true);
	ARogueExperiencePickup* FindPickupMergeTarget(const FVector& Origin, float MaxDistance) const;

	// ---- 查询接口 ----
	const TArray<TWeakObjectPtr<ARogueEnemy>>& GetActiveEnemies() const { return EnemyTracker.GetActiveEnemies(); }
	int32 GetActiveEnemyCount() const { return EnemyTracker.GetActiveEnemyCount(); }
	bool ShouldCullCombatEffects() const;

private:
	FRogueEnemyTracker EnemyTracker;
	FRogueRuntimePerformanceSettings PerformanceSettings;
};