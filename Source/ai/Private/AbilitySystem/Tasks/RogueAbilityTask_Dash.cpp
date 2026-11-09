#include "AbilitySystem/Tasks/RogueAbilityTask_Dash.h"

#include "GameFramework/Actor.h"
#include "Player/RogueCharacter.h"

URogueAbilityTask_Dash::URogueAbilityTask_Dash()
{
	// 构造时开启逐帧任务，激活后由任务组件调度 TickTask。
	bTickingTask = true;
}

URogueAbilityTask_Dash* URogueAbilityTask_Dash::Dash(UGameplayAbility* OwningAbility, FVector Direction, float Speed, float Duration)
{
	URogueAbilityTask_Dash* Task = NewAbilityTask<URogueAbilityTask_Dash>(OwningAbility);
	if (Task == nullptr)
	{
		return nullptr;
	}

	Task->DashDirection = Direction.GetSafeNormal2D();
	Task->DashSpeed = Speed;
	Task->TimeRemaining = Duration;
	return Task;
}

void URogueAbilityTask_Dash::Activate()
{
	Super::Activate();

	if (DashSpeed <= 0.0f || TimeRemaining <= 0.0f || DashDirection.IsNearlyZero() || !IsValid(GetAvatarActor()))
	{
		OnDashFinished.Broadcast();
		EndTask();
	}
}

void URogueAbilityTask_Dash::TickTask(float DeltaTime)
{
	Super::TickTask(DeltaTime);

	AActor* Avatar = GetAvatarActor();
	if (!IsValid(Avatar))
	{
		OnDashFinished.Broadcast();
		EndTask();
		return;
	}

	// 冲刺直接移动 Actor，停用 CharacterMovement 并不能停止已经运行的任务。
	const ARogueCharacter* Character = Cast<ARogueCharacter>(Avatar);
	if (Character != nullptr && !Character->IsAlive())
	{
		OnDashFinished.Broadcast();
		EndTask();
		return;
	}

	TimeRemaining -= DeltaTime;

	FHitResult HitResult;
	Avatar->AddActorWorldOffset(DashDirection * DashSpeed * DeltaTime, /*bSweep=*/true, &HitResult);
	if (HitResult.bBlockingHit)
	{
		TimeRemaining = 0.0f;
	}

	if (TimeRemaining <= 0.0f)
	{
		// 先广播再 EndTask：让能力有机会在任务销毁前结束自己
		OnDashFinished.Broadcast();
		EndTask();
	}
}
