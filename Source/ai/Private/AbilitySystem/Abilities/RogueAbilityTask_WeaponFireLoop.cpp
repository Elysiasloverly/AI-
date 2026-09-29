#include "AbilitySystem/Abilities/RogueAbilityTask_WeaponFireLoop.h"

#include "AbilitySystem/Abilities/RogueWeaponAbility.h"

URogueAbilityTask_WeaponFireLoop::URogueAbilityTask_WeaponFireLoop()
{
	// 打开逐帧回调（不设这一位，引擎不会调用 TickTask —— 见头文件说明）。
	bTickingTask = true;
}

URogueAbilityTask_WeaponFireLoop* URogueAbilityTask_WeaponFireLoop::RunWeaponFireLoop(UGameplayAbility* OwningAbility)
{
	URogueAbilityTask_WeaponFireLoop* Task = NewAbilityTask<URogueAbilityTask_WeaponFireLoop>(OwningAbility);
	return Task;
}

void URogueAbilityTask_WeaponFireLoop::Activate()
{
	Super::Activate();

	// Ability 为空说明创建方式不对（NewAbilityTask 没传 OwningAbility）。
	// 直接结束，避免留下一个永远什么都不做、却一直占着逐帧回调的任务。
	if (Ability == nullptr)
	{
		EndTask();
	}
}

void URogueAbilityTask_WeaponFireLoop::TickTask(float DeltaTime)
{
	Super::TickTask(DeltaTime);

	URogueWeaponAbility* WeaponAbility = Cast<URogueWeaponAbility>(Ability);
	if (WeaponAbility == nullptr || WeaponAbility->IsActive() == false)
	{
		// 能力没了，心跳也就没有意义（正常结束路径：能力在 EndAbility 里会销毁任务，
		// 这里是"任务比能力活得久"的兜底）。
		EndTask();
		return;
	}

	WeaponAbility->RunFireLoopStep(DeltaTime);
}
