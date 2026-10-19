#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "RogueAbilityTask_WeaponFireLoop.generated.h"

class URogueWeaponAbility;

/** 持续武器能力的逐帧任务，将 TickTask 转给 RunFireLoopStep。
 * 选择逐帧检查用于及时响应目标出现；其性能与定时轮询的延迟取舍需实测。
 * 能力结束时 GAS 通知任务清理；本任务不承载武器开火/数值实现。 */
UCLASS()
class AI_API URogueAbilityTask_WeaponFireLoop : public UAbilityTask
{
	GENERATED_BODY()

public:
	URogueAbilityTask_WeaponFireLoop();

	/** 只创建任务；调用方须 ReadyForActivation 后才进入激活与调度。 */
	static URogueAbilityTask_WeaponFireLoop* RunWeaponFireLoop(UGameplayAbility* OwningAbility);

	virtual void Activate() override;
	virtual void TickTask(float DeltaTime) override;
};
