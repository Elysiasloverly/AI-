// 敌人追踪子系统 —— 实现
#include "Subsystems/RogueEnemyTrackerSubsystem.h"

#include "Enemies/RogueEnemy.h"
#include "World/RogueExperiencePickup.h"

void URogueEnemyTrackerSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	EnemyTracker.TickMaintenance(DeltaTime, PerformanceSettings);
}

TStatId URogueEnemyTrackerSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(URogueEnemyTrackerSubsystem, STATGROUP_Tickables);
}

void URogueEnemyTrackerSubsystem::Configure(const FRogueRuntimePerformanceSettings& InSettings)
{
	PerformanceSettings = InSettings;
}

void URogueEnemyTrackerSubsystem::RegisterEnemy(ARogueEnemy* Enemy)
{
	EnemyTracker.RegisterEnemy(Enemy);
}

void URogueEnemyTrackerSubsystem::UnregisterEnemy(ARogueEnemy* Enemy)
{
	EnemyTracker.UnregisterEnemy(Enemy);
}

void URogueEnemyTrackerSubsystem::RegisterExperiencePickup(ARogueExperiencePickup* Pickup)
{
	EnemyTracker.RegisterPickup(Pickup);
}

void URogueEnemyTrackerSubsystem::UnregisterExperiencePickup(ARogueExperiencePickup* Pickup)
{
	EnemyTracker.UnregisterPickup(Pickup);
}

ARogueEnemy* URogueEnemyTrackerSubsystem::FindNearestEnemyInRange(const FVector& Origin, float MaxRange, const TArray<TObjectPtr<ARogueEnemy>>& IgnoredEnemies)
{
	return EnemyTracker.FindNearestEnemyInRange(Origin, MaxRange, IgnoredEnemies, PerformanceSettings);
}

void URogueEnemyTrackerSubsystem::CollectEnemiesInRange(const FVector& Origin, float Range, TArray<ARogueEnemy*>& OutEnemies, int32 MaxResults, bool bSortByDistance)
{
	EnemyTracker.CollectEnemiesInRange(Origin, Range, OutEnemies, MaxResults, bSortByDistance, PerformanceSettings);
}

ARogueExperiencePickup* URogueEnemyTrackerSubsystem::FindPickupMergeTarget(const FVector& Origin, float MaxDistance) const
{
	return EnemyTracker.FindPickupMergeTarget(Origin, MaxDistance);
}

bool URogueEnemyTrackerSubsystem::ShouldCullCombatEffects() const
{
	return PerformanceSettings.CombatEffectsCullEnemyCount > 0 && EnemyTracker.GetActiveEnemyCount() >= PerformanceSettings.CombatEffectsCullEnemyCount;
}
