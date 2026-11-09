#include "Combat/Targeting/RogueEnemyTrackerSubsystem.h"
#include "Debug/RoguePerfStats.h"
// 敌人追踪子系统 —— 实现

#include "Enemies/RogueEnemy.h"
#include "World/RogueExperiencePickup.h"

void URogueEnemyTrackerSubsystem::Tick(float DeltaTime)
{
    CSV_SCOPED_TIMING_STAT(RoguePerf, TargetMaintenance);
	Super::Tick(DeltaTime);
	EnemyTracker.TickMaintenance(DeltaTime, PerformanceSettings);
    for (auto It = WeaponTargets.CreateIterator(); It; ++It)
    {
        PruneWeaponTargets(It.Value());
        if (!It.Key().IsValid() || It.Key()->IsActorBeingDestroyed() || It.Value().IsEmpty())
        {
            for (const auto& Entry : It.Value()) { ReservingWeapons.Remove(Entry.Key); }
            It.RemoveCurrent();
        }
    }
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

	// 按类型统计时线性扫描当前注册表，并过滤死亡、回池与可选的 Boss。
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


namespace
{
bool IsWeaponTargetAlive(const ARogueEnemy* Enemy)
{
    return IsValid(Enemy) && !Enemy->IsActorBeingDestroyed() && !Enemy->IsDead()
        && !Enemy->IsAvailableInPool() && Enemy->GetCurrentHealth() > 0.f;
}
}

void URogueEnemyTrackerSubsystem::PruneWeaponTargets(FWeaponReservations& Reservations)
{
    for (auto It = Reservations.CreateIterator(); It; ++It)
    {
        const ARogueEnemy* Enemy = It.Value().Enemy.Get();
        if (!It.Key().IsValid() || It.Key()->IsActorBeingDestroyed() || !IsWeaponTargetAlive(Enemy)
            || Enemy->GetSpawnCycleId() != It.Value().SpawnCycle)
        {
            ReservingWeapons.Remove(It.Key());
            It.RemoveCurrent();
        }
    }
}

ARogueEnemy* URogueEnemyTrackerSubsystem::SelectWeaponTarget(AActor* Weapon,
    const TArray<ARogueEnemy*>& Candidates, const AActor* Requester, const TArray<ARogueEnemy*>& VolleyTargets)
{
    CSV_SCOPED_TIMING_STAT(RoguePerf, TargetSelect);
    TMap<ARogueEnemy*, int32> Loads;
    if (FWeaponReservations* Reservations = WeaponTargets.Find(Weapon))
    {
        PruneWeaponTargets(*Reservations);
        for (const auto& Entry : *Reservations)
        {
            if (Entry.Key.Get() != Requester) { ++Loads.FindOrAdd(Entry.Value.Enemy.Get()); }
        }
    }
    for (ARogueEnemy* Enemy : VolleyTargets)
    {
        if (IsWeaponTargetAlive(Enemy)) { ++Loads.FindOrAdd(Enemy); }
    }
    ARogueEnemy* Best = nullptr;
    int32 BestLoad = MAX_int32;
    for (ARogueEnemy* Enemy : Candidates)
    {
        if (!IsWeaponTargetAlive(Enemy)) { continue; }
        const int32 Load = Loads.FindRef(Enemy);
        if (Load < BestLoad)
        {
            Best = Enemy; BestLoad = Load;
            if (Load == 0) { break; }
        }
    }
    return Best;
}

void URogueEnemyTrackerSubsystem::ReserveWeaponTarget(AActor* Weapon, AActor* Requester, ARogueEnemy* Enemy)
{
    ReleaseWeaponTarget(Requester);
    if (!IsValid(Weapon) || !IsValid(Requester) || !IsWeaponTargetAlive(Enemy)) { return; }
    FWeaponTargetReservation Reservation; Reservation.Enemy = Enemy; Reservation.SpawnCycle = Enemy->GetSpawnCycleId();
    WeaponTargets.FindOrAdd(Weapon).Add(Requester, Reservation);
    ReservingWeapons.Add(Requester, Weapon);
}

void URogueEnemyTrackerSubsystem::ReleaseWeaponTarget(AActor* Requester)
{
    TWeakObjectPtr<AActor> Weapon;
    if (!ReservingWeapons.RemoveAndCopyValue(Requester, Weapon)) { return; }
    if (FWeaponReservations* Reservations = WeaponTargets.Find(Weapon))
    {
        Reservations->Remove(Requester);
        if (Reservations->IsEmpty()) { WeaponTargets.Remove(Weapon); }
    }
}

void URogueEnemyTrackerSubsystem::ReleaseWeaponTargets(AActor* Weapon)
{
    if (const FWeaponReservations* Reservations = WeaponTargets.Find(Weapon))
    {
        for (const auto& Entry : *Reservations) { ReservingWeapons.Remove(Entry.Key); }
    }
    WeaponTargets.Remove(Weapon);
}

ARogueEnemy* URogueEnemyTrackerSubsystem::GetWeaponTarget(const AActor* Requester) const
{
    const auto* Weapon = ReservingWeapons.Find(const_cast<AActor*>(Requester));
    const auto* Reservations = Weapon ? WeaponTargets.Find(*Weapon) : nullptr;
    const auto* Reservation = Reservations ? Reservations->Find(const_cast<AActor*>(Requester)) : nullptr;
    ARogueEnemy* Enemy = Reservation ? Reservation->Enemy.Get() : nullptr;
    return IsWeaponTargetAlive(Enemy) && Enemy->GetSpawnCycleId() == Reservation->SpawnCycle ? Enemy : nullptr;
}
