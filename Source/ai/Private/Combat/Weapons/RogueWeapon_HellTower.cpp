// 地狱塔武器 —— 持续锁定 + 伤害递增逻辑
#include "Combat/Weapons/RogueWeapon_HellTower.h"
#include "Combat/Visuals/RogueLaserBeam.h"
#include "Combat/Visuals/RogueInfernoLockSubsystem.h"
#include "Combat/Visuals/RogueInfernoLockRing.h"
#include "Engine/World.h"
#include "AbilitySystem/Abilities/RogueWeaponAbility.h"
#include "AbilitySystem/RogueAttributeSet.h"

#include "Player/RogueCharacter.h"
#include "Enemies/RogueEnemy.h"
#include "AbilitySystem/RogueAbilitySystemLibrary.h"
#include "AbilitySystem/RogueGameplayTags.h"
#include "Kismet/GameplayStatics.h"

ARogueWeapon_HellTower::ARogueWeapon_HellTower()
{
	FireAbilityClass = URogueWeaponAbility_HellTower::StaticClass();
}

// 注：本函数由 URogueWeaponAbility_HellTower 通过逐帧心跳调用（见 UsesTargetedFireLoop 的说明）。
// 它**不再**由 ARogueCharacter::TickWeapons 调用 —— 后者的职责已经缩小到"持续型表现武器"。
void ARogueWeapon_HellTower::WeaponTick(float DeltaSeconds)
{
	const int32 EffectiveCount = GetEffectiveCount();
	if (EffectiveCount <= 0)
	{
        ReleaseAllLocks();
		UpdateAccumulator = 0.0f;
		HellTowerTargets.Reset();
		HellTowerTargetSpawnIds.Reset();
		HellTowerCurrentDamages.Reset();
		HellTowerDamageTickTimers.Reset();
		HellTowerBeamTimers.Reset();
        HellTowerLockAges.Reset();
		return;
	}

	ARogueCharacter* OwnerChar = GetOwnerCharacter();
	if (OwnerChar == nullptr)
	{
		return;
	}

	const bool bHeavy = IsHeavyCombat();
	const float UpdateStep = bHeavy ? Config.HeavyCombatUpdateInterval : Config.UpdateInterval;
	UpdateAccumulator += DeltaSeconds;
	if (UpdateAccumulator < UpdateStep)
	{
		return;
	}

	const float SimulatedDelta = FMath::Min(UpdateAccumulator, UpdateStep * 2.0f);
	UpdateAccumulator = 0.0f;

    TSet<ARogueEnemy*> PreviousTargets;
    for(const auto& T:HellTowerTargets)if(T.IsValid())PreviousTargets.Add(T.Get());
	HellTowerTargets.SetNum(EffectiveCount);
	HellTowerTargetSpawnIds.SetNum(EffectiveCount);
	HellTowerCurrentDamages.SetNum(EffectiveCount);
	HellTowerDamageTickTimers.SetNum(EffectiveCount);
	HellTowerBeamTimers.SetNum(EffectiveCount);
    HellTowerLockAges.SetNum(EffectiveCount);

	const FVector PlayerLocation = OwnerChar->GetActorLocation();
	const float RangeSquared = FMath::Square(GetEffectiveRange());
	TArray<TObjectPtr<ARogueEnemy>> LockedEnemies;
	LockedEnemies.Reserve(EffectiveCount);

	// 验证现有目标
	for (int32 TowerIndex = 0; TowerIndex < EffectiveCount; ++TowerIndex)
	{
		ARogueEnemy* CurrentTarget = HellTowerTargets[TowerIndex].Get();
		const bool bValid =
			IsValid(CurrentTarget) &&
			HellTowerTargetSpawnIds[TowerIndex] == CurrentTarget->GetSpawnCycleId() &&
			!CurrentTarget->IsDead() &&
			!CurrentTarget->IsAvailableInPool() &&
			!CurrentTarget->IsActorBeingDestroyed() &&
			FVector::DistSquared2D(PlayerLocation, CurrentTarget->GetActorLocation()) <= RangeSquared;

		if (!bValid)
		{
			HellTowerTargets[TowerIndex] = nullptr;
			HellTowerTargetSpawnIds[TowerIndex].Invalidate();
			HellTowerCurrentDamages[TowerIndex] = FMath::Max(FMath::Max(0.f, Config.HellTowerMinDamage), GetEffectiveBaseDamage());
			HellTowerDamageTickTimers[TowerIndex] = 0.0f;
			HellTowerBeamTimers[TowerIndex] = 0.0f;
            HellTowerLockAges[TowerIndex]=0;
		}
		else
		{
			HellTowerCurrentDamages[TowerIndex] = FMath::Max(HellTowerCurrentDamages[TowerIndex], FMath::Max(FMath::Max(0.f, Config.HellTowerMinDamage), GetEffectiveBaseDamage()));
			LockedEnemies.Add(CurrentTarget);
		}
	}

    // 保留持续锁定的伤害递增。目标不足时允许共用，出现空闲目标后再拆开重复锁定。
    TSet<ARogueEnemy*> SeenTargets;
    TArray<ARogueEnemy*> Candidates;
    bool bCollectedCandidates=false;
    for (int32 TowerIndex=0;TowerIndex<EffectiveCount;++TowerIndex)
    {
        ARogueEnemy* CurrentTarget=HellTowerTargets[TowerIndex].Get();
        ARogueEnemy* NewTarget=nullptr;
        if (CurrentTarget)
        {
            if (SeenTargets.Contains(CurrentTarget))
            {
                if (!bCollectedCandidates) { CollectEnemiesInRange(GetEffectiveRange(),Candidates); bCollectedCandidates=true; }
                for (ARogueEnemy* Enemy:Candidates)
                { if (!LockedEnemies.Contains(Enemy)) { NewTarget=Enemy; break; } }
            }
            if (!NewTarget) { SeenTargets.Add(CurrentTarget); continue; }
            LockedEnemies.RemoveSingle(CurrentTarget);
        }
        else
        {
            if (!bCollectedCandidates) { CollectEnemiesInRange(GetEffectiveRange(),Candidates); bCollectedCandidates=true; }
            TArray<ARogueEnemy*> Allocated;
            for (ARogueEnemy* Enemy:LockedEnemies) { Allocated.Add(Enemy); }
            NewTarget=SelectDistributedTarget(Candidates,Allocated);
        }
        if (!NewTarget) { continue; }
        HellTowerTargets[TowerIndex]=NewTarget;
        HellTowerTargetSpawnIds[TowerIndex]=NewTarget->GetSpawnCycleId();
        HellTowerCurrentDamages[TowerIndex]=FMath::Max(FMath::Max(0.f,Config.HellTowerMinDamage),GetEffectiveBaseDamage());
        HellTowerDamageTickTimers[TowerIndex]=0;
        HellTowerBeamTimers[TowerIndex]=0;
        HellTowerLockAges[TowerIndex]=0;
        LockedEnemies.Add(NewTarget);
        SeenTargets.Add(NewTarget);
    }

    for(ARogueEnemy* Old:PreviousTargets)
        if(!HellTowerTargets.ContainsByPredicate([Old](const auto& Current){return Current.Get()==Old;}))ReleaseLock(Old);

	// 计算 Beam 发射位置
	FVector LateralDirection = GetOwnerCameraRightVector();
	const int32 TowerHalfCount = EffectiveCount / 2;
	const float BeamSpacing = 18.0f;
	const FVector BeamOriginBase = PlayerLocation + FVector(0.0f, 0.0f, 62.0f);
	const float EffectiveBeamRefresh = FMath::Max(.01f, GetBeamVisualDefaults()->RefreshInterval) + (bHeavy ? FMath::Max(0.f, GetBeamVisualDefaults()->HeavyCombatRefreshPenalty) : 0.0f);
	bool bAudioTagged = false;

	for (int32 TowerIndex = 0; TowerIndex < EffectiveCount; ++TowerIndex)
	{
		ARogueEnemy* TargetEnemy = HellTowerTargets[TowerIndex].Get();
		if (!IsValid(TargetEnemy) || TargetEnemy->IsDead() || TargetEnemy->IsAvailableInPool() || TargetEnemy->IsActorBeingDestroyed())
		{
			continue;
		}

        HellTowerLockAges[TowerIndex]+=SimulatedDelta;
		// 伤害 tick
		// ⚠️ 防御：DamageTickInterval 来自 DataTable，若被填成 0（批量填表时很常见），
		//    原来的 `while (Timer >= Interval) { Timer -= Interval; }` 会因"条件恒真 + 自减 0"
		//    变成同帧死循环、直接卡死主线程。这里钳到下限，保证每轮至少推进 0.01s。
		const float SafeDamageTickInterval = FMath::Max(GetEffectiveDamageTickInterval(), 0.01f);
		HellTowerDamageTickTimers[TowerIndex] += SimulatedDelta;
		while (HellTowerDamageTickTimers[TowerIndex] >= SafeDamageTickInterval)
		{
			HellTowerDamageTickTimers[TowerIndex] -= SafeDamageTickInterval;
			// 地狱塔是持续光束伤害，Tag 用 Damage.Source.Beam
			RogueAbilitySystem::ApplyDamageToTarget(OwnerChar, TargetEnemy, HellTowerCurrentDamages[TowerIndex], RogueGameplayTags::Damage_Source_Beam);
			HellTowerCurrentDamages[TowerIndex] += GetEffectiveDamageRampPerTick();
		}

		// Beam 刷新
		HellTowerBeamTimers[TowerIndex] -= SimulatedDelta;
		if (HellTowerBeamTimers[TowerIndex] > 0.0f)
		{
			continue;
		}

		const float OffsetIndex = static_cast<float>(TowerIndex - TowerHalfCount);
		const float LateralOffset = EffectiveCount % 2 == 0 ? (OffsetIndex + 0.5f) * BeamSpacing : OffsetIndex * BeamSpacing;
		const FVector BeamOrigin = BeamOriginBase + LateralDirection * LateralOffset;
		const FVector TargetLocation = TargetEnemy->GetActorLocation() + FVector(0.0f, 0.0f, 42.0f);
        FRogueBeamLockCosmetic Lock;Lock.Target=TargetEnemy;Lock.Generation=TargetEnemy->GetPresentationGeneration();Lock.LockedSeconds=HellTowerLockAges[TowerIndex];
        SpawnLaserBeam(BeamOrigin, TargetLocation, true, false, EffectiveBeamRefresh + FMath::Max(0.f, GetBeamVisualDefaults()->LifetimePadding), !bAudioTagged,Lock);
		bAudioTagged = true;
		HellTowerBeamTimers[TowerIndex] = EffectiveBeamRefresh;
	}
}

