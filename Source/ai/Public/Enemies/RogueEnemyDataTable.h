
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Core/RogueTypes.h"
#include "Enemies/RogueEnemyArchetypes.h"
#include "RogueEnemyDataTable.generated.h"

// ===================================================================
//  每种敌人的专属属性结构体
// ===================================================================

/** 冲锋者（Charger）专属属性 - 爆发冲刺参数 */
USTRUCT(BlueprintType)
struct AI_API FRogueChargerSpecialParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "爆发周期时间"))
	float BurstCycleTime = 2.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "爆发持续时间"))
	float BurstDuration = 0.9f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "爆发激活速度倍率"))
	float BurstActiveMultiplier = 2.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "爆发空闲速度倍率"))
	float BurstIdleMultiplier = 1.0f;
};

/** 坦克（Tank）专属属性 - 减速参数 */
USTRUCT(BlueprintType)
struct AI_API FRogueTankSpecialParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "基础移动速度倍率"))
	float BaseMoveMultiplier = 0.72f;
};

/** 环绕者（Orbiter）专属属性 - 环绕运动参数 */
USTRUCT(BlueprintType)
struct AI_API FRogueOrbiterSpecialParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "远距离"))
	float FarDistance = 700.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "近距离"))
	float CloseDistance = 420.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "远距离朝向权重"))
	float FarTowardWeight = 0.75f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "远距离切线权重"))
	float FarTangentWeight = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "近距离朝向权重"))
	float CloseTowardWeight = -0.55f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "近距离切线权重"))
	float CloseTangentWeight = 0.80f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "中距离朝向权重"))
	float MidTowardWeight = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "中距离切线权重"))
	float MidTangentWeight = 0.85f;
};

/** 射手（Shooter）专属属性 - 远程风筝 + 射击参数 */
USTRUCT(BlueprintType)
struct AI_API FRogueShooterSpecialParams
{
	GENERATED_BODY()

	// --- 移动参数 ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (DisplayName = "偏好距离"))
	float PreferredRange = 760.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (DisplayName = "远距离"))
	float FarDistance = 940.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (DisplayName = "近距离"))
	float CloseDistance = 420.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (DisplayName = "远距离朝向权重"))
	float FarTowardWeight = 0.92f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (DisplayName = "远距离切线权重"))
	float FarTangentWeight = 0.18f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (DisplayName = "近距离朝向权重"))
	float CloseTowardWeight = -0.90f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (DisplayName = "近距离切线权重"))
	float CloseTangentWeight = 0.45f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (DisplayName = "中距离朝向权重"))
	float MidTowardWeight = 0.30f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (DisplayName = "中距离切线权重"))
	float MidTangentWeight = 0.90f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (DisplayName = "使用距离偏差计算中距离朝向"))
	bool bUseRangeBiasForMidToward = true;

	// --- 射击参数 ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "射击模式"))
	ERogueEnemyShotPattern ShotPattern = ERogueEnemyShotPattern::Single;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "初始冷却最小值"))
	float InitialCooldownMin = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "初始冷却最大值"))
	float InitialCooldownMax = 0.85f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "最小攻击距离"))
	float MinimumRange = 420.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "最大攻击距离"))
	float MaximumRange = 1300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "攻击距离最小系数"))
	float AttackRangeMinFactor = 0.55f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "射击冷却时间"))
	float NextShotCooldown = 1.18f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "弹丸速度"))
	float ProjectileSpeed = 980.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "弹丸伤害倍率"))
	float ProjectileDamageMultiplier = 0.85f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "大型弹丸视觉"))
	bool bLargeVisual = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "生成枪口特效"))
	bool bSpawnMuzzleEffect = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "枪口特效缩放"))
	FVector MuzzleEffectScale = FVector(0.24f, 0.24f, 0.24f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "枪口特效持续时间"))
	float MuzzleEffectLifetime = 0.12f;
};

/** 游击者（Skirmisher）专属属性 - Z字形追击参数 */
USTRUCT(BlueprintType)
struct AI_API FRogueSkirmisherSpecialParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "近距离"))
	float CloseDistance = 220.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "近距离朝向权重"))
	float CloseTowardWeight = -0.30f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "近距离切线权重"))
	float CloseTangentWeight = 0.95f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "Z字形频率"))
	float ZigZagFrequency = 7.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "Z字形朝向权重"))
	float ZigZagTowardWeight = 0.96f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "Z字形切线权重"))
	float ZigZagTangentWeight = 0.82f;
};

/** 炮兵（Artillery）专属属性 - 远程风筝 + 重型射击参数 */
USTRUCT(BlueprintType)
struct AI_API FRogueArtillerySpecialParams
{
	GENERATED_BODY()

