#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "GameplayTagContainer.h"
#include "RogueStateEffects.generated.h"

class UAbilitySystemComponent;

/** 状态 GE 的公共基类：使用 TargetTags GE 组件授予标签。
 * 构造期通过 CreateDefaultSubobject 创建组件，并加入 GEComponents 以参与缓存重建。
 * 不直接修改已弃用的 InheritableOwnedTagsContainer；旧资产另有版本迁移代码。 */
UCLASS(Abstract)
class AI_API URogueStateTagEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	URogueStateTagEffect();

protected:
	/** 在派生类构造函数里调用：让本效果在持有者身上授予这个状态标签。 */
	void GrantStateTag(FGameplayTag StateTag);
};

/** 无限死亡标签效果 State.Dead，直到生命周期逻辑显式移除。
 * 属性集在致死时施加，角色/敌人监听标签执行后续处理。
 * 玩家是否存活及复活期限另由复制的 LifeState 决定，GE 不是玩家生命周期的唯一状态源。 */
UCLASS()
class AI_API URogueDeathEffect : public URogueStateTagEffect
{
	GENERATED_BODY()

public:
	URogueDeathEffect();
};

/** 受伤延迟效果：授予 State.RecentlyDamaged。
 * 时长在施加时捕获目标 ArmorRechargeDelay；该标签阻断护甲周期回复。
 * 当前不合栈，重复受伤新增独立实例，直到最后一个实例到期才清除标签。 */
UCLASS()
class AI_API URogueRecentlyDamagedEffect : public URogueStateTagEffect
{
	GENERATED_BODY()

public:
	URogueRecentlyDamagedEffect();
};

/** 冲刺冷却：施加时捕获目标 DashCooldownDuration，并授予 Cooldown.Dash。
 * Dash 的 CooldownGameplayEffectClass 指向本类；GAS 的 CheckCooldown 查询其授予标签。 */
UCLASS()
class AI_API URogueDashCooldownEffect : public URogueStateTagEffect
{
	GENERATED_BODY()

public:
	URogueDashCooldownEffect();
};

/** 武器开火冷却：时长取 SetByCaller(Data.Cooldown)，授予 Cooldown.Weapon.AutoAttack。
 * 目标型武器能力持续运行，按自己的 CooldownHandle 查询节拍，不通过共享标签互相阻塞。
 * 各武器计算生效秒数；地狱塔的持续伤害节拍仍在武器内部。 */
UCLASS()
class AI_API URogueWeaponCooldownEffect : public URogueStateTagEffect
{
	GENERATED_BODY()

public:
	URogueWeaponCooldownEffect();
};

/** 敌人攻击冷却记录：时长取 SetByCaller(Data.Cooldown)，授予共享 Cooldown.Enemy.Attack。
 * 当前能力未将本类设为 CooldownGameplayEffectClass，也未据句柄/标签拒绝再次激活；
 * 实际攻击节流依赖 ARogueEnemy 的各攻击计时器，不能把本 GE 描述为独立冷却闸门。 */
UCLASS()
class AI_API URogueEnemyAttackCooldownEffect : public URogueStateTagEffect
{
	GENERATED_BODY()

public:
	URogueEnemyAttackCooldownEffect();
};

/** 濒死 GAS 镜像：授予 State.Downed 与持续 Cue，复活/团灭时显式移除。
 * 生命状态与十秒截止时间由角色 LifeState 管理；本效果不参与计时判定。 */
UCLASS()
class AI_API URogueDownedEffect : public URogueStateTagEffect
{
	GENERATED_BODY()

public:
	URogueDownedEffect();
};

/** 复活无敌：授予 State.Invulnerable，时长由 SetByCaller(Data.Duration) 给出。
 * 由 GAS 的效果计时移除，无需角色 Tick 单独维护截止时刻。 */
UCLASS()
class AI_API URogueInvulnerableEffect : public URogueStateTagEffect
{
	GENERATED_BODY()

public:
	URogueInvulnerableEffect();
};

// ===========================================================
//  自然回复：生命 / 护甲（Periodic GameplayEffect）
// ===========================================================

/** 常驻周期回复基类：每 0.25 秒将回复速率乘以周期，加到目标属性基础值。
 * 非快照捕获使后续速率升级能更新量级；阻断标签由 TargetTagRequirements 组件配置。
 * 当前值钳制由 AttributeSet 实现，钳制不等于重写基础值/聚合器中的累计量。
 * 周期结算由服务端执行，客户端接收配置为复制的属性数据；Period=0 表示无周期。 */
UCLASS(Abstract)
class AI_API URoguePeriodicRegenEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	URoguePeriodicRegenEffect();

protected:
	/** 派生类构造里调用：每个结算周期给 `TargetAttribute` 加 `RateAttribute × Period`。 */
	void ConfigureRegen(FGameplayAttribute TargetAttribute, FGameplayAttribute RateAttribute);

	/** 将 BlockingTag 加入持续条件的 IgnoreTags；存在该标签时效果被抑制。
	 * 效果仍保留；条件恢复时按 PeriodicInhibitionPolicy 恢复周期，不是逐帧重新施加。 */
	void BlockRegenWhileTagPresent(FGameplayTag BlockingTag);
};

/** 每周期增加 HealthRegenPerSecond × Period；State.Dead 存在时暂停。 */
UCLASS()
class AI_API URogueHealthRegenEffect : public URoguePeriodicRegenEffect
{
	GENERATED_BODY()

public:
	URogueHealthRegenEffect();
};

/** 每周期增加 ArmorRechargeRate × Period；State.Dead 或 State.RecentlyDamaged 存在时暂停。
 * 标签消失后恢复周期执行，首次回复时刻还受周期调度策略影响。 */
UCLASS()
class AI_API URogueArmorRechargeEffect : public URoguePeriodicRegenEffect
{
	GENERATED_BODY()

public:
	URogueArmorRechargeEffect();
};
