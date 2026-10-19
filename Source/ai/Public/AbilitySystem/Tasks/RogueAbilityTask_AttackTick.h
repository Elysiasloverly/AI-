#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "RogueAbilityTask_AttackTick.generated.h"

class URogueEnemyAttackAbility;

/** 敌人持续攻击的逐帧任务，目前用于地面波。
 * 接触/远程攻击在激活中完成执行并结束，不创建此任务。
 * 所属能力结束时 GAS 通知任务清理；EndTask 本身不反向结束能力。 */
UCLASS()
class AI_API URogueAbilityTask_AttackTick : public UAbilityTask
{
	GENERATED_BODY()

public:
	URogueAbilityTask_AttackTick();

	static URogueAbilityTask_AttackTick* RunAttackTick(UGameplayAbility* OwningAbility);

	virtual void Activate() override;
	virtual void TickTask(float DeltaTime) override;
};
