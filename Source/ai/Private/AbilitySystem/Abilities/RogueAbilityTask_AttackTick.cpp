#include "AbilitySystem/Abilities/RogueAbilityTask_AttackTick.h"

#include "AbilitySystem/Abilities/RogueEnemyAttackAbility.h"

URogueAbilityTask_AttackTick::URogueAbilityTask_AttackTick()
{
	// 打开逐帧回调（不设这一位，引擎不会调用 TickTask）。
	bTickingTask = true;
}

URogueAbilityTask_AttackTick* URogueAbilityTask_AttackTick::RunAttackTick(UGameplayAbility* OwningAbility)
{
	return NewAbilityTask<URogueAbilityTask_AttackTick>(OwningAbility);
}

void URogueAbilityTask_AttackTick::Activate()
{
	Super::Activate();

	if (Ability == nullptr)
	{
		EndTask();
	}
}

void URogueAbilityTask_AttackTick::TickTask(float DeltaTime)
{
	Super::TickTask(DeltaTime);

	URogueEnemyAttackAbility* AttackAbility = Cast<URogueEnemyAttackAbility>(Ability);
	if (AttackAbility == nullptr || AttackAbility->IsActive() == false)
	{
		EndTask();
		return;
	}

	AttackAbility->TickAttackStep(DeltaTime);
}
