#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayEffectTypes.h"
#include "RogueEnemyAttackAbility.generated.h"

class ARogueEnemy;
class URogueAbilityTask_AttackTick;

/** 敌人攻击执行次数的进程级计数（Phase 3 自检/性能对比用）。
 *
 *  为什么是"进程级"而不是每只怪各存一份：验证"攻击真的改成能力驱动了"要看的是
 *  "三种攻击各执行了多少次"，把所有怪的数字加起来才有意义；而每只怪的计数器
 *  需要一个遍历所有敌人的聚合过程，反而更贵、更容易在池化复用时漏清。
 *  代价是它不区分敌人，只用于日志对比 —— 所以不进任何玩法逻辑。 */
namespace RogueEnemyAttackStats
{
	struct FCounters
	{
		/** AI 判定"该打了"并尝试激活能力的次数（含被冷却/标签挡下的） */
		int32 ContactAttempts = 0;
		int32 RangedAttempts = 0;
		int32 GroundWaveAttempts = 0;

		/** 能力真的跑完一次攻击的次数 */
		int32 Contact = 0;
		int32 Ranged = 0;
		int32 GroundWave = 0;
	};

	AI_API const FCounters& Get();
	AI_API void Reset();

	/** AI 侧调用：判定"该打了"并尝试激活能力（不管最终成没成）。
	 *  必须与"执行次数"分开统计 —— 只有把两者放在一起看，才能区分
	 *  "敌人根本没走到攻击距离"和"走到了但 GAS 激活失败"。
	 *  （实测踩过：只统计执行次数时，两者都是 0，看起来一模一样。） */
	AI_API void CountContactAttempt();
	AI_API void CountRangedAttempt();
	AI_API void CountGroundWaveAttempt();
}

/**
 * 敌人攻击的 GameplayAbility 基类。
 *
 * ============ 与 AI 的分工（Phase 3 的核心）============
 *   · **AI（ARogueEnemy）只决定"要不要打"**：目标是谁、距离够不够、决策节流到点了没。
 *     这些都是每帧都在变的东西，放在 AI 里做才便宜（一次距离比较，没有 UObject/GE 开销）。
 *   · **能力负责"怎么打"**：真正结算伤害、生成弹体、推进地面波，以及"这一发之后冷却多久"。
 * 所以 AI 侧每个决策点只有一句 `TryActivateAttack(Handle)`，
 * 而 GAS 的调用次数与**实际攻击次数**成正比，与"敌人数量 × 帧数"无关 ——
 * 这条就是 200+ 敌人不退化性能的关键（详见 Docs/GAS-Main-Architecture-Plan.md 的 Phase 3）。
 *
 * ============ 为什么是 ServerOnly ============
 * 敌人没有"本人客户端"：攻击表现走已有的复制通道（弹体 Multicast、地面波复制半径），
 * 能力只需要在服务端跑一份。用 ServerOnly 也让客户端**完全没有**敌人技能实例，
 * 那部分开销直接归零。
 *
 * ============ 三个子类 ============
 *   · `URogueEnemyContactAttackAbility`   接触伤害（贴身的每个玩家都吃一发）
 *   · `URogueEnemyRangedAttackAbility`    远程弹幕
 *   · `URogueEnemyGroundWaveAttackAbility` 地面波：**持续到波结束**，期间逐帧推进扩散
 */
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

	/** 这一发之后冷却多久（秒）——由子类从敌人原型里取，AI 也从同一个函数取值，
	 *  保证"AI 什么时候再试"和"能力冷却多久"永远一致。 */
	virtual float GetAttackCooldownSeconds() const PURE_VIRTUAL(URogueEnemyAttackAbility::GetAttackCooldownSeconds, return 0.0f;);

protected:
	/** 施加冷却 GE（时长 = GetAttackCooldownSeconds()）。 */
	void ApplyAttackCooldown();

	/** 逐帧推进（只有"持续到结束"的能力会用到，见 URogueEnemyGroundWaveAttackAbility）。 */
	virtual void TickAttackStep(float DeltaSeconds) {}

	/** 需要逐帧推进的子类在激活后调用它挂上心跳任务。 */
	void StartTickTask();

	UPROPERTY(Transient)
	TObjectPtr<URogueAbilityTask_AttackTick> TickTask;

private:
	FActiveGameplayEffectHandle CooldownHandle;

	friend class URogueAbilityTask_AttackTick;

	/** 只在自检/调试日志里用：本机这份实例执行了多少次攻击。 */
	int32 ExecuteCount = 0;

public:
	int32 GetExecuteCount() const { return ExecuteCount; }

protected:
	void CountExecution() { ++ExecuteCount; }
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

/**
 * 地面波：激活后**一直保持激活**，逐帧推进波的扩散与伤害，波结束后才结束能力并进入冷却。
 *
 * 为什么不像另外两种那样"一次性"：波的扩散本身就是一段持续时间的行为
 * （`ARogueEnemy::UpdateGroundWave` 每帧算半径、判命中、刷视觉），
 * 用"持续激活的能力 + 逐帧心跳"表达它，正好让 AI 彻底不用管波 ——
 * 改造前 `HandleGroundWaveAttack` 里那段 "if (bGroundWaveActive) UpdateGroundWave(Delta)" 就是 AI 在替能力干活。
 */
UCLASS()
class AI_API URogueEnemyGroundWaveAttackAbility : public URogueEnemyAttackAbility
{
	GENERATED_BODY()

public:
	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual float GetAttackCooldownSeconds() const override;

protected:
	virtual void TickAttackStep(float DeltaSeconds) override;
};
