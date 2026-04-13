#include "Core/RogueEnemyTracker.h"

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
	bEnemySpatialHashDirty = true;
	EnemySpatialHashRefreshTimer = 0.0f;
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
		bEnemySpatialHashDirty = true;
		EnemySpatialHashRefreshTimer = 0.0f;
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
		if (!IsValid(ExistingPickup) || ExistingPickup->IsActorBeingDestroyed())
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
	bEnemySpatialHashDirty = true;
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
	bEnemySpatialHashDirty = true;
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
	EnemySpatialHash.Reset();

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

	ActiveEnemyCount = ActiveEnemies.Num();
	bEnemySpatialHashDirty = false;
}

void FRogueEnemyTracker::EnsureEnemySpatialHashUpToDate(const FRogueRuntimePerformanceSettings& Settings)
{
	if (bEnemySpatialHashDirty || EnemySpatialHashRefreshTimer <= 0.0f)
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
