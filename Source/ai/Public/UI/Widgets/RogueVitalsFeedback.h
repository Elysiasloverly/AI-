#pragma once
#include "CoreMinimal.h"

enum class ERogueVitalsChange : uint8 { None, Damage, Restore, ArmorBreak, Experience, LevelUp };

/** Local presentation only. Continuous gains/hits merge into one short-lived number. */
struct FRogueVitalsFeedback
{
    ERogueVitalsChange Kind=ERogueVitalsChange::None;
    float Amount=0, Elapsed=0, Duration=0;
    void Reset(){Kind=ERogueVitalsChange::None;Amount=Elapsed=Duration=0;}
    bool Active()const{return Kind!=ERogueVitalsChange::None&&Elapsed<Duration;}
    void Trigger(ERogueVitalsChange InKind,float Delta,float Lifetime)
    {
        Amount=(Active()&&Kind==InKind)?Amount+Delta:Delta;
        Kind=InKind;Elapsed=0;Duration=FMath::Max(.01f,Lifetime);
    }
    void Step(float Delta){Elapsed=FMath::Min(Duration,Elapsed+FMath::Max(0.f,Delta));}
    float Progress()const{return Duration>0?FMath::Clamp(Elapsed/Duration,0.f,1.f):1.f;}
    float Opacity()const{return Active()?1.f-FMath::SmoothStep(.25f,1.f,Progress()):0.f;}
};
