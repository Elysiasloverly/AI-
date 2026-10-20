
#include "Enemies/Data/RogueEnemyDataTable.h"

// TSoftObjectPtr<URogueAbilitySet> 的赋值需要**完整类型**（前置声明不够）。
#include "AbilitySystem/RogueAbilitySet.h"

FRogueEnemyArchetype FRogueEnemyArchetypeRow::ToArchetype(bool bIsBoss) const
{
	FRogueEnemyArchetype Archetype = RogueEnemyArchetypes::BuildEnemyArchetype(EnemyType, bIsBoss);

	// 移动模型由 EnemyType 的基线决定；数据表只覆盖移动倍率，避免每行都要重复填模型。
	Archetype.Movement.BaseMoveMultiplier = BaseMoveMultiplier;

	// 攻击能力集（数据驱动）。留空时 ARogueEnemy::GrantAttackAbilities 会回退到
	// 按远近/接触标志推导的硬编码逻辑 —— 所以老数据表不动也能跑。
	Archetype.AttackAbilitySet = AttackAbilitySet;

	// 根据敌人类型填充专属属性
	switch (EnemyType)
	{
	case ERogueEnemyType::Assault:
		{
			const FRogueChargerSpecialParams& P = SpecialParams.Charger;
			Archetype.Movement.BurstCycleTime = P.BurstCycleTime;
			Archetype.Movement.BurstDuration = P.BurstDuration;
			Archetype.Movement.BurstActiveMultiplier = P.BurstActiveMultiplier;
			Archetype.Movement.BurstIdleMultiplier = P.BurstIdleMultiplier;
		}
		break;

	case ERogueEnemyType::Juggernaut:
		// 坦克没有专属参数；BaseMoveMultiplier 在 switch 前已统一写入。
		break;

	case ERogueEnemyType::Orbiter:
		{
			const FRogueOrbiterSpecialParams& P = SpecialParams.Orbiter;
			Archetype.Movement.FarDistance = P.FarDistance;
			Archetype.Movement.CloseDistance = P.CloseDistance;
			Archetype.Movement.FarTowardWeight = P.FarTowardWeight;
			Archetype.Movement.FarTangentWeight = P.FarTangentWeight;
			Archetype.Movement.CloseTowardWeight = P.CloseTowardWeight;
			Archetype.Movement.CloseTangentWeight = P.CloseTangentWeight;
			Archetype.Movement.MidTowardWeight = P.MidTowardWeight;
			Archetype.Movement.MidTangentWeight = P.MidTangentWeight;
		}
		break;

	case ERogueEnemyType::Sniper:
	case ERogueEnemyType::Repeater:
	case ERogueEnemyType::Marksman:
		{
			const FRogueShooterSpecialParams& P = SpecialParams.Shooter;
			float EffectivePreferredRange = P.PreferredRange;
			float EffectiveCloseDistance = P.CloseDistance;
			float EffectiveNextShotCooldown = P.NextShotCooldown;
			float EffectiveProjectileSpeed = P.ProjectileSpeed;
			float EffectiveProjectileDamageMultiplier = P.ProjectileDamageMultiplier;
			bool bEffectiveLargeVisual = P.bLargeVisual;
			bool bEffectiveMuzzleEffect = P.bSpawnMuzzleEffect;

			if (bIsBoss && BossOverride.bEnableBossOverride)
			{
				if (BossOverride.BossPreferredRange > 0.0f) EffectivePreferredRange = BossOverride.BossPreferredRange;
				if (BossOverride.BossCloseDistance > 0.0f) EffectiveCloseDistance = BossOverride.BossCloseDistance;
				if (BossOverride.BossNextShotCooldown > 0.0f) EffectiveNextShotCooldown = BossOverride.BossNextShotCooldown;
				if (BossOverride.BossProjectileSpeed > 0.0f) EffectiveProjectileSpeed = BossOverride.BossProjectileSpeed;
				if (BossOverride.BossProjectileDamageMultiplier > 0.0f) EffectiveProjectileDamageMultiplier = BossOverride.BossProjectileDamageMultiplier;
				// ⚠️ bool 没有"未设置"的哨兵值，所以不能像上面那些 float 一样用 `> 0` 保护。
				//    无条件赋值会让**没配这两列的 Boss** 把基础值一起清掉：
				//    Artillery 的枪口特效、Shooter/Spitter 的放大视觉都会静默消失。
				//    取"叠加"语义：Boss 覆盖只能**打开**表现，不能把基础配置关掉。
				bEffectiveLargeVisual = bEffectiveLargeVisual || BossOverride.bBossLargeVisual;
				bEffectiveMuzzleEffect = bEffectiveMuzzleEffect || BossOverride.bBossSpawnMuzzleEffect;
			}

			Archetype.Movement.PreferredRange = EffectivePreferredRange;
			Archetype.Movement.FarDistance = P.FarDistance > 0.0f ? P.FarDistance : (EffectivePreferredRange + 180.0f);
			Archetype.Movement.CloseDistance = EffectiveCloseDistance;
			Archetype.Movement.FarTowardWeight = P.FarTowardWeight;
			Archetype.Movement.FarTangentWeight = P.FarTangentWeight;
			Archetype.Movement.CloseTowardWeight = P.CloseTowardWeight;
			Archetype.Movement.CloseTangentWeight = P.CloseTangentWeight;
			Archetype.Movement.MidTowardWeight = P.MidTowardWeight;
			Archetype.Movement.MidTangentWeight = P.MidTangentWeight;
			Archetype.Movement.bUseRangeBiasForMidToward = P.bUseRangeBiasForMidToward;

			Archetype.Ranged.bUsesRangedAttack = true;
			Archetype.Ranged.ShotPattern = P.ShotPattern;
			Archetype.Ranged.InitialCooldownMin = P.InitialCooldownMin;
			Archetype.Ranged.InitialCooldownMax = P.InitialCooldownMax;
			Archetype.Ranged.PreferredRange = EffectivePreferredRange;
			Archetype.Ranged.NextShotCooldown = EffectiveNextShotCooldown;
			Archetype.Ranged.ProjectileSpeed = EffectiveProjectileSpeed;
			Archetype.Ranged.ProjectileDamageMultiplier = EffectiveProjectileDamageMultiplier;
			Archetype.Ranged.bLargeVisual = bEffectiveLargeVisual;
			Archetype.Ranged.bSpawnMuzzleEffect = bEffectiveMuzzleEffect;
			Archetype.Ranged.MuzzleEffectScale = P.MuzzleEffectScale;
			Archetype.Ranged.MuzzleEffectLifetime = P.MuzzleEffectLifetime;
		}
		break;

	case ERogueEnemyType::Skirmisher:
		{
			const FRogueSkirmisherSpecialParams& P = SpecialParams.Skirmisher;
			Archetype.Movement.CloseDistance = P.CloseDistance;
			Archetype.Movement.CloseTowardWeight = P.CloseTowardWeight;
			Archetype.Movement.CloseTangentWeight = P.CloseTangentWeight;
			Archetype.Movement.ZigZagFrequency = P.ZigZagFrequency;
			Archetype.Movement.ZigZagTowardWeight = P.ZigZagTowardWeight;
			Archetype.Movement.ZigZagTangentWeight = P.ZigZagTangentWeight;
		}
		break;

	case ERogueEnemyType::Cannoneer:
		{
			const FRogueArtillerySpecialParams& P = SpecialParams.Artillery;
			float EffectivePreferredRange = P.PreferredRange;
			float EffectiveCloseDistance = P.CloseDistance;
			float EffectiveNextShotCooldown = P.NextShotCooldown;
			float EffectiveProjectileSpeed = P.ProjectileSpeed;
			float EffectiveProjectileDamageMultiplier = P.ProjectileDamageMultiplier;
			bool bEffectiveLargeVisual = P.bLargeVisual;
			bool bEffectiveMuzzleEffect = P.bSpawnMuzzleEffect;

			if (bIsBoss && BossOverride.bEnableBossOverride)
			{
				if (BossOverride.BossPreferredRange > 0.0f) EffectivePreferredRange = BossOverride.BossPreferredRange;
				if (BossOverride.BossCloseDistance > 0.0f) EffectiveCloseDistance = BossOverride.BossCloseDistance;
				if (BossOverride.BossNextShotCooldown > 0.0f) EffectiveNextShotCooldown = BossOverride.BossNextShotCooldown;
				if (BossOverride.BossProjectileSpeed > 0.0f) EffectiveProjectileSpeed = BossOverride.BossProjectileSpeed;
				if (BossOverride.BossProjectileDamageMultiplier > 0.0f) EffectiveProjectileDamageMultiplier = BossOverride.BossProjectileDamageMultiplier;
				// ⚠️ bool 没有"未设置"的哨兵值，所以不能像上面那些 float 一样用 `> 0` 保护。
				//    无条件赋值会让**没配这两列的 Boss** 把基础值一起清掉：
				//    Artillery 的枪口特效、Shooter/Spitter 的放大视觉都会静默消失。
				//    取"叠加"语义：Boss 覆盖只能**打开**表现，不能把基础配置关掉。
				bEffectiveLargeVisual = bEffectiveLargeVisual || BossOverride.bBossLargeVisual;
				bEffectiveMuzzleEffect = bEffectiveMuzzleEffect || BossOverride.bBossSpawnMuzzleEffect;
			}

			Archetype.Movement.PreferredRange = EffectivePreferredRange;
			Archetype.Movement.FarDistance = P.FarDistance > 0.0f ? P.FarDistance : (EffectivePreferredRange + 240.0f);
			Archetype.Movement.CloseDistance = EffectiveCloseDistance;
			Archetype.Movement.FarTowardWeight = P.FarTowardWeight;
			Archetype.Movement.FarTangentWeight = P.FarTangentWeight;
			Archetype.Movement.CloseTowardWeight = P.CloseTowardWeight;
			Archetype.Movement.CloseTangentWeight = P.CloseTangentWeight;
			Archetype.Movement.MidTowardWeight = P.MidTowardWeight;
			Archetype.Movement.MidTangentWeight = P.MidTangentWeight;

			Archetype.Ranged.bUsesRangedAttack = true;
			Archetype.Ranged.ShotPattern = P.ShotPattern;
			Archetype.Ranged.InitialCooldownMin = P.InitialCooldownMin;
			Archetype.Ranged.InitialCooldownMax = P.InitialCooldownMax;
			Archetype.Ranged.PreferredRange = EffectivePreferredRange;
			Archetype.Ranged.NextShotCooldown = EffectiveNextShotCooldown;
			Archetype.Ranged.ProjectileSpeed = EffectiveProjectileSpeed;
			Archetype.Ranged.ProjectileDamageMultiplier = EffectiveProjectileDamageMultiplier;
			Archetype.Ranged.bLargeVisual = bEffectiveLargeVisual;
			Archetype.Ranged.bSpawnMuzzleEffect = bEffectiveMuzzleEffect;
			Archetype.Ranged.MuzzleEffectScale = P.MuzzleEffectScale;
			Archetype.Ranged.MuzzleEffectLifetime = P.MuzzleEffectLifetime;
		}
		break;

	case ERogueEnemyType::Shotgunner:
	case ERogueEnemyType::Scatterer:
		{
			const FRogueSpitterSpecialParams& P = SpecialParams.Spitter;
			float EffectivePreferredRange = P.PreferredRange;
			float EffectiveCloseDistance = P.CloseDistance;
			float EffectiveNextShotCooldown = P.NextShotCooldown;
			float EffectiveProjectileSpeed = P.ProjectileSpeed;
			float EffectiveProjectileDamageMultiplier = P.ProjectileDamageMultiplier;
			bool bEffectiveLargeVisual = P.bLargeVisual;
			bool bEffectiveMuzzleEffect = P.bSpawnMuzzleEffect;
			float EffectiveSpreadAngleStep = P.SpreadAngleStep;

			if (bIsBoss && BossOverride.bEnableBossOverride)
			{
				if (BossOverride.BossPreferredRange > 0.0f) EffectivePreferredRange = BossOverride.BossPreferredRange;
				if (BossOverride.BossCloseDistance > 0.0f) EffectiveCloseDistance = BossOverride.BossCloseDistance;
				if (BossOverride.BossNextShotCooldown > 0.0f) EffectiveNextShotCooldown = BossOverride.BossNextShotCooldown;
				if (BossOverride.BossProjectileSpeed > 0.0f) EffectiveProjectileSpeed = BossOverride.BossProjectileSpeed;
				if (BossOverride.BossProjectileDamageMultiplier > 0.0f) EffectiveProjectileDamageMultiplier = BossOverride.BossProjectileDamageMultiplier;
				// ⚠️ bool 没有"未设置"的哨兵值，所以不能像上面那些 float 一样用 `> 0` 保护。
				//    无条件赋值会让**没配这两列的 Boss** 把基础值一起清掉：
				//    Artillery 的枪口特效、Shooter/Spitter 的放大视觉都会静默消失。
				//    取"叠加"语义：Boss 覆盖只能**打开**表现，不能把基础配置关掉。
				bEffectiveLargeVisual = bEffectiveLargeVisual || BossOverride.bBossLargeVisual;
				bEffectiveMuzzleEffect = bEffectiveMuzzleEffect || BossOverride.bBossSpawnMuzzleEffect;
				if (BossOverride.BossSpreadAngleStep > 0.0f) EffectiveSpreadAngleStep = BossOverride.BossSpreadAngleStep;
			}

			Archetype.Movement.PreferredRange = EffectivePreferredRange;
			Archetype.Movement.FarDistance = P.FarDistance > 0.0f ? P.FarDistance : (EffectivePreferredRange + 120.0f);
			Archetype.Movement.CloseDistance = EffectiveCloseDistance;
			Archetype.Movement.FarTowardWeight = P.FarTowardWeight;
			Archetype.Movement.FarTangentWeight = P.FarTangentWeight;
			Archetype.Movement.CloseTowardWeight = P.CloseTowardWeight;
			Archetype.Movement.CloseTangentWeight = P.CloseTangentWeight;
			Archetype.Movement.MidTowardWeight = P.MidTowardWeight;
			Archetype.Movement.MidTangentWeight = P.MidTangentWeight;

			Archetype.Ranged.bUsesRangedAttack = true;
			Archetype.Ranged.ShotPattern = P.ShotPattern;
			Archetype.Ranged.InitialCooldownMin = P.InitialCooldownMin;
			Archetype.Ranged.InitialCooldownMax = P.InitialCooldownMax;
			Archetype.Ranged.PreferredRange = EffectivePreferredRange;
			Archetype.Ranged.NextShotCooldown = EffectiveNextShotCooldown;
			Archetype.Ranged.ProjectileSpeed = EffectiveProjectileSpeed;
			Archetype.Ranged.ProjectileDamageMultiplier = EffectiveProjectileDamageMultiplier;
			Archetype.Ranged.bLargeVisual = bEffectiveLargeVisual;
			Archetype.Ranged.bSpawnMuzzleEffect = bEffectiveMuzzleEffect;
			Archetype.Ranged.MuzzleEffectScale = P.MuzzleEffectScale;
			Archetype.Ranged.MuzzleEffectLifetime = P.MuzzleEffectLifetime;
			Archetype.Ranged.SpreadShotCount = P.SpreadShotCount;
			Archetype.Ranged.SpreadAngleStep = EffectiveSpreadAngleStep;
			Archetype.Ranged.SpreadSpawnOffset = P.SpreadSpawnOffset;
		}
		break;

	case ERogueEnemyType::Ringcaster:
		{
			const FRogueShockPillarSpecialParams& P = SpecialParams.ShockPillar;
			Archetype.Ranged.bUsesRangedAttack = true;
			Archetype.Ranged.ShotPattern = ERogueEnemyShotPattern::RadialRing;
			Archetype.Ranged.InitialCooldownMin = FMath::Max(0.0f, P.InitialCooldownMin);
			Archetype.Ranged.InitialCooldownMax = FMath::Max(Archetype.Ranged.InitialCooldownMin, P.InitialCooldownMax);
			Archetype.Ranged.NextShotCooldown = FMath::Max(0.1f, P.AttackCooldown);
			Archetype.Ranged.BulletRing = P.BulletRing.Sanitized();
			Archetype.Ranged.ProjectileSpeed = Archetype.Ranged.BulletRing.ProjectileSpeed;
			Archetype.Ranged.ProjectileDamageMultiplier = Archetype.Ranged.BulletRing.DamageMultiplier;
		}
		break;

	case ERogueEnemyType::ShockPillar:
		{
			const FRogueShockPillarSpecialParams& P = SpecialParams.ShockPillar;
			Archetype.Movement.Model = ERogueEnemyMovementModel::Stationary;
			Archetype.Movement.BaseMoveMultiplier = 0.0f;
			Archetype.Ranged.bUsesRangedAttack = true;
			Archetype.Ranged.ShotPattern = ERogueEnemyShotPattern::RadialRing;
			Archetype.Ranged.InitialCooldownMin = FMath::Max(0.0f, P.InitialCooldownMin);
			Archetype.Ranged.InitialCooldownMax = FMath::Max(Archetype.Ranged.InitialCooldownMin, P.InitialCooldownMax);
			Archetype.Ranged.NextShotCooldown = FMath::Max(0.1f, P.AttackCooldown);
			Archetype.Ranged.BulletRing = P.BulletRing.Sanitized();
			Archetype.Ranged.ProjectileSpeed = Archetype.Ranged.BulletRing.ProjectileSpeed;
			Archetype.Ranged.ProjectileDamageMultiplier = Archetype.Ranged.BulletRing.DamageMultiplier;
		}
		break;

	case ERogueEnemyType::Rifleman:
	default:
		// 猎手使用基础属性，无专属参数
		break;
	}

	Archetype.ProjectileClass = ProjectileClass;
	Archetype.Visual = bIsBoss ? BossVisual : Visual;
	return Archetype;
}
