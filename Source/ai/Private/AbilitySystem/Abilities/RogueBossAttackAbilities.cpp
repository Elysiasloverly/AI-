#include "AbilitySystem/Abilities/RogueBossAttackAbilities.h"
#include "Combat/Projectiles/RogueBossAttackActor.h"
#include "Combat/Projectiles/RogueEnemyProjectile.h"
#include "Enemies/RogueEnemy.h"
#include "Enemies/Data/RogueEnemyArchetypes.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/GameplayEffects/RogueEnemyHealEffect.h"
#include "AbilitySystem/RogueGameplayTags.h"
#include "AbilitySystem/RogueAttributeSet.h"
#include "Player/RogueCharacter.h"
#include "TimerManager.h"

FRogueBossAttackSettings URogueBossAttackAbility::Tuning() const
{
    const auto State=GetExecutionState();
    const auto* Row=State.Skill.DataTable?State.Skill.GetRow<FRogueBossSkillRow>(TEXT("Boss attack")):nullptr;
    auto AttackSettings=Row?Row->Attack:FRogueBossAttackSettings();
    if (IsA<URogueBossJudgmentRayAbility>()) AttackSettings.DamageInterval=GetCastSettings().PulseInterval;
    return AttackSettings;
}
FVector URogueBossAttackAbility::AimDirection() const
{
    const auto* E=GetSkillEnemy();if(!E)return FVector::ForwardVector;
    const auto State=GetExecutionState();
    const FVector Target=IsValid(State.Target)?State.Target->GetActorLocation():State.TargetLocation;
    FVector Dir=(Target-E->GetActorLocation()).GetSafeNormal2D();
    return Dir.IsNearlyZero()?E->GetActorForwardVector().GetSafeNormal2D():Dir;
}
ARogueBossAttackActor* URogueBossAttackAbility::SpawnEffect(ERogueBossAttackShape Shape,const FVector& Origin,const FVector& Direction,bool Released,float Seconds)
{
    auto* E=GetSkillEnemy();if(!E||!E->HasAuthority())return nullptr;
    const auto S=Tuning();FActorSpawnParameters P;P.Owner=E;P.Instigator=E;P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* A=GetWorld()->SpawnActor<ARogueBossAttackActor>(S.EffectClass?S.EffectClass.Get():ARogueBossAttackActor::StaticClass(),Origin,FRotator::ZeroRotator,P);
    if (A)
    {
        const FRogueBossSkillState State = GetExecutionState();
        A->InitializeAttack(E, Shape, S, Origin, Direction, E->GetContactDamage() * FMath::Max(0.f, S.DamageScale),
            Seconds, Released, State.Target, State.PhaseEndServerTime - State.PhaseStartServerTime);
    }
    return A;
}
void URogueBossAttackAbility::EmitRing(int32 Count,float Yaw)
{
    auto* E=GetSkillEnemy();if(!E||!E->HasAuthority())return;
    const auto S=Tuning();auto Ring=E->GetCurrentArchetype().Ranged.BulletRing;
    Ring.ProjectileCount=FMath::Clamp(Count,8,128);Ring.ProjectileSpeed=FMath::Max(100.f,S.Speed);Ring.ProjectileScale=1;
    Ring.ProjectileLifetime = FRogueEnemyBulletRingSettings::MaxProjectileLifetime;
    Ring.HeightAboveGround = 55;
    FVector Center=E->GetActorLocation()-FVector(0,0,E->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()-55);
    FVector Ground;
    if (ARogueBossAttackActor::FindGround(GetWorld(),Center,E,Ground)) Center.Z=Ground.Z+55;
    const auto Class=S.BulletClass?S.BulletClass:E->GetEnemyProjectileClass();
    const float Radius=E->GetCapsuleComponent()->GetScaledCapsuleRadius()+30;
    E->SpawnBulletRing(Center,Yaw,Radius,Ring,E->GetContactDamage()*FMath::Max(0.f,S.DamageScale),Class);
    E->Multicast_SpawnCosmeticBulletRing(Center,Yaw,Radius,Ring,Class);E->NotifyAttackFeedback(Center);
}
void URogueBossAttackAbility::HealOwner(float Amount)
{
    auto* E=GetSkillEnemy();if(!E||!E->HasAuthority()||E->IsDead()||E->IsAvailableInPool())return;
    auto* ASC=E->GetAbilitySystemComponent();const auto* A=E->GetRogueAttributeSet();
    if(!ASC||!A||A->GetHealth()<=0||ASC->HasMatchingGameplayTag(RogueGameplayTags::State_Dead))return;
    Amount=FMath::Clamp(Amount,0.f,A->GetMaxHealth()-A->GetHealth());if(Amount<=0)return;
    auto Context=ASC->MakeEffectContext();Context.AddSourceObject(E);Context.AddInstigator(E,E);
    auto Spec=ASC->MakeOutgoingSpec(URogueEnemyHealEffect::StaticClass(),1,Context);
    if(Spec.IsValid()){Spec.Data->SetSetByCallerMagnitude(RogueGameplayTags::Data_Magnitude,Amount);ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());}
}

