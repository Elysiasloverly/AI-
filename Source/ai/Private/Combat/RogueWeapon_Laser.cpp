// 激光武器 —— 激光爆发 + 折射链逻辑
#include "Combat/RogueWeapon_Laser.h"
#include "AbilitySystem/RogueAttributeSet.h"

#include "Player/RogueCharacter.h"
#include "Enemies/RogueEnemy.h"
#include "AbilitySystem/RogueAbilitySystemLibrary.h"
#include "Core/RogueGameplayTags.h"
#include "Kismet/GameplayStatics.h"

void ARogueWeapon_Laser::WeaponTick(float DeltaSeconds)
{
	if (GetEffectiveCount() <= 0)
	{
		return;
	}

	LaserTimer -= DeltaSeconds;
	if (LaserTimer > 0.0f)
	{
		return;
	}

	TArray<ARogueEnemy*> Enemies;
	CollectEnemiesInRange(GetEffectiveRange(), Enemies, FMath::Max(8, GetEffectiveCount() * 3));
	if (Enemies.Num() == 0)
	{
		return;
	}

	FireLaserBurst(Enemies);
	LaserTimer = GetEffectiveCooldown();
}

// 生效数值：激光的换算系数（攻速 ×2.0/下限 0.35、折射范围 ×0.45）沿用自研版本。
// 升级只改玩家 ASC 上的加成属性，这里负责"基础值 + 加成×本武器系数"。

float ARogueWeapon_Laser::GetEffectiveCooldown() const
{
	const FRogueWeaponBonusValues& Bonuses = GetWeaponBonusValues();
	const float Bonus = Bonuses.AttackSpeed;
	return FMath::Max(0.35f, GetConfig().Cooldown - Bonus * 2.0f);
}

float ARogueWeapon_Laser::GetEffectiveRefractionRange() const
{
	const FRogueWeaponBonusValues& Bonuses = GetWeaponBonusValues();
	const float Bonus = Bonuses.AttackRange;
	return GetConfig().RefractionRange + Bonus * 0.45f;
}

void ARogueWeapon_Laser::FireLaserBurst(const TArray<ARogueEnemy*>& Enemies)
{
	ARogueCharacter* OwnerChar = GetOwnerCharacter();
	if (OwnerChar == nullptr)
	{
		return;
	}

	const FVector BeamOriginBase = OwnerChar->GetActorLocation() + FVector(0.0f, 0.0f, 70.0f);
	const int32 EffectiveCount = GetEffectiveCount();
	const FVector LateralDirection = GetOwnerCameraRightVector();
	const int32 HalfCount = EffectiveCount / 2;
	const float BeamSpacing = 34.0f;

	for (int32 LaserIndex = 0; LaserIndex < EffectiveCount; ++LaserIndex)
	{
		if (!Enemies.IsValidIndex(LaserIndex % Enemies.Num()) || !IsValid(Enemies[LaserIndex % Enemies.Num()]))
		{
			continue;
		}

		ARogueEnemy* TargetEnemy = Enemies[LaserIndex % Enemies.Num()];
		const float OffsetIndex = static_cast<float>(LaserIndex - HalfCount);
		const float LateralOffset = EffectiveCount % 2 == 0 ? (OffsetIndex + 0.5f) * BeamSpacing : OffsetIndex * BeamSpacing;
		const FVector BeamOrigin = BeamOriginBase + LateralDirection * LateralOffset;
		const FVector TargetLocation = TargetEnemy->GetActorLocation() + FVector(0.0f, 0.0f, 40.0f);
		RogueAbilitySystem::ApplyDamageToTarget(OwnerChar, TargetEnemy, GetEffectiveDamage(), RogueGameplayTags::Damage_Source_Beam);
		SpawnLaserBeam(BeamOrigin, TargetLocation);

		if (GetEffectiveRefractionCount() > 0)
		{
			FireLaserRefractionChain(TargetEnemy, TargetLocation, GetEffectiveDamage() * 0.5f);
		}
	}
}

void ARogueWeapon_Laser::FireLaserRefractionChain(ARogueEnemy* InitialTarget, const FVector& InitialImpactLocation, float InitialDamage)
{
	ARogueCharacter* OwnerChar = GetOwnerCharacter();
	if (!IsValid(InitialTarget) || GetEffectiveRefractionCount() <= 0 || InitialDamage <= 0.0f || OwnerChar == nullptr)
	{
		return;
	}

	TArray<TObjectPtr<ARogueEnemy>> HitEnemies;
	HitEnemies.Add(InitialTarget);

	ARogueEnemy* CurrentTarget = InitialTarget;
	FVector CurrentStartLocation = InitialImpactLocation;
	float CurrentDamage = InitialDamage;

	for (int32 RefractionIndex = 0; RefractionIndex < GetEffectiveRefractionCount(); ++RefractionIndex)
	{
		if (!IsValid(CurrentTarget) || CurrentDamage <= 1.0f)
		{
			break;
		}

		ARogueEnemy* NextTarget = FindNearestEnemyExcluding(CurrentTarget->GetActorLocation(), HitEnemies, GetEffectiveRefractionRange());
		if (!IsValid(NextTarget))
		{
			break;
		}

		const FVector NextImpactLocation = NextTarget->GetActorLocation() + FVector(0.0f, 0.0f, 40.0f);
		// 折射段：同样属于光束伤害
		RogueAbilitySystem::ApplyDamageToTarget(OwnerChar, NextTarget, CurrentDamage, RogueGameplayTags::Damage_Source_Beam);
		SpawnLaserBeam(CurrentStartLocation, NextImpactLocation);

		HitEnemies.Add(NextTarget);
		CurrentTarget = NextTarget;
		CurrentStartLocation = NextImpactLocation;
	}
}

void ARogueWeapon_Laser::PlayCosmeticFire(const FRogueWeaponCosmeticFire& Fire)
{
	// 光束没有弹体要复现，要的只是"从哪到哪再画一条线"（含折射的每一段，
	// 服务端每段都会各自广播一次）。
	// 这里**不要**补 BroadcastCosmeticFire：SpawnLaserBeam 内部已经广播过了，
	// 而且本函数本来就只在不权威的客户端上跑。
	PlayCosmeticBeam(Fire);
}
