#include "AbilitySystem/RogueAttributeUpgradeEffect.h"

#include "AbilitySystem/RogueUpgradeExecution.h"
#include "Core/RogueGameplayTags.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"

URogueAttributeUpgradeEffect::URogueAttributeUpgradeEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	// 用 ExecutionCalculation 而不是固定修饰器：
	// 修饰器的 Attribute 是烤在 CDO 上的，一份 GE 只能改一个属性；
	// 而升级要改的属性是运行时才知道的（由载荷 Tag 决定）。
	FGameplayEffectExecutionDefinition ExecutionDef;
	ExecutionDef.CalculationClass = URogueUpgradeExecution::StaticClass();
	Executions.Add(ExecutionDef);

	// 打上族标签，方便"一次性清空所有升级"（重开一局时用）。
	// 注意 Instant GE 不会留在身上，所以这个 Tag 主要给"抑制同类 GE"之类的判定用 ——
	// 真要做"撤销升级"需要改回 BaseValue，那是另一个需求。
	//
	// ⚠️ 同样必须走 GE 组件：往废弃的 InheritableOwnedTagsContainer 里塞标签不会生效
	// （GetGrantedTags() 读的是由 GEComponents 重建的 CachedGrantedTags）。
	// 构造期只能用 CreateDefaultSubobject（AddComponent<> 内部走 NewObject，构造函数里非法），
	// 且必须挂进 GEComponents —— 详见 URogueStateTagEffect 的注释。
	// （本类没有继承 URogueStateTagEffect：它是 Instant + ExecutionCalculation，
	//   只有"授予标签"这一段写法与那边相同。）
	UTargetTagsGameplayEffectComponent* TargetTags = CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("TargetTags"));
	GEComponents.Add(TargetTags);

	FInheritedTagContainer GrantedTags;
	GrantedTags.Added.AddTag(RogueGameplayTags::Effect_Upgrade_Attribute);
	TargetTags->SetAndApplyTargetTagChanges(GrantedTags);
}
