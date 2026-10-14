#include "Combat/Targeting/RogueEnemyTracker.h"

#include "Enemies/RogueEnemy.h"
#include "World/RogueExperiencePickup.h"

void FRogueEnemyTracker::TickMaintenance(float DeltaSeconds, const FRogueRuntimePerformanceSettings& Settings)
{
	EnemyRegistryCleanupTimer -= DeltaSeconds;
	EnemySpatialHashRefreshTimer -= DeltaSeconds;
	PickupRegistryCleanupTimer -= DeltaSeconds;

	if (EnemyRegistryCleanupTimer <= 0.0f)
	{
		CompactEnemyRegistry();
		EnemyRegistryCleanupTimer = Settings.EnemyRegistryCleanupInterval;
	}

	if (EnemySpatialHashRefreshTimer <= 0.0f)
	{
		RebuildEnemySpatialHash(Settings.EnemySpatialCellSize);
		EnemySpatialHashRefreshTimer = Settings.EnemySpatialHashRefreshInterval;
	}

	if (PickupRegistryCleanupTimer <= 0.0f)
	{
		CompactPickupRegistry();
		PickupRegistryCleanupTimer = Settings.PickupRegistryCleanupInterval;
	}
}

void FRogueEnemyTracker::RegisterEnemy(ARogueEnemy* Enemy)
{
	if (!IsValid(Enemy))
	{
		return;
	}

	if (ActiveEnemies.ContainsByPredicate([Enemy](const TWeakObjectPtr<ARogueEnemy>& ExistingEnemy)
	{
		return ExistingEnemy.Get() == Enemy;
	}))
	{
		return;
	}

	ActiveEnemies.Add(Enemy);
	++ActiveEnemyCount;
}

void FRogueEnemyTracker::UnregisterEnemy(ARogueEnemy* Enemy)
{
	if (!IsValid(Enemy))
	{
		return;
	}

	const int32 FoundIndex = ActiveEnemies.IndexOfByPredicate([Enemy](const TWeakObjectPtr<ARogueEnemy>& ExistingEnemy)
	{
		return ExistingEnemy.Get() == Enemy;
	});
	if (FoundIndex != INDEX_NONE)
	{
		ActiveEnemies.RemoveAtSwap(FoundIndex);
		ActiveEnemyCount = FMath::Max(0, ActiveEnemyCount - 1);
	}
}

void FRogueEnemyTracker::RegisterPickup(ARogueExperiencePickup* Pickup)
{
	if (!IsValid(Pickup))
	{
		return;
	}

	if (ActiveExperiencePickups.ContainsByPredicate([Pickup](const TWeakObjectPtr<ARogueExperiencePickup>& ExistingPickup)
	{
		return ExistingPickup.Get() == Pickup;
	}))
	{
		return;
	}

	ActiveExperiencePickups.Add(Pickup);
}

void FRogueEnemyTracker::UnregisterPickup(ARogueExperiencePickup* Pickup)
{
	if (!IsValid(Pickup))
	{
		return;
	}

	const int32 FoundIndex = ActiveExperiencePickups.IndexOfByPredicate([Pickup](const TWeakObjectPtr<ARogueExperiencePickup>& ExistingPickup)
	{
		return ExistingPickup.Get() == Pickup;
	});
	if (FoundIndex != INDEX_NONE)
	{
		ActiveExperiencePickups.RemoveAtSwap(FoundIndex);
	}
}

