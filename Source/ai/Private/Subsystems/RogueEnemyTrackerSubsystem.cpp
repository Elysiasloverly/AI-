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

int32 URogueEnemyTrackerSubsystem::GetActiveEnemyCountOfType(ERogueEnemyType EnemyType, bool bIncludeBoss) const
{
	int32 Count = 0;

	// 线性遍历：刷怪频率是"每 0.2s 一次"这种级别，数组通常也就几十个元素，
	// 为它单独维护一张"按类型索引"的表不划算（维护成本 > 收益）。
	for (const TWeakObjectPtr<ARogueEnemy>& WeakEnemy : EnemyTracker.GetActiveEnemies())
	{
		const ARogueEnemy* Enemy = WeakEnemy.Get();
		if (Enemy == nullptr)
		{
			continue;   // 弱引用失效（正在被 GC / 已销毁），跳过而不是解引用
		}

		// 三个过滤条件缺一不可：
		//  IsDead()           —— 死亡帧到 Unregister 之间它还留在数组里，不能算"存活"
		//  IsAvailableInPool()—— 已回收到池里的不算场上单位（正常路径下它也已 Unregister，这里是双保险）
		//  类型 / Boss 过滤    —— 只数目标类型；Boss 默认排除，避免它占掉普通怪的名额
		if (Enemy->IsDead() || Enemy->IsAvailableInPool() || Enemy->GetEnemyType() != EnemyType)
		{
			continue;
		}

		if (!bIncludeBoss && Enemy->IsBoss())
		{
			continue;
		}

		++Count;
	}

	return Count;
}
