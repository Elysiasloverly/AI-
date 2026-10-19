#include "AbilitySystem/GameplayEffects/RogueDamageEffect.h"

#include "AbilitySystem/RogueAttributeSet.h"
#include "AbilitySystem/RogueGameplayTags.h"

URogueDamageEffect::URogueDamageEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FSetByCallerFloat SetByCallerDamage;
	// 使用原生 Tag 变量统一读写键；变量名拼错可被编译器发现。
	// FName/Tag 两条 SetByCaller 取值路径在缺值时都返回默认值，并可按参数记录 Error。
	SetByCallerDamage.DataTag = RogueGameplayTags::Data_Damage;

	FGameplayModifierInfo DamageModifier;
	DamageModifier.Attribute = URogueAttributeSet::GetDamageAttribute();
	// 每个伤害 Spec 对 Damage 做 Additive；PostGameplayEffectExecute 处理后立即清零。
	// 同帧多个命中仍按各自的执行顺序逐次结算，不是整帧累积后统一处理。
	DamageModifier.ModifierOp = EGameplayModOp::Additive;
	DamageModifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCallerDamage);

	Modifiers.Add(DamageModifier);
}
