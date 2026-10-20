#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayEffectTypes.h"
#include "RogueEnemyAttackAbility.generated.h"

class ARogueEnemy;

/** 进程级攻击尝试/执行计数，只用于调试，不复制、不区分世界或敌人。
 * 单进程多世界测试会共享这些值；计数不进入玩法判断。 */
namespace RogueEnemyAttackStats
{
	struct FCounters
	{
		/** AI 判定"该打了"并尝试激活能力的次数（含被冷却/标签挡下的） */
		int32 ContactAttempts = 0;
		int32 RangedAttempts = 0;

		/** 接触执行入口次数、成功远程发射次数；不都代表命中。 */
		int32 Contact = 0;
		int32 Ranged = 0;
	};

	AI_API const FCounters& Get();
	AI_API void Reset();

	/** AI 侧调用：判定"该打了"并尝试激活能力（不管最终成没成）。
	 *  必须与"执行次数"分开统计 —— 只有把两者放在一起看，才能区分
	 *  "敌人根本没走到攻击距离"和"走到了但 GAS 激活失败"。
	 *  （实测踩过：只统计执行次数时，两者都是 0，看起来一模一样。） */
	AI_API void CountContactAttempt();
	AI_API void CountRangedAttempt();
}

/** 服务端敌人攻击能力：AI 判断距离、目标及各自计时器后尝试激活。
 * 能力调用敌人的接触伤害/弹体执行函数。
 * ServerOnly 决定执行位置；客户端是否存在 Spec/实例还受授予、拥有者与复制规则影响。
 * 当前冷却 GE 只记录状态，未接入能力激活闸门，实际节流仍由 AI 计时器保证。 */
UCLASS(Abstract)
class AI_API URogueEnemyAttackAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	URogueEnemyAttackAbility();

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	/** 本能力驱动的敌人（拿不到返回 nullptr）。 */
	ARogueEnemy* GetEnemy() const;

	/** 攻击结束后记录到 GE 的冷却秒数；子类从当前敌人原型读取。
	 * AI 计时器另行读取原型配置，并非调用此函数。 */
	virtual float GetAttackCooldownSeconds() const PURE_VIRTUAL(URogueEnemyAttackAbility::GetAttackCooldownSeconds, return 0.0f;);

protected:
	/** 施加冷却 GE（时长 = GetAttackCooldownSeconds()）。 */
	void ApplyAttackCooldown();

private:
	FActiveGameplayEffectHandle CooldownHandle;
};

/** 接触伤害：对**所有**贴身玩家各结算一次（沿用改造前 TouchPlayer 的口径）。 */
UCLASS()
class AI_API URogueEnemyContactAttackAbility : public URogueEnemyAttackAbility
{
	GENERATED_BODY()

public:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual float GetAttackCooldownSeconds() const override;
};

/** 远程弹幕：对着当前锁定目标打一发（弹体生成与广播沿用 ARogueEnemy::ExecuteRangedAttack）。 */
UCLASS()
class AI_API URogueEnemyRangedAttackAbility : public URogueEnemyAttackAbility
{
	GENERATED_BODY()

public:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual float GetAttackCooldownSeconds() const override;
};
