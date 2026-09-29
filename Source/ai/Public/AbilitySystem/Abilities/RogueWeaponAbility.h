#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayEffectTypes.h"
#include "RogueWeaponAbility.generated.h"

class ARogueCharacter;
class ARogueWeaponBase;
class URogueWeaponCooldownEffect;

/**
 * 武器自动开火的 GameplayAbility。
 *
 * ============ 分工（这条边界的理由）============
 *   · 能力（本类）：**什么时候能开火** —— 冷却 GE 的节拍、"是否死亡"、目标选择、
 *     以及"把这一发写成一次冷却"。联机角色分工也在这里（见 ActivateAbility）。
 *   · 武器（ARogueWeaponBase 及其子类）：**这一发是什么** —— 伤害/弹速/散射/换算系数，
 *     以及池化弹体的取得与广播。这些是每把武器独有的知识，能力不该知道。
 * 所以能力里没有一行"伤害多少"的代码，武器里也没有一行"什么时候开火"的代码。
 *
 * ============ 为什么是"持续激活" ============
 * 武器是自动攻击，没有"按下激活"这个动作，而且**只要游戏在跑就一直要开火**。
 * 所以每把武器一份 InstancedPerActor 实例，激活一次之后一直跑，直到角色销毁：
 *   · 不这么做的话就得每帧 TryActivateAbility —— 那是把 GameplayAbility 当函数调用，
 *     既浪费（每帧匹配一遍技能列表）又丢掉了"持续状态"这个 GAS 原生概念。
 *   · 持续激活的代价是 GAS 内置的"冷却期间禁止重新激活"用不上（本来也没得重新激活），
 *     所以冷却的职责变成**节拍**：每帧问一次"这把武器的冷却 GE 还在不在"。
 *
 * ============ 一个武器一个能力实例 ============
 * 六把武器各有自己的冷却与目标，必须是六个独立实例。身份**不靠 SourceObject**
 * （那是 `TWeakObjectPtr`，不复制，客户端拿不到），而是靠**能力类**：
 * 每个武器类型一个本类的子类，子类用 `GetDrivenWeaponClass()` 声明自己驱动哪把武器，
 * 武器类则在构造里用 `GetFireAbilityClass()` 指向对应的能力类。
 * 两端都只依赖"类"这一层，所以不需要任何额外的复制字段 —— 这是刻意的选择。
 */
UCLASS(Abstract)
class AI_API URogueWeaponAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	URogueWeaponAbility();

	// ---- UGameplayAbility ----

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	// ---- 开火循环（由 URogueAbilityTask_WeaponFireLoop 每帧调用）----

	/** 每帧一步：目标选择 → 开火 → 把这一发写成一次冷却 GE。
	 *  @return 本帧是否真的开了一枪（供调试/HUD 用） */
	bool RunFireLoopStep(float DeltaSeconds);

	// ---- 查询 ----

	/** 本能力驱动哪一类武器（子类在构造里指定）。 */
	virtual TSubclassOf<ARogueWeaponBase> GetDrivenWeaponClass() const PURE_VIRTUAL(URogueWeaponAbility::GetDrivenWeaponClass, return nullptr;);

	/** 本能力驱动的武器实例（各端各自解析；解析失败返回 nullptr）。 */
	ARogueWeaponBase* GetDrivenWeapon() const;

	/** 冷却 GE 是否还在计时 —— 开火节拍的唯一判据。 */
	bool IsWeaponCoolingDown() const;

	/** 本机这份能力实例一共开火了多少次（自检/调试用；不复制，各端各自计数）。 */
	int32 GetFireCount() const { return FireCount; }

	/** 持续型武器的心跳次数（地狱塔这种"节拍在武器内部"的武器没有"开火"这个概念，
	 *  它的开火计数恒为 0 —— 只看那个数字会误判成"能力没跑起来"）。 */
	int32 GetContinuousTickCount() const { return ContinuousTickCount; }

	/** 冷却剩余秒数（自检用；-1 表示没有生效中的冷却）。
	 *  与 IsWeaponCoolingDown() 读同一个句柄，所以"冷却中=1 但剩余<=0"这种矛盾一眼可见。 */
	float GetCooldownRemainingForSelfTest() const;

protected:
	/** 冷却效果类（六把武器共用同一个 GE，时长走 SetByCaller）。 */
	TSubclassOf<UGameplayEffect> GetWeaponCooldownEffectClass() const;

	/** 把"这一发开完了"写成一冷却：施加带时长的冷却 GE，时长 = 武器算出来的生效冷却。 */
	void StampWeaponCooldown(float Seconds);

	/** 拥有者角色（拿不到返回 nullptr）。 */
	ARogueCharacter* GetOwnerCharacter() const;

	/** 逐帧心跳任务（能力结束时由 GAS 一并销毁）。 */
	UPROPERTY(Transient)
	TObjectPtr<class URogueAbilityTask_WeaponFireLoop> FireLoopTask;

private:
	/** 本机这一份冷却 GE 的句柄。
	 *  ⚠️ 句柄只在"施加它的那台机器"上有效：服务端与本人客户端各自施加、
	 *     各自持有自己的句柄，不跨端同步（也没必要同步，冷却只影响本机的开火节拍）。 */
	FActiveGameplayEffectHandle CooldownHandle;

	/** 解析出来的武器缓存（不复制，各端自己算）。 */
	UPROPERTY(Transient)
	mutable TObjectPtr<ARogueWeaponBase> CachedWeapon;

	/** 开火次数（自检用）。 */
	int32 FireCount = 0;

	/** 持续型武器的逐帧心跳次数（自检用；与 FireCount 互斥 —— 一把武器只会走其中一条路）。 */
	int32 ContinuousTickCount = 0;
};

// ===========================================================
//  六把武器的能力子类
// ===========================================================
//  它们体内只有一行"我驱动哪把武器"。存在的唯一理由是**让武器身份能跨端确定**：
//  能力的**类**是复制的（技能列表本身按 COND_OwnerOnly 复制给本人），
//  而 SourceObject 之类的运行时指针不复制。

UCLASS()
class AI_API URogueWeaponAbility_Projectile : public URogueWeaponAbility
{
	GENERATED_BODY()
public:
	virtual TSubclassOf<ARogueWeaponBase> GetDrivenWeaponClass() const override;
};

UCLASS()
class AI_API URogueWeaponAbility_Rocket : public URogueWeaponAbility
{
	GENERATED_BODY()
public:
	virtual TSubclassOf<ARogueWeaponBase> GetDrivenWeaponClass() const override;
};

UCLASS()
class AI_API URogueWeaponAbility_Mortar : public URogueWeaponAbility
{
	GENERATED_BODY()
public:
	virtual TSubclassOf<ARogueWeaponBase> GetDrivenWeaponClass() const override;
};

UCLASS()
class AI_API URogueWeaponAbility_Laser : public URogueWeaponAbility
{
	GENERATED_BODY()
public:
	virtual TSubclassOf<ARogueWeaponBase> GetDrivenWeaponClass() const override;
};

UCLASS()
class AI_API URogueWeaponAbility_HellTower : public URogueWeaponAbility
{
	GENERATED_BODY()
public:
	virtual TSubclassOf<ARogueWeaponBase> GetDrivenWeaponClass() const override;
};

UCLASS()
class AI_API URogueWeaponAbility_Scythe : public URogueWeaponAbility
{
	GENERATED_BODY()
public:
	virtual TSubclassOf<ARogueWeaponBase> GetDrivenWeaponClass() const override;
};