	// --- 移动参数 ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (DisplayName = "偏好距离"))
	float PreferredRange = 1120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (DisplayName = "远距离"))
	float FarDistance = 1360.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (DisplayName = "近距离"))
	float CloseDistance = 620.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (DisplayName = "远距离朝向权重"))
	float FarTowardWeight = 0.90f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (DisplayName = "远距离切线权重"))
	float FarTangentWeight = 0.16f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (DisplayName = "近距离朝向权重"))
	float CloseTowardWeight = -0.96f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (DisplayName = "近距离切线权重"))
	float CloseTangentWeight = 0.28f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (DisplayName = "中距离朝向权重"))
	float MidTowardWeight = 0.08f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (DisplayName = "中距离切线权重"))
	float MidTangentWeight = 0.52f;

	// --- 射击参数 ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "射击模式"))
	ERogueEnemyShotPattern ShotPattern = ERogueEnemyShotPattern::Single;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "初始冷却最小值"))
	float InitialCooldownMin = 0.75f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "初始冷却最大值"))
	float InitialCooldownMax = 1.30f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "最小攻击距离"))
	float MinimumRange = 620.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "最大攻击距离"))
	float MaximumRange = 1720.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "攻击距离最小系数"))
	float AttackRangeMinFactor = 0.82f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "射击冷却时间"))
	float NextShotCooldown = 1.85f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "弹丸速度"))
	float ProjectileSpeed = 860.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "弹丸伤害倍率"))
	float ProjectileDamageMultiplier = 1.10f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "大型弹丸视觉"))
	bool bLargeVisual = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "生成枪口特效"))
	bool bSpawnMuzzleEffect = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "枪口特效缩放"))
	FVector MuzzleEffectScale = FVector(0.28f, 0.28f, 0.28f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "枪口特效持续时间"))
	float MuzzleEffectLifetime = 0.18f;
};

/** 喷射者（Spitter）专属属性 - 散射弹幕参数 */
USTRUCT(BlueprintType)
struct AI_API FRogueSpitterSpecialParams
{
	GENERATED_BODY()

	// --- 移动参数 ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (DisplayName = "偏好距离"))
	float PreferredRange = 540.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (DisplayName = "远距离"))
	float FarDistance = 660.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (DisplayName = "近距离"))
	float CloseDistance = 260.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (DisplayName = "远距离朝向权重"))
	float FarTowardWeight = 0.98f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (DisplayName = "远距离切线权重"))
	float FarTangentWeight = 0.26f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (DisplayName = "近距离朝向权重"))
	float CloseTowardWeight = -0.72f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (DisplayName = "近距离切线权重"))
	float CloseTangentWeight = 0.78f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (DisplayName = "中距离朝向权重"))
	float MidTowardWeight = 0.20f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (DisplayName = "中距离切线权重"))
	float MidTangentWeight = 0.96f;

	// --- 射击参数 ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "射击模式"))
	ERogueEnemyShotPattern ShotPattern = ERogueEnemyShotPattern::Spread3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "初始冷却最小值"))
	float InitialCooldownMin = 0.18f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "初始冷却最大值"))
	float InitialCooldownMax = 0.48f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "最小攻击距离"))
	float MinimumRange = 260.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "最大攻击距离"))
	float MaximumRange = 940.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "攻击距离最小系数"))
	float AttackRangeMinFactor = 0.40f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "射击冷却时间"))
	float NextShotCooldown = 0.70f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "弹丸速度"))
	float ProjectileSpeed = 980.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "弹丸伤害倍率"))
	float ProjectileDamageMultiplier = 0.42f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "大型弹丸视觉"))
	bool bLargeVisual = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "生成枪口特效"))
	bool bSpawnMuzzleEffect = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "枪口特效缩放"))
	FVector MuzzleEffectScale = FVector(0.18f, 0.18f, 0.18f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "枪口特效持续时间"))
	float MuzzleEffectLifetime = 0.10f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "散射弹丸数量"))
	int32 SpreadShotCount = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "散射角度步长"))
	float SpreadAngleStep = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "散射生成偏移"))
	float SpreadSpawnOffset = 12.0f;
};