ARogueEnemy* FRogueEnemyTracker::FindNearestEnemyInRange(const FVector& Origin, float MaxRange, const TArray<TObjectPtr<ARogueEnemy>>& IgnoredEnemies, const FRogueRuntimePerformanceSettings& Settings)
{
	EnsureEnemySpatialHashUpToDate(Settings);

	ARogueEnemy* ClosestEnemy = nullptr;
	const float MaxRangeSquared = MaxRange > 0.0f ? FMath::Square(MaxRange) : FLT_MAX;
	float ClosestDistanceSquared = MaxRangeSquared;

	const FIntPoint MinCell = GetEnemySpatialCell(Origin - FVector(MaxRange, MaxRange, 0.0f), Settings.EnemySpatialCellSize);
	const FIntPoint MaxCell = GetEnemySpatialCell(Origin + FVector(MaxRange, MaxRange, 0.0f), Settings.EnemySpatialCellSize);

	for (int32 CellX = MinCell.X; CellX <= MaxCell.X; ++CellX)
	{
		for (int32 CellY = MinCell.Y; CellY <= MaxCell.Y; ++CellY)
		{
			if (TArray<TWeakObjectPtr<ARogueEnemy>>* CellEnemies = EnemySpatialHash.Find(FIntPoint(CellX, CellY)))
			{
				for (const TWeakObjectPtr<ARogueEnemy>& EnemyPtr : *CellEnemies)
				{
					ARogueEnemy* Enemy = EnemyPtr.Get();
					if (!IsValid(Enemy) || Enemy->IsDead() || Enemy->IsActorBeingDestroyed() || Enemy->IsAvailableInPool() || IgnoredEnemies.Contains(Enemy))
					{
						continue;
					}

					const float DistanceSquared = FVector::DistSquared2D(Origin, Enemy->GetActorLocation());
					if (DistanceSquared < ClosestDistanceSquared)
					{
						ClosestDistanceSquared = DistanceSquared;
						ClosestEnemy = Enemy;
					}
				}
			}
		}
	}

	return ClosestEnemy;
}

void FRogueEnemyTracker::CollectEnemiesInRange(const FVector& Origin, float Range, TArray<ARogueEnemy*>& OutEnemies, int32 MaxResults, bool bSortByDistance, const FRogueRuntimePerformanceSettings& Settings)
{
	EnsureEnemySpatialHashUpToDate(Settings);
	OutEnemies.Reset();

	const float RangeSquared = Range > 0.0f ? FMath::Square(Range) : FLT_MAX;
	const FIntPoint MinCell = GetEnemySpatialCell(Origin - FVector(Range, Range, 0.0f), Settings.EnemySpatialCellSize);
	const FIntPoint MaxCell = GetEnemySpatialCell(Origin + FVector(Range, Range, 0.0f), Settings.EnemySpatialCellSize);

	for (int32 CellX = MinCell.X; CellX <= MaxCell.X; ++CellX)
	{
		for (int32 CellY = MinCell.Y; CellY <= MaxCell.Y; ++CellY)
		{
			if (TArray<TWeakObjectPtr<ARogueEnemy>>* CellEnemies = EnemySpatialHash.Find(FIntPoint(CellX, CellY)))
			{
				for (const TWeakObjectPtr<ARogueEnemy>& EnemyPtr : *CellEnemies)
				{
					ARogueEnemy* Enemy = EnemyPtr.Get();
					if (!IsValid(Enemy) || Enemy->IsDead() || Enemy->IsActorBeingDestroyed() || Enemy->IsAvailableInPool())
					{
						continue;
					}

					if (FVector::DistSquared2D(Origin, Enemy->GetActorLocation()) <= RangeSquared)
					{
						OutEnemies.Add(Enemy);
					}
				}
			}
		}
	}

	if (bSortByDistance)
	{
		OutEnemies.Sort([&Origin](const ARogueEnemy& Left, const ARogueEnemy& Right)
		{
			return FVector::DistSquared2D(Origin, Left.GetActorLocation()) < FVector::DistSquared2D(Origin, Right.GetActorLocation());
		});
	}

	if (MaxResults > 0 && OutEnemies.Num() > MaxResults)
	{
		OutEnemies.SetNum(MaxResults, EAllowShrinking::No);
	}
}

ARogueExperiencePickup* FRogueEnemyTracker::FindPickupMergeTarget(const FVector& Origin, float MaxDistance) const
{
	ARogueExperiencePickup* MergeTargetPickup = nullptr;
	float BestDistanceSquared = FMath::Square(MaxDistance);

	for (const TWeakObjectPtr<ARogueExperiencePickup>& PickupPtr : ActiveExperiencePickups)
	{
		ARogueExperiencePickup* ExistingPickup = PickupPtr.Get();
		// ⚠️ 除了有效性，还必须排除**已经回池**的球。
		// 注册表里的注销发生在 DeactivateToPool 里，而它开头有一个
		// `if (!bPoolAvailable) return;` 的守卫 —— 也就是说"已经是池可用状态"的球
		// 不会再走一次注销，会留在注册表里直到下一次 CompactPickupRegistry。
		// 在这个窗口内合并经验，会并进一颗**已经被回收**的球：它不再 Tick、不会被拾取，
		// 那批经验就直接消失了。敌人路径（:223/:262）一直有这个检查，这里漏了。
		if (!IsValid(ExistingPickup) || ExistingPickup->IsActorBeingDestroyed()
			|| ExistingPickup->IsAvailableInPool())
		{
			continue;
		}

		const float DistanceSquared = FVector::DistSquared2D(Origin, ExistingPickup->GetActorLocation());
		if (DistanceSquared < BestDistanceSquared)
		{
			BestDistanceSquared = DistanceSquared;
			MergeTargetPickup = ExistingPickup;
		}
	}

	return MergeTargetPickup;
}

