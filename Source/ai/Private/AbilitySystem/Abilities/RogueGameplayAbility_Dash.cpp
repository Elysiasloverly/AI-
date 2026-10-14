#include "AbilitySystem/Abilities/RogueGameplayAbility_Dash.h"

#include "AbilitySystem/Tasks/RogueAbilityTask_Dash.h"
#include "AbilitySystem/RogueAttributeSet.h"
#include "AbilitySystem/GameplayEffects/RogueStateEffects.h"
#include "Combat/Visuals/RogueImpactEffect.h"
#include "AbilitySystem/RogueGameplayTags.h"
#include "Framework/RogueGameState.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Player/RogueCharacter.h"

URogueGameplayAbility_Dash::URogueGameplayAbility_Dash()
{
	// 每个角色一份实例：技能需要保存 DashTask 这类运行期状态。
	// NonInstanced 会退化成 CDO 上的静态状态，多玩家同场时必然串味。
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;

	// 本地预测：客户端按下冲刺立刻有反馈，服务端随后权威确认。
	// 这是联机手感的关键 —— 若用 ServerOnly，玩家要等一个 RTT 才动。
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;

	// 技能身份标签。注意 5.5 起 AbilityTags 已废弃（UE_DEPRECATED_FORGAME），
	// 必须用 SetAssetTags 写默认值，且只能在构造函数里设。
	FGameplayTagContainer AssetTags;
	AssetTags.AddTag(RogueGameplayTags::Ability_Dash);
	SetAssetTags(AssetTags);

	// 激活期间授予 State.Dashing —— 全局可查询，客户端也拿得到。
	ActivationOwnedTags.AddTag(RogueGameplayTags::State_Dashing);

	// 死亡 / 已在冲刺中 → 不可激活。
	// 这两条取代了旧 `Dash()` 开头的 `if (bDead || bDashActive || ...) return;`
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
	if (Character == nullptr)
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

	// ⚠️ 顺序很重要：**先做完所有前置校验，最后才 CommitAbility**。
	//
	// CommitAbility 会真的把冷却 GE 施加到自己身上（扣冷却）。原来它排在方向校验之前，
	// 于是"拿到零方向"或"属性集没就位"这种失败路径会白扣一次冷却 ——
	// 玩家表现为"按了冲刺没动，冷却却转起来了"，而且没有任何报错。
	// 校验失败的路径本来就要 EndAbility（不提交、不扣冷却）。

	// 方向由角色提供：它需要相机朝向 + 本帧输入，这两样都在角色侧
	const FVector DashDirection = Character->GetDashDirection();
	if (DashDirection.IsNearlyZero())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 提交：检查冷却 / 阻塞标签 / 代价，并把冷却 GE 施加到自己身上。
	// 失败说明条件不满足（例如冷却还没走完），直接结束。
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const float DashSpeed = AttributeSet->GetDashSpeed();
	const float DashDuration = AttributeSet->GetDashDuration();

	// 与旧实现一致：先停下当前移动、面向冲刺方向
	if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
	}
	Character->SetActorRotation(DashDirection.Rotation());

	// 表现层：走 GameplayCue，让**所有客户端**都看到这次冲刺。
	//
	// 演进过程值得记一笔（每一版都比上一版更接近 GAS 的原生做法）：
	//   最初直接 SpawnImpactEffect（本地生成）→ 联机下只有本机看得见；
	//   加 IsLocallyControlled() 守卫避免了双份，但**其他玩家完全看不到你的冲刺**；
	//   然后改成 GameState 上的手写 Multicast（服务端发一次，各端本地生成一次）；
	//   现在是 GameplayCue：多播、相关性过滤、预测去重都由 AbilitySystem 负责，
	//   项目里不再需要为每种特效维护一条 RPC 和一个枚举。
	//
	// 仍然只在服务端发起：本技能是 LocalPredicted，预测客户端会再跑一遍 ActivateAbility；
	// 若客户端也执行 cue，它会先在本地播一次，服务端确认后再播一次 → 拥有者看到双份。
	// 用"由权威端发起"这一条规则，两端行为就完全确定了（与之前 Multicast 版的约束一致）。
	// 注意 UGameplayAbility::HasAuthority 是带参数的版本（要 Handle/ActorInfo），
	// 这里直接问 Avatar 的权限最直观。
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (AvatarActor != nullptr && AvatarActor->HasAuthority())
	{
		ARogueGameState::PlayImpactEffect(
			AvatarActor,
			Character->GetActorLocation() + FVector(0.0f, 0.0f, 38.0f),
			DashDirection.Rotation(),
			ERogueImpactVisualStyle::Laser,
			FVector(0.42f, 0.42f, 0.20f));
	}

	DashTask = URogueAbilityTask_Dash::Dash(this, DashDirection, DashSpeed, DashDuration);
	if (DashTask == nullptr)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	DashTask->OnDashFinished.AddDynamic(this, &URogueGameplayAbility_Dash::HandleDashFinished);
	DashTask->ReadyForActivation();
}

void URogueGameplayAbility_Dash::HandleDashFinished()
{
	if (IsActive())
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo,
			/*bReplicateEndAbility=*/true, /*bWasCancelled=*/false);
	}
}
