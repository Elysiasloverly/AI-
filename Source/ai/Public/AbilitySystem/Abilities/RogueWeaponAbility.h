#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayEffectTypes.h"
#include "RogueWeaponAbility.generated.h"

class ARogueCharacter;
class ARogueWeaponBase;
class URogueWeaponCooldownEffect;

/** 自动开火能力：目标型武器使用自己的冷却句柄驱动开火循环，
 * 武器类负责目标收集、数值换算、弹体/命中实现；地狱塔内部保留持续伤害节拍。
 * 每个武器类型对应一个子类与能力 Spec，各端按类型解析本地武器 Actor。
 * FGameplayAbilitySpec::SourceObject 本身可随 Spec 复制，但本项目武器 Actor 不复制，
 * 不能用该对象引用在两端寻找各自独立生成的对应武器。
 * LocalPredicted 只声明激活策略；长期逐帧开火和冷却的预测确认仍有实现限制。 */
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

	/** 本能力驱动的武器类型，由子类重写返回。 */
	virtual TSubclassOf<ARogueWeaponBase> GetDrivenWeaponClass() const PURE_VIRTUAL(URogueWeaponAbility::GetDrivenWeaponClass, return nullptr;);

	/** 本能力驱动的武器实例（各端各自解析；解析失败返回 nullptr）。 */
	ARogueWeaponBase* GetDrivenWeapon() const;

	/** 本实例记录的冷却 GE 是否仍有剩余时间；开火还需满足生命、解锁和目标等条件。 */
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
	/** 本地活动 GE 句柄，不复制；服务器/拥有者预测效果各有自己的句柄。
	 * 预测确认可能移除本地效果；当前代码未将句柄重新映射到服务端复制的冷却实例。 */
	FActiveGameplayEffectHandle CooldownHandle;

	/** 解析出来的武器缓存（不复制，各端自己算）。 */
	UPROPERTY(Transient)
	mutable TObjectPtr<ARogueWeaponBase> CachedWeapon;

	/** 开火次数（自检用）。 */
	int32 FireCount = 0;

	/** 持续型武器的逐帧心跳次数（自检用；与 FireCount 互斥 —— 一把武器只会走其中一条路）。 */
	int32 ContinuousTickCount = 0;
};

// ===================================================================
// 各武器子类返回对应武器类型，两端据此解析各自生成的本地 Actor。
// SourceObject 可以复制对象引用，但不会自动把两份非复制 Actor 对应起来。
// ===================================================================

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
