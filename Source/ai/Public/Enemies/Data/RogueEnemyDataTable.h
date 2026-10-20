
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Shared/RogueTypes.h"
#include "Enemies/Data/RogueEnemyArchetypes.h"
#include "RogueEnemyDataTable.generated.h"

class ARogueEnemyProjectile;

// ===================================================================
//  每种敌人的专属参数结构体 —— 一个怪物一个块，名字与 ERogueEnemyType 一一对应
//
//  两种字段语义（ToolTip 里有标注）：
//    · 直接覆盖：字段值无条件覆盖类型基线（沿用旧共享块的语义，默认值 = 该怪非 Boss 基线）；
//    · 哨兵覆盖：0 / 不填 = 跟随类型基线（含 Boss 基线差分），> 0 才覆盖。
//      新增块/新增段用哨兵语义，这样老资产不填就保持原行为，Boss 也不会被普通值抹平。
//
//  ⚠️ 距离类字段（偏好距离 / 远距离 / 近距离 / 各种权重）只参与【走位】计算
//     （GetMovementDirection），决定"想站多远、什么时候绕圈"，
//     不参与攻击判定：攻击距离门控已取消，敌人在任何距离都会尝试开火。
// ===================================================================

/** 移动模型覆盖：行级开关，默认跟随 EnemyType 的 C++ 基线。 */
UENUM(BlueprintType)
enum class ERogueEnemyMovementModelOverride : uint8
{
	FromBaseline UMETA(DisplayName = "类型基线（默认）"),
	Direct       UMETA(DisplayName = "直线追击"),
	BurstCharge  UMETA(DisplayName = "爆发冲刺"),
	Orbit        UMETA(DisplayName = "环绕运动"),
	RangedKite   UMETA(DisplayName = "远程风筝"),
	ZigZagChase  UMETA(DisplayName = "Z字形追击"),
	Stationary   UMETA(DisplayName = "固定不动")
};

/** 妖精（Rifleman）专属参数 —— 远程风筝 + 单发。全部哨兵语义：0 = 跟随基线。 */
USTRUCT(BlueprintType)
struct AI_API FRogueRiflemanSpecialParams
{
	GENERATED_BODY()

	// --- 移动（远程风筝），0 = 跟随基线 ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ToolTip = "风筝站位偏好距离：中距离区间围绕这个距离计算朝向偏置（仅开启「使用距离偏差计算中距离朝向」时生效）。只影响走位，不影响开火——攻击距离门控已取消。（0 = 跟随类型基线，含 Boss 基线差分）", DisplayName = "偏好距离(0=基线)"))
	float PreferredRange = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ToolTip = "超过这个距离就转身追近（用远距离权重混合方向）。只影响走位，不影响开火。（0 = 跟随类型基线，含 Boss 基线差分）", DisplayName = "远距离(0=基线)"))
	float FarDistance = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ToolTip = "近于这个距离就后撤并绕圈（用近距离权重混合方向）。只影响走位，不影响开火。（0 = 跟随类型基线，含 Boss 基线差分）", DisplayName = "近距离(0=基线)"))
	float CloseDistance = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ToolTip = "远距离时朝玩家方向的权重，越大越倾向于直冲。（0 = 跟随类型基线，含 Boss 基线差分）", DisplayName = "远距离朝向权重(0=基线)"))
	float FarTowardWeight = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ToolTip = "远距离时切线（绕圈）方向的权重，越大越偏侧向。（0 = 跟随类型基线，含 Boss 基线差分）", DisplayName = "远距离切线权重(0=基线)"))
	float FarTangentWeight = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ToolTip = "近距离时朝玩家方向的权重；负值表示朝反方向后撤。（0 = 跟随类型基线，含 Boss 基线差分）", DisplayName = "近距离朝向权重(0=基线)"))
	float CloseTowardWeight = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ToolTip = "近距离时切线方向的权重，越大绕圈越强。（0 = 跟随类型基线，含 Boss 基线差分）", DisplayName = "近距离切线权重(0=基线)"))
	float CloseTangentWeight = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ToolTip = "中距离时朝玩家方向的权重；开启距离偏差后会随「当前距离 vs 偏好距离」动态缩放。（0 = 跟随类型基线，含 Boss 基线差分）", DisplayName = "中距离朝向权重(0=基线)"))
	float MidTowardWeight = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ToolTip = "中距离时切线方向的权重。（0 = 跟随类型基线，含 Boss 基线差分）", DisplayName = "中距离切线权重(0=基线)"))
	float MidTangentWeight = 0.0f;

	// --- 射击（单发），0 = 跟随基线 ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "初始冷却最小值(0=基线)"))
	float InitialCooldownMin = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "初始冷却最大值(0=基线)"))
	float InitialCooldownMax = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "射击冷却时间(0=基线)"))
	float NextShotCooldown = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "弹丸速度(0=基线)"))
	float ProjectileSpeed = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "弹丸伤害倍率(0=基线)"))
	float ProjectileDamageMultiplier = 0.0f;
};

