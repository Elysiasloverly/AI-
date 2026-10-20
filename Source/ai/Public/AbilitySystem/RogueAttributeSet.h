#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystem/RogueAttributeMacros.h"
#include "AbilitySystemComponent.h"
// 14 个武器加成属性的上限结构（-1 = 不设上限），由玩家数值资产配置。
#include "Player/RoguePlayerBalanceAsset.h"
#include "RogueAttributeSet.generated.h"

/** 玩家与敌人共用的 GAS 战斗属性集。
 * 初始化通过 ASC 写基础值；升级使用无周期 Infinite GE 修饰 CurrentValue；
 * 伤害使用 Damage 元属性，在执行回调中依次处理减伤、护甲与生命。
 * BaseValue/CurrentValue 分离并不会自动防止重复初始化或重复施加效果。 */

UCLASS()
class AI_API URogueAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	URogueAttributeSet();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	virtual void PostGameplayEffectExecute(const struct FGameplayEffectModCallbackData& Data) override;

	/** 设置武器加成上限（负值不设上限），由角色加载平衡资产后写入。
	 * PreAttributeChange 钳制 GAS 当前值写入；不会替调用方修正聚合器中的原始量级或基础值。 */
	void SetWeaponBonusCaps(const FRogueWeaponBonusCaps& InCaps) { WeaponBonusCaps = InCaps; }

	/** 取某个加成属性对应的上限。返回 **-1 表示不设上限**。 */
	float GetWeaponBonusCapFor(const FGameplayAttribute& Attribute) const;

	/** 武器加成上限，负值表示不设上限；不含 UObject 引用，无需 GC 跟踪。
	 * 当前不复制，客户端若需要同样的本地钳制必须通过配置初始化。 */
	FRogueWeaponBonusCaps WeaponBonusCaps;

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

	/** 减伤比例，范围 [0,1]；1 表示完全减免。PreAttributeChange 钳制当前值，
	 * 伤害结算还会再钳制一次比例，防止越界值导致负伤害。 */
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

	/** 冲刺冷却下限；属性钳制与升级可选性判断应读取同一来源。 */
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Rogue|机动")
	FGameplayAttributeData MinDashCooldown;
	ATTRIBUTE_ACCESSORS(URogueAttributeSet, MinDashCooldown);

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Rogue|机动")
	FGameplayAttributeData DashDuration;
	ATTRIBUTE_ACCESSORS(URogueAttributeSet, DashDuration);

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Rogue|机动")
	FGameplayAttributeData DashSpeed;
	ATTRIBUTE_ACCESSORS(URogueAttributeSet, DashSpeed);

	/** 经验获取倍率，1 表示不加成。基础值由玩家平衡资产初始化。 */
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

	/** 服务端伤害中转值，不复制；每次执行后读取并清零。
	 * PostGameplayEffectExecute 在此执行减伤 → 护甲吸收 → 剩余扣血，并施加受伤/死亡状态。
	 * GAS 不自动调用 AActor::TakeDamage：当前入口未检查 CanBeDamaged，也不触发 OnTakeAnyDamage。 */
	UPROPERTY(BlueprintReadOnly, Category = "Rogue|Meta")
	FGameplayAttributeData Damage;
	ATTRIBUTE_ACCESSORS(URogueAttributeSet, Damage);

	/** 最近一次 Damage 执行上下文中的 EffectCauser（弱引用，不复制）。
	 * 归属是否正确取决于调用方是否正确填充上下文；非 Pawn 来源目前存在默认目标归属限制。 */
	AActor* GetLastDamageCauser() const { return LastDamageCauser.Get(); }

	/** 清掉"最后伤害来源"。
	 *  池化对象复用前必须调 —— 这个弱引用不参与复制、也不属于任何 GE，
	 *  不主动清的话，复用出来的敌人会继承**上一只怪的击杀者**，
	 *  于是击杀归属（金币/经验/击杀计数）会算到上一只的凶手头上。
	 *  （调用点在 URogueAbilitySystemComponent::ResetForPool） */
	void ClearLastDamageCauser() const { LastDamageCauser.Reset(); }

	// 加成增量：0 表示无加成；各武器合并数据表基础值并应用自己的换算系数。
	// 与生命/护甲共享初始化和复制路径。敌人也使用本属性集，保留这些未使用的字段。
	// 这属于当前项目取舍，UE 并不限制一个 ASC 只能持有一套属性集。

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

	/** 镰刀额外尺寸比例；视觉与伤害碰撞范围共用，0.2 表示基础尺寸的 1.2 倍。 */
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Rogue|武器加成")
	FGameplayAttributeData ScytheSizeBonus;
	ATTRIBUTE_ACCESSORS(URogueAttributeSet, ScytheSizeBonus);

	/** 镰刀吸血比例：[0,1]，0.2 表示造成伤害的 20% 回复生命。 */
	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Rogue|武器加成")
	FGameplayAttributeData ScytheLifestealBonus;
	ATTRIBUTE_ACCESSORS(URogueAttributeSet, ScytheLifestealBonus);

private:
	/** 血量归零时给自己施加死亡效果（授予 State.Dead）。幂等：已死则直接返回。 */
	void TryApplyDeathEffect();

	/** 受伤后施加"护甲回复延迟"效果（授予 State.RecentlyDamaged）。 */
	void ApplyRecentlyDamagedEffect();

	/** 最近一次伤害来源。mutable 是为了让"清账本"能在 const 属性集上做（池化复用时会拿到 const 引用）。 */
	mutable TWeakObjectPtr<AActor> LastDamageCauser;
};