void ARogueWeapon_HellTower::OnOwnerDied()
{
    ReleaseAllLocks();
    if(auto* Rings=GetWorld()?GetWorld()->GetSubsystem<URogueInfernoLockSubsystem>():nullptr)Rings->RemoveSource(this);
	Super::OnOwnerDied();
	HellTowerTargets.Reset();
	HellTowerTargetSpawnIds.Reset();
	UpdateAccumulator = 0.0f;
	HellTowerCurrentDamages.Reset();
	HellTowerDamageTickTimers.Reset();
	HellTowerBeamTimers.Reset();
        HellTowerLockAges.Reset();
}

// 生效数值：基础伤害、递增伤害、结算间隔与范围的换算系数读取本武器配置；跳间隔最低0.03秒。
// 加成输入来自角色的 GAS 属性快照。

float ARogueWeapon_HellTower::GetEffectiveBaseDamage() const
{
	const FRogueWeaponBonusValues& Bonuses = GetWeaponBonusValues();
	const float Bonus = Bonuses.AttackPower;
	return GetConfig().BaseDamage + Bonus * GetConfig().BaseDamageBonusScale;
}

float ARogueWeapon_HellTower::GetEffectiveDamageRampPerTick() const
{
	const FRogueWeaponBonusValues& Bonuses = GetWeaponBonusValues();
	const float Bonus = Bonuses.AttackPower;
	return FMath::Max(0.0f, GetConfig().DamageRampPerTick + Bonus * GetConfig().DamageRampBonusScale + Bonuses.HellTowerDamageRamp);
}

