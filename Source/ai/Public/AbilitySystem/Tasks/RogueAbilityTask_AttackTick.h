#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "RogueAbilityTask_AttackTick.generated.h"

class URogueEnemyAttackAbility;

/**
 * 敌人攻击能力的逐帧心跳（与 `URogueAbilityTask_WeaponFireLoop` 同构）。
 *
 * 只有"持续到结束"的攻击需要它（地面波）。一次性攻击（接触 / 远程）在 `ActivateAbility`
 * 里干完就 `EndAbility`，一个任务都不创建 —— 200+ 敌人时这一点很重要：
 * 一次性攻击每次尝试都建任务的话，光是 UObject 分配就够呛。
 */
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
