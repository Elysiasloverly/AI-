#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "RogueAbilityTask_Dash.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRogueDashTaskDelegate);

/** 冲刺逐帧任务：按方向和速度直接 AddActorWorldOffset，并执行碰撞扫描。
 * 撞墙或时限耗尽时广播完成；不负责 CharacterMovement 的移动预测/重放。
 * EndTask 只结束任务，不会自动结束所属能力；完成委托由能力绑定以调用 EndAbility。 */
UCLASS()
class AI_API URogueAbilityTask_Dash : public UAbilityTask
{
	GENERATED_BODY()

public:
	URogueAbilityTask_Dash();

	/** 正常到期、撞墙或发现角色已非存活时广播。
	 * 参数非法、Avatar 无效的早退仅 EndTask，不广播；外部取消能力也不保证本委托触发。 */
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
