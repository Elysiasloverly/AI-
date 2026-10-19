#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "RogueGameplayAbility_Dash.generated.h"

class URogueAbilityTask_Dash;

/** 玩家冲刺：按输入激活，提交冷却后用自定义逐帧任务移动。
 * 使用 LocalPredicted 激活；当前任务直接 AddActorWorldOffset，未接入 CharacterMovement
 * 的 SavedMove/RootMotionSource 预测与重放，不能据执行策略推断移动纠正已完整实现。 */
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
