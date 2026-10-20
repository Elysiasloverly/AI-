
#include "Enemies/Data/RogueEnemyDataTable.h"

// TSoftObjectPtr<URogueAbilitySet> 的赋值需要**完整类型**（前置声明不够）。
#include "AbilitySystem/RogueAbilitySet.h"

namespace
{
	/** 哨兵语义：> 0 才覆盖，0 / 负数 = 跟随类型基线。 */
	void OverrideIfPositive(float& Target, float Value)
	{
		if (Value > 0.0f)
		{
			Target = Value;
		}
	}

	void OverrideIfPositive(int32& Target, int32 Value)
	{
		if (Value > 0)
		{
			Target = Value;
		}
	}

	/** Boss 覆盖：float 以 > 0 为"已设置"哨兵；bool 没有哨兵，只能叠加打开（OR）。
	 *  地藏/亡灵（环形弹幕）不读 Boss 覆盖 —— 弹幕参数在各自专属块里单独调。 */
	void ApplyBossOverride(FRogueEnemyArchetype& Archetype, const FRogueEnemyBossOverride& Override, bool bIsBoss)
	{
		if (!bIsBoss || !Override.bEnableBossOverride)
		{
			return;
		}

		OverrideIfPositive(Archetype.Movement.PreferredRange, Override.BossPreferredRange);
		OverrideIfPositive(Archetype.Movement.CloseDistance, Override.BossCloseDistance);
		OverrideIfPositive(Archetype.Ranged.NextShotCooldown, Override.BossNextShotCooldown);
		OverrideIfPositive(Archetype.Ranged.ProjectileSpeed, Override.BossProjectileSpeed);
		OverrideIfPositive(Archetype.Ranged.ProjectileDamageMultiplier, Override.BossProjectileDamageMultiplier);
		OverrideIfPositive(Archetype.Ranged.SpreadAngleStep, Override.BossSpreadAngleStep);

		// ⚠️ bool 没有"未设置"的哨兵值，无条件赋值会让没配这两列的 Boss 把基础值一起清掉。
		//    取"叠加"语义：Boss 覆盖只能**打开**表现，不能把基础配置关掉。
		Archetype.Ranged.bLargeVisual = Archetype.Ranged.bLargeVisual || Override.bBossLargeVisual;
		Archetype.Ranged.bSpawnMuzzleEffect = Archetype.Ranged.bSpawnMuzzleEffect || Override.bBossSpawnMuzzleEffect;
	}

	ERogueEnemyMovementModel ToMovementModel(ERogueEnemyMovementModelOverride Override)
	{
		switch (Override)
		{
		case ERogueEnemyMovementModelOverride::BurstCharge:
			return ERogueEnemyMovementModel::BurstCharge;
		case ERogueEnemyMovementModelOverride::Orbit:
			return ERogueEnemyMovementModel::Orbit;
		case ERogueEnemyMovementModelOverride::RangedKite:
			return ERogueEnemyMovementModel::RangedKite;
		case ERogueEnemyMovementModelOverride::ZigZagChase:
			return ERogueEnemyMovementModel::ZigZagChase;
		case ERogueEnemyMovementModelOverride::Stationary:
			return ERogueEnemyMovementModel::Stationary;
		case ERogueEnemyMovementModelOverride::Direct:
		default:
			return ERogueEnemyMovementModel::Direct;
		}
	}

