#include "AbilitySystem/Abilities/RogueGameplayAbility_Dash.h"

#include "AbilitySystem/Tasks/RogueAbilityTask_Dash.h"
#include "AbilitySystem/RogueAttributeSet.h"
#include "AbilitySystem/GameplayEffects/RogueStateEffects.h"
#include "Components/CapsuleComponent.h"
#include "AbilitySystem/RogueGameplayTags.h"
#include "Framework/RogueGameState.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Player/RogueCharacter.h"

URogueGameplayAbility_Dash::URogueGameplayAbility_Dash()
{
	// 每个 ASC 的能力 Spec 保留实例，用于持有本次 DashTask 等运行状态。
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	// 拥有者可先激活，服务端再权威处理。此策略不自动实现自定义位移的纠正/重放。
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	// 技能身份标签。注意 5.5 起 AbilityTags 已废弃（UE_DEPRECATED_FORGAME），
	// 必须用 SetAssetTags 写默认值，且只能在构造函数里设。
	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(RogueGameplayTags::Ability_Dash);
	SetAssetTags(AssetTags);

	// 激活期间在执行能力的 ASC 上添加 State.Dashing。
	// 对其他代理的复制还取决于 AbilitySystemGlobals 的激活标签复制配置。
	ActivationOwnedTags.AddTag(RogueGameplayTags::State_Dashing);

	// 已死亡或已冲刺时阻止激活；其他生命状态还由角色输入/生命周期逻辑处理。
	ActivationBlockedTags.AddTag(RogueGameplayTags::State_Dead);
	ActivationBlockedTags.AddTag(RogueGameplayTags::State_Dashing);

	// 冷却不需要在这里配 Tags —— 基类 UGameplayAbility::GetCooldownTags() 的实现是
	//     return CDGE ? &CDGE->GetGrantedTags() : nullptr;
	// 也就是**冷却标签直接取自冷却 GE 的授予标签**。
	// 所以只需要在 URogueDashCooldownEffect 里授予 Cooldown.Dash 就够了，
	// GAS 内置的 CheckCooldown 会拿它和持有者身上的 Tag 比对。
	// （这也是为什么那个 GE 必须真的把标签授予出去 —— 见 AbilitySystem/GameplayEffects/RogueStateEffects.h。）
	CooldownGameplayEffectClass = URogueDashCooldownEffect::StaticClass();
}

void URogueGameplayAbility_Dash::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	ARogueCharacter* Character = Cast<ARogueCharacter>(GetAvatarActorFromActorInfo());
	if (!IsValid(Character) || !Character->IsAlive())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, /*bReplicateEndAbility=*/true, /*bWasCancelled=*/true);
		return;
	}

	// 属性从 AttributeSet 读，而不是角色上的 float —— 这正是迁移的目的：
	// 数值只有一个真相源，升级改的是属性，技能不需要知道升级是怎么来的。
	const URogueAttributeSet* AttributeSet = Character->GetRogueAttributeSet();
	if (AttributeSet == nullptr)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 先完成方向/属性集校验，再提交，避免这些早期失败路径消耗冷却。
	// 任务创建或任务参数失效发生在提交后，目前不会自动退款冷却。

	// 方向由角色提供：它需要相机朝向 + 本帧输入，这两样都在角色侧
	const FVector DashDirection = Character->GetDashDirection();
	if (DashDirection.IsNearlyZero())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const float DashSpeed = AttributeSet->GetDashSpeed();
	const float DashDuration = AttributeSet->GetDashDuration();
	if (!FMath::IsFinite(DashSpeed) || DashSpeed <= 0.f || !FMath::IsFinite(DashDuration) || DashDuration <= 0.f)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// CommitAbility 重新检查有效 Spec、冷却和代价，然后施加冷却/代价。
	// 它不等于重新执行完整 CanActivateAbility，也不自动重查全部激活阻塞标签。
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 与旧实现一致：先停下当前移动、面向冲刺方向
	if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
	}
	Character->SetActorRotation(DashDirection.Rotation());

	DashTask = URogueAbilityTask_Dash::Dash(this, DashDirection, DashSpeed, DashDuration);
	if (DashTask == nullptr)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

    // Both endpoints are sent once by authority; the predicted ability does not duplicate them.
    if (Character->HasAuthority())
    {
        const FVector Feet = Character->GetActorLocation() -
            FVector(0, 0, Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() - 20.f);
        Character->Multicast_DashPresentation(true, Feet, DashDirection);
    }
	DashTask->OnDashFinished.AddDynamic(this, &URogueGameplayAbility_Dash::HandleDashFinished);
	DashTask->ReadyForActivation();
}

void URogueGameplayAbility_Dash::HandleDashFinished()
{
	if (IsActive())
	{
        if (auto* Character = Cast<ARogueCharacter>(GetAvatarActorFromActorInfo());
            Character && Character->HasAuthority() && Character->IsAlive())
        {
            const FVector Feet = Character->GetActorLocation() -
                FVector(0, 0, Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() - 8.f);
            Character->Multicast_DashPresentation(false, Feet, Character->GetActorForwardVector().GetSafeNormal2D());
        }
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo,
			/*bReplicateEndAbility=*/true, /*bWasCancelled=*/false);
	}
}
