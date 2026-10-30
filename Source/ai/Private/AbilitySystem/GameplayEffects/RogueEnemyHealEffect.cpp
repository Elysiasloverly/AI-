#include "AbilitySystem/GameplayEffects/RogueEnemyHealEffect.h"
#include "AbilitySystem/RogueAttributeSet.h"
#include "AbilitySystem/RogueGameplayTags.h"
URogueEnemyHealEffect::URogueEnemyHealEffect()
{
    DurationPolicy=EGameplayEffectDurationType::Instant;
    FSetByCallerFloat Amount;Amount.DataTag=RogueGameplayTags::Data_Magnitude;
    FGameplayModifierInfo M;M.Attribute=URogueAttributeSet::GetHealthAttribute();M.ModifierOp=EGameplayModOp::Additive;
    M.ModifierMagnitude=FGameplayEffectModifierMagnitude(Amount);Modifiers.Add(M);
}
