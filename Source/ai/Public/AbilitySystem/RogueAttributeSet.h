#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystem/RogueAttributeMacros.h"
#include "AbilitySystemComponent.h"
#include "RogueAttributeSet.generated.h"

/**
 * 玩家属性集（GAS 版）。
 *
 * 取代 `FRoguePlayerAttributeSet` + `FRoguePlayerAttributeSystem`（自研）。
 *
 * 和旧系统最重要的三个差别：
 *   1) **不再有"累加器 + Base 组再补一遍"的双重计数**。
 *      旧系统用 `*Attributes = {}` 复位、再让 BaseGroup 的 Additive 修饰器补一遍，
 *      于是必须把字段默认值全改成 0 才不会翻倍（项目曾经的 P0-1 bug）。
 *      GAS 里属性有独立的 `BaseValue` 与 `CurrentValue`：基础值用
 *      `SetNumericAttributeBase` 写一次绝对值，升级走 GE 修饰器，两者天然不重叠，
 *      这类"翻倍"错误在结构上就不可能发生。
 *   2) **Additive / Multiplier / Clamp 三个自研计算器 → GE 的 Modifier Op**
 *      （Additive / Multiplicative / Override）+ `PreAttributeChange` 钳制。
 *   3) **分组结算（BaseGroup / UpgradeGroup）→ GE 的生命周期**
 *      （Instant 用于基础值，Infinite 用于升级，移除 GE 即撤销升级）。
 *
 * 属性命名沿用引擎惯例：成对出现 `Xxx`（当前值）/ `MaxXxx`（上限）。
 */

