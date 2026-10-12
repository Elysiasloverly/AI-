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
	float JumpZVelocity = 720.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "跳跃", meta = (DisplayName = "空中控制力"))
	float AirControl = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "跳跃", meta = (DisplayName = "重力倍率"))
	float GravityScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "跳跃", meta = (DisplayName = "最大跳跃次数", ClampMin = "0", UIMin = "0"))
	int32 JumpMaxCount = 1;
};

/**
 * 12 个**武器加成属性**的上限。
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
 * `PreAttributeChange` 里这 12 个属性原本**只有下限 0、没有上限**，
 * 也就是升级卡买多少次就叠多少。想给某种加成封顶就在这里填。
 *
 * ⚠️ 数量类（后 7 项）会再取整，所以上限建议填整数。
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
};

UCLASS(BlueprintType)
class AI_API URoguePlayerBalanceAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	URoguePlayerBalanceAsset();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "属性", meta = (DisplayName = "恢复默认玩家数值"))
	void ResetToDefaults();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "属性", meta = (DisplayName = "基础属性配置"))
	FRoguePlayerBaseStatConfig BaseStats;

	/** 武器加成属性的上限（**-1 = 不设上限**，默认全 -1）。
	 *  作用在**属性层**：升级卡往这些加成属性上累加时会被它夹住。详见结构体注释。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "属性", meta = (DisplayName = "武器加成上限(-1=不限)"))
	FRogueWeaponBonusCaps WeaponBonusCaps;

	// 武器配置已迁移到 DataTable（DT_WeaponConfig），此处不再包含武器字段
};
