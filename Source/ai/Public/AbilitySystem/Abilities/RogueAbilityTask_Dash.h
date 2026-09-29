#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "RogueAbilityTask_Dash.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRogueDashTaskDelegate);

/**
 * 冲刺位移任务（自定义 AbilityTask）。
 *
 * 为什么需要一个 Task 来做位移，而不是在 `ActivateAbility` 里循环：
 *   GameplayAbility 本身**不 Tick**。想在技能激活期间逐帧做事，
 *   标准做法就是派生一个 `bTickingTask = true` 的 UAbilityTask，
 *   把逐帧逻辑放进 `TickTask`。这也是 GAS 里最常被问到的一块。
 *
 * 位移方式刻意与迁移前的 `ARogueCharacter::HandleDash` 完全一致：
 *   `AddActorWorldOffset(方向 * 速度 * Δt, bSweep = true)`，撞墙立即结束。
 *   换成 root motion（`UAbilityTask_ApplyRootMotionConstantForce`）虽然更"正统"，
 *   但会改变撞墙表现与手感，所以迁移期先保证行为等价。
 */
UCLASS()
class AI_API URogueAbilityTask_Dash : public UAbilityTask
{
	GENERATED_BODY()

public:
	URogueAbilityTask_Dash();

	/** 冲刺结束（时间用完 / 撞墙 / Avatar 失效）时广播 */
	UPROPERTY(BlueprintAssignable)
	FRogueDashTaskDelegate OnDashFinished;

	virtual void Activate() override;
	virtual void TickTask(float DeltaTime) override;

	/** 创建任务。惯例写法：静态工厂 + NewAbilityTask，便于蓝图与调试器识别实例名。 */
	UFUNCTION(BlueprintCallable, Category = "Ability|Tasks",
		meta = (HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "TRUE"))
	static URogueAbilityTask_Dash* Dash(UGameplayAbility* OwningAbility, FVector Direction, float Speed, float Duration);

private:
	FVector DashDirection = FVector::ZeroVector;
	float DashSpeed = 0.0f;
	float TimeRemaining = 0.0f;
};