/** 夜雀（Assault）专属参数 —— 爆发冲刺 + 散射。爆发段直接覆盖；射击段哨兵语义（0=基线）。 */
USTRUCT(BlueprintType)
struct AI_API FRogueAssaultSpecialParams
{
	GENERATED_BODY()

	// --- 移动（爆发冲刺），直接覆盖 ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ToolTip = "爆发周期（秒）：两次冲刺之间完整周期长度。", DisplayName = "爆发周期时间"))
	float BurstCycleTime = 2.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ToolTip = "爆发持续（秒）：一次冲刺持续多久。", DisplayName = "爆发持续时间"))
	float BurstDuration = 0.9f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ToolTip = "冲刺期间的速度倍率（相对 MoveSpeed）。", DisplayName = "爆发激活速度倍率"))
	float BurstActiveMultiplier = 2.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ToolTip = "非冲刺期间的速度倍率（相对 MoveSpeed）。", DisplayName = "爆发空闲速度倍率"))
	float BurstIdleMultiplier = 1.0f;

	// --- 射击（散射），0 = 跟随基线 ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "初始冷却最小值(0=基线)"))
	float InitialCooldownMin = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "初始冷却最大值(0=基线)"))
	float InitialCooldownMax = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "射击冷却时间(0=基线)"))
	float NextShotCooldown = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "弹丸速度(0=基线)"))
	float ProjectileSpeed = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "弹丸伤害倍率(0=基线)"))
	float ProjectileDamageMultiplier = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "散射弹丸数量(0=基线)"))
	int32 SpreadShotCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "散射角度步长(0=基线)"))
	float SpreadAngleStep = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "散射生成偏移(0=基线)"))
	float SpreadSpawnOffset = 0.0f;
};

/** 鬼（Juggernaut）专属参数 —— 直线追击（速度由顶层 BaseMoveMultiplier 控制）+ 重型单发。全部哨兵语义。 */
USTRUCT(BlueprintType)
struct AI_API FRogueJuggernautSpecialParams
{
	GENERATED_BODY()

	// --- 射击（重型单发），0 = 跟随基线 ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "初始冷却最小值(0=基线)"))
	float InitialCooldownMin = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "初始冷却最大值(0=基线)"))
	float InitialCooldownMax = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "射击冷却时间(0=基线)"))
	float NextShotCooldown = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "弹丸速度(0=基线)"))
	float ProjectileSpeed = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "弹丸伤害倍率(0=基线)"))
	float ProjectileDamageMultiplier = 0.0f;
};

/** 人魂（Orbiter）专属参数 —— 环绕运动 + 单发。移动段直接覆盖；射击段哨兵语义（0=基线）。 */
USTRUCT(BlueprintType)
struct AI_API FRogueOrbiterSpecialParams
{
	GENERATED_BODY()

	// --- 移动（环绕），直接覆盖 ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ToolTip = "超过这个距离就转身追近（用远距离权重混合方向）。只影响走位，不影响开火。", DisplayName = "远距离"))
	float FarDistance = 700.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ToolTip = "近于这个距离就后撤并绕圈（用近距离权重混合方向）。只影响走位，不影响开火。", DisplayName = "近距离"))
	float CloseDistance = 420.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ToolTip = "远距离时朝玩家方向的权重，越大越倾向于直冲。", DisplayName = "远距离朝向权重"))
	float FarTowardWeight = 0.75f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ToolTip = "远距离时切线（绕圈）方向的权重，越大越偏侧向。", DisplayName = "远距离切线权重"))
	float FarTangentWeight = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ToolTip = "近距离时朝玩家方向的权重；负值表示朝反方向后撤。", DisplayName = "近距离朝向权重"))
	float CloseTowardWeight = -0.55f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ToolTip = "近距离时切线方向的权重，越大绕圈越强。", DisplayName = "近距离切线权重"))
	float CloseTangentWeight = 0.80f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ToolTip = "中距离时朝玩家方向的权重；开启距离偏差后会随「当前距离 vs 偏好距离」动态缩放。", DisplayName = "中距离朝向权重"))
	float MidTowardWeight = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ToolTip = "中距离时切线方向的权重。", DisplayName = "中距离切线权重"))
	float MidTangentWeight = 0.85f;

	// --- 射击（单发），0 = 跟随基线 ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "初始冷却最小值(0=基线)"))
	float InitialCooldownMin = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "初始冷却最大值(0=基线)"))
	float InitialCooldownMax = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "射击冷却时间(0=基线)"))
	float NextShotCooldown = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "弹丸速度(0=基线)"))
	float ProjectileSpeed = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "弹丸伤害倍率(0=基线)"))
	float ProjectileDamageMultiplier = 0.0f;
};

/** 鸦天狗（Marksman）专属参数 —— 远程风筝 + 单发。全部直接覆盖，默认值 = 鸦天狗非 Boss 基线。 */
USTRUCT(BlueprintType)
struct AI_API FRogueMarksmanSpecialParams
{
	GENERATED_BODY()