	/** 远程风筝移动段的哨兵覆盖；远距离缺省时按"偏好距离 + FamilyOffset"补（与旧共享块一致）。 */
	void ApplyKiteMovementSentinel(float PreferredRange, float FarDistance, float CloseDistance,
		float FarTowardWeight, float FarTangentWeight, float CloseTowardWeight, float CloseTangentWeight,
		float MidTowardWeight, float MidTangentWeight, float FamilyOffset, FRogueEnemyArchetype& Archetype)
	{
		OverrideIfPositive(Archetype.Movement.PreferredRange, PreferredRange);
		OverrideIfPositive(Archetype.Movement.CloseDistance, CloseDistance);
		OverrideIfPositive(Archetype.Movement.FarTowardWeight, FarTowardWeight);
		OverrideIfPositive(Archetype.Movement.FarTangentWeight, FarTangentWeight);
		// 近距离/中距离权重允许为负（近距离排斥），用非零判断而不是 > 0。
		if (!FMath::IsNearlyZero(CloseTowardWeight)) { Archetype.Movement.CloseTowardWeight = CloseTowardWeight; }
		if (!FMath::IsNearlyZero(CloseTangentWeight)) { Archetype.Movement.CloseTangentWeight = CloseTangentWeight; }
		if (!FMath::IsNearlyZero(MidTowardWeight)) { Archetype.Movement.MidTowardWeight = MidTowardWeight; }
		if (!FMath::IsNearlyZero(MidTangentWeight)) { Archetype.Movement.MidTangentWeight = MidTangentWeight; }

		if (FarDistance > 0.0f)
		{
			Archetype.Movement.FarDistance = FarDistance;
		}
		else if (PreferredRange > 0.0f)
		{
			Archetype.Movement.FarDistance = Archetype.Movement.PreferredRange + FamilyOffset;
		}
	}
}

