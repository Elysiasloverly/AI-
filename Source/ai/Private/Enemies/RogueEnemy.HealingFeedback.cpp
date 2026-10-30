#include "Enemies/RogueEnemy.h"
#include "Combat/Visuals/RogueCombatFeedback.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"

void ARogueEnemy::RecordHealingFeedback(float Amount)
{
    if (!HasAuthority()||bSuppressDamageFeedback||bPoolAvailable||bDead||bPendingDeath||!FMath::IsFinite(Amount)||Amount<=0) return;
    ++HealingFeedback.Sequence;
    HealingFeedback.TotalHealing+=Amount;
    HealingFeedback.LatestHealing=Amount;
    HealingFeedback.ServerTime=GetWorld()->GetTimeSeconds();
    if (GetNetMode()!=NM_DedicatedServer)
        RogueCombatFeedback::AddHealingNumber(this,GetHealthBarWorldLocation()+FVector(0,0,18),Amount);
}

void ARogueEnemy::OnRep_HealingFeedback()
{
    ConsumeReplicatedHealingFeedback();
}

void ARogueEnemy::ConsumeReplicatedHealingFeedback()
{
    const bool NewGeneration=ObservedHealingGeneration!=HealingFeedback.Generation;
    if (!HealingFeedback.bActive||HealingFeedback.Sequence==0)
    {
        ObservedHealingGeneration=HealingFeedback.Generation;
        ObservedHealingSequence=HealingFeedback.Sequence;
        ObservedTotalHealing=HealingFeedback.TotalHealing;
        return;
    }
    // 激活位、生命属性与表现档案的到达顺序不同；等当前周期就绪再消费。
    if (!bReplicatedActive||bPoolAvailable||bDead||bPendingDeath||HealingFeedback.Generation!=HitFeedback.Generation) return;
    if (!NewGeneration&&ObservedHealingSequence==HealingFeedback.Sequence) return;
    const float Amount=NewGeneration?HealingFeedback.LatestHealing:FMath::Max(0.f,HealingFeedback.TotalHealing-ObservedTotalHealing);
    ObservedHealingGeneration=HealingFeedback.Generation;
    ObservedHealingSequence=HealingFeedback.Sequence;
    ObservedTotalHealing=HealingFeedback.TotalHealing;
    const auto* State=GetWorld()->GetGameState();
    const float Now=State?State->GetServerWorldTimeSeconds():GetWorld()->GetTimeSeconds();
    if (Now-HealingFeedback.ServerTime>1.f||Now-HealingFeedback.ServerTime<-.5f) return;
    if (Amount>0&&GetNetMode()!=NM_DedicatedServer)
        RogueCombatFeedback::AddHealingNumber(this,GetHealthBarWorldLocation()+FVector(0,0,18),Amount);
}
