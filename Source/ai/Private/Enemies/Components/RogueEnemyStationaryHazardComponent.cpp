#include "Enemies/Components/RogueEnemyStationaryHazardComponent.h"
#include "Enemies/RogueEnemy.h"
#include "Player/RogueCharacter.h"
#include "Player/RoguePlayerQuery.h"
#include "AbilitySystem/RogueAbilitySystemLibrary.h"
#include "AbilitySystem/RogueGameplayTags.h"
#include "AbilitySystem/RogueAttributeSet.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/StaticMesh.h"
URogueEnemyStationaryHazardComponent::URogueEnemyStationaryHazardComponent()
{ PrimaryComponentTick.bCanEverTick=true;PrimaryComponentTick.bStartWithTickEnabled=false;PrimaryComponentTick.TickInterval=.1f; }
void URogueEnemyStationaryHazardComponent::InitializeRuntime()
{
    Timer=0;auto* E=Cast<ARogueEnemy>(GetOwner());
    SetComponentTickEnabled(E&&E->HasAuthority()&&bEnabled&&!E->IsAvailableInPool()
        &&E->GetMovementModel()==ERogueEnemyMovementModel::Stationary);
}
void URogueEnemyStationaryHazardComponent::ResetForPool()
{ Timer=0;SetComponentTickEnabled(false); }
UStaticMeshComponent* URogueEnemyStationaryHazardComponent::GetAreaMesh() const
{
    for (auto* Mesh:TInlineComponentArray<UStaticMeshComponent*>(GetOwner()))
        if (Mesh->ComponentHasTag(AreaMeshTag)&&Mesh->GetStaticMesh()) return Mesh;
    return nullptr;
}
bool URogueEnemyStationaryHazardComponent::IsPlayerInside(ARogueCharacter* P) const
{
    auto* Mesh=GetAreaMesh();if (!Mesh||!P||!P->IsAlive()||!Mesh->IsVisible()||Mesh->bHiddenInGame) return false;
    const auto Box=Mesh->GetStaticMesh()->GetBoundingBox();const FVector Size=Box.GetExtent();
    // 选模型最薄的轴作为法阵法线，其余两轴形成椭圆；支持蓝图旋转和非均匀缩放。
    int32 N=0;if (Size.Y<Size[N]) N=1;if (Size.Z<Size[N]) N=2;
    const int32 U=(N+1)%3,V=(N+2)%3;
    const auto X=Mesh->GetComponentTransform();const FVector Scale=X.GetScale3D().GetAbs();
    if (Scale.GetMin()<=KINDA_SMALL_NUMBER||Size[U]<=0||Size[V]<=0) return false;
    if (FMath::Abs(FVector::DotProduct(X.GetUnitAxis(static_cast<EAxis::Type>(N+1)),FVector::UpVector))<.7f) return false;
    const FVector Center=X.TransformPosition(Box.GetCenter());
    if (FMath::Abs(P->GetActorLocation().Z-Center.Z)>FMath::Max(50.f,HeightTolerance)) return false;
    const FVector Local=X.InverseTransformPosition(P->GetActorLocation())-Box.GetCenter();
    const float R=P->GetCapsuleComponent()->GetScaledCapsuleRadius();
    const double A=Size[U]+R/Scale[U],B=Size[V]+R/Scale[V];
    return FMath::Square(Local[U]/A)+FMath::Square(Local[V]/B)<=1;
}
void URogueEnemyStationaryHazardComponent::TickComponent(float D,ELevelTick T,FActorComponentTickFunction* F)
{
    Super::TickComponent(D,T,F);auto* E=Cast<ARogueEnemy>(GetOwner());
    if (!E||!E->HasAuthority()||E->IsDead()||E->IsAvailableInPool()||!bEnabled
        ||E->GetMovementModel()!=ERogueEnemyMovementModel::Stationary||!E->GetRogueAttributeSet()||E->GetRogueAttributeSet()->GetHealth()<=0)
    { ResetForPool();return; }
    Timer-=D;if (Timer>0) return;
    Timer=FMath::Max(.2f,Interval)/E->GetAttackSpeedMultiplier();
    const float Damage=E->GetContactDamage()*FMath::Max(0.f,DamageMultiplier);
    TArray<ARogueCharacter*,TInlineAllocator<4>> Players;RoguePlayerQuery::CollectAlivePlayers(GetWorld(),Players);
    for (auto* P:Players) if (IsPlayerInside(P))
        RogueAbilitySystem::ApplyDamageToTarget(E,P,Damage,RogueGameplayTags::Damage_Source_Contact);
}
