#pragma once
#include "CoreMinimal.h"

/** 纯显示插值，绝不回写 GAS。指数衰减独立于帧率；连续受击重置残影停留时间。 */
struct AI_API FRogueBarAnimation
{
    float Displayed = 0, Trail = 0, Target = 0, HoldRemaining = 0;
    bool bInitialized = false;
    bool bTrailingDamage = false;
    void Snap(float Value)
    {
        Displayed = Trail = Target = FMath::Clamp(FMath::IsFinite(Value) ? Value : 0.f, 0.f, 1.f);
        HoldRemaining = 0; bInitialized = true; bTrailingDamage = false;
    }
    void SetTarget(float Value, float Hold)
    {
        Value = FMath::Clamp(FMath::IsFinite(Value) ? Value : 0.f, 0.f, 1.f);
        if (!bInitialized) { Snap(Value); return; }
        if (Value < Target - KINDA_SMALL_NUMBER)
        {
            Trail = FMath::Max3(Trail, Displayed, Target);
            HoldRemaining = FMath::Max(0.f, Hold);
            bTrailingDamage = true;
        }
        else if (Value > Target + KINDA_SMALL_NUMBER)
        {
            bTrailingDamage = false; HoldRemaining = 0;
        }
        Target = Value;
    }
    void Step(float Delta, float Speed, float TrailSpeed)
    {
        Delta = FMath::Max(0.f, Delta);
        const float BeforeHold = HoldRemaining;
        HoldRemaining = FMath::Max(0.f, HoldRemaining - Delta);
        auto Approach = [](float From, float To, float Rate, float Time)
        {
            const float Next = Rate <= 0 ? To : FMath::Lerp(From, To, 1.f - FMath::Exp(-Rate * Time));
            return FMath::Abs(Next - To) < .0005f ? To : Next;
        };
        Displayed = Approach(Displayed, Target, Speed, Delta);
        if (!bTrailingDamage) { Trail = Displayed; }
        else
        {
            const float TrailDelta = FMath::Max(0.f, Delta - BeforeHold);
            if (TrailDelta > 0) { Trail = Approach(Trail, Target, TrailSpeed, TrailDelta); }
            Trail = FMath::Max(Trail, Displayed);
        }
    }
};
