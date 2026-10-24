#include "UI/Widgets/RogueHUDViewData.h"
#include "AbilitySystem/RogueAbilitySystemComponent.h"
#include "AbilitySystem/RogueGameplayTags.h"
#include "AbilitySystem/Abilities/RogueGameplayAbility_Dash.h"
#include "Player/RogueCharacter.h"

void RogueVitalsView::UpdateDashState(const ARogueCharacter* Character, FRoguePlayerVitalsView& View)
{
    View.bDashUsable = View.bDashReady = View.bDashing = false;
    View.DashCooldownDuration = View.DashCooldownRemaining = View.DashCooldownPercent = 0.f;
    const URogueAbilitySystemComponent* ASC = Character ? Character->GetRogueAbilitySystemComponent() : nullptr;
    if (!ASC) return;

    bool bHasDash = false;
    for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
    {
        if (Spec.Ability && (Spec.Ability->IsA<URogueGameplayAbility_Dash>()
            || Spec.Ability->GetAssetTags().HasTagExact(RogueGameplayTags::Ability_Dash)))
        {
            bHasDash = true;
            break;
        }
    }
    View.bDashUsable = Character->IsAlive() && bHasDash;
    View.bDashing = ASC->HasMatchingGameplayTag(RogueGameplayTags::State_Dashing);
    FGameplayTagContainer Tags(RogueGameplayTags::Cooldown_Dash);
    const auto Times = ASC->GetActiveEffectsTimeRemainingAndDuration(FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(Tags));
    // 多个独立冷却同时存在时，最后到期的效果决定何时可再次冲刺。
    for (const auto& Time : Times)
    {
        if (FMath::IsFinite(Time.Key) && FMath::IsFinite(Time.Value)
            && Time.Key > View.DashCooldownRemaining && Time.Value > 0.f)
        {
            View.DashCooldownRemaining = Time.Key;
            View.DashCooldownDuration = Time.Value;
        }
    }
    View.bDashReady = View.bDashUsable && !View.bDashing
        && !ASC->HasMatchingGameplayTag(RogueGameplayTags::Cooldown_Dash)
        && View.DashCooldownRemaining <= KINDA_SMALL_NUMBER;
    if (View.bDashUsable)
    {
        View.DashCooldownPercent = View.bDashReady ? 1.f : View.DashCooldownDuration > KINDA_SMALL_NUMBER
            ? FMath::Clamp(1.f - View.DashCooldownRemaining / View.DashCooldownDuration, 0.f, 1.f) : 0.f;
    }
}