	// --- 移动参数 ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ToolTip = "风筝站位偏好距离：中距离区间围绕这个距离计算朝向偏置（仅开启「使用距离偏差计算中距离朝向」时生效）。只影响走位，不影响开火——攻击距离门控已取消。", DisplayName = "偏好距离"))
	float PreferredRange = 760.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ToolTip = "超过这个距离就转身追近（用远距离权重混合方向）。只影响走位，不影响开火。", DisplayName = "远距离(0=偏好距离+180)"))
	float FarDistance = 940.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ToolTip = "近于这个距离就后撤并绕圈（用近距离权重混合方向）。只影响走位，不影响开火。", DisplayName = "近距离"))
	float CloseDistance = 420.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ToolTip = "远距离时朝玩家方向的权重，越大越倾向于直冲。", DisplayName = "远距离朝向权重"))
	float FarTowardWeight = 0.92f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ToolTip = "远距离时切线（绕圈）方向的权重，越大越偏侧向。", DisplayName = "远距离切线权重"))
	float FarTangentWeight = 0.18f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ToolTip = "近距离时朝玩家方向的权重；负值表示朝反方向后撤。", DisplayName = "近距离朝向权重"))
	float CloseTowardWeight = -0.90f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ToolTip = "近距离时切线方向的权重，越大绕圈越强。", DisplayName = "近距离切线权重"))
	float CloseTangentWeight = 0.45f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ToolTip = "中距离时朝玩家方向的权重；开启距离偏差后会随「当前距离 vs 偏好距离」动态缩放。", DisplayName = "中距离朝向权重"))
	float MidTowardWeight = 0.30f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ToolTip = "中距离时切线方向的权重。", DisplayName = "中距离切线权重"))
	float MidTangentWeight = 0.90f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ToolTip = "中距离朝向权重是否随「当前距离 vs 偏好距离」做偏置：越贴近玩家，朝向权重越趋向负值（后退）。", DisplayName = "使用距离偏差计算中距离朝向"))
	bool bUseRangeBiasForMidToward = true;

	// --- 射击参数 ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "射击模式"))
	ERogueEnemyShotPattern ShotPattern = ERogueEnemyShotPattern::Single;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "初始冷却最小值"))
	float InitialCooldownMin = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "初始冷却最大值"))
	float InitialCooldownMax = 0.85f;

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

/** 妖狐（Skirmisher）专属参数 —— Z 字形追击 + 散射。移动段直接覆盖；射击段哨兵语义（0=基线）。 */
USTRUCT(BlueprintType)
struct AI_API FRogueSkirmisherSpecialParams
{
	GENERATED_BODY()

	// --- 移动（Z 字），直接覆盖 ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ToolTip = "近于这个距离就后撤并绕圈（用近距离权重混合方向）。只影响走位，不影响开火。", DisplayName = "近距离"))
	float CloseDistance = 220.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ToolTip = "近距离时朝玩家方向的权重；负值表示朝反方向后撤。", DisplayName = "近距离朝向权重"))
	float CloseTowardWeight = -0.30f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ToolTip = "近距离时切线方向的权重，越大绕圈越强。", DisplayName = "近距离切线权重"))
	float CloseTangentWeight = 0.95f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ToolTip = "Z 字摆动频率，越大摆动越快。", DisplayName = "Z字形频率"))
	float ZigZagFrequency = 7.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ToolTip = "Z 字追击时朝玩家方向的权重。", DisplayName = "Z字形朝向权重"))
	float ZigZagTowardWeight = 0.96f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ToolTip = "Z 字追击时切线摆动的强度。", DisplayName = "Z字形切线权重"))
	float ZigZagTangentWeight = 0.82f;

	// --- 射击（散射），0 = 跟随基线 ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "初始冷却最小值(0=基线)"))
	float InitialCooldownMin = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "初始冷却最大值(0=基线)"))
	float InitialCooldownMax = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "射击冷却时间(0=基线)"))
	float NextShotCooldown = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "弹丸速度(0=基线)"))
	float ProjectileSpeed = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "弹丸伤害倍率(0=基线)"))
	float ProjectileDamageMultiplier = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "散射弹丸数量(0=基线)"))
	int32 SpreadShotCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "散射角度步长(0=基线)"))
	float SpreadAngleStep = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "散射生成偏移(0=基线)"))
	float SpreadSpawnOffset = 0.0f;
};

/** 山蜘蛛（Cannoneer）专属参数 —— 远程风筝 + 重型单发。全部直接覆盖，默认值 = 山蜘蛛非 Boss 基线。 */
USTRUCT(BlueprintType)
struct AI_API FRogueCannoneerSpecialParams
{
	GENERATED_BODY()

