
#include "Enemies/RogueEnemyDataTable.h"

// TSoftObjectPtr<URogueAbilitySet> 的赋值需要**完整类型**（前置声明不够）。
#include "AbilitySystem/RogueAbilitySet.h"

FRogueEnemyArchetype FRogueEnemyArchetypeRow::ToArchetype(bool bIsBoss) const
{
	FRogueEnemyArchetype Archetype = RogueEnemyArchetypes::BuildEnemyArchetype(EnemyType, bIsBoss);

	// 基础移动属性
	//
	// ⚠️ 这一列会**覆盖** `BuildEnemyArchetype` 按类型给出的移动模型。
	//    而移动模型基本就是每种敌人的身份：
	//      Charger=BurstCharge / Orbiter=Orbit / Skirmisher=ZigZagChase /
	//      Shooter·Artillery·Spitter=RangedKite / ShockPillar=Stationary
	//    所以表里**漏填**这一列（于是取到结构体默认值 Direct）会让该类型静默退化成直追。
	//    更麻烦的是 Hunter 本来就是 Direct —— 默认表根本测不出这个问题。
	//
	//    这里**不改变**"数据表优先"的设计（那是项目明确的取舍），
	//    只把静默失效变成一条响亮的警告。若想改成"按类型固定、表不参与"，
	//    删掉下面两行赋值即可。
	if (MovementModel != Archetype.Movement.Model)
	{
		// 每个类型只警告一次：本函数每次刷怪会被调多次（配置 + 两个上限查询），
		// 不加限制的话一场对局能刷出上百条同样的警告，反而把日志淹掉。
		// 类型最多 9 种，用一个位掩码记录就够。
		static uint32 WarnedTypesMask = 0;
		const uint32 TypeBit = 1u << static_cast<uint32>(EnemyType);
		if ((WarnedTypesMask & TypeBit) == 0)
		{
			WarnedTypesMask |= TypeBit;
			UE_LOG(LogTemp, Warning,
				TEXT("[Rogue] 敌人原型：类型 %d 的移动模型被数据表覆盖（表=%d，按类型基线=%d）。")
				TEXT("若不是有意配置，请检查 DT_FRogueEnemyArchetypeRow 的 MovementModel 列是否漏填。"),
				static_cast<int32>(EnemyType),
				static_cast<int32>(MovementModel),
				static_cast<int32>(Archetype.Movement.Model));
		}
	}
	Archetype.Movement.Model = MovementModel;
	Archetype.Movement.BaseMoveMultiplier = BaseMoveMultiplier;

	// 攻击能力集（数据驱动）。留空时 ARogueEnemy::GrantAttackAbilities 会回退到
	// 按 Ranged/GroundWave 标志推导的硬编码逻辑 —— 所以老数据表不动也能跑。
	Archetype.AttackAbilitySet = AttackAbilitySet;

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
