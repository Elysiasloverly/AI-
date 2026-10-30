#include "Enemies/Components/RogueEnemySupportComponent.h"
#include "Enemies/RogueEnemy.h"
#include "Enemies/Visuals/RogueEnemyEffectVisual.h"
#include "AbilitySystem/GameplayEffects/RogueEnemyHealEffect.h"
#include "AbilitySystem/RogueAttributeSet.h"
#include "AbilitySystem/RogueGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "Combat/Targeting/RogueEnemyTrackerSubsystem.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
URogueEnemySupportComponent::URogueEnemySupportComponent()
{ PrimaryComponentTick.bCanEverTick=true;PrimaryComponentTick.bStartWithTickEnabled=false;SetIsReplicatedByDefault(true); }
void URogueEnemySupportComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{ Super::GetLifetimeReplicatedProps(OutLifetimeProps);DOREPLIFETIME(URogueEnemySupportComponent,Link); }
void URogueEnemySupportComponent::InitializeRuntime()
{
    Timer=0;MovementTarget.Reset();MovementCycle.Invalidate();SearchRemaining=SteeringRemaining=0;CachedMoveDirection=FVector::ZeroVector;bWasRetreating=false;
    auto* E=Cast<ARogueEnemy>(GetOwner());
    SetComponentTickEnabled(E&&RogueEnemyArchetypes::IsHealerType(E->GetEnemyType())&&!E->IsAvailableInPool());
}
void URogueEnemySupportComponent::ResetForPool()
{
    if (GetOwner()->HasAuthority()) SetTarget(nullptr);
    TargetCycle.Invalidate();Timer=0;MovementTarget.Reset();MovementCycle.Invalidate();SearchRemaining=SteeringRemaining=0;CachedMoveDirection=FVector::ZeroVector;bWasRetreating=false;
    if (Circle) Circle->SetActorHiddenInGame(true);if (Beam) Beam->SetActorHiddenInGame(true);
    SetComponentTickEnabled(false);
}
bool URogueEnemySupportComponent::IsSupportAlly(const ARogueEnemy* T) const
{
    const auto* E=Cast<ARogueEnemy>(GetOwner());
    if (!E||!IsValid(T)||T==E||T->IsDead()||T->IsAvailableInPool()||RogueEnemyArchetypes::IsHealerType(T->GetEnemyType())) return false;
    const auto* A=T->GetRogueAttributeSet();const auto* ASC=T->GetAbilitySystemComponent();
    return A&&ASC&&A->GetHealth()>0&&!ASC->HasMatchingGameplayTag(RogueGameplayTags::State_Dead);
}
bool URogueEnemySupportComponent::IsWoundedAlly(const ARogueEnemy* T) const
{
    return IsSupportAlly(T)&&T->GetRogueAttributeSet()->GetHealth()<T->GetRogueAttributeSet()->GetMaxHealth()-.01f;
}
bool URogueEnemySupportComponent::HasHealingSight(const ARogueEnemy* T) const
{
    if (!IsValid(T)||!GetWorld()) return false;
    FCollisionQueryParams Q(SCENE_QUERY_STAT(EnemyHealSight),false,GetOwner());Q.AddIgnoredActor(T);
    FHitResult Hit;
    return !GetWorld()->LineTraceSingleByObjectType(Hit,GetOwner()->GetActorLocation(),T->GetActorLocation(),FCollisionObjectQueryParams(ECC_WorldStatic),Q);
}
bool URogueEnemySupportComponent::CanHeal(const ARogueEnemy* T) const
{
    const auto* E=Cast<ARogueEnemy>(GetOwner());
    return IsWoundedAlly(T)&&FVector::DistSquared2D(T->GetActorLocation(),E->GetActorLocation())<=FMath::Square(E->GetCurrentArchetype().Healing.Sanitized().Radius)&&(!E->GetCurrentArchetype().Healing.bRequireLineOfSight||HasHealingSight(T));
}
float URogueEnemySupportComponent::HealTarget(ARogueEnemy* T,float Amount)
{
    auto* E=Cast<ARogueEnemy>(GetOwner());
    if (!E||!E->HasAuthority()||E->IsDead()||E->IsAvailableInPool()||!RogueEnemyArchetypes::IsHealerType(E->GetEnemyType())
        ||!E->GetRogueAttributeSet()||E->GetRogueAttributeSet()->GetHealth()<=0||!CanHeal(T)||!FMath::IsFinite(Amount)||Amount<=0) return 0;
    auto* ASC=T->GetAbilitySystemComponent();const float Before=T->GetRogueAttributeSet()->GetHealth();
    Amount=FMath::Min(Amount,T->GetRogueAttributeSet()->GetMaxHealth()-Before);
    auto Context=ASC->MakeEffectContext();Context.AddSourceObject(E);Context.AddInstigator(E,E);
    auto Spec=ASC->MakeOutgoingSpec(URogueEnemyHealEffect::StaticClass(),1,Context);
    if (!Spec.IsValid()) return 0;
    Spec.Data->SetSetByCallerMagnitude(RogueGameplayTags::Data_Magnitude,Amount);
    ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
    return FMath::Max(0.f,T->GetRogueAttributeSet()->GetHealth()-Before);
}
void URogueEnemySupportComponent::SetTarget(ARogueEnemy* T)
{
    auto* E=Cast<ARogueEnemy>(GetOwner());
    if (!E||!E->HasAuthority()) return;
    const uint32 TG=T?T->GetPresentationGeneration():0;
    if (Link.Target==T&&Link.TargetGeneration==TG&&Link.SourceGeneration==E->GetPresentationGeneration()) return;
    Link.Target=T;Link.TargetGeneration=TG;Link.SourceGeneration=E->GetPresentationGeneration();
    TargetCycle=T?T->GetSpawnCycleId():FGuid();E->ForceNetUpdate();
}
ARogueEnemyEffectVisual* URogueEnemySupportComponent::EnsureVisual(TObjectPtr<ARogueEnemyEffectVisual>& V,TSubclassOf<ARogueEnemyEffectVisual> C)
{
    if (!V&&C&&GetNetMode()!=NM_DedicatedServer)
    { FActorSpawnParameters P;P.Owner=GetOwner();P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;V=GetWorld()->SpawnActor<ARogueEnemyEffectVisual>(C,FTransform(),P); }
    return V;
}
void URogueEnemySupportComponent::UpdateVisuals()
{
    auto* E=Cast<ARogueEnemy>(GetOwner());
    if (GetNetMode()==NM_DedicatedServer||!E) return;
    if (E->GetEnemyType()==ERogueEnemyType::AuraHealer)
    {
        if (auto* V=EnsureVisual(Circle,HealingCircleClass))
        { auto P=E->GetActorLocation();P.Z-=E->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()-5;V->ConfigureCircle(P,E->GetCurrentArchetype().Healing.Sanitized().Radius); }
    }
    const auto* T=Link.Target.Get();
    const bool Active=E->GetEnemyType()==ERogueEnemyType::LinkHealer&&T&&!T->IsDead()&&!T->IsAvailableInPool()
        &&E->GetPresentationGeneration()==Link.SourceGeneration&&T->GetPresentationGeneration()==Link.TargetGeneration;
    if (Active)
    { if (auto* V=EnsureVisual(Beam,HealingLinkClass)) V->ConfigureBeam(E->GetActorLocation()+FVector(0,0,25),T->GetActorLocation()+FVector(0,0,25)); }
    else if (Beam) Beam->SetActorHiddenInGame(true);
}
void URogueEnemySupportComponent::TickComponent(float D,ELevelTick Type,FActorComponentTickFunction* F)
{
    Super::TickComponent(D,Type,F);auto* E=Cast<ARogueEnemy>(GetOwner());
    if (!E||E->IsDead()||E->IsAvailableInPool()||!RogueEnemyArchetypes::IsHealerType(E->GetEnemyType())) { ResetForPool();return; }
    if (E->HasAuthority())
    {
        // 每帧只维护现有连线，空间查找和回血按周期执行；死亡待处理状态不能被治疗救回。
        if (Link.Target&&(!CanHeal(Link.Target)||Link.Target->GetSpawnCycleId()!=TargetCycle)) SetTarget(nullptr);
        Timer-=D;
        if (Timer<=0)
        {
            const auto S=E->GetCurrentArchetype().Healing.Sanitized();Timer=S.Interval;
            TArray<ARogueEnemy*> Targets;
            if (auto* Tracker=GetWorld()->GetSubsystem<URogueEnemyTrackerSubsystem>()) Tracker->CollectEnemiesInRange(E->GetActorLocation(),S.Radius,Targets,0,false);
            Targets.RemoveAll([&](const auto* T){return !CanHeal(T);});
            Targets.Sort([](const ARogueEnemy& A,const ARogueEnemy& B){return A.GetHealthPercent()<B.GetHealthPercent();});
            const bool Single=E->GetEnemyType()==ERogueEnemyType::LinkHealer;
            if (Single)
            {
                if (!Link.Target) SetTarget(Targets.Num()?Targets[0]:nullptr);
                Targets.Reset();if (Link.Target) Targets.Add(Link.Target);
            }
            const int32 Limit=Single?1:S.MaxTargets;
            for (int32 I=0;I<FMath::Min(Targets.Num(),Limit);++I)
            {
                auto* T=Targets[I];
                const float Amount=T->GetMaxHealth()*S.HealFraction*(T->IsBoss()?S.BossFactor:1.f);
                HealTarget(T,Amount);
            }
        }
    }
    UpdateVisuals();
}
void URogueEnemySupportComponent::EndPlay(const EEndPlayReason::Type R)
{ if (Circle) Circle->Destroy();if (Beam) Beam->Destroy();Super::EndPlay(R); }