	// --- 移动参数 ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ToolTip = "风筝站位偏好距离：中距离区间围绕这个距离计算朝向偏置（仅开启「使用距离偏差计算中距离朝向」时生效）。只影响走位，不影响开火——攻击距离门控已取消。", DisplayName = "偏好距离"))
	float PreferredRange = 1120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ToolTip = "超过这个距离就转身追近（用远距离权重混合方向）。只影响走位，不影响开火。", DisplayName = "远距离(0=偏好距离+240)"))
	float FarDistance = 1360.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ToolTip = "近于这个距离就后撤并绕圈（用近距离权重混合方向）。只影响走位，不影响开火。", DisplayName = "近距离"))
	float CloseDistance = 620.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ToolTip = "远距离时朝玩家方向的权重，越大越倾向于直冲。", DisplayName = "远距离朝向权重"))
	float FarTowardWeight = 0.90f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ToolTip = "远距离时切线（绕圈）方向的权重，越大越偏侧向。", DisplayName = "远距离切线权重"))
	float FarTangentWeight = 0.16f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ToolTip = "近距离时朝玩家方向的权重；负值表示朝反方向后撤。", DisplayName = "近距离朝向权重"))
	float CloseTowardWeight = -0.96f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ToolTip = "近距离时切线方向的权重，越大绕圈越强。", DisplayName = "近距离切线权重"))
	float CloseTangentWeight = 0.28f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ToolTip = "中距离时朝玩家方向的权重；开启距离偏差后会随「当前距离 vs 偏好距离」动态缩放。", DisplayName = "中距离朝向权重"))
	float MidTowardWeight = 0.08f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ToolTip = "中距离时切线方向的权重。", DisplayName = "中距离切线权重"))
	float MidTangentWeight = 0.52f;

	// --- 射击参数 ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "射击模式"))
	ERogueEnemyShotPattern ShotPattern = ERogueEnemyShotPattern::Single;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "初始冷却最小值"))
	float InitialCooldownMin = 0.75f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "初始冷却最大值"))
	float InitialCooldownMax = 1.30f;

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

/** 唐伞（Scatterer）专属参数 —— 远程风筝 + 散射。全部直接覆盖，默认值 = 唐伞非 Boss 基线。 */
USTRUCT(BlueprintType)
struct AI_API FRogueScattererSpecialParams
{
	GENERATED_BODY()

	// --- 移动参数 ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ToolTip = "风筝站位偏好距离：中距离区间围绕这个距离计算朝向偏置（仅开启「使用距离偏差计算中距离朝向」时生效）。只影响走位，不影响开火——攻击距离门控已取消。", DisplayName = "偏好距离"))
	float PreferredRange = 540.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ToolTip = "超过这个距离就转身追近（用远距离权重混合方向）。只影响走位，不影响开火。", DisplayName = "远距离(0=偏好距离+120)"))
	float FarDistance = 660.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ToolTip = "近于这个距离就后撤并绕圈（用近距离权重混合方向）。只影响走位，不影响开火。", DisplayName = "近距离"))
	float CloseDistance = 260.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ToolTip = "远距离时朝玩家方向的权重，越大越倾向于直冲。", DisplayName = "远距离朝向权重"))
	float FarTowardWeight = 0.98f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ToolTip = "远距离时切线（绕圈）方向的权重，越大越偏侧向。", DisplayName = "远距离切线权重"))
	float FarTangentWeight = 0.26f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ToolTip = "近距离时朝玩家方向的权重；负值表示朝反方向后撤。", DisplayName = "近距离朝向权重"))
	float CloseTowardWeight = -0.72f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ToolTip = "近距离时切线方向的权重，越大绕圈越强。", DisplayName = "近距离切线权重"))
	float CloseTangentWeight = 0.78f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ToolTip = "中距离时朝玩家方向的权重；开启距离偏差后会随「当前距离 vs 偏好距离」动态缩放。", DisplayName = "中距离朝向权重"))
	float MidTowardWeight = 0.20f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ToolTip = "中距离时切线方向的权重。", DisplayName = "中距离切线权重"))
	float MidTangentWeight = 0.96f;

	// --- 射击参数 ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "射击模式"))
	ERogueEnemyShotPattern ShotPattern = ERogueEnemyShotPattern::Spread3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "初始冷却最小值"))
	float InitialCooldownMin = 0.18f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "初始冷却最大值"))
	float InitialCooldownMax = 0.48f;

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

/** 地藏（ShockPillar）专属参数 —— 固定柱体 + 整圈弹幕。直接覆盖；移动模型被强制为固定不动。 */
USTRUCT(BlueprintType)
struct AI_API FRogueShockPillarSpecialParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "环形弹幕", meta = (DisplayName = "初始冷却最小值"))
	float InitialCooldownMin = 1.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "环形弹幕", meta = (DisplayName = "初始冷却最大值"))
	float InitialCooldownMax = 3.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "环形弹幕", meta = (DisplayName = "攻击冷却时间"))
	float AttackCooldown = 4.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "环形弹幕", meta = (DisplayName = "子弹参数"))
	FRogueEnemyBulletRingSettings BulletRing;
};