float ARogueWeapon_HellTower::GetEffectiveDamageTickInterval() const
{
	const FRogueWeaponBonusValues& Bonuses = GetWeaponBonusValues();
	const float Bonus = Bonuses.AttackSpeed;
	return FMath::Max(FMath::Max(.01f,Config.HellTowerMinTickInterval), GetConfig().DamageTickInterval - Bonus * GetConfig().DamageTickIntervalReductionScale);
}

float ARogueWeapon_HellTower::GetEffectiveRange() const
{
	const FRogueWeaponBonusValues& Bonuses = GetWeaponBonusValues();
	const float Bonus = Bonuses.AttackRange;
	return GetConfig().Range + Bonus * GetConfig().HellTowerRangeBonusScale;
}

void ARogueWeapon_HellTower::PlayCosmeticFire(const FRogueWeaponCosmeticFire& Fire)
{
    if(Fire.Lock.Target&&Fire.Lock.LockedSeconds<0)return;
	// 地狱塔每刷新一次光束，服务端的 SpawnLaserBeam 就会广播一条新的起终点，
	// 队友那边照着重画即可 —— 锁定目标、伤害递增这些状态都不用复现，
	// 因为光束只是"这一段时间里连到了哪"的画面。
	// 这里**不要**补 BroadcastCosmeticFire：SpawnLaserBeam 内部已经广播过了。
	//
	// 注意参数要与权威那一次**逐项对齐**（RogueWeapon_HellTower.cpp 里的权威调用）：
	// 权威是 bInfernoStyle=true、bImpactEffect=**false**。
	// 不能图省事转调 PlayCosmeticBeam —— 基类那个辅助函数固定 bImpactEffect=true，
	// 会让队友那一屏每段光束都多出一个命中特效（本人和主机都看不到，最难对比的那种不一致）。
	SpawnLaserBeam(Fire.Start, Fire.End, /*bInfernoStyle=*/true, /*bImpactEffect=*/false, Fire.Param);
}

