#include "AbilitySystem/RogueAttributeUpgradeEffect.h"

#include "AbilitySystem/RogueUpgradeExecution.h"

URogueAttributeUpgradeEffect::URogueAttributeUpgradeEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	// 用 ExecutionCalculation 而不是固定修饰器：
	// 修饰器的 Attribute 是烤在 CDO 上的，一份 GE 只能改一个属性；
	// 而升级要改的属性是运行时才知道的（由载荷 Tag 决定，属性类走 URogueAttributeSet、
	// 武器类走 URogueAttributeSet）。
	FGameplayEffectExecutionDefinition ExecutionDef;
	ExecutionDef.CalculationClass = URogueUpgradeExecution::StaticClass();
	Executions.Add(ExecutionDef);

	// ⚠️ 这里**故意没有**授予族标签（曾经有过一个 TargetTagsGameplayEffectComponent，已删除）。
	//
	// 原因：Instant 的 GE 不会留在身上，GAS 会直接拒绝并报错：
	//   "GE ... is set to Instant so TargetTagsGameplayEffectComponent will not be able to apply its tags."
	// 也就是说那个组件从来没生效过，只是在每个进程启动时贡献一条 Error 日志。
	//
	// 为什么不需要它：升级是 Instant 效果，改的是属性 BaseValue（永久生效），
	// 身上根本不存在"待清除的升级 GE" —— 所以既不需要族标签去批量清除，
	// 也不需要靠标签抑制同类 GE。真要做"撤销升级"，正确做法是把属性改回 BaseValue
	// （或改用带时长的 Infinite GE + 移除），那是另一个需求，不该靠一个不生效的组件兜着。
}