/** 月兔（Sniper）专属参数 —— 远程风筝 + 单发狙击。全部直接覆盖，默认值 = 月兔非 Boss 基线。 */
USTRUCT(BlueprintType)
struct AI_API FRogueSniperSpecialParams
{
	GENERATED_BODY()

	// --- 移动参数 ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ToolTip = "风筝站位偏好距离：中距离区间围绕这个距离计算朝向偏置（仅开启「使用距离偏差计算中距离朝向」时生效）。只影响走位，不影响开火——攻击距离门控已取消。", DisplayName = "偏好距离"))
	float PreferredRange = 1150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ToolTip = "超过这个距离就转身追近（用远距离权重混合方向）。只影响走位，不影响开火。", DisplayName = "远距离(0=偏好距离+180)"))
	float FarDistance = 1330.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ToolTip = "近于这个距离就后撤并绕圈（用近距离权重混合方向）。只影响走位，不影响开火。", DisplayName = "近距离"))
	float CloseDistance = 680.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ToolTip = "远距离时朝玩家方向的权重，越大越倾向于直冲。", DisplayName = "远距离朝向权重"))
	float FarTowardWeight = 0.98f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ToolTip = "远距离时切线（绕圈）方向的权重，越大越偏侧向。", DisplayName = "远距离切线权重"))
	float FarTangentWeight = 0.12f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ToolTip = "近距离时朝玩家方向的权重；负值表示朝反方向后撤。", DisplayName = "近距离朝向权重"))
	float CloseTowardWeight = -0.95f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ToolTip = "近距离时切线方向的权重，越大绕圈越强。", DisplayName = "近距离切线权重"))
	float CloseTangentWeight = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ToolTip = "中距离时朝玩家方向的权重；开启距离偏差后会随「当前距离 vs 偏好距离」动态缩放。", DisplayName = "中距离朝向权重"))
	float MidTowardWeight = 0.55f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ToolTip = "中距离时切线方向的权重。", DisplayName = "中距离切线权重"))
	float MidTangentWeight = 0.65f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ToolTip = "中距离朝向权重是否随「当前距离 vs 偏好距离」做偏置：越贴近玩家，朝向权重越趋向负值（后退）。", DisplayName = "使用距离偏差计算中距离朝向"))
	bool bUseRangeBiasForMidToward = false;

	// --- 射击参数 ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "射击模式"))
	ERogueEnemyShotPattern ShotPattern = ERogueEnemyShotPattern::Single;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "初始冷却最小值"))
	float InitialCooldownMin = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "初始冷却最大值"))
	float InitialCooldownMax = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "射击冷却时间"))
	float NextShotCooldown = 2.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "弹丸速度"))
	float ProjectileSpeed = 1700.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "弹丸伤害倍率"))
	float ProjectileDamageMultiplier = 1.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "大型弹丸视觉"))
	bool bLargeVisual = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "生成枪口特效"))
	bool bSpawnMuzzleEffect = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "枪口特效缩放"))
	FVector MuzzleEffectScale = FVector(0.30f, 0.30f, 0.30f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "枪口特效持续时间"))
	float MuzzleEffectLifetime = 0.16f;
};

/** 毒茸（Shotgunner）专属参数 —— 远程风筝 + 散射（5 发）。全部直接覆盖，默认值 = 毒茸非 Boss 基线。 */
USTRUCT(BlueprintType)
struct AI_API FRogueShotgunnerSpecialParams
{
	GENERATED_BODY()

	// --- 移动参数 ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ToolTip = "风筝站位偏好距离：中距离区间围绕这个距离计算朝向偏置（仅开启「使用距离偏差计算中距离朝向」时生效）。只影响走位，不影响开火——攻击距离门控已取消。", DisplayName = "偏好距离"))
	float PreferredRange = 480.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ToolTip = "超过这个距离就转身追近（用远距离权重混合方向）。只影响走位，不影响开火。", DisplayName = "远距离(0=偏好距离+100)"))
	float FarDistance = 580.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ToolTip = "近于这个距离就后撤并绕圈（用近距离权重混合方向）。只影响走位，不影响开火。", DisplayName = "近距离"))
	float CloseDistance = 250.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ToolTip = "远距离时朝玩家方向的权重，越大越倾向于直冲。", DisplayName = "远距离朝向权重"))
	float FarTowardWeight = 0.98f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ToolTip = "远距离时切线（绕圈）方向的权重，越大越偏侧向。", DisplayName = "远距离切线权重"))
	float FarTangentWeight = 0.20f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ToolTip = "近距离时朝玩家方向的权重；负值表示朝反方向后撤。", DisplayName = "近距离朝向权重"))
	float CloseTowardWeight = -0.78f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ToolTip = "近距离时切线方向的权重，越大绕圈越强。", DisplayName = "近距离切线权重"))
	float CloseTangentWeight = 0.72f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ToolTip = "中距离时朝玩家方向的权重；开启距离偏差后会随「当前距离 vs 偏好距离」动态缩放。", DisplayName = "中距离朝向权重"))
	float MidTowardWeight = 0.18f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ToolTip = "中距离时切线方向的权重。", DisplayName = "中距离切线权重"))
	float MidTangentWeight = 0.94f;

	// --- 射击参数 ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "射击模式"))
	ERogueEnemyShotPattern ShotPattern = ERogueEnemyShotPattern::Spread3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "初始冷却最小值"))
	float InitialCooldownMin = 0.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "初始冷却最大值"))
	float InitialCooldownMax = 0.85f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "射击冷却时间"))
	float NextShotCooldown = 1.65f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "弹丸速度"))
	float ProjectileSpeed = 860.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "弹丸伤害倍率"))
	float ProjectileDamageMultiplier = 0.30f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "大型弹丸视觉"))
	bool bLargeVisual = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "生成枪口特效"))
	bool bSpawnMuzzleEffect = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "枪口特效缩放"))
	FVector MuzzleEffectScale = FVector(0.20f, 0.20f, 0.20f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "枪口特效持续时间"))
	float MuzzleEffectLifetime = 0.10f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "散射弹丸数量"))
	int32 SpreadShotCount = 5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "散射角度步长"))
	float SpreadAngleStep = 11.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "散射生成偏移"))
	float SpreadSpawnOffset = 10.0f;
};

