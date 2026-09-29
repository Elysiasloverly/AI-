#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "RogueGameplayAbility_Dash.generated.h"

class URogueAbilityTask_Dash;

/**
 * 冲刺技能 —— 项目里第一个从"角色里的一个 bool + 计时器"迁移到 GAS 的能力。
 *
 * 选冲刺当第一个迁移对象的原因：它是唯一的**主动**技能，
 * 能一次性验证 ability 的完整生命周期：
 *   授予 → 输入激活 → 阻塞判定 → 提交（冷却）→ 授权期间 Tag → 逐帧任务 → 结束
 *
 * 迁移前后的对照：
 *   旧：`ARogueCharacter::Dash()` 里一堆 if（bDead / bDashActive / 暂停 / 冷却），
 *       冷却存在 `DashCooldownRemaining` 这个 float 里，只在本地有意义。
 *   新：这些判断全部变成声明式的标签与 GE ——
 *       `ActivationBlockedTags`（能否激活）、`CooldownTags` + 冷却 GE（冷却）、
 *       `ActivationOwnedTags`（激活期间的全局状态）。
 *       好处是"为什么不能冲"这件事在客户端（复制代理）也能查得到，
 *       而旧写法里那个 float 只有服务端/本地自己知道。
 */
UCLASS()
class AI_API URogueGameplayAbility_Dash : public UGameplayAbility
{
	GENERATED_BODY()

public:
	URogueGameplayAbility_Dash();

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

private:
	/** 位移任务结束 → 结束技能 */
	UFUNCTION()
	void HandleDashFinished();

	UPROPERTY(Transient)
	TObjectPtr<URogueAbilityTask_Dash> DashTask;
};
