#include "Enemies/Components/RogueEnemySupportComponent.h"
#include "Enemies/RogueEnemy.h"
#include "AbilitySystem/RogueAttributeSet.h"
#include "Combat/Targeting/RogueEnemyTrackerSubsystem.h"
#include "Player/RogueCharacter.h"
#include "Player/RoguePlayerQuery.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"

FVector URogueEnemySupportComponent::CalculateSupportMovement(float DeltaSeconds)
{
    auto* Enemy=Cast<ARogueEnemy>(GetOwner());
    if (!Enemy||!Enemy->HasAuthority()||Enemy->IsDead()||Enemy->IsAvailableInPool()) return FVector::ZeroVector;
    const auto Settings=Movement.Sanitized();
    const auto Healing=Enemy->GetCurrentArchetype().Healing.Sanitized();
    const FVector Origin=Enemy->GetActorLocation();
    const float Delta=FMath::IsFinite(DeltaSeconds)?FMath::Clamp(DeltaSeconds,0.f,1.f):0.f;
    TArray<ARogueCharacter*,TInlineAllocator<4>> Players;
    RoguePlayerQuery::CollectAlivePlayers(GetWorld(),Players);
    auto NearestDistance=[&](const FVector& Point)
    {
        float Distance=FLT_MAX;
        for (auto* Player:Players) Distance=FMath::Min(Distance,float(FVector::Dist2D(Point,Player->GetActorLocation())));
        return Distance;
    };
    auto CanFollow=[&](const ARogueEnemy* Ally)
    {
        return IsSupportAlly(Ally)
            &&FVector::DistSquared2D(Origin,Ally->GetActorLocation())<=FMath::Square(Settings.SearchRadius);
    };
    SearchRemaining-=Delta;
    if (MovementCycle.IsValid()&&(!CanFollow(MovementTarget.Get())||MovementTarget->GetSpawnCycleId()!=MovementCycle))
    { MovementTarget.Reset();MovementCycle.Invalidate();SearchRemaining=0;SteeringRemaining=0; }
    if (SearchRemaining<=0)
    {
        SearchRemaining=Settings.SearchInterval;
        ARogueEnemy* Best=nullptr;
        float BestScore=FLT_MAX;
        auto WoundScore=[&](const ARogueEnemy* Ally)
        {
            const auto* Attributes=Ally->GetRogueAttributeSet();
            return Attributes->GetHealth()/FMath::Max(1.f,Attributes->GetMaxHealth())
                +float(FVector::Dist2D(Origin,Ally->GetActorLocation()))/Settings.SearchRadius*.05f;
        };
        // 连线仍有效时保持施法；否则优先救治伤员，满血友军仅用于空闲跟随。
        if (CanFollow(Link.Target)&&IsWoundedAlly(Link.Target)) Best=Link.Target;
        else
        {
            TArray<ARogueEnemy*> Allies;
            if (auto* Tracker=GetWorld()->GetSubsystem<URogueEnemyTrackerSubsystem>())
                Tracker->CollectEnemiesInRange(Origin,Settings.SearchRadius,Allies,0,false);
            for (auto* Ally:Allies)
            {
                if (!CanFollow(Ally)||!IsWoundedAlly(Ally)) continue;
                const float Value=WoundScore(Ally);
                if (Value<BestScore) { Best=Ally;BestScore=Value; }
            }
            // 只在同属伤员时保持原目标，不能让近处满血怪压过远处伤员。
            if (Best&&CanFollow(MovementTarget.Get())&&IsWoundedAlly(MovementTarget.Get())
                &&MovementTarget->GetSpawnCycleId()==MovementCycle
                &&WoundScore(MovementTarget.Get())<=BestScore+.08f) Best=MovementTarget.Get();
            if (!Best)
            {
                float BestDistance=FLT_MAX;
                for (auto* Ally:Allies)
                {
                    if (!CanFollow(Ally)) continue;
                    const float Distance=FVector::Dist2D(Origin,Ally->GetActorLocation());
                    if (Distance<BestDistance) { Best=Ally;BestDistance=Distance; }
                }
                // 相近的友军不反复切换，跟随目标移动后仍会更新站位。
                if (Best&&CanFollow(MovementTarget.Get())&&MovementTarget->GetSpawnCycleId()==MovementCycle
                    &&FVector::Dist2D(Origin,MovementTarget->GetActorLocation())<=BestDistance+Settings.ProbeDistance)
                    Best=MovementTarget.Get();
            }
        }
        if (MovementTarget.Get()!=Best) SteeringRemaining=0;
        MovementTarget=Best;MovementCycle=Best?Best->GetSpawnCycleId():FGuid();
    }


    ARogueCharacter* Nearest=nullptr;float PlayerDistance=FLT_MAX;
    for (auto* Player:Players)
    {
        const float Distance=FVector::Dist2D(Origin,Player->GetActorLocation());
        if (Distance<PlayerDistance) { Nearest=Player;PlayerDistance=Distance; }
    }
    // 救治伤员时兼容DT治疗半径；跟随满血怪时保留完整退避距离。
    const float SafetyDistance=IsWoundedAlly(MovementTarget.Get())?FMath::Min(Settings.RetreatDistance,Healing.Radius*.55f):Settings.RetreatDistance;
    const bool Retreat=PlayerDistance<SafetyDistance;
    float GoalDistance=Settings.ProbeDistance;
    FVector Goal=FVector::ZeroVector;
    if (Retreat) Goal=(Origin-Nearest->GetActorLocation()).GetSafeNormal2D();
    else if (auto* Ally=MovementTarget.Get())
    {
        ARogueCharacter* Threat=RoguePlayerQuery::FindNearestAlivePlayer(GetWorld(),Ally->GetActorLocation());
        FVector SafeSide=Threat?(Ally->GetActorLocation()-Threat->GetActorLocation()).GetSafeNormal2D()
            :(Origin-Ally->GetActorLocation()).GetSafeNormal2D();
        if (SafeSide.IsNearlyZero()) SafeSide=(Origin-Ally->GetActorLocation()).GetSafeNormal2D();
        if (SafeSide.IsNearlyZero()) SafeSide=FVector::ForwardVector;
        const bool Wounded=IsWoundedAlly(Ally);
        const float StandOff=Wounded?Healing.Radius*Settings.HoldRangeRatio:Settings.FollowDistance;
        FVector Destination=Ally->GetActorLocation()+SafeSide*StandOff;
        if (!Wounded)
        {
            // 空闲跟随站在怪群后排；前排友军贴近玩家时，不跟着冲入玩家脚下。
            if (auto* DestinationThreat=RoguePlayerQuery::FindNearestAlivePlayer(GetWorld(),Destination))
            {
                const FVector FromPlayer=Destination-DestinationThreat->GetActorLocation();
                if (FromPlayer.Size2D()<Settings.PreferredPlayerDistance)
                {
                    FVector Away=FromPlayer.GetSafeNormal2D();
                    if (Away.IsNearlyZero()) Away=SafeSide;
                    Destination=DestinationThreat->GetActorLocation()+Away*Settings.PreferredPlayerDistance;
                }
            }
        }
        const bool UsefulPosition=Wounded&&CanHeal(Ally)
            &&FVector::Dist2D(Origin,Ally->GetActorLocation())<=Healing.Radius*.9f
            &&FVector::DotProduct((Origin-Ally->GetActorLocation()).GetSafeNormal2D(),SafeSide)>.1f;
        if (!UsefulPosition)
        {
            if (Wounded&&Healing.bRequireLineOfSight&&!HasHealingSight(Ally))
            {
                float BestDistance=FLT_MAX;
                for (float Angle:{0.f,45.f,-45.f,90.f,-90.f,135.f,-135.f,180.f})
                {
                    FVector Candidate=Ally->GetActorLocation()+SafeSide.RotateAngleAxis(Angle,FVector::UpVector)*Healing.Radius*.7f;
                    Candidate.Z=Origin.Z;
                    if (NearestDistance(Candidate)<SafetyDistance) continue;
                    FCollisionQueryParams Q(SCENE_QUERY_STAT(HealerVantage),false,Enemy);Q.AddIgnoredActor(Ally);FHitResult Hit;
                    if (GetWorld()->LineTraceSingleByObjectType(Hit,Candidate,Ally->GetActorLocation(),FCollisionObjectQueryParams(ECC_WorldStatic),Q)) continue;
                    const float Distance=FVector::DistSquared2D(Origin,Candidate);
                    if (Distance<BestDistance) { BestDistance=Distance;Destination=Candidate; }
                }
            }
            GoalDistance=FVector::Dist2D(Origin,Destination);
            const float Tolerance=Wounded?FMath::Clamp(Healing.Radius*(1-Settings.HoldRangeRatio)*.4f,8.f,45.f)
                :FMath::Clamp(Settings.FollowDistance*.25f,35.f,100.f);
            if (GoalDistance>Tolerance||(Wounded&&!CanHeal(Ally))) Goal=(Destination-Origin).GetSafeNormal2D();
        }
    }
    else if (Nearest&&PlayerDistance<Settings.PreferredPlayerDistance)
        Goal=(Origin-Nearest->GetActorLocation()).GetSafeNormal2D();
    if (Goal.IsNearlyZero())
    {
        if (!Retreat) { CachedMoveDirection=FVector::ZeroVector;SteeringRemaining=0;return CachedMoveDirection; }
        Goal=Enemy->GetActorForwardVector().GetSafeNormal2D();
    }
    SteeringRemaining-=Delta;
    if (Retreat!=bWasRetreating) SteeringRemaining=0;
    bWasRetreating=Retreat;
    if (SteeringRemaining>0) return CachedMoveDirection;
    SteeringRemaining=.12f;

    // 仅用现有碰撞做短程绕障，不依赖地图新增NavMesh；查询忽略友方Pawn。
    const auto* Capsule=Enemy->GetCapsuleComponent();
    const float Radius=Capsule->GetScaledCapsuleRadius()*.9f;
    const auto Shape=FCollisionShape::MakeCapsule(Radius,FMath::Max(Radius,Capsule->GetScaledCapsuleHalfHeight()-6));
    FCollisionQueryParams Query(SCENE_QUERY_STAT(HealerSteering),false,Enemy);
    // 玩家距离由威胁评分处理，忽略其胶囊，允许从已经重叠的位置逃离。
    for (auto* Player:Players) Query.AddIgnoredActor(Player);
    const FVector ProbeOrigin=Origin+FVector(0,0,4);
    const float Probe=FMath::Clamp(GoalDistance,12.f,Settings.ProbeDistance);
    float BestValue=-FLT_MAX;CachedMoveDirection=FVector::ZeroVector;
    const float Side=Enemy->GetUniqueID()%2?1.f:-1.f;
    for (float Angle:{0.f,30.f,-30.f,60.f,-60.f,90.f,-90.f,120.f,-120.f,150.f,-150.f,180.f})
    {
        const FVector Direction=Goal.RotateAngleAxis(Angle*Side,FVector::UpVector);
        const FVector Next=Origin+Direction*Probe;
        const float NextDistance=NearestDistance(Next);
        if (!Retreat&&NextDistance<SafetyDistance) continue;
        FHitResult Hit;
        if (GetWorld()->SweepSingleByChannel(Hit,ProbeOrigin,ProbeOrigin+Direction*Probe,FQuat::Identity,ECC_Pawn,Shape,Query)
            &&Hit.ImpactNormal.Z<Enemy->GetCharacterMovement()->GetWalkableFloorZ()) continue;
        const float Improvement=Players.Num()?(NextDistance-PlayerDistance)/Probe:0.f;
        const float Value=Retreat?Improvement*10+FVector::DotProduct(Direction,Goal)*.1f
            :FVector::DotProduct(Direction,Goal)*2+Improvement*.15f;
        if (Value>BestValue) { BestValue=Value;CachedMoveDirection=Direction; }
        if (!Retreat&&FMath::IsNearlyZero(Angle)) break;
    }
    return CachedMoveDirection;
}
