#pragma once
#if WITH_DEV_AUTOMATION_TESTS
#include "Enemies/RogueEnemy.h"
#include "UI/RogueHUD.h"
struct FRogueEnemyHealingTestAccess
{
    static const FRogueEnemyHealingFeedback& Feedback(const ARogueEnemy* E) { return E->HealingFeedback; }
    static void Consume(ARogueEnemy* E) { E->ConsumeReplicatedHealingFeedback(); }
    static int32 GreenCount(const ARogueHUD* H)
    {
        int32 Count=0;
        if (H) for (const auto& N:H->DamageNumbers)
            if (N.Color.G>N.Color.R&&N.CachedText.StartsWith(TEXT("+"))) ++Count;
        return Count;
    }
    static const TArray<FRogueDamageNumber>& Numbers(const ARogueHUD* H) { return H->DamageNumbers; }
    static void Clear(ARogueHUD* H) { H->DamageNumbers.Reset(); }
};
#endif