int32 URogueBossAttackAbility::GetOrdinaryRingCount() const
{
    const auto* E=GetSkillEnemy();if(!E)return 0;
    const auto Normal=RogueEnemyArchetypes::BuildEnemyArchetypeFromDataTable(E->EnemyArchetypeDataTable,E->GetEnemyType(),false);
    return Normal.Ranged.BulletRing.Sanitized().ProjectileCount;
}
void URogueBossOctagramAbility::OnChargeStarted_Implementation(const FRogueBossSkillState&)
{
    auto* E=GetSkillEnemy();if(!E)return;
    InitialYaw=E->GetActorRotation().Yaw;SpinStartedAt=GetWorld()->GetTimeSeconds();SpinSpeed=Tuning().RotationSpeed;
    GetWorld()->GetTimerManager().SetTimer(SpinTimer,this,&URogueBossOctagramAbility::Spin,.033f,true);
}
void URogueBossOctagramAbility::Spin()
{ if(auto* E=GetSkillEnemy()) E->SetActorRotation(FRotator(0,InitialYaw+(GetWorld()->GetTimeSeconds()-SpinStartedAt)*SpinSpeed,0)); }
void URogueBossOctagramAbility::StopSpin() { if(GetWorld())GetWorld()->GetTimerManager().ClearTimer(SpinTimer); }
void URogueBossOctagramAbility::OnRecoveryStarted_Implementation(const FRogueBossSkillState&) { StopSpin(); }
void URogueBossOctagramAbility::OnSkillStopped_Implementation(bool) { StopSpin(); }
void URogueBossOctagramAbility::OnSkillPulse_Implementation(const FRogueBossSkillState&)
{ Spin();if(auto* E=GetSkillEnemy())EmitRing(8,E->GetActorRotation().Yaw); }
void URogueBossFlameCrescentsAbility::OnSkillPulse_Implementation(const FRogueBossSkillState&)
{
    auto* E=GetSkillEnemy();if(!E)return;
    const auto S=Tuning();const FVector Aim=AimDirection();FVector Ground=E->GetActorLocation()-FVector(0,0,E->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
    ARogueBossAttackActor::FindGround(GetWorld(),Ground,E,Ground);
    for(int32 I=-1;I<=1;++I)
    {
        const FVector Dir=Aim.RotateAngleAxis(I*S.SpreadDegrees,FVector::UpVector);
        SpawnEffect(ERogueBossAttackShape::FlameWave,Ground+Dir*60+FVector(0,0,S.WaveHeightAboveGround),Dir,true,30);
    }
}
void URogueBossEndlessRingAbility::OnSkillPulse_Implementation(const FRogueBossSkillState&)
{
    auto* E=GetSkillEnemy();if(!E)return;
    const auto S=Tuning();
    EmitRing(S.RingCount>0?S.RingCount:GetOrdinaryRingCount(),E->GetActorRotation().Yaw);
}
