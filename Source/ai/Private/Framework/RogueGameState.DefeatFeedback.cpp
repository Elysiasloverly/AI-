#include "Framework/RogueGameState.h"
#include "Combat/Visuals/RogueDefeatFeedbackSubsystem.h"
#include "Combat/Visuals/RogueEnemyDefeatEffect.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/PlayerController.h"
#include "UI/RogueHUD.h"
#include "TimerManager.h"
void ARogueGameState::QueueDefeatFeedback(const FRogueDefeatVisualEvent& Event)
{
    if(!HasAuthority()||!GetWorld()||Event.Location.ContainsNaN())return;
    if(PendingDefeats.Num()<96)PendingDefeats.Add(Event);
    else
    {
        // Saturated cosmetic frames retain credited kill counts, while coalescing one visual position.
        auto* Same=PendingDefeats.FindByPredicate([&](const auto& V){return V.CreditedPlayer==Event.CreditedPlayer;});
        if(Same){Same->Count=uint16(FMath::Min(65535,int32(Same->Count)+int32(Event.Count)));Same->bBoss|=Event.bBoss;}
    }
    if(!bDefeatFlushScheduled)
    {bDefeatFlushScheduled=true;GetWorld()->GetTimerManager().SetTimer(DefeatFlushTimer,this,&ARogueGameState::FlushDefeatFeedback,.06f,false);}
}
void ARogueGameState::FlushDefeatFeedback()
{
    bDefeatFlushScheduled=false;
    if(!PendingDefeats.IsEmpty())
    {
        TArray<FRogueDefeatVisualEvent> Batch;const int32 Num=FMath::Min(24,PendingDefeats.Num());Batch.Append(PendingDefeats.GetData(),Num);PendingDefeats.RemoveAt(0,Num,EAllowShrinking::No);MulticastDefeatFeedback(Batch);
    }
    if(!PendingDefeats.IsEmpty())
    {bDefeatFlushScheduled=true;GetWorld()->GetTimerManager().SetTimer(DefeatFlushTimer,this,&ARogueGameState::FlushDefeatFeedback,.06f,false);}
}
void ARogueGameState::MulticastDefeatFeedback_Implementation(const TArray<FRogueDefeatVisualEvent>& Events)
{
    UWorld* W=GetWorld();if(!W||W->GetNetMode()==NM_DedicatedServer)return;
    auto* Effects=W->GetSubsystem<URogueDefeatFeedbackSubsystem>();
    for(const auto& Event:Events)
    {
        if(Effects)Effects->Play(Event.EffectClass,Event.Location,Event.bBoss,Event.Size);
        if(!Event.CreditedPlayer)continue;
        for(auto It=W->GetPlayerControllerIterator();It;++It)
            if(auto* PC=It->Get();PC&&PC->IsLocalController()&&PC->PlayerState==Event.CreditedPlayer)
                if(auto* HUD=Cast<ARogueHUD>(PC->GetHUD()))HUD->NotifyEnemyDefeated(Event.Count,Event.bBoss);
    }
}
