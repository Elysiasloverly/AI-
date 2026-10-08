#include "AbilitySystem/RogueAbilitySystemComponent.h"

#include "AbilitySystem/RogueAbilitySet.h"

#include "AbilitySystem/Abilities/RogueGameplayAbility_Dash.h"
#include "AbilitySystem/RogueAttributeSet.h"
#include "AbilitySystemGlobals.h"

URogueAbilitySystemComponent::URogueAbilitySystemComponent()
{
	// ASC 要参与复制（组件默认不复制）
	SetIsReplicatedByDefault(true);

	// ReplicationMode 不是 UPROPERTY，构造函数里给个保守默认；
	// 玩家侧由 ARoguePlayerState 显式调 ConfigureAsPlayer() 覆盖。
	// 特意不在这里默认 Full：Full 是"把所有 GE 明细广播给所有人"，
	// 在 200+ 敌人的场景下这是最贵的模式，绝不能当默认值。
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
	// 1) 撤销所有已授予的能力，避免上一只残留的技能带到下一只身上。
	//    注：本项目**没有给敌人配 GameplayAbility**（全项目只有冲刺一个 GA，且只授给玩家），
	//    敌人行为是 ARogueEnemy::Tick 里的自研状态机。这里保留 ClearAllAbilities 是为了
	//    "池复用后状态干净"这条契约本身成立，将来真给敌人加技能也不用回来补。
	ClearAllAbilities();

	// 2) 清掉所有激活中的 GE。
	//    刻意不用 `FGameplayEffectQuery()` 空查询 + RemoveActiveEffects：
	//    空查询"是否匹配全部"依赖 FGameplayEffectQuery::Matches 的默认分支实现，
	//    语义不够直观。这里改成先列出全部再逐个移除，行为一目了然。
	const TArray<FActiveGameplayEffectHandle> ActiveEffects = GetActiveEffects(FGameplayEffectQuery());
	for (const FActiveGameplayEffectHandle& EffectHandle : ActiveEffects)
	{
		RemoveActiveGameplayEffect(EffectHandle);
	}

	// 3) 属性基值的重置由调用方负责（敌人生成流程在激活时用 SetNumericAttributeBase 写入）——
	//    因为"重置成多少"是业务知识，ASC 不该知道。
	//
	// 4) 但"最后伤害来源"必须在这里清：它是个弱引用、既不是 GE 也不在属性基值里，
	//    属于 GAS 之外的账本。不清的话复用出来的敌人会带着上一只怪的击杀者，
	//    击杀归属（金币/经验/击杀计数）就会算到上一只的凶手头上。
	if (const URogueAttributeSet* RogueAttributeSet = GetSet<URogueAttributeSet>())
	{
		RogueAttributeSet->ClearLastDamageCauser();
	}
}

void URogueAbilitySystemComponent::GrantDefaultAbilities(const URogueAbilitySet* AbilitySet)
{
	// 能力是权威数据：只在服务端授予，客户端通过复制拿到自己的技能列表。
	// IsOwnerActorAuthoritative() 是 GAS 提供的判定（等价于 OwnerActor 有 Authority），
	// 比直接用 GetOwner()->HasAuthority() 更贴切，因为 OwnerActor 未必是 Actor 的所有者。
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

	// ---- ① 优先：数据驱动的能力集 ----
	//
	// 传了能力集就完全按它授予 —— 于是"加一个玩家主动技能"变成
	// "写能力子类 + 改一个资产"，C++ 一行都不用动。
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

	// ---- ② 回退：硬编码的冲刺 ----
	//
	// 为什么保留：能力集是**资产**，可能没配、配错或加载失败。那种情况下若什么都不授予，
	// 玩家会连冲刺都没有 —— 而"没有冲刺"很难被归因为"资产没配好"。
	// 留一条硬编码路径，让"资产缺失"退化成"少几个主动技能"，而不是"玩不了"。
	//
	// 冲刺：Level 1，**绑定输入 ID**。
	// 输入侧走 GAS 原生路径：IA_Dash 的处理函数调 `AbilityLocalInputPressed(RogueAbilityInputID::Dash)`，
	// 由 GAS 自己按 InputID 找能力并激活 —— 不需要输入层认识技能类，也不需要按标签再查一次。
	// （迁移前这里是 InputID = INDEX_NONE + "输入侧直接 TryActivateAbilitiesByTag"。）
	FGameplayAbilitySpec DashSpec(URogueGameplayAbility_Dash::StaticClass(), /*Level=*/1, /*InputID=*/RogueAbilityInputID::Dash, /*SourceObject=*/this);
	GiveAbility(DashSpec);

	UE_LOG(LogTemp, Log, TEXT("[GAS] 已授予默认技能：冲刺（回退路径：未配置能力集）"));
}
