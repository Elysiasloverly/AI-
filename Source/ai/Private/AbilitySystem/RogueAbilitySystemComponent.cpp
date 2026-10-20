#include "AbilitySystem/RogueAbilitySystemComponent.h"

#include "AbilitySystem/RogueAbilitySet.h"

#include "AbilitySystem/Abilities/RogueGameplayAbility_Dash.h"
#include "AbilitySystem/RogueAttributeSet.h"
#include "AbilitySystemGlobals.h"

URogueAbilitySystemComponent::URogueAbilitySystemComponent()
{
	// ASC 要参与复制（组件默认不复制）
	SetIsReplicatedByDefault(true);

	// 默认使用敌人的 Minimal；PlayerState 构造时会切为 Mixed。
	// Full 向相关客户端发送完整 GE，是否采用应结合玩法需求与实际网络测量。
	ConfigureAsEnemy();
}

void URogueAbilitySystemComponent::ConfigureAsPlayer()
{
	SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
}

void URogueAbilitySystemComponent::ConfigureAsEnemy()
{
	SetReplicationMode(EGameplayEffectReplicationMode::Minimal);
}

void URogueAbilitySystemComponent::ResetForPool()
{
	// 回收前撤销敌人攻击能力，包括仍在运行的攻击能力及其任务。
	ClearAllAbilities();

	// 先取得所有活动 GE 的句柄，再逐个移除，避免直接遍历内部容器时删除元素。
	const TArray<FActiveGameplayEffectHandle> ActiveEffects = GetActiveEffects(FGameplayEffectQuery());
	for (const FActiveGameplayEffectHandle& EffectHandle : ActiveEffects)
	{
		RemoveActiveGameplayEffect(EffectHandle);
	}

	// 基础值由调用方按新原型重置；ASC 不决定具体数值。
	// 最后伤害来源不是属性或 GE，须另行清空，避免池化复用带入旧归属。
	if (const URogueAttributeSet* RogueAttributeSet = GetSet<URogueAttributeSet>())
	{
		RogueAttributeSet->ClearLastDamageCauser();
	}
}

void URogueAbilitySystemComponent::GrantDefaultAbilities(const URogueAbilitySet* AbilitySet)
{
	// 仅权威端授予。该查询使用 ASC 初始化/更新时缓存的网络模拟标记；
	// 因此调用前应已正确建立 AbilityActorInfo（本项目玩家的 OwnerActor 是 PlayerState）。
	if (!IsOwnerActorAuthoritative())
	{
		return;
	}

	// PossessedBy 与 OnRep_PlayerState 都会走到这里，必须防重复授予
	if (bDefaultAbilitiesGranted)
	{
		return;
	}
	bDefaultAbilitiesGranted = true;

	// 优先使用配置的能力集合；输入映射和技能激活条件仍由各自系统维护。
	if (AbilitySet != nullptr)
	{
		// SourceObject 传 ASC 自己：与下面回退路径的写法保持一致
		//（FGameplayAbilitySpec 的 SourceObject 是 UObject*，ASC 完全合格 ——
		//  注意 ASC 是 UActorComponent 不是 AActor，写成 AActor* 会传不进来）。
		AbilitySet->GiveToAbilitySystem(this, this);

		UE_LOG(LogTemp, Log, TEXT("[GAS] 已按能力集授予默认技能：%s（%d 条）"),
			*AbilitySet->GetName(), AbilitySet->Entries.Num());
		return;
	}

	// 未配置能力集合时回退到冲刺。非空但内容为空/错误的集合不会触发此回退。
	// 冲刺绑定输入 ID，由 AbilityLocalInputPressed 查找并尝试激活。
	FGameplayAbilitySpec DashSpec(URogueGameplayAbility_Dash::StaticClass(), /*Level=*/1, /*InputID=*/RogueAbilityInputID::Dash, /*SourceObject=*/this);
	GiveAbility(DashSpec);

	UE_LOG(LogTemp, Log, TEXT("[GAS] 已授予默认技能：冲刺（回退路径：未配置能力集）"));
}
