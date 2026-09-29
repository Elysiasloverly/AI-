#include "AbilitySystem/RogueDamageEffect.h"

#include "AbilitySystem/RogueAttributeSet.h"
#include "Core/RogueGameplayTags.h"

URogueDamageEffect::URogueDamageEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;

	FSetByCallerFloat SetByCallerDamage;
	// 用 Tag 而不是 FName：Tag 有原生声明（RogueGameplayTags），拼错编译期就报错，
	// 而 SetByCaller 的 FName 写错只会在运行时静默取到 0 伤害。
	SetByCallerDamage.DataTag = RogueGameplayTags::Data_Damage;

	FGameplayModifierInfo DamageModifier;
	DamageModifier.Attribute = URogueAttributeSet::GetDamageAttribute();
	// Additive：元属性 Damage 每次结算前都是 0（PostGameplayEffectExecute 里会清零），
	// 所以"加上本次伤害量"就是它的语义。同时在一帧内被多个来源打中时会自然累加。
	DamageModifier.ModifierOp = EGameplayModOp::Additive;
	DamageModifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCallerDamage);

	Modifiers.Add(DamageModifier);
}