UCLASS()
class AI_API URogueAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	URogueAttributeSet();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostGameplayEffectExecute(const struct FGameplayEffectModCallbackData& Data) override;

	// ===========================================================
	//  生命 / 护甲
	// ===========================================================

	/** 当前生命。 */
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Rogue|生命")
	FGameplayAttributeData Health;
	ATTRIBUTE_ACCESSORS(URogueAttributeSet, Health);

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Rogue|生命")
	FGameplayAttributeData MaxHealth;
	ATTRIBUTE_ACCESSORS(URogueAttributeSet, MaxHealth);

	/** 当前护甲。伤害先扣护甲再扣血。 */
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Rogue|生命")
	FGameplayAttributeData Armor;
	ATTRIBUTE_ACCESSORS(URogueAttributeSet, Armor);

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Rogue|生命")
	FGameplayAttributeData MaxArmor;
	ATTRIBUTE_ACCESSORS(URogueAttributeSet, MaxArmor);

	/** 每秒生命回复。 */
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Rogue|生命")
	FGameplayAttributeData HealthRegenPerSecond;
	ATTRIBUTE_ACCESSORS(URogueAttributeSet, HealthRegenPerSecond);

	/** 护甲开始回复前的等待时间（受伤后重置）。 */
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Rogue|生命")
	FGameplayAttributeData ArmorRechargeDelay;
	ATTRIBUTE_ACCESSORS(URogueAttributeSet, ArmorRechargeDelay);

	/** 护甲每秒回复量。 */
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Rogue|生命")
	FGameplayAttributeData ArmorRechargeRate;
	ATTRIBUTE_ACCESSORS(URogueAttributeSet, ArmorRechargeRate);

	/** 减伤比例，**取值范围是 0~1 的比例，不是百分数**。
	 *  结算公式（见 URogueAttributeSet::PostGameplayEffectExecute）：
	 *  `MitigatedDamage = LocalDamage * (1.0f - DamageReductionPercent)`。
	 *  不钳制的话，一旦被升级堆到 >1，`(1 - x)` 变负 →
	 *  后续 `RemainingDamage > 0` 判定为假 → 玩家直接免疫所有伤害。
	 *  所以在 PreAttributeChange 里把它钳到 [0,1]。 */
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Rogue|防御")
	FGameplayAttributeData DamageReductionPercent;
	ATTRIBUTE_ACCESSORS(URogueAttributeSet, DamageReductionPercent);

	// ===========================================================
	//  机动 / 通用
	// ===========================================================

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Rogue|机动")
	FGameplayAttributeData MoveSpeed;
	ATTRIBUTE_ACCESSORS(URogueAttributeSet, MoveSpeed);

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Rogue|机动")
	FGameplayAttributeData DashCooldownDuration;
	ATTRIBUTE_ACCESSORS(URogueAttributeSet, DashCooldownDuration);

	/** 冲刺冷却的下限。**做成属性而不是代码里的常量**：
	 *  项目里这个阈值原本有两处（升级规则资产的 MinimumDashCooldown 用于"到上限就剔除卡牌"、
	 *  平衡资产的 DashMinCooldown 用于钳制），两处一旦分叉就会出问题（清单里的 P2-24）。
	 *  放进属性集后，"钳制"与"剔除判定"可以读同一个来源。 */
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Rogue|机动")
	FGameplayAttributeData MinDashCooldown;
	ATTRIBUTE_ACCESSORS(URogueAttributeSet, MinDashCooldown);

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Rogue|机动")
	FGameplayAttributeData DashDuration;
	ATTRIBUTE_ACCESSORS(URogueAttributeSet, DashDuration);

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Rogue|机动")
	FGameplayAttributeData DashSpeed;
	ATTRIBUTE_ACCESSORS(URogueAttributeSet, DashSpeed);

	/** 经验获取倍率。旧系统里它由 BaseGroup 提供 1.0 并走加性链路。 */
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Rogue|成长")
	FGameplayAttributeData ExperienceMultiplier;
	ATTRIBUTE_ACCESSORS(URogueAttributeSet, ExperienceMultiplier);

	/** 经验球拾取半径。 */
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Rogue|成长")
	FGameplayAttributeData PickupRadius;
	ATTRIBUTE_ACCESSORS(URogueAttributeSet, PickupRadius);

	// ===========================================================
	//  元属性（Meta Attribute）
	// ===========================================================

	/** 伤害入口。**不复制**：它是服务端结算用的中转值，算完立刻清零。
	 *
	 *  为什么要有它，而不是让 GE 直接改 Health：
	 *  本项目有一条真实的减伤链（减伤比例 → 护甲吸收 → 剩余扣血），
	 *  而且受伤要重置护甲回复计时。如果 GE 直接改 Health，这条链就没有落脚点。
	 *  标准做法就是"GE 改元属性 Damage → PostGameplayEffectExecute 里跑完整链路"。
	 *
	 *  顺带修掉了自研系统的一个老问题：那条旧伤害链完全绕开了 Actor 基类
	 *  （不检查 bCanBeDamaged、不触发 OnTakeAnyDamage）。走 GE 之后这些语义自动成立。 */
	UPROPERTY(BlueprintReadOnly, Category = "Rogue|Meta")
	FGameplayAttributeData Damage;
	ATTRIBUTE_ACCESSORS(URogueAttributeSet, Damage);

	/** 最近一次造成伤害的来源（用于击杀归属）。
	 *  死亡是用 GE 表达的，而"谁打死的"这个信息不在死亡 GE 自己身上，
	 *  所以在结算伤害时把它记在属性集里 —— 属性集是"每个单位一份"，
	 *  存这里最自然，也让 AttributeSet 不必认识具体的角色类。 */
	AActor* GetLastDamageCauser() const { return LastDamageCauser.Get(); }

	/** 清掉"最后伤害来源"。
	 *  池化对象复用前必须调 —— 这个弱引用不参与复制、也不属于任何 GE，
	 *  不主动清的话，复用出来的敌人会继承**上一只怪的击杀者**，
	 *  于是击杀归属（金币/经验/击杀计数）会算到上一只的凶手头上。
	 *  （调用点在 URogueAbilitySystemComponent::ResetForPool） */
	void ClearLastDamageCauser() const { LastDamageCauser.Reset(); }

	// ===========================================================
	//  武器加成（升级的落点）—— 与"生命/护甲"共用这一套属性集
	// ===========================================================
	//  为什么放在这里，而不是单独一套 URogueAttributeSet（曾经那么做过，已回退）：
	//  实测（双进程 + Rogue.SelfTest.WeaponUpgradeChain）**第二套属性集实例的值没有复制到客户端** ——
	//  客户端上它确实登记进了 ASC 的 SpawnedAttributes，但读到的 12 个值全是 0；
	//  而主属性集的 BaseValue 改动（开局灌属性）复制正常。
	//  在没定位清楚根因之前，把加成放进**已验证可复制**的这一套里是更稳的选择。
	//  代价：敌人也挂这一套，会多带 12 个恒定为 0 的浮点（与类注释里"共用一套属性集"的取舍一致）。
	//
	//  语义：全部是**加成增量**（0 = 无加成）；武器侧负责"数据表基础值 + 加成×本武器系数"
	//  （同一个升级在不同武器上的换算系数不同，见 ARogueWeaponBase::GetEffective*）。

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Rogue|武器加成")
	FGameplayAttributeData AttackPowerBonus;
	ATTRIBUTE_ACCESSORS(URogueAttributeSet, AttackPowerBonus);

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Rogue|武器加成")
	FGameplayAttributeData AttackSpeedBonus;
	ATTRIBUTE_ACCESSORS(URogueAttributeSet, AttackSpeedBonus);

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Rogue|武器加成")
	FGameplayAttributeData AttackRangeBonus;
	ATTRIBUTE_ACCESSORS(URogueAttributeSet, AttackRangeBonus);

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Rogue|武器加成")
	FGameplayAttributeData ProjectileSpeedBonus;
	ATTRIBUTE_ACCESSORS(URogueAttributeSet, ProjectileSpeedBonus);

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Rogue|武器加成")
	FGameplayAttributeData ProjectileCountBonus;
	ATTRIBUTE_ACCESSORS(URogueAttributeSet, ProjectileCountBonus);

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Rogue|武器加成")
	FGameplayAttributeData ScytheCountBonus;
	ATTRIBUTE_ACCESSORS(URogueAttributeSet, ScytheCountBonus);

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Rogue|武器加成")
	FGameplayAttributeData RocketCountBonus;
	ATTRIBUTE_ACCESSORS(URogueAttributeSet, RocketCountBonus);

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Rogue|武器加成")
	FGameplayAttributeData LaserCountBonus;
	ATTRIBUTE_ACCESSORS(URogueAttributeSet, LaserCountBonus);

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Rogue|武器加成")
	FGameplayAttributeData HellTowerCountBonus;
	ATTRIBUTE_ACCESSORS(URogueAttributeSet, HellTowerCountBonus);

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Rogue|武器加成")
	FGameplayAttributeData MortarCountBonus;
	ATTRIBUTE_ACCESSORS(URogueAttributeSet, MortarCountBonus);

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Rogue|武器加成")
	FGameplayAttributeData LaserRefractionBonus;
	ATTRIBUTE_ACCESSORS(URogueAttributeSet, LaserRefractionBonus);

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Rogue|武器加成")
	FGameplayAttributeData MortarBlastRadiusBonus;
	ATTRIBUTE_ACCESSORS(URogueAttributeSet, MortarBlastRadiusBonus);

private:
	/** 血量归零时给自己施加死亡效果（授予 State.Dead）。幂等：已死则直接返回。 */
	void TryApplyDeathEffect();

	/** 受伤后施加"护甲回复延迟"效果（授予 State.RecentlyDamaged）。 */
	void ApplyRecentlyDamagedEffect();

	/** 最近一次伤害来源。mutable 是为了让"清账本"能在 const 属性集上做（池化复用时会拿到 const 引用）。 */
	mutable TWeakObjectPtr<AActor> LastDamageCauser;
};