/** 血蝠（Repeater）专属参数 —— 远程风筝 + 单发速射。全部直接覆盖，默认值 = 血蝠非 Boss 基线。 */
USTRUCT(BlueprintType)
struct AI_API FRogueRepeaterSpecialParams
{
	GENERATED_BODY()

	// --- 移动参数 ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ToolTip = "风筝站位偏好距离：中距离区间围绕这个距离计算朝向偏置（仅开启「使用距离偏差计算中距离朝向」时生效）。只影响走位，不影响开火——攻击距离门控已取消。", DisplayName = "偏好距离"))
	float PreferredRange = 700.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ToolTip = "超过这个距离就转身追近（用远距离权重混合方向）。只影响走位，不影响开火。", DisplayName = "远距离(0=偏好距离+160)"))
	float FarDistance = 860.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ToolTip = "近于这个距离就后撤并绕圈（用近距离权重混合方向）。只影响走位，不影响开火。", DisplayName = "近距离"))
	float CloseDistance = 380.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ToolTip = "远距离时朝玩家方向的权重，越大越倾向于直冲。", DisplayName = "远距离朝向权重"))
	float FarTowardWeight = 0.90f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ToolTip = "远距离时切线（绕圈）方向的权重，越大越偏侧向。", DisplayName = "远距离切线权重"))
	float FarTangentWeight = 0.22f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ToolTip = "近距离时朝玩家方向的权重；负值表示朝反方向后撤。", DisplayName = "近距离朝向权重"))
	float CloseTowardWeight = -0.82f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ToolTip = "近距离时切线方向的权重，越大绕圈越强。", DisplayName = "近距离切线权重"))
	float CloseTangentWeight = 0.55f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ToolTip = "中距离时朝玩家方向的权重；开启距离偏差后会随「当前距离 vs 偏好距离」动态缩放。", DisplayName = "中距离朝向权重"))
	float MidTowardWeight = 0.26f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ToolTip = "中距离时切线方向的权重。", DisplayName = "中距离切线权重"))
	float MidTangentWeight = 0.88f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ToolTip = "中距离朝向权重是否随「当前距离 vs 偏好距离」做偏置：越贴近玩家，朝向权重越趋向负值（后退）。", DisplayName = "使用距离偏差计算中距离朝向"))
	bool bUseRangeBiasForMidToward = false;

	// --- 射击参数 ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "射击模式"))
	ERogueEnemyShotPattern ShotPattern = ERogueEnemyShotPattern::Single;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "初始冷却最小值"))
	float InitialCooldownMin = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "初始冷却最大值"))
	float InitialCooldownMax = 0.55f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "射击冷却时间"))
	float NextShotCooldown = 0.48f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "弹丸速度"))
	float ProjectileSpeed = 1180.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "弹丸伤害倍率"))
	float ProjectileDamageMultiplier = 0.28f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "大型弹丸视觉"))
	bool bLargeVisual = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "生成枪口特效"))
	bool bSpawnMuzzleEffect = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "枪口特效缩放"))
	FVector MuzzleEffectScale = FVector(0.16f, 0.16f, 0.16f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged", meta = (DisplayName = "枪口特效持续时间"))
	float MuzzleEffectLifetime = 0.08f;
};

/** 亡灵（Ringcaster）专属参数 —— 远程风筝 + 整圈弹幕。弹幕段直接覆盖；移动段哨兵语义（0=基线）。 */
USTRUCT(BlueprintType)
struct AI_API FRogueRingcasterSpecialParams
{
	GENERATED_BODY()

