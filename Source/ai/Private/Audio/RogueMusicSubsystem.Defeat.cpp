#include "Audio/RogueMusicSubsystem.h"
#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundConcurrency.h"

void URogueMusicSubsystem::PlayDefeatFeedback(int32 Streak,bool bBoss)
{
    if(!CanPlayInWorld() || !LoadedConfig || SoundEffectsVolume<=KINDA_SMALL_NUMBER)return;
    UWorld* W=GetWorld();if(!W)return;
    const double Now=W->GetTimeSeconds();
    // Dense multi-kills form a single quiet confirmation rather than an overlapping wall of chimes.
    if(!bBoss && Now>=LastDefeatSoundTime && Now-LastDefeatSoundTime<.10)return;
    USoundBase* Sound=bBoss?LoadedConfig->BossDefeatSound.Get():LoadedConfig->DefeatSound.Get();
    const float Volume=FMath::IsFinite(LoadedConfig->DefeatVolume)?FMath::Clamp(LoadedConfig->DefeatVolume,0.f,1.f):0.f;
    if(!Sound || Volume<=KINDA_SMALL_NUMBER)return;
    if(MixWorld.Get()!=W)ApplyVolumes();
    if(!DefeatConcurrency)
    {
        DefeatConcurrency=NewObject<USoundConcurrency>(this);DefeatConcurrency->Concurrency.MaxCount=3;
        DefeatConcurrency->Concurrency.ResolutionRule=EMaxConcurrentResolutionRule::StopOldest;
        DefeatConcurrency->Concurrency.VoiceStealReleaseTime=.025f;
    }
    const float Pitch=bBoss?1.f:1.f+.018f*FMath::Min(8,FMath::Max(0,Streak-1));
    auto* C=UGameplayStatics::CreateSound2D(this,Sound,Volume,Pitch,0,DefeatConcurrency,false,true);
    if(!C)return;C->SoundClassOverride=GetEffectsClass();C->bIsUISound=false;C->bAllowSpatialization=false;C->Play();LastDefeatSoundTime=Now;
}