/** 震荡柱（ShockPillar）专属属性 - 固定柱体 + 地面波参数 */
USTRUCT(BlueprintType)
struct AI_API FRogueShockPillarSpecialParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "地面波", meta = (DisplayName = "初始冷却最小值"))
	float InitialCooldownMin = 1.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "地面波", meta = (DisplayName = "初始冷却最大值"))
	float InitialCooldownMax = 3.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "地面波", meta = (DisplayName = "攻击冷却时间"))
	float AttackCooldown = 4.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "地面波", meta = (DisplayName = "最大半径"))
	float MaxRadius = 980.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "地面波", meta = (DisplayName = "最短扩散持续时间"))
	float ExpansionDuration = 1.05f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "地面波", meta = (DisplayName = "扩散速度（单位/秒）"))
	float ExpansionSpeed = 2600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "地面波", meta = (DisplayName = "命中厚度"))
	float HitThickness = 92.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "地面波", meta = (DisplayName = "伤害倍率"))
	float DamageMultiplier = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "地面波", meta = (DisplayName = "跳跃可躲避"))
	bool bJumpCanDodge = true;
};

// ===================================================================
//  Boss 覆盖参数（可选，用于覆盖普通敌人的专属参数）
// ===================================================================

/** Boss 覆盖参数 - 当敌人作为 Boss 时的属性覆盖 */
USTRUCT(BlueprintType)
struct AI_API FRogueEnemyBossOverride
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "启用Boss覆盖"))
	bool bEnableBossOverride = false;

	// --- 射手类Boss专属覆盖 ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RangedBossOverride", meta = (DisplayName = "Boss偏好距离"))
	float BossPreferredRange = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RangedBossOverride", meta = (DisplayName = "Boss近距离"))
	float BossCloseDistance = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RangedBossOverride", meta = (DisplayName = "Boss最大攻击距离"))
	float BossMaximumRange = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RangedBossOverride", meta = (DisplayName = "Boss射击冷却时间"))
	float BossNextShotCooldown = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RangedBossOverride", meta = (DisplayName = "Boss弹丸速度"))
	float BossProjectileSpeed = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RangedBossOverride", meta = (DisplayName = "Boss弹丸伤害倍率"))
	float BossProjectileDamageMultiplier = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RangedBossOverride", meta = (DisplayName = "Boss大型弹丸视觉"))
	bool bBossLargeVisual = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "RangedBossOverride", meta = (DisplayName = "Boss生成枪口特效"))
	bool bBossSpawnMuzzleEffect = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpreadBossOverride", meta = (DisplayName = "Boss散射角度步长"))
	float BossSpreadAngleStep = 0.0f;
};

// ===================================================================
//  DataTable 行结构体 - 敌人原型配置表
// ===================================================================

/** 敌人原型配置表行 - 用于 DataTable 配置每种敌人的完整属性 */
USTRUCT(BlueprintType)
struct AI_API FRogueEnemyArchetypeRow : public FTableRowBase
{
	GENERATED_BODY()

	// ===== 基础信息 =====
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base", meta = (DisplayName = "敌人类型"))
	ERogueEnemyType EnemyType = ERogueEnemyType::Hunter;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base", meta = (DisplayName = "移动模型"))
	ERogueEnemyMovementModel MovementModel = ERogueEnemyMovementModel::Direct;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base", meta = (DisplayName = "基础移动倍率"))
	float BaseMoveMultiplier = 1.0f;

	// ===== 各敌人专属属性（仅对应类型生效） =====
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpecialParams|Charger", meta = (DisplayName = "冲锋者专属参数"))
	FRogueChargerSpecialParams ChargerParams;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpecialParams|Tank", meta = (DisplayName = "坦克专属参数"))
	FRogueTankSpecialParams TankParams;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpecialParams|Orbiter", meta = (DisplayName = "环绕者专属参数"))
	FRogueOrbiterSpecialParams OrbiterParams;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpecialParams|Shooter", meta = (DisplayName = "射手专属参数"))
	FRogueShooterSpecialParams ShooterParams;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpecialParams|Skirmisher", meta = (DisplayName = "游击者专属参数"))
	FRogueSkirmisherSpecialParams SkirmisherParams;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpecialParams|Artillery", meta = (DisplayName = "炮兵专属参数"))
	FRogueArtillerySpecialParams ArtilleryParams;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpecialParams|Spitter", meta = (DisplayName = "喷射者专属参数"))
	FRogueSpitterSpecialParams SpitterParams;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "专属参数|震荡柱", meta = (DisplayName = "震荡柱专属参数"))
	FRogueShockPillarSpecialParams ShockPillarParams;

	// ===== Boss 覆盖 =====
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BossOverride", meta = (DisplayName = "Boss覆盖参数"))
	FRogueEnemyBossOverride BossOverride;

	/** 将此行数据转换为运行时 FRogueEnemyArchetype 结构体 */
	FRogueEnemyArchetype ToArchetype(bool bIsBoss) const;
};