void FRogueEnemyTracker::ForceSpatialRefresh(const FRogueRuntimePerformanceSettings& Settings)
{
	EnemySpatialHashRefreshTimer = 0.0f;
	EnsureEnemySpatialHashUpToDate(Settings);
}

void FRogueEnemyTracker::CompactEnemyRegistry()
{
	for (int32 Index = ActiveEnemies.Num() - 1; Index >= 0; --Index)
	{
		ARogueEnemy* Enemy = ActiveEnemies[Index].Get();
		if (!IsValid(Enemy) || Enemy->IsDead() || Enemy->IsActorBeingDestroyed() || Enemy->IsAvailableInPool())
		{
			ActiveEnemies.RemoveAtSwap(Index);
		}
	}

	ActiveEnemyCount = ActiveEnemies.Num();
}

void FRogueEnemyTracker::CompactPickupRegistry()
{
	for (int32 Index = ActiveExperiencePickups.Num() - 1; Index >= 0; --Index)
	{
		ARogueExperiencePickup* Pickup = ActiveExperiencePickups[Index].Get();
		if (!IsValid(Pickup) || Pickup->IsActorBeingDestroyed())
		{
			ActiveExperiencePickups.RemoveAtSwap(Index);
		}
	}
}

void FRogueEnemyTracker::RebuildEnemySpatialHash(float CellSize)
{
	// 保留容量地清空每个格子。
	//
	// 为什么不是 Reset()：Reset() 会把每个格子的 TArray **连同已分配内存一起释放**，
	// 而这个哈希每 0.15s 就整体重建一次 —— 250 只怪散在几百个格子里时，
	// 每次重建是 150~250 次堆分配（约 1000~1700 次/秒），而且重建发生在
	// "某个武器/弹体发起查询"的调用栈里，会变成周期性尖峰。
	// SetNum(0, EAllowShrinking::No) 只把长度归零、保留缓冲，
	// 稳态下（格子集合基本不变）几乎不再产生分配。
	for (auto& Cell : EnemySpatialHash)
	{
		Cell.Value.SetNum(0, EAllowShrinking::No);
	}

	for (int32 Index = ActiveEnemies.Num() - 1; Index >= 0; --Index)
	{
		ARogueEnemy* Enemy = ActiveEnemies[Index].Get();
		if (!IsValid(Enemy) || Enemy->IsDead() || Enemy->IsActorBeingDestroyed() || Enemy->IsAvailableInPool())
		{
			ActiveEnemies.RemoveAtSwap(Index);
			continue;
		}

		EnemySpatialHash.FindOrAdd(GetEnemySpatialCell(Enemy->GetActorLocation(), CellSize)).Add(Enemy);
	}

	// 剔除本轮没用到的空格子：敌人会移动，格子集合会漂移，
	// 不清理的话 TMap 会随对局时间无限增长（那是真的泄漏，不只是浪费）。
	for (auto It = EnemySpatialHash.CreateIterator(); It; ++It)
	{
		if (It.Value().Num() == 0)
		{
			It.RemoveCurrent();
		}
	}

	ActiveEnemyCount = ActiveEnemies.Num();
}

void FRogueEnemyTracker::EnsureEnemySpatialHashUpToDate(const FRogueRuntimePerformanceSettings& Settings)
{
	const bool bHashMissing = ActiveEnemyCount > 0 && EnemySpatialHash.Num() == 0;
	if (bHashMissing || EnemySpatialHashRefreshTimer <= 0.0f)
	{
		RebuildEnemySpatialHash(Settings.EnemySpatialCellSize);
		EnemySpatialHashRefreshTimer = Settings.EnemySpatialHashRefreshInterval;
	}
}

FIntPoint FRogueEnemyTracker::GetEnemySpatialCell(const FVector& Location, float CellSize)
{
	const float SafeCellSize = FMath::Max(CellSize, 1.0f);
	return FIntPoint(
		FMath::FloorToInt(Location.X / SafeCellSize),
		FMath::FloorToInt(Location.Y / SafeCellSize));
}
