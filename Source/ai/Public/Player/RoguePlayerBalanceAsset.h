#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RoguePlayerBalanceAsset.generated.h"

USTRUCT(BlueprintType)
struct AI_API FRoguePlayerBaseStatConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "成长", meta = (DisplayName = "升级所需经验"))
	int32 ExperienceToNextLevel = 6;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "属性", meta = (DisplayName = "最大生命值"))
	float MaxHealth = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "属性", meta = (DisplayName = "最大护甲值"))
	float MaxArmor = 25.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "属性", meta = (DisplayName = "移动速度"))
	float MoveSpeed = 650.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "属性", meta = (DisplayName = "拾取范围"))
	float PickupRadius = 260.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "属性", meta = (DisplayName = "每秒生命恢复"))
	float HealthRegenPerSecond = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "属性", meta = (DisplayName = "伤害减免百分比"))
	float DamageReductionPercent = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "属性", meta = (DisplayName = "经验获取倍率"))
	float ExperienceMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "护甲", meta = (DisplayName = "护甲开始恢复延迟"))
	float ArmorRechargeDelay = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "护甲", meta = (DisplayName = "护甲恢复速度"))
	float ArmorRechargeRate = 45.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "机动", meta = (DisplayName = "冲刺冷却时间"))
	float DashCooldownDuration = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "机动", meta = (DisplayName = "冲刺最小冷却"))
	float DashMinCooldown = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "机动", meta = (DisplayName = "冲刺持续时间"))
	float DashDuration = 0.16f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "机动", meta = (DisplayName = "冲刺速度"))
	float DashSpeed = 4200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "跳跃", meta = (DisplayName = "跳跃初速度"))
	float JumpZVelocity = 680.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "跳跃", meta = (DisplayName = "空中控制力"))
	float AirControl = 0.55f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "跳跃", meta = (DisplayName = "重力倍率"))
	float GravityScale = 1.8f;

	/** 下落阶段额外乘的重力倍率；>1 让落地更干脆。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "跳跃", meta = (DisplayName = "下落重力倍率", ClampMin = "1.0", UIMin = "1.0"))
	float FallGravityMultiplier = 1.45f;

	/** 提前松开跳跃键时，上升速度保留的比例；0.45 表示砍掉 55%。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "跳跃", meta = (DisplayName = "松键跳跃衰减", ClampMin = "0.0", ClampMax = "1.0"))
	float JumpCutMultiplier = 0.45f;

	/** 落地前这段时间内按跳，会在落地瞬间自动起跳。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "跳跃", meta = (DisplayName = "跳跃输入缓冲(秒)", ClampMin = "0.0", Units = "s"))
	float JumpBufferSeconds = 0.12f;

	/** 刚离开地面后的宽限时间，仍允许起跳一次。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "跳跃", meta = (DisplayName = "土狼时间(秒)", ClampMin = "0.0", Units = "s"))
	float CoyoteTimeSeconds = 0.10f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "跳跃", meta = (DisplayName = "最大跳跃次数", ClampMin = "0", UIMin = "0"))
	int32 JumpMaxCount = 1;
};

/**
 * 14 个**武器加成属性**的上限。
 *
 * ## 语义
 *
 * **`-1` = 不设上限**（默认）。填 `>= 0` 才生效。
 * 注意与武器表 `DT_WeaponConfig` 里那组上限的区别：
 *   · 这里限制的是**属性层**的加成值（升级卡往属性上累加的那个量）；
 *   · 那里限制的是**最终生效值**（基础值 + 加成合并之后）。
 * 两者独立、可以同时用；最终值仍要过武器层那一道。
 *
 * ## 为什么要限制
 *
 * `PreAttributeChange` 里这 14 个属性原本**只有下限 0、没有上限**，
 * 也就是升级卡买多少次就叠多少。想给某种加成封顶就在这里填。
 *
 * 数量类（ProjectileCount、ScytheCount、RocketCount、LaserCount、HellTowerCount、
 * LaserRefraction、MortarCount）在属性层取整，建议填整数；MortarBlastRadius 为连续半径。
 */
USTRUCT(BlueprintType)
struct AI_API FRogueWeaponBonusCaps
{
	GENERATED_BODY()

