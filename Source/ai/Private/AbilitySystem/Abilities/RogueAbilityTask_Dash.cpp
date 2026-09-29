#include "AbilitySystem/Abilities/RogueAbilityTask_Dash.h"

#include "GameFramework/Actor.h"

URogueAbilityTask_Dash::URogueAbilityTask_Dash()
{
	// 打开逐帧回调。不设这一位，引擎不会调用 TickTask。
	// （bTickingTask 声明在 UGameplayTask 上，属于 protected，只能在构造里设。）
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

	// 参数非法就直接结束，避免创建一个什么都不做的任务
	if (DashSpeed <= 0.0f || TimeRemaining <= 0.0f || DashDirection.IsNearlyZero())
	{
		EndTask();
		return;
	}

	// Avatar 可能还没就位（例如 PlayerState 已复制但 Pawn 未生成）
	if (GetAvatarActor() == nullptr)
	{
		EndTask();
		return;
	}
}

void URogueAbilityTask_Dash::TickTask(float DeltaTime)
{
	Super::TickTask(DeltaTime);

	AActor* Avatar = GetAvatarActor();
	if (!IsValid(Avatar))
	{
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
