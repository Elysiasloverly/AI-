#include "AbilitySystem/Abilities/RogueBossSkillAbility.h"
#include "Enemies/Components/RogueBossSkillComponent.h"
#include "Enemies/RogueEnemy.h"
#include "Engine/World.h"
#include "TimerManager.h"

URogueBossSkillAbility::URogueBossSkillAbility()
{
    InstancingPolicy=EGameplayAbilityInstancingPolicy::InstancedPerActor;
    NetExecutionPolicy=EGameplayAbilityNetExecutionPolicy::ServerOnly;
    bRetriggerInstancedAbility=false;
}

void URogueBossSkillAbility::OnChargeStarted_Implementation(const FRogueBossSkillState&) {}
void URogueBossSkillAbility::OnSkillPulse_Implementation(const FRogueBossSkillState&) {}
void URogueBossSkillAbility::OnRecoveryStarted_Implementation(const FRogueBossSkillState&) {}
void URogueBossSkillAbility::OnSkillStopped_Implementation(bool) {}

bool URogueBossSkillAbility::CanActivateAbility(const FGameplayAbilitySpecHandle Handle,const FGameplayAbilityActorInfo* Info,
    const FGameplayTagContainer* Source,const FGameplayTagContainer* Target,FGameplayTagContainer* Relevant) const
{
    const auto* E=Info?Cast<ARogueEnemy>(Info->AvatarActor.Get()):nullptr;
    const auto* C=E?E->FindComponentByClass<URogueBossSkillComponent>():nullptr;
    return C && C->IsActivationAllowed(Handle) && Super::CanActivateAbility(Handle,Info,Source,Target,Relevant);
}

void URogueBossSkillAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,const FGameplayAbilityActorInfo* Info,
    const FGameplayAbilityActivationInfo Activation,const FGameplayEventData* Event)
{
    Super::ActivateAbility(Handle,Info,Activation,Event);
    if (!IsActive()) return;
    auto* E=GetSkillEnemy();
    auto* C=E?E->FindComponentByClass<URogueBossSkillComponent>():nullptr;
    if (!C || !C->AttachExecution(this,Handle)) { EndAbility(Handle,Info,Activation,false,true); return; }
    Coordinator=C; Settings=C->GetActiveSettings(); SpawnId=E->GetSpawnCycleId();
    CastId=C->GetSkillState().CastId; Pulses=0;
    CastStartedAt=CastEndsAt=0;
    const bool Charging=Settings.ChargeSeconds>0 || Settings.bPulseWhileCharging;
    const float InitialPhaseTime=Settings.ChargeSeconds+(Settings.bPulseWhileCharging ? Settings.DurationSeconds : 0.f);
    C->PublishPhase(Charging ? ERogueBossSkillPhase::Charging : ERogueBossSkillPhase::Firing,
        Charging ? InitialPhaseTime : Settings.DurationSeconds,0);
    if (!CheckExecution()) return;
    OnChargeStarted(C->GetSkillState());
    if (!CheckExecution()) return;
    if (Settings.ChargeSeconds>0)
        GetWorld()->GetTimerManager().SetTimer(SequenceTimer,this,&URogueBossSkillAbility::FirePulse,Settings.ChargeSeconds,false);
    else FirePulse();
}

ARogueEnemy* URogueBossSkillAbility::GetSkillEnemy() const { return Cast<ARogueEnemy>(GetAvatarActorFromActorInfo()); }
FRogueBossSkillState URogueBossSkillAbility::GetExecutionState() const
{ return Coordinator.IsValid()?Coordinator->GetSkillState():FRogueBossSkillState(); }

void URogueBossSkillAbility::StopExecution()
{ if (IsActive() && !bEnding) EndAbility(CurrentSpecHandle,CurrentActorInfo,CurrentActivationInfo,false,true); }

void URogueBossSkillAbility::TrackActorUntilSkillEnds(AActor* Actor)
{
    if (!IsValid(Actor) || Actor->GetWorld()!=GetWorld() || Actor->IsA<APawn>()) return;
    if (!IsActive() || bEnding) { Actor->Destroy(); return; }
    SkillActors.AddUnique(Actor);
}