	// 全部默认 -1 = 不设上限。每项独立。

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "武器加成上限", meta = (DisplayName = "伤害加成上限(-1=不限)"))
	float AttackPower = -1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "武器加成上限", meta = (DisplayName = "攻速加成上限(-1=不限)"))
	float AttackSpeed = -1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "武器加成上限", meta = (DisplayName = "范围加成上限(-1=不限)"))
	float AttackRange = -1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "武器加成上限", meta = (DisplayName = "弹速加成上限(-1=不限)"))
	float ProjectileSpeed = -1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "武器加成上限", meta = (DisplayName = "弹体数量加成上限(-1=不限)"))
	float ProjectileCount = -1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "武器加成上限", meta = (DisplayName = "镰刀数量加成上限(-1=不限)"))
	float ScytheCount = -1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "武器加成上限", meta = (DisplayName = "火箭数量加成上限(-1=不限)"))
	float RocketCount = -1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "武器加成上限", meta = (DisplayName = "激光数量加成上限(-1=不限)"))
	float LaserCount = -1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "武器加成上限", meta = (DisplayName = "地狱塔数量加成上限(-1=不限)"))
	float HellTowerCount = -1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "武器加成上限", meta = (DisplayName = "折射加成上限(-1=不限)"))
	float LaserRefraction = -1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "武器加成上限", meta = (DisplayName = "迫击炮数量加成上限(-1=不限)"))
	float MortarCount = -1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "武器加成上限", meta = (DisplayName = "迫击炮爆炸半径加成上限(-1=不限)"))
	float MortarBlastRadius = -1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "武器加成上限", meta = (DisplayName = "镰刀尺寸加成上限(-1=不限)", ToolTip = "限制额外尺寸比例；例如 1 表示最多增加基础尺寸的 100%，最终为基础尺寸的 2 倍。负值不限制。"))
	float ScytheSize = -1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "武器加成上限", meta = (DisplayName = "灵符连诵加成上限(-1=不限)"))
	float ProjectileBurst = -1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "武器加成上限", meta = (DisplayName = "炉心扩散加成上限(-1=不限)"))
	float RocketBlastRadius = -1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "武器加成上限", meta = (DisplayName = "神威增幅加成上限(-1=不限)"))
	float HellTowerDamageRamp = -1.0f;


	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "武器加成上限", meta = (DisplayName = "镰刀吸血上限(-1=不限)", ToolTip = "限制镰刀吸血比例；0.5 表示最多 50%。无论怎么配，最终都会钳制在 100%。"))
	float ScytheLifesteal = -1.0f;
};

/** 开局写入 GAS 的角色武器加成，仍受下方武器加成上限及武器表最终上限限制。 */
USTRUCT(BlueprintType)
struct AI_API FRogueInitialWeaponBonusConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "初始武器加成", meta = (DisplayName = "伤害加成", ToolTip = "额外伤害数值，不是百分比。例如填 5，基础伤害 20 的武器变为 25，仍受武器伤害上限限制。", ClampMin = "0", UIMin = "0"))
	float AttackPower = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "初始武器加成", meta = (DisplayName = "攻速加成", ToolTip = "攻击间隔缩短值，单位秒，不是攻速百分比。部分武器另有换算系数与最短间隔，例如激光和迫击炮按本值的 2 倍缩短间隔。", ClampMin = "0", UIMin = "0"))
	float AttackSpeed = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "初始武器加成", meta = (DisplayName = "攻击范围加成", ToolTip = "额外攻击范围，单位厘米，不是百分比。部分武器的折射范围、爆炸范围等按各自系数换算。", ClampMin = "0", UIMin = "0"))
	float AttackRange = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "初始武器加成", meta = (DisplayName = "弹速加成", ToolTip = "额外弹速，单位厘米/秒，不是百分比；按武器系数应用，例如火箭使用 0.75 倍、迫击炮使用 0.5 倍。", ClampMin = "0", UIMin = "0"))
	float ProjectileSpeed = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "初始武器加成", meta = (DisplayName = "弹体数量加成", ToolTip = "增加的弹体数量，不是最终数量。", ClampMin = "0", UIMin = "0"))
	int32 ProjectileCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "初始武器加成", meta = (DisplayName = "镰刀数量加成", ToolTip = "增加的镰刀数量，不是最终数量。", ClampMin = "0", UIMin = "0"))
	int32 ScytheCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "初始武器加成", meta = (DisplayName = "火箭数量加成", ToolTip = "增加的火箭数量，不是最终数量。", ClampMin = "0", UIMin = "0"))
	int32 RocketCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "初始武器加成", meta = (DisplayName = "激光数量加成", ToolTip = "增加的激光数量；可使原本数量为零的激光在开局解锁。", ClampMin = "0", UIMin = "0"))
	int32 LaserCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "初始武器加成", meta = (DisplayName = "地狱塔数量加成", ToolTip = "增加的地狱塔数量，不是最终数量。", ClampMin = "0", UIMin = "0"))
	int32 HellTowerCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "初始武器加成", meta = (DisplayName = "激光折射次数加成", ToolTip = "增加的折射次数；是否生效还取决于武器本身的配置。", ClampMin = "0", UIMin = "0"))
	int32 LaserRefraction = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "初始武器加成", meta = (DisplayName = "迫击炮数量加成", ToolTip = "增加的迫击炮数量，不是最终数量。", ClampMin = "0", UIMin = "0"))
	int32 MortarCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "初始武器加成", meta = (DisplayName = "迫击炮爆炸半径加成", ToolTip = "额外增加的半径，单位厘米，不是比例。", ClampMin = "0", UIMin = "0"))
	float MortarBlastRadius = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "初始武器加成", meta = (DisplayName = "镰刀尺寸加成", ToolTip = "额外尺寸比例：0.2 表示镰刀与伤害碰撞范围为基础值的 1.2 倍；不改变环绕距离。", ClampMin = "0", UIMin = "0"))
	float ScytheSize = 0.0f;

};

