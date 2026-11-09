#include "AbilitySystem/Abilities/RogueBossAttackAbilities.h"
#include "Combat/Projectiles/RogueBossAttackActor.h"
#include "Enemies/RogueEnemy.h"
#include "Player/RogueCharacter.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/RogueGameplayTags.h"
#include "AbilitySystem/RogueAttributeSet.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"

void URogueBossRollingSunsAbility::PrepareBalls()
{
    Balls.Reset();bLaunched=false;auto* E=GetSkillEnemy();if(!E)return;
    const auto S=Tuning();const FVector Aim=AimDirection(),Right=FVector::CrossProduct(FVector::UpVector,Aim);
    FVector Ground=E->GetActorLocation()-FVector(0,0,E->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
    ARogueBossAttackActor::FindGround(GetWorld(),Ground,E,Ground);
    for(int32 I=-1;I<=1;++I)
        Balls.Add(SpawnEffect(ERogueBossAttackShape::RollingBall,Ground+Aim*(S.Radius*1.5f+60)+Right*(I*S.Radius*2.2f)+FVector(0,0,S.Radius),Aim,false,30));
}
void URogueBossRollingSunsAbility::OnChargeStarted_Implementation(const FRogueBossSkillState&) { PrepareBalls(); }
void URogueBossRollingSunsAbility::OnSkillPulse_Implementation(const FRogueBossSkillState&)
{
    if(bLaunched || Balls.IsEmpty())PrepareBalls();const FVector Aim=AimDirection();const auto S=Tuning();
    for(int32 I=0;I<Balls.Num();++I)if(Balls[I].IsValid())Balls[I]->Release(Aim.RotateAngleAxis((I-1)*S.SpreadDegrees,FVector::UpVector));
    bLaunched=true;
}
void URogueBossRollingSunsAbility::OnSkillStopped_Implementation(bool)
{ if(!bLaunched)for(const auto& A:Balls)if(A.IsValid())A->Destroy();Balls.Reset(); }

void URogueBossJudgmentRayAbility::OnChargeStarted_Implementation(const FRogueBossSkillState& C)
{
    auto* E=GetSkillEnemy();if(!E)return;
    FVector Ground=E->GetActorLocation()-FVector(0,0,E->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
    ARogueBossAttackActor::FindGround(GetWorld(),Ground,E,Ground);
    Beam=SpawnEffect(ERogueBossAttackShape::Beam,Ground+FVector(0,0,88),AimDirection(),false,GetCastSettings().DurationSeconds);
    if(Beam.IsValid())TrackActorUntilSkillEnds(Beam.Get());
}
void URogueBossJudgmentRayAbility::OnSkillPulse_Implementation(const FRogueBossSkillState& C)
{
    if(!Beam.IsValid())OnChargeStarted_Implementation(C);
    if(!Beam.IsValid())return;
    const FVector Target=IsValid(C.Target)?C.Target->GetActorLocation():C.TargetLocation;
    Beam->Release((Target-Beam->GetActorLocation()).GetSafeNormal2D());
}

void URogueBossLastBloomAbility::OnChargeStarted_Implementation(const FRogueBossSkillState&)
{
    auto* E=GetSkillEnemy();StartingHealth=E?E->GetCurrentHealth():0;
    if(E)if(auto* A=SpawnEffect(ERogueBossAttackShape::HealAura,E->GetActorLocation(),FVector::UpVector,true,GetCastSettings().ChargeSeconds+GetCastSettings().DurationSeconds))TrackActorUntilSkillEnds(A);
}
void URogueBossLastBloomAbility::OnSkillPulse_Implementation(const FRogueBossSkillState& C)
{
    auto* E=GetSkillEnemy();if(!E)return;
    const float Progress=GetCastProgress();
    const float Goal=FMath::Lerp(StartingHealth,E->GetMaxHealth(),Progress);
    HealOwner(FMath::Max(0.f,Goal-E->GetCurrentHealth()));
}
void URogueBossLastBloomAbility::OnRecoveryStarted_Implementation(const FRogueBossSkillState&)
{
    // 正常持续时间结束时补齐最后一段；死亡或取消不进入此回调。
    if (auto* E=GetSkillEnemy()) HealOwner(E->GetMaxHealth()-E->GetCurrentHealth());
}
void URogueBossDiamondReversalAbility::OnSkillPulse_Implementation(const FRogueBossSkillState&)
{
    auto* E=GetSkillEnemy();if(!E)return;
    E->GetAbilitySystemComponent()->AddLooseGameplayTag(RogueGameplayTags::State_BossDamageInversion);bGranted=true;
    if(auto* A=SpawnEffect(ERogueBossAttackShape::GoldenWard,E->GetActorLocation(),FVector::UpVector,true,GetCastSettings().DurationSeconds))TrackActorUntilSkillEnds(A);
}
void URogueBossDiamondReversalAbility::OnSkillStopped_Implementation(bool)
{
    if(bGranted)if(auto* E=GetSkillEnemy())E->GetAbilitySystemComponent()->RemoveLooseGameplayTag(RogueGameplayTags::State_BossDamageInversion);
    bGranted=false;
}
void URogueBossDiamondReversalAbility::OnRecoveryStarted_Implementation(const FRogueBossSkillState&)
{ OnSkillStopped_Implementation(false); }