void ARogueWeapon_HellTower::UpdateCosmeticLock(const FRogueWeaponCosmeticFire& Fire)
{
    auto* Target=Cast<ARogueEnemy>(Fire.Lock.Target.Get());
    if(!GetWorld()||!Target)return;
    auto* Rings=GetWorld()->GetSubsystem<URogueInfernoLockSubsystem>();if(!Rings)return;
    if(Fire.Lock.LockedSeconds<0){Rings->Release(this,Target,Fire.Lock.Generation);return;}
    const auto Class=GetBeamVisualDefaults()->InfernoLockRingClass;
    if(!Class)return;
    const float Lease=FMath::Max(Class.GetDefaultObject()->RefreshGrace,Fire.Param+.35f);
    Rings->Touch(this,Target,Fire.Lock.Generation,Fire.Lock.LockedSeconds,Class,Lease);
}
void ARogueWeapon_HellTower::ReleaseLock(ARogueEnemy* Target)
{
    const auto* Player=GetOwnerCharacter();if(!Target||!Player||!Player->IsAuthoritySide())return;
    FRogueWeaponCosmeticFire Fire;Fire.bAlternateStyle=true;Fire.Lock.Target=Target;
    Fire.Lock.Generation=Target->GetPresentationGeneration();Fire.Lock.LockedSeconds=-1;
    UpdateCosmeticLock(Fire);BroadcastCosmeticFire(Fire);
}
void ARogueWeapon_HellTower::ReleaseAllLocks()
{
    TSet<ARogueEnemy*> Unique;
    for(const auto& Target:HellTowerTargets)if(Target.IsValid())Unique.Add(Target.Get());
    for(auto* Target:Unique)ReleaseLock(Target);
}