USTRUCT(BlueprintType)
struct AI_API FRoguePlayerProgressionConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "成长", meta = (DisplayName = "升级经验增长倍率", ToolTip = "升级后的经验需求 = 四舍五入(本次需求 × 倍率 + 固定增长)，结果至少为 1。", ClampMin = "0", UIMin = "0"))
	float ExperienceGrowthMultiplier = 1.30f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "成长", meta = (DisplayName = "每级经验固定增长", ToolTip = "在倍率计算后额外增加的经验需求。", ClampMin = "0", UIMin = "0"))
	int32 ExperienceGrowthAddend = 2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "成长", meta = (DisplayName = "初始金币", ToolTip = "仅在首次初始化玩家属性时发放，复活和重复接管不会重复发放。", ClampMin = "0", UIMin = "0"))
	int32 StartingMoney = 0;

	/** 保留原先的单精度四舍五入规则，并防止异常配置产生零需求或整数溢出。 */
	int32 GetNextExperienceRequirement(int32 CurrentRequirement) const;
};

USTRUCT(BlueprintType)
struct AI_API FRoguePlayerSpawnStateConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "出生状态", meta = (DisplayName = "初始生命比例", ToolTip = "1 为满血，0.5 为一半生命。出生时至少保留 1 点生命，并受最大生命值限制。", ClampMin = "0", ClampMax = "1", UIMin = "0", UIMax = "1"))
	float HealthRatio = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "出生状态", meta = (DisplayName = "初始护甲比例", ToolTip = "1 为满护甲，0 为无护甲，仅在首次初始化时生效。", ClampMin = "0", ClampMax = "1", UIMin = "0", UIMax = "1"))
	float ArmorRatio = 1.0f;
};

USTRUCT(BlueprintType)
struct AI_API FRoguePlayerRevivalConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "濒死与复活", meta = (DisplayName = "濒死等待时间（秒）", ToolTip = "由服务器计时；濒死期间关闭碰撞且不被索敌。全员濒死仍立即结束游戏。最小 0.01 秒。", ClampMin = "0.01", UIMin = "0.01", Units = "s"))
	float DownedDuration = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "濒死与复活", meta = (DisplayName = "复活生命比例", ToolTip = "按复活时的最大生命计算，至少恢复 1 点，并受最大生命限制；不改变现有护甲值。", ClampMin = "0", ClampMax = "1", UIMin = "0", UIMax = "1"))
	float HealthRatio = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "濒死与复活", meta = (DisplayName = "复活无敌时间（秒）", ToolTip = "复活后免疫伤害的时间；0 表示无保护。", ClampMin = "0", UIMin = "0", Units = "s"))
	float InvulnerableSeconds = 1.5f;
};

UCLASS(BlueprintType)
class AI_API URoguePlayerBalanceAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	URoguePlayerBalanceAsset();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "属性", meta = (DisplayName = "恢复默认玩家数值"))
	void ResetToDefaults();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "01 基础属性", meta = (DisplayName = "基础属性配置", ToolTip = "开局写入 GAS 的属性与跳跃参数；武器本身的基础参数在 DT_WeaponConfig 中设置。"))
	FRoguePlayerBaseStatConfig BaseStats;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "02 出生状态", meta = (DisplayName = "初始生命与护甲"))
	FRoguePlayerSpawnStateConfig SpawnState;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "03 等级与金币", meta = (DisplayName = "成长规则"))
	FRoguePlayerProgressionConfig Progression;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "04 濒死与复活", meta = (DisplayName = "濒死与复活规则"))
	FRoguePlayerRevivalConfig Revival;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "05 初始武器加成", meta = (DisplayName = "初始武器加成", ToolTip = "角色开局的额外加成，不替代武器表基础值；默认全零。"))
	FRogueInitialWeaponBonusConfig InitialWeaponBonuses;

	/** 武器加成属性的上限（**-1 = 不设上限**，默认全 -1）。
	 *  作用在**属性层**：升级卡往这些加成属性上累加时会被它夹住。详见结构体注释。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "06 武器加成上限", meta = (DisplayName = "武器加成上限（-1 为不限）"))
	FRogueWeaponBonusCaps WeaponBonusCaps;

	// 武器基础参数继续由 DT_WeaponConfig 管理，避免同一参数出现两份配置来源。
};
