
#include "Enemies/RogueEnemyDataTable.h"

FRogueEnemyArchetype FRogueEnemyArchetypeRow::ToArchetype(bool bIsBoss) const
{
	FRogueEnemyArchetype Archetype = RogueEnemyArchetypes::BuildEnemyArchetype(EnemyType, bIsBoss);

	// 基础移动属性
	Archetype.Movement.Model = MovementModel;
	Archetype.Movement.BaseMoveMultiplier = BaseMoveMultiplier;

	// 根据敌人类型填充专属属性
	switch (EnemyType)
	{
	case ERogueEnemyType::Charger:
		{
			const FRogueChargerSpecialParams& P = ChargerParams;
			Archetype.Movement.BurstCycleTime = P.BurstCycleTime;
			Archetype.Movement.BurstDuration = P.BurstDuration;
			Archetype.Movement.BurstActiveMultiplier = P.BurstActiveMultiplier;
			Archetype.Movement.BurstIdleMultiplier = P.BurstIdleMultiplier;
		}
		break;

	case ERogueEnemyType::Tank:
		{
			const FRogueTankSpecialParams& P = TankParams;
			Archetype.Movement.BaseMoveMultiplier = P.BaseMoveMultiplier;
		}
		break;

	case ERogueEnemyType::Orbiter:
		{
			const FRogueOrbiterSpecialParams& P = OrbiterParams;
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

	case ERogueEnemyType::Shooter:
		{
			const FRogueShooterSpecialParams& P = ShooterParams;
			float EffectivePreferredRange = P.PreferredRange;
			float EffectiveCloseDistance = P.CloseDistance;
			float EffectiveMaximumRange = P.MaximumRange;
			float EffectiveNextShotCooldown = P.NextShotCooldown;
			float EffectiveProjectileSpeed = P.ProjectileSpeed;
			float EffectiveProjectileDamageMultiplier = P.ProjectileDamageMultiplier;
			bool bEffectiveLargeVisual = P.bLargeVisual;
			bool bEffectiveMuzzleEffect = P.bSpawnMuzzleEffect;

			if (bIsBoss && BossOverride.bEnableBossOverride)
			{
				if (BossOverride.BossPreferredRange > 0.0f) EffectivePreferredRange = BossOverride.BossPreferredRange;
				if (BossOverride.BossCloseDistance > 0.0f) EffectiveCloseDistance = BossOverride.BossCloseDistance;
				if (BossOverride.BossMaximumRange > 0.0f) EffectiveMaximumRange = BossOverride.BossMaximumRange;
				if (BossOverride.BossNextShotCooldown > 0.0f) EffectiveNextShotCooldown = BossOverride.BossNextShotCooldown;
				if (BossOverride.BossProjectileSpeed > 0.0f) EffectiveProjectileSpeed = BossOverride.BossProjectileSpeed;
				if (BossOverride.BossProjectileDamageMultiplier > 0.0f) EffectiveProjectileDamageMultiplier = BossOverride.BossProjectileDamageMultiplier;
				bEffectiveLargeVisual = BossOverride.bBossLargeVisual;
				bEffectiveMuzzleEffect = BossOverride.bBossSpawnMuzzleEffect;
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
			Archetype.Ranged.MinimumRange = EffectiveCloseDistance;
			Archetype.Ranged.MaximumRange = EffectiveMaximumRange;
			Archetype.Ranged.AttackRangeMinFactor = P.AttackRangeMinFactor;
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
			const FRogueSkirmisherSpecialParams& P = SkirmisherParams;
			Archetype.Movement.CloseDistance = P.CloseDistance;
			Archetype.Movement.CloseTowardWeight = P.CloseTowardWeight;
			Archetype.Movement.CloseTangentWeight = P.CloseTangentWeight;
			Archetype.Movement.ZigZagFrequency = P.ZigZagFrequency;
			Archetype.Movement.ZigZagTowardWeight = P.ZigZagTowardWeight;
			Archetype.Movement.ZigZagTangentWeight = P.ZigZagTangentWeight;
		}
		break;

	case ERogueEnemyType::Artillery:
		{
			const FRogueArtillerySpecialParams& P = ArtilleryParams;
			float EffectivePreferredRange = P.PreferredRange;
			float EffectiveCloseDistance = P.CloseDistance;
			float EffectiveMaximumRange = P.MaximumRange;
			float EffectiveNextShotCooldown = P.NextShotCooldown;
			float EffectiveProjectileSpeed = P.ProjectileSpeed;
			float EffectiveProjectileDamageMultiplier = P.ProjectileDamageMultiplier;
			bool bEffectiveLargeVisual = P.bLargeVisual;
			bool bEffectiveMuzzleEffect = P.bSpawnMuzzleEffect;

			if (bIsBoss && BossOverride.bEnableBossOverride)
			{
				if (BossOverride.BossPreferredRange > 0.0f) EffectivePreferredRange = BossOverride.BossPreferredRange;
				if (BossOverride.BossCloseDistance > 0.0f) EffectiveCloseDistance = BossOverride.BossCloseDistance;
				if (BossOverride.BossMaximumRange > 0.0f) EffectiveMaximumRange = BossOverride.BossMaximumRange;
				if (BossOverride.BossNextShotCooldown > 0.0f) EffectiveNextShotCooldown = BossOverride.BossNextShotCooldown;
				if (BossOverride.BossProjectileSpeed > 0.0f) EffectiveProjectileSpeed = BossOverride.BossProjectileSpeed;
				if (BossOverride.BossProjectileDamageMultiplier > 0.0f) EffectiveProjectileDamageMultiplier = BossOverride.BossProjectileDamageMultiplier;
				bEffectiveLargeVisual = BossOverride.bBossLargeVisual;
				bEffectiveMuzzleEffect = BossOverride.bBossSpawnMuzzleEffect;
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
			Archetype.Ranged.MinimumRange = EffectiveCloseDistance;
			Archetype.Ranged.MaximumRange = EffectiveMaximumRange;
			Archetype.Ranged.AttackRangeMinFactor = P.AttackRangeMinFactor;
			Archetype.Ranged.NextShotCooldown = EffectiveNextShotCooldown;
			Archetype.Ranged.ProjectileSpeed = EffectiveProjectileSpeed;
			Archetype.Ranged.ProjectileDamageMultiplier = EffectiveProjectileDamageMultiplier;
			Archetype.Ranged.bLargeVisual = bEffectiveLargeVisual;
			Archetype.Ranged.bSpawnMuzzleEffect = bEffectiveMuzzleEffect;
			Archetype.Ranged.MuzzleEffectScale = P.MuzzleEffectScale;
			Archetype.Ranged.MuzzleEffectLifetime = P.MuzzleEffectLifetime;
		}
		break;

	case ERogueEnemyType::Spitter:
		{
			const FRogueSpitterSpecialParams& P = SpitterParams;
			float EffectivePreferredRange = P.PreferredRange;
			float EffectiveCloseDistance = P.CloseDistance;
			float EffectiveMaximumRange = P.MaximumRange;
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
				if (BossOverride.BossMaximumRange > 0.0f) EffectiveMaximumRange = BossOverride.BossMaximumRange;
				if (BossOverride.BossNextShotCooldown > 0.0f) EffectiveNextShotCooldown = BossOverride.BossNextShotCooldown;
				if (BossOverride.BossProjectileSpeed > 0.0f) EffectiveProjectileSpeed = BossOverride.BossProjectileSpeed;
				if (BossOverride.BossProjectileDamageMultiplier > 0.0f) EffectiveProjectileDamageMultiplier = BossOverride.BossProjectileDamageMultiplier;
				bEffectiveLargeVisual = BossOverride.bBossLargeVisual;
				bEffectiveMuzzleEffect = BossOverride.bBossSpawnMuzzleEffect;
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
			Archetype.Ranged.MinimumRange = EffectiveCloseDistance;
			Archetype.Ranged.MaximumRange = EffectiveMaximumRange;
			Archetype.Ranged.AttackRangeMinFactor = P.AttackRangeMinFactor;
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

	case ERogueEnemyType::ShockPillar:
		{
			const FRogueShockPillarSpecialParams& P = ShockPillarParams;
			Archetype.Movement.Model = ERogueEnemyMovementModel::Stationary;
			Archetype.Movement.BaseMoveMultiplier = 0.0f;
			Archetype.GroundWave.bUsesGroundWave = true;
			Archetype.GroundWave.InitialCooldownMin = P.InitialCooldownMin;
			Archetype.GroundWave.InitialCooldownMax = P.InitialCooldownMax;
			Archetype.GroundWave.AttackCooldown = P.AttackCooldown;
			Archetype.GroundWave.MaxRadius = P.MaxRadius;
			Archetype.GroundWave.ExpansionDuration = P.ExpansionDuration;
			Archetype.GroundWave.ExpansionSpeed = P.ExpansionSpeed;
			Archetype.GroundWave.HitThickness = P.HitThickness;
			Archetype.GroundWave.DamageMultiplier = P.DamageMultiplier;
			Archetype.GroundWave.bJumpCanDodge = P.bJumpCanDodge;
		}
		break;

	case ERogueEnemyType::Hunter:
	default:
		// 猎手使用基础属性，无专属参数
		break;
	}

	return Archetype;
}