	/** 弹幕默认值 = 亡灵非 Boss 基线（16 发 / 760 弹速 / 3.2s 寿命 / 0.32 缩放 / 0.28 伤害 / 高 50）。 */
	FRogueRingcasterSpecialParams()
	{
		BulletRing.ProjectileCount = 16;
		BulletRing.ProjectileSpeed = 760.0f;
		BulletRing.ProjectileLifetime = 3.2f;
		BulletRing.ProjectileScale = 0.32f;
		BulletRing.DamageMultiplier = 0.28f;
		BulletRing.HeightAboveGround = 50.0f;
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "环形弹幕", meta = (DisplayName = "初始冷却最小值"))
	float InitialCooldownMin = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "环形弹幕", meta = (DisplayName = "初始冷却最大值"))
	float InitialCooldownMax = 2.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "环形弹幕", meta = (DisplayName = "攻击冷却时间"))
	float AttackCooldown = 3.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "环形弹幕", meta = (DisplayName = "子弹参数"))
	FRogueEnemyBulletRingSettings BulletRing;

	// --- 移动（远程风筝），0 = 跟随基线 ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ToolTip = "风筝站位偏好距离：中距离区间围绕这个距离计算朝向偏置（仅开启「使用距离偏差计算中距离朝向」时生效）。只影响走位，不影响开火——攻击距离门控已取消。（0 = 跟随类型基线，含 Boss 基线差分）", DisplayName = "偏好距离(0=基线)"))
	float PreferredRange = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ToolTip = "超过这个距离就转身追近（用远距离权重混合方向）。只影响走位，不影响开火。（0 = 跟随类型基线，含 Boss 基线差分）", DisplayName = "远距离(0=基线)"))
	float FarDistance = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ToolTip = "近于这个距离就后撤并绕圈（用近距离权重混合方向）。只影响走位，不影响开火。（0 = 跟随类型基线，含 Boss 基线差分）", DisplayName = "近距离(0=基线)"))
	float CloseDistance = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ToolTip = "远距离时朝玩家方向的权重，越大越倾向于直冲。（0 = 跟随类型基线，含 Boss 基线差分）", DisplayName = "远距离朝向权重(0=基线)"))
	float FarTowardWeight = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ToolTip = "远距离时切线（绕圈）方向的权重，越大越偏侧向。（0 = 跟随类型基线，含 Boss 基线差分）", DisplayName = "远距离切线权重(0=基线)"))
	float FarTangentWeight = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ToolTip = "近距离时朝玩家方向的权重；负值表示朝反方向后撤。（0 = 跟随类型基线，含 Boss 基线差分）", DisplayName = "近距离朝向权重(0=基线)"))
	float CloseTowardWeight = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ToolTip = "近距离时切线方向的权重，越大绕圈越强。（0 = 跟随类型基线，含 Boss 基线差分）", DisplayName = "近距离切线权重(0=基线)"))
	float CloseTangentWeight = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ToolTip = "中距离时朝玩家方向的权重；开启距离偏差后会随「当前距离 vs 偏好距离」动态缩放。（0 = 跟随类型基线，含 Boss 基线差分）", DisplayName = "中距离朝向权重(0=基线)"))
	float MidTowardWeight = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement", meta = (ToolTip = "中距离时切线方向的权重。（0 = 跟随类型基线，含 Boss 基线差分）", DisplayName = "中距离切线权重(0=基线)"))
	float MidTangentWeight = 0.0f;
};

/** 所有敌人类型的专属参数集合：一个怪物一个子结构，名字与 ERogueEnemyType 一一对应。
 *  表里只需要填 EnemyType 对应的子结构，其余保持默认。 */
USTRUCT(BlueprintType)
struct AI_API FRogueEnemySpecialParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "专属参数|妖精", meta = (DisplayName = "妖精"))
	FRogueRiflemanSpecialParams Rifleman;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "专属参数|夜雀", meta = (DisplayName = "夜雀"))
	FRogueAssaultSpecialParams Assault;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "专属参数|鬼", meta = (DisplayName = "鬼"))
	FRogueJuggernautSpecialParams Juggernaut;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "专属参数|人魂", meta = (DisplayName = "人魂"))
	FRogueOrbiterSpecialParams Orbiter;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "专属参数|鸦天狗", meta = (DisplayName = "鸦天狗"))
	FRogueMarksmanSpecialParams Marksman;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "专属参数|妖狐", meta = (DisplayName = "妖狐"))
	FRogueSkirmisherSpecialParams Skirmisher;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "专属参数|山蜘蛛", meta = (DisplayName = "山蜘蛛"))
	FRogueCannoneerSpecialParams Cannoneer;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "专属参数|唐伞", meta = (DisplayName = "唐伞"))
	FRogueScattererSpecialParams Scatterer;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "专属参数|地藏", meta = (DisplayName = "地藏"))
	FRogueShockPillarSpecialParams ShockPillar;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "专属参数|月兔", meta = (DisplayName = "月兔"))
	FRogueSniperSpecialParams Sniper;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "专属参数|毒茸", meta = (DisplayName = "毒茸"))
	FRogueShotgunnerSpecialParams Shotgunner;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "专属参数|血蝠", meta = (DisplayName = "血蝠"))
	FRogueRepeaterSpecialParams Repeater;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "专属参数|亡灵", meta = (DisplayName = "亡灵"))
	FRogueRingcasterSpecialParams Ringcaster;
};