bool URogueBossSkillAbility::CheckExecution()
{
    auto* E=GetSkillEnemy();
    if (IsActive() && !bEnding && E && E->GetSpawnCycleId()==SpawnId && Coordinator.IsValid() && Coordinator->IsExecutionCurrent(this,CastId)) return true;
    if (IsActive() && !bEnding) EndAbility(CurrentSpecHandle,CurrentActorInfo,CurrentActivationInfo,false,true);
    return false;
}

float URogueBossSkillAbility::GetCastProgress() const
{
    if (Settings.DurationSeconds<=0) return 1.f;
    return FMath::Clamp(float((GetWorld()->GetTimeSeconds()-CastStartedAt)/Settings.DurationSeconds),0.f,1.f);
}

void URogueBossSkillAbility::FirePulse()
{
    if (!CheckExecution()) return;
    const double Now=GetWorld()->GetTimeSeconds();
    if (Pulses==0)
    {
        CastStartedAt=Now;
        CastEndsAt=Now+Settings.DurationSeconds;
    }
    else if (Now>=CastEndsAt-UE_KINDA_SMALL_NUMBER)
    {
        EnterRecovery();
        return;
    }

    ++Pulses;
    const auto Phase=Settings.bPulseWhileCharging ? ERogueBossSkillPhase::Charging : ERogueBossSkillPhase::Firing;
    const float Remaining=FMath::Max(0.f,float(CastEndsAt-Now));
    Coordinator->PublishPhase(Phase,Remaining,Pulses);
    if (!CheckExecution()) return;
    OnSkillPulse(Coordinator->GetSkillState());
    if (!CheckExecution()) return;

    if (Remaining<=0) EnterRecovery();
    else if (IsSustainedEffect() || Remaining<=Settings.PulseInterval+UE_KINDA_SMALL_NUMBER)
        GetWorld()->GetTimerManager().SetTimer(SequenceTimer,this,&URogueBossSkillAbility::EnterRecovery,Remaining,false);
    else
        // 每次只安排下一轮。卡顿不补发，固定截止时间也不会随卡顿向后延长。
        GetWorld()->GetTimerManager().SetTimer(SequenceTimer,this,&URogueBossSkillAbility::FirePulse,Settings.PulseInterval,false);
}

void URogueBossSkillAbility::EnterRecovery()
{
    if (!CheckExecution()) return;
    ClearSkillActors();
    if (!CheckExecution()) return;
    Coordinator->PublishPhase(ERogueBossSkillPhase::Recovery,Settings.RecoverySeconds,Pulses);
    if (!CheckExecution()) return;
    OnRecoveryStarted(Coordinator->GetSkillState());
    if (!CheckExecution()) return;
    if (Settings.RecoverySeconds>0) GetWorld()->GetTimerManager().SetTimer(SequenceTimer,this,&URogueBossSkillAbility::FinishSequence,Settings.RecoverySeconds,false);
    else FinishSequence();
}

void URogueBossSkillAbility::FinishSequence()
{ if (CheckExecution()) EndAbility(CurrentSpecHandle,CurrentActorInfo,CurrentActivationInfo,false,false); }

void URogueBossSkillAbility::ClearSkillActors()
{
    auto Owned=MoveTemp(SkillActors);
    for (const auto& Actor:Owned) if (Actor.IsValid()) Actor->Destroy();
}

void URogueBossSkillAbility::EndAbility(const FGameplayAbilitySpecHandle Handle,const FGameplayAbilityActorInfo* Info,
    const FGameplayAbilityActivationInfo Activation,bool bReplicate,bool bCancelled)
{
    if (bEnding || !IsActive()) return;
    TGuardValue<bool> Guard(bEnding,true);
    if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(SequenceTimer);
    OnSkillStopped(bCancelled);
    ClearSkillActors();
    if (Coordinator.IsValid()) Coordinator->NotifyExecutionEnded(this,bCancelled);
    Coordinator.Reset();
    Super::EndAbility(Handle,Info,Activation,bReplicate,bCancelled);
}
