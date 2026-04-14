// 地狱塔武器 —— 持续锁定 + 伤害递增逻辑
#include "Combat/RogueWeapon_HellTower.h"

#include "Player/RogueCharacter.h"
#include "Enemies/RogueEnemy.h"
#include "Kismet/GameplayStatics.h"

void ARogueWeapon_HellTower::WeaponTick(float DeltaSeconds)
{
	const int32 EffectiveCount = GetEffectiveCount();
	if (EffectiveCount <= 0)
	{
		UpdateAccumulator = 0.0f;
		HellTowerTargets.Reset();
		HellTowerCurrentDamages.Reset();
		HellTowerDamageTickTimers.Reset();
		HellTowerBeamTimers.Reset();
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

	HellTowerTargets.SetNum(EffectiveCount);
	HellTowerCurrentDamages.SetNum(EffectiveCount);
	HellTowerDamageTickTimers.SetNum(EffectiveCount);
	HellTowerBeamTimers.SetNum(EffectiveCount);

	const FVector PlayerLocation = OwnerChar->GetActorLocation();
	const float RangeSquared = FMath::Square(Config.Range);
	TArray<TObjectPtr<ARogueEnemy>> LockedEnemies;
	LockedEnemies.Reserve(EffectiveCount);

	// 验证现有目标
	for (int32 TowerIndex = 0; TowerIndex < EffectiveCount; ++TowerIndex)
	{
		ARogueEnemy* CurrentTarget = HellTowerTargets[TowerIndex].Get();
		const bool bValid =
			IsValid(CurrentTarget) &&
			!CurrentTarget->IsDead() &&
			!CurrentTarget->IsAvailableInPool() &&
			!CurrentTarget->IsActorBeingDestroyed() &&
			FVector::DistSquared2D(PlayerLocation, CurrentTarget->GetActorLocation()) <= RangeSquared;

		if (!bValid)
		{
			HellTowerTargets[TowerIndex] = nullptr;
			HellTowerCurrentDamages[TowerIndex] = FMath::Max(1.0f, Config.BaseDamage);
			HellTowerDamageTickTimers[TowerIndex] = 0.0f;
			HellTowerBeamTimers[TowerIndex] = 0.0f;
		}
		else
		{
			HellTowerCurrentDamages[TowerIndex] = FMath::Max(HellTowerCurrentDamages[TowerIndex], FMath::Max(1.0f, Config.BaseDamage));
			LockedEnemies.Add(CurrentTarget);
		}
	}

	// 为空闲塔寻找新目标
	for (int32 TowerIndex = 0; TowerIndex < EffectiveCount; ++TowerIndex)
	{
		if (HellTowerTargets[TowerIndex].IsValid())
		{
			continue;
		}

		ARogueEnemy* NewTarget = FindNearestEnemyExcluding(PlayerLocation, LockedEnemies, Config.Range);
		if (!IsValid(NewTarget))
		{
			continue;
		}

		HellTowerTargets[TowerIndex] = NewTarget;
		HellTowerCurrentDamages[TowerIndex] = FMath::Max(1.0f, Config.BaseDamage);
		HellTowerDamageTickTimers[TowerIndex] = 0.0f;
		HellTowerBeamTimers[TowerIndex] = 0.0f;
		LockedEnemies.Add(NewTarget);
	}

	// 计算 Beam 发射位置
	FVector LateralDirection = GetOwnerCameraRightVector();
	const int32 TowerHalfCount = EffectiveCount / 2;
	const float BeamSpacing = 18.0f;
	const FVector BeamOriginBase = PlayerLocation + FVector(0.0f, 0.0f, 62.0f);
	const float EffectiveBeamRefresh = Config.BeamRefreshInterval + (bHeavy ? Config.HeavyCombatBeamRefreshPenalty : 0.0f);

	for (int32 TowerIndex = 0; TowerIndex < EffectiveCount; ++TowerIndex)
	{
		ARogueEnemy* TargetEnemy = HellTowerTargets[TowerIndex].Get();
		if (!IsValid(TargetEnemy) || TargetEnemy->IsDead() || TargetEnemy->IsAvailableInPool() || TargetEnemy->IsActorBeingDestroyed())
		{
			continue;
		}

		// 伤害 tick
		HellTowerDamageTickTimers[TowerIndex] += SimulatedDelta;
		while (HellTowerDamageTickTimers[TowerIndex] >= Config.DamageTickInterval)
		{
			HellTowerDamageTickTimers[TowerIndex] -= Config.DamageTickInterval;
			UGameplayStatics::ApplyDamage(TargetEnemy, HellTowerCurrentDamages[TowerIndex], nullptr, OwnerChar, UDamageType::StaticClass());
			HellTowerCurrentDamages[TowerIndex] += Config.DamageRampPerTick;
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
		SpawnLaserBeam(BeamOrigin, TargetLocation, true, false, EffectiveBeamRefresh + 0.03f);
		HellTowerBeamTimers[TowerIndex] = EffectiveBeamRefresh;
	}
}

void ARogueWeapon_HellTower::OnOwnerDied()
{
	HellTowerTargets.Reset();
	HellTowerCurrentDamages.Reset();
	HellTowerDamageTickTimers.Reset();
	HellTowerBeamTimers.Reset();
}

bool ARogueWeapon_HellTower::OnUpgradeApplied(ERogueUpgradeType UpgradeType, float Magnitude)
{
	// 地狱塔数量升级
	if (UpgradeType == ERogueUpgradeType::HellTowerCount)
	{
		Config.Count = FMath::Max(1, Config.Count + FMath::RoundToInt(Magnitude));
		return true;
	}
	return false;
}

void ARogueWeapon_HellTower::ApplySharedDamageBonus(float Magnitude)
{
	Config.BaseDamage += Magnitude * 0.125f;
	Config.DamageRampPerTick += Magnitude * 0.0625f;
}

void ARogueWeapon_HellTower::ApplySharedSpeedBonus(float Magnitude)
{
	Config.DamageTickInterval = FMath::Max(0.03f, Config.DamageTickInterval - Magnitude * 0.08f);
}

void ARogueWeapon_HellTower::ApplySharedRangeBonus(float Magnitude)
{
	Config.Range += Magnitude * 0.70f;
}