#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "RogueAbilityTask_WeaponFireLoop.generated.h"

class URogueWeaponAbility;

/**
 * 武器开火的"心跳"任务：**每帧一次**调用能力的一步开火逻辑。
 *
 * 为什么需要它：GameplayAbility 本身没有 Tick（`UGameplayAbility` 上没有逐帧回调），
 * 而武器开火的节奏检查必须是逐帧的 —— 改造前 `ARogueCharacter::TickWeapons` 就是每帧调一次
 * `WeaponTick`，任何"改成定时器轮询"的写法都会引入一帧到几十毫秒的额外延迟
 * （典型表现：敌人刚进射程时武器要等一小会儿才开第一枪）。
 *
 * 实现方式：`bTickingTask = true` 会让 ASC 上的 `UGameplayTasksComponent` 每帧调 `TickTask`。
 * 这一位必须在构造函数里设（`bTickingTask` 是 `UGameplayTask` 的 protected 成员）。
 * 本项目早前迁移冲刺时已经验证过这条路可行（见 `URogueAbilityTask_Dash`），
 * 包括"ASC 挂在 PlayerState 上"这种非 Pawn 宿主的情况。
 *
 * 任务本身不含玩法逻辑：它只是把心跳转给 `URogueWeaponAbility::RunFireLoopStep`，
 * 这样"谁在跑、什么时候停"由能力管，"怎么开火"由武器管。
 */
UCLASS()
class AI_API URogueAbilityTask_WeaponFireLoop : public UAbilityTask
{
	GENERATED_BODY()

public:
	URogueAbilityTask_WeaponFireLoop();

	/** 创建并激活心跳（调用方负责 ReadyForActivation）。 */
	static URogueAbilityTask_WeaponFireLoop* RunWeaponFireLoop(UGameplayAbility* OwningAbility);

	virtual void Activate() override;
	virtual void TickTask(float DeltaTime) override;
};