// ===================================================================
//  Boss 覆盖参数（可选，用于覆盖普通敌人的专属参数）
// ===================================================================

/** Boss 覆盖参数 - 当敌人作为 Boss 时的属性覆盖。
 *  float 字段以 > 0 为"已设置"哨兵；bool 字段只能叠加打开（OR 语义），不能关掉基础配置。 */
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

/** 敌人原型配置表行：移动/攻击/视觉原型与生成上限；生命、伤害等奖励数值由生成方的 Profile 提供 */
USTRUCT(BlueprintType)
struct AI_API FRogueEnemyArchetypeRow : public FTableRowBase
{
	GENERATED_BODY()

	// ===== 基础信息 =====
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base", meta = (DisplayName = "敌人类型"))
	ERogueEnemyType EnemyType = ERogueEnemyType::Rifleman;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base", meta = (DisplayName = "基础移动倍率", ClampMin = "0"))
	float BaseMoveMultiplier = 1.0f;

	/** 可选攻击能力集；加载失败或未配置时按原型标志授予默认接触/远程能力。
	 *  默认 AI 只识别这三类能力槽；新增独立攻击决策仍需扩展 C++ 或蓝图调用路径。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base", meta = (DisplayName = "攻击能力集(留空=按标志推导)"))
	TSoftObjectPtr<URogueAbilitySet> AttackAbilitySet;

	/** 同时存活上限：场上这一种敌人**同时**最多存在多少只，死掉才会腾出名额。
	 *  **不填 / 填 0 = 不限制**。
	 *  与 `MaxSpawnPerWave` 的区别：
	 *    MaxAlive        —— 动态闸门，敌人死掉名额就回来了，管的是"瞬间压力"；
	 *    MaxSpawnPerWave —— 静态预算，本波刷够就没了，死了也不补，管的是"这一波的资源总量"。
	 *  两个字段可以同时填，谁先满足谁生效。Boss 不计入（Boss 有独立的生成条件）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base", meta = (DisplayName = "同时存活上限(0=不限)", ClampMin = "0"))
	int32 MaxAlive = 0;

	/** 每波刷新上限：一个波次内这一种敌人**总共**最多刷出多少只（死了也不补）。
	 *  **不填 / 填 0 = 不限制**。
	 *  与权重（`EnemyWaveEntries` 里的 Weight）的分工：
	 *    Weight    —— 决定"抽中的概率"，管不住总数，抽中多少次就刷多少只；
	 *    本字段    —— 是硬闸门，达到后本次波次内该类型直接从抽签池里移除（权重也不计入）。
	 *  计数在每次波次切换时归零；Boss 不算在内（Boss 有独立的生成条件与波次间隔）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base", meta = (DisplayName = "每波刷新上限(0=不限)", ClampMin = "0"))
	int32 MaxSpawnPerWave = 0;

	/** 这一种敌人使用的弹体蓝图；留空时回退敌人 BP 的 EnemyProjectileClass / 原生类。
	 *  同一类型可以在这里单独换弹体，不需要改敌人 BP。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base", meta = (DisplayName = "弹体蓝图类"))
	TSubclassOf<ARogueEnemyProjectile> ProjectileClass;

	/** 移动模型覆盖：默认"类型基线"（移动模型由 EnemyType 的 C++ 基线决定）。
	 *  手动指定后强制替换基线模型 —— 注意新模型会沿用本行/基线的现有参数运行，
	 *  比如把风筝怪改成环绕，环绕半径权重仍来自风筝参数，效果需实测。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base", meta = (DisplayName = "移动模型覆盖"))
	ERogueEnemyMovementModelOverride MovementModelOverride = ERogueEnemyMovementModelOverride::FromBaseline;

	// ===== 视觉 =====
	/** 普通形态视觉；表格现在直接保存，缺省时才回退 BuildEnemyArchetype 的基线。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual", meta = (DisplayName = "普通视觉"))
	FRogueEnemyVisualArchetype Visual;

	/** Boss 形态视觉；表格直接保存缩放/胶囊，避免 Boss 尺寸只写在 C++。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual", meta = (DisplayName = "Boss视觉"))
	FRogueEnemyVisualArchetype BossVisual;

	// ===== 各敌人专属属性（只填 EnemyType 对应的子结构） =====
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Base", meta = (DisplayName = "专属参数（只填当前类型）", ToolTip = "只展开 EnemyType 对应的子结构；其它子结构保持默认即可。"))
	FRogueEnemySpecialParams SpecialParams;

	// ===== Boss 覆盖 =====
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BossOverride", meta = (DisplayName = "Boss覆盖参数"))
	FRogueEnemyBossOverride BossOverride;

	/** 将此行数据转换为运行时 FRogueEnemyArchetype 结构体 */
	FRogueEnemyArchetype ToArchetype(bool bIsBoss) const;
};
