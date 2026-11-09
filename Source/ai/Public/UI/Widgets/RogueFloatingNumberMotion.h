#pragma once
#include "CoreMinimal.h"
/** Screen-space motion stays continuous between the existing world-projection refreshes. */
struct FRogueFloatingNumberMotion
{
    FVector2D Offset=FVector2D::ZeroVector;
    float Scale=1,Opacity=0;
    static FRogueFloatingNumberMotion Sample(float Age,float Lifetime,float Drift,bool Healing,bool Boss)
    {
        FRogueFloatingNumberMotion R;
        const float T=FMath::Max(0.f,Age),P=FMath::Clamp(T/FMath::Max(.05f,Lifetime),0.f,1.f);
        const float Travel=1-FMath::Pow(1-P,2.2f);
        R.Offset=FVector2D(Drift*Travel,-(Healing?30.f:38.f)*Travel);
        R.Scale=(Healing?.94f:Boss?1.08f:1.f)*(1+.18f*FMath::Exp(-T*15));
        R.Opacity=FMath::SmoothStep(0.f,.035f,T)*(1-FMath::SmoothStep(.42f,1.f,P));return R;
    }
};