FRogueEnemyArchetype FRogueEnemyArchetypeRow::ToArchetype(bool bIsBoss) const
{
	FRogueEnemyArchetype Archetype = RogueEnemyArchetypes::BuildEnemyArchetype(EnemyType, bIsBoss);

	// 移动模型由 EnemyType 的基线决定；数据表只覆盖移动倍率，避免每行都要重复填模型。
	Archetype.Movement.BaseMoveMultiplier = BaseMoveMultiplier;

	// 攻击能力集（数据驱动）。留空时 ARogueEnemy::GrantAttackAbilities 会回退到
	// 按远近/接触标志推导的硬编码逻辑 —— 所以老数据表不动也能跑。
	Archetype.AttackAbilitySet = AttackAbilitySet;

	// 按敌人类型读取各自的专属参数块（一个怪物一个块）。
	switch (EnemyType)
	{
	case ERogueEnemyType::Rifleman:
		{
			// 妖精：远程风筝 + 单发，全部哨兵语义（老行不填 = 保持基线行为）。
			const FRogueRiflemanSpecialParams& P = SpecialParams.Rifleman;
			ApplyKiteMovementSentinel(P.PreferredRange, P.FarDistance, P.CloseDistance,
				P.FarTowardWeight, P.FarTangentWeight, P.CloseTowardWeight, P.CloseTangentWeight,
				P.MidTowardWeight, P.MidTangentWeight, 160.0f, Archetype);

			OverrideIfPositive(Archetype.Ranged.InitialCooldownMin, P.InitialCooldownMin);
			OverrideIfPositive(Archetype.Ranged.InitialCooldownMax, P.InitialCooldownMax);
			OverrideIfPositive(Archetype.Ranged.NextShotCooldown, P.NextShotCooldown);
			OverrideIfPositive(Archetype.Ranged.ProjectileSpeed, P.ProjectileSpeed);
			OverrideIfPositive(Archetype.Ranged.ProjectileDamageMultiplier, P.ProjectileDamageMultiplier);
		}
		break;

	case ERogueEnemyType::Assault:
		{
			// 夜雀：爆发冲刺段直接覆盖（默认值 = 基线），射击段哨兵语义。
			const FRogueAssaultSpecialParams& P = SpecialParams.Assault;
			Archetype.Movement.BurstCycleTime = P.BurstCycleTime;
			Archetype.Movement.BurstDuration = P.BurstDuration;
			Archetype.Movement.BurstActiveMultiplier = P.BurstActiveMultiplier;
			Archetype.Movement.BurstIdleMultiplier = P.BurstIdleMultiplier;

			OverrideIfPositive(Archetype.Ranged.InitialCooldownMin, P.InitialCooldownMin);
			OverrideIfPositive(Archetype.Ranged.InitialCooldownMax, P.InitialCooldownMax);
			OverrideIfPositive(Archetype.Ranged.NextShotCooldown, P.NextShotCooldown);
			OverrideIfPositive(Archetype.Ranged.ProjectileSpeed, P.ProjectileSpeed);
			OverrideIfPositive(Archetype.Ranged.ProjectileDamageMultiplier, P.ProjectileDamageMultiplier);
			OverrideIfPositive(Archetype.Ranged.SpreadShotCount, P.SpreadShotCount);
			OverrideIfPositive(Archetype.Ranged.SpreadAngleStep, P.SpreadAngleStep);
			OverrideIfPositive(Archetype.Ranged.SpreadSpawnOffset, P.SpreadSpawnOffset);
		}
		break;

	case ERogueEnemyType::Juggernaut:
		{
			// 鬼：直线追击（速度由顶层 BaseMoveMultiplier 控制），射击参数哨兵语义。
			const FRogueJuggernautSpecialParams& P = SpecialParams.Juggernaut;
			OverrideIfPositive(Archetype.Ranged.InitialCooldownMin, P.InitialCooldownMin);
			OverrideIfPositive(Archetype.Ranged.InitialCooldownMax, P.InitialCooldownMax);
			OverrideIfPositive(Archetype.Ranged.NextShotCooldown, P.NextShotCooldown);
			OverrideIfPositive(Archetype.Ranged.ProjectileSpeed, P.ProjectileSpeed);
			OverrideIfPositive(Archetype.Ranged.ProjectileDamageMultiplier, P.ProjectileDamageMultiplier);
		}
		break;

	case ERogueEnemyType::Orbiter:
		{
			// 人魂：环绕移动段直接覆盖（默认值 = 基线），射击段哨兵语义。
			const FRogueOrbiterSpecialParams& P = SpecialParams.Orbiter;
			Archetype.Movement.FarDistance = P.FarDistance;
			Archetype.Movement.CloseDistance = P.CloseDistance;
			Archetype.Movement.FarTowardWeight = P.FarTowardWeight;
			Archetype.Movement.FarTangentWeight = P.FarTangentWeight;
			Archetype.Movement.CloseTowardWeight = P.CloseTowardWeight;
			Archetype.Movement.CloseTangentWeight = P.CloseTangentWeight;
			Archetype.Movement.MidTowardWeight = P.MidTowardWeight;
			Archetype.Movement.MidTangentWeight = P.MidTangentWeight;

			OverrideIfPositive(Archetype.Ranged.InitialCooldownMin, P.InitialCooldownMin);
			OverrideIfPositive(Archetype.Ranged.InitialCooldownMax, P.InitialCooldownMax);
			OverrideIfPositive(Archetype.Ranged.NextShotCooldown, P.NextShotCooldown);
			OverrideIfPositive(Archetype.Ranged.ProjectileSpeed, P.ProjectileSpeed);
			OverrideIfPositive(Archetype.Ranged.ProjectileDamageMultiplier, P.ProjectileDamageMultiplier);
		}
		break;

	case ERogueEnemyType::Marksman:
		{
			// 鸦天狗：远程风筝 + 单发，全部直接覆盖（默认值 = 鸦天狗非 Boss 基线）。
			const FRogueMarksmanSpecialParams& P = SpecialParams.Marksman;
			Archetype.Movement.PreferredRange = P.PreferredRange;
			Archetype.Movement.CloseDistance = P.CloseDistance;
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
			Archetype.Ranged.NextShotCooldown = P.NextShotCooldown;
			Archetype.Ranged.ProjectileSpeed = P.ProjectileSpeed;
			Archetype.Ranged.ProjectileDamageMultiplier = P.ProjectileDamageMultiplier;
			Archetype.Ranged.bLargeVisual = P.bLargeVisual;
			Archetype.Ranged.bSpawnMuzzleEffect = P.bSpawnMuzzleEffect;
			Archetype.Ranged.MuzzleEffectScale = P.MuzzleEffectScale;
			Archetype.Ranged.MuzzleEffectLifetime = P.MuzzleEffectLifetime;

			ApplyBossOverride(Archetype, BossOverride, bIsBoss);
			Archetype.Movement.FarDistance = P.FarDistance > 0.0f ? P.FarDistance : (Archetype.Movement.PreferredRange + 180.0f);
		}
		break;

	case ERogueEnemyType::Skirmisher:
		{
			// 妖狐：Z 字移动段直接覆盖（默认值 = 基线），射击段哨兵语义。
			const FRogueSkirmisherSpecialParams& P = SpecialParams.Skirmisher;
			Archetype.Movement.CloseDistance = P.CloseDistance;
			Archetype.Movement.CloseTowardWeight = P.CloseTowardWeight;
			Archetype.Movement.CloseTangentWeight = P.CloseTangentWeight;
			Archetype.Movement.ZigZagFrequency = P.ZigZagFrequency;
			Archetype.Movement.ZigZagTowardWeight = P.ZigZagTowardWeight;
			Archetype.Movement.ZigZagTangentWeight = P.ZigZagTangentWeight;

			OverrideIfPositive(Archetype.Ranged.InitialCooldownMin, P.InitialCooldownMin);
			OverrideIfPositive(Archetype.Ranged.InitialCooldownMax, P.InitialCooldownMax);
			OverrideIfPositive(Archetype.Ranged.NextShotCooldown, P.NextShotCooldown);
			OverrideIfPositive(Archetype.Ranged.ProjectileSpeed, P.ProjectileSpeed);
			OverrideIfPositive(Archetype.Ranged.ProjectileDamageMultiplier, P.ProjectileDamageMultiplier);
			OverrideIfPositive(Archetype.Ranged.SpreadShotCount, P.SpreadShotCount);
			OverrideIfPositive(Archetype.Ranged.SpreadAngleStep, P.SpreadAngleStep);
			OverrideIfPositive(Archetype.Ranged.SpreadSpawnOffset, P.SpreadSpawnOffset);
		}
		break;

	case ERogueEnemyType::Cannoneer:
		{
			// 山蜘蛛：远程风筝 + 重型单发，全部直接覆盖（默认值 = 山蜘蛛非 Boss 基线）。
			const FRogueCannoneerSpecialParams& P = SpecialParams.Cannoneer;
			Archetype.Movement.PreferredRange = P.PreferredRange;
			Archetype.Movement.CloseDistance = P.CloseDistance;
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
			Archetype.Ranged.NextShotCooldown = P.NextShotCooldown;
			Archetype.Ranged.ProjectileSpeed = P.ProjectileSpeed;
			Archetype.Ranged.ProjectileDamageMultiplier = P.ProjectileDamageMultiplier;
			Archetype.Ranged.bLargeVisual = P.bLargeVisual;
			Archetype.Ranged.bSpawnMuzzleEffect = P.bSpawnMuzzleEffect;
			Archetype.Ranged.MuzzleEffectScale = P.MuzzleEffectScale;
			Archetype.Ranged.MuzzleEffectLifetime = P.MuzzleEffectLifetime;

			ApplyBossOverride(Archetype, BossOverride, bIsBoss);
			Archetype.Movement.FarDistance = P.FarDistance > 0.0f ? P.FarDistance : (Archetype.Movement.PreferredRange + 240.0f);
		}
		break;

	case ERogueEnemyType::Scatterer:
		{
			// 唐伞：远程风筝 + 散射，全部直接覆盖（默认值 = 唐伞非 Boss 基线）。
			const FRogueScattererSpecialParams& P = SpecialParams.Scatterer;
			Archetype.Movement.PreferredRange = P.PreferredRange;
			Archetype.Movement.CloseDistance = P.CloseDistance;
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
			Archetype.Ranged.NextShotCooldown = P.NextShotCooldown;
			Archetype.Ranged.ProjectileSpeed = P.ProjectileSpeed;
			Archetype.Ranged.ProjectileDamageMultiplier = P.ProjectileDamageMultiplier;
			Archetype.Ranged.bLargeVisual = P.bLargeVisual;
			Archetype.Ranged.bSpawnMuzzleEffect = P.bSpawnMuzzleEffect;
			Archetype.Ranged.MuzzleEffectScale = P.MuzzleEffectScale;
			Archetype.Ranged.MuzzleEffectLifetime = P.MuzzleEffectLifetime;
			Archetype.Ranged.SpreadShotCount = P.SpreadShotCount;
			Archetype.Ranged.SpreadAngleStep = P.SpreadAngleStep;
			Archetype.Ranged.SpreadSpawnOffset = P.SpreadSpawnOffset;

			ApplyBossOverride(Archetype, BossOverride, bIsBoss);
			Archetype.Movement.FarDistance = P.FarDistance > 0.0f ? P.FarDistance : (Archetype.Movement.PreferredRange + 120.0f);
		}
		break;

	case ERogueEnemyType::ShockPillar:
		{
			// 地藏：固定柱体 + 整圈弹幕（弹幕参数直接覆盖，默认值 = 基线）。
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

	case ERogueEnemyType::Sniper:
		{
			// 月兔：远程风筝 + 单发狙击，全部直接覆盖（默认值 = 月兔非 Boss 基线）。
			const FRogueSniperSpecialParams& P = SpecialParams.Sniper;
			Archetype.Movement.PreferredRange = P.PreferredRange;
			Archetype.Movement.CloseDistance = P.CloseDistance;
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
			Archetype.Ranged.NextShotCooldown = P.NextShotCooldown;
			Archetype.Ranged.ProjectileSpeed = P.ProjectileSpeed;
			Archetype.Ranged.ProjectileDamageMultiplier = P.ProjectileDamageMultiplier;
			Archetype.Ranged.bLargeVisual = P.bLargeVisual;
			Archetype.Ranged.bSpawnMuzzleEffect = P.bSpawnMuzzleEffect;
			Archetype.Ranged.MuzzleEffectScale = P.MuzzleEffectScale;
			Archetype.Ranged.MuzzleEffectLifetime = P.MuzzleEffectLifetime;

			ApplyBossOverride(Archetype, BossOverride, bIsBoss);
			Archetype.Movement.FarDistance = P.FarDistance > 0.0f ? P.FarDistance : (Archetype.Movement.PreferredRange + 180.0f);
		}
		break;

	case ERogueEnemyType::Shotgunner:
		{
			// 毒茸：远程风筝 + 5 发散射，全部直接覆盖（默认值 = 毒茸非 Boss 基线）。
			const FRogueShotgunnerSpecialParams& P = SpecialParams.Shotgunner;
			Archetype.Movement.PreferredRange = P.PreferredRange;
			Archetype.Movement.CloseDistance = P.CloseDistance;
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
			Archetype.Ranged.NextShotCooldown = P.NextShotCooldown;
			Archetype.Ranged.ProjectileSpeed = P.ProjectileSpeed;
			Archetype.Ranged.ProjectileDamageMultiplier = P.ProjectileDamageMultiplier;
			Archetype.Ranged.bLargeVisual = P.bLargeVisual;
			Archetype.Ranged.bSpawnMuzzleEffect = P.bSpawnMuzzleEffect;
			Archetype.Ranged.MuzzleEffectScale = P.MuzzleEffectScale;
			Archetype.Ranged.MuzzleEffectLifetime = P.MuzzleEffectLifetime;
			Archetype.Ranged.SpreadShotCount = P.SpreadShotCount;
			Archetype.Ranged.SpreadAngleStep = P.SpreadAngleStep;
			Archetype.Ranged.SpreadSpawnOffset = P.SpreadSpawnOffset;

			ApplyBossOverride(Archetype, BossOverride, bIsBoss);
			Archetype.Movement.FarDistance = P.FarDistance > 0.0f ? P.FarDistance : (Archetype.Movement.PreferredRange + 100.0f);
		}
		break;

	case ERogueEnemyType::Repeater:
		{
			// 血蝠：远程风筝 + 单发速射，全部直接覆盖（默认值 = 血蝠非 Boss 基线）。
			const FRogueRepeaterSpecialParams& P = SpecialParams.Repeater;
			Archetype.Movement.PreferredRange = P.PreferredRange;
			Archetype.Movement.CloseDistance = P.CloseDistance;
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
			Archetype.Ranged.NextShotCooldown = P.NextShotCooldown;
			Archetype.Ranged.ProjectileSpeed = P.ProjectileSpeed;
			Archetype.Ranged.ProjectileDamageMultiplier = P.ProjectileDamageMultiplier;
			Archetype.Ranged.bLargeVisual = P.bLargeVisual;
			Archetype.Ranged.bSpawnMuzzleEffect = P.bSpawnMuzzleEffect;
			Archetype.Ranged.MuzzleEffectScale = P.MuzzleEffectScale;
			Archetype.Ranged.MuzzleEffectLifetime = P.MuzzleEffectLifetime;

			ApplyBossOverride(Archetype, BossOverride, bIsBoss);
			Archetype.Movement.FarDistance = P.FarDistance > 0.0f ? P.FarDistance : (Archetype.Movement.PreferredRange + 160.0f);
		}
		break;

	case ERogueEnemyType::Ringcaster:
		{
			// 亡灵：远程风筝 + 整圈弹幕。弹幕段直接覆盖；移动段哨兵语义（0=基线）。
			const FRogueRingcasterSpecialParams& P = SpecialParams.Ringcaster;
			Archetype.Ranged.bUsesRangedAttack = true;
			Archetype.Ranged.ShotPattern = ERogueEnemyShotPattern::RadialRing;
			Archetype.Ranged.InitialCooldownMin = FMath::Max(0.0f, P.InitialCooldownMin);
			Archetype.Ranged.InitialCooldownMax = FMath::Max(Archetype.Ranged.InitialCooldownMin, P.InitialCooldownMax);
			Archetype.Ranged.NextShotCooldown = FMath::Max(0.1f, P.AttackCooldown);
			Archetype.Ranged.BulletRing = P.BulletRing.Sanitized();
			Archetype.Ranged.ProjectileSpeed = Archetype.Ranged.BulletRing.ProjectileSpeed;
			Archetype.Ranged.ProjectileDamageMultiplier = Archetype.Ranged.BulletRing.DamageMultiplier;

			ApplyKiteMovementSentinel(P.PreferredRange, P.FarDistance, P.CloseDistance,
				P.FarTowardWeight, P.FarTangentWeight, P.CloseTowardWeight, P.CloseTangentWeight,
				P.MidTowardWeight, P.MidTangentWeight, 140.0f, Archetype);
		}
		break;

	default:
		// 枚举新增但没写分支：保持基线行为。
		break;
	}

	// 移动模型覆盖：最后应用，显式选择优先于类型基线（包括地藏的强制固定不动）。
	if (MovementModelOverride != ERogueEnemyMovementModelOverride::FromBaseline)
	{
		Archetype.Movement.Model = ToMovementModel(MovementModelOverride);
	}

	Archetype.ProjectileClass = ProjectileClass;
	Archetype.Visual = bIsBoss ? BossVisual : Visual;
	return Archetype;
}
