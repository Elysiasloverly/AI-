#include "AbilitySystem/Abilities/RogueBossAttackAbilities.h"
#include "Combat/Projectiles/RogueBossAttackActor.h"
#include "World/RogueEnemySpawnRegion.h"
#include "Enemies/RogueEnemy.h"
#include "Player/RogueCharacter.h"
#include "Player/RoguePlayerQuery.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"

void URogueBossScarletRainAbility::OnChargeStarted_Implementation(const FRogueBossSkillState&)
{
    SafeCenters.Reset();RecentImpacts.Reset();
    TArray<ARogueCharacter*,TInlineAllocator<4>> Players;RoguePlayerQuery::CollectAlivePlayers(GetWorld(),Players);
    for(auto* P:Players)SafeCenters.Add(P->GetActorLocation());
}
void URogueBossScarletRainAbility::OnSkillPulse_Implementation(const FRogueBossSkillState& C)
{
    auto* E=GetSkillEnemy();if(!E)return;
    ARogueEnemySpawnRegion* Region=nullptr;for(TActorIterator<ARogueEnemySpawnRegion> It(GetWorld());It;++It){Region=*It;break;}
    if(!Region)return;const auto S=Tuning();
    TArray<ARogueCharacter*,TInlineAllocator<4>> Players;RoguePlayerQuery::CollectAlivePlayers(GetWorld(),Players);
    TArray<FVector> Corners;Region->GetWorldCorners(Corners);FBox Box(ForceInit);for(const auto& P:Corners)Box+=P;
    const int32 BombCount=FMath::Clamp(S.BombsPerPulse,1,8);
    const float SweepAngle=float(C.CastId % 360)*29.f+float(C.PulseIndex-1)*137.508f;
    const FVector Downwind=FVector(.43f,.18f,-1.f).GetSafeNormal().RotateAngleAxis(float(C.CastId % 12)*23.f,FVector::UpVector);
    for (int32 Index=0; Index<BombCount; ++Index)
    {
        // 同一轮有一半陨火沿玩家附近的扇区成组落下，另一半继续覆盖全场。
        // 近处找不到合法落点时回退到全图，所有分布都复用下方的安全区和落点间距。
        const bool bNearbyGroup=Index < BombCount/2 && !Players.IsEmpty();
        for (int32 Attempt=0; Attempt<32; ++Attempt)
        {
            FVector Candidate;
            if (bNearbyGroup && Attempt<15)
            {
                const FVector Focus=Players[(C.PulseIndex-1)%Players.Num()]->GetActorLocation();
                const float Minimum=FMath::Max(700.f,S.SafeRadius+S.Radius+110.f);
                const float Yaw=SweepAngle+FMath::FRandRange(-42.f,42.f)+float(Attempt/5)*78.f;
                Candidate=Focus+FRotator(0,Yaw,0).Vector()*FMath::FRandRange(Minimum,Minimum+690.f);
            }
            else if ((!bNearbyGroup && Attempt<8) && Box.IsValid)
            {
                const int32 Cell=((C.PulseIndex-1)*BombCount+Index+Attempt*3)*7%25;
                Candidate=Box.GetCenter();
                Candidate.X=FMath::Lerp(Box.Min.X,Box.Max.X,((Cell%5)+FMath::FRandRange(.15f,.85f))/5);
                Candidate.Y=FMath::Lerp(Box.Min.Y,Box.Max.Y,((Cell/5)+FMath::FRandRange(.15f,.85f))/5);
            }
            else Candidate=Region->GetRandomCandidate();
            FVector Start,End;FHitResult Hit;FCollisionQueryParams Q(SCENE_QUERY_STAT(BossRainGround),false,E);
            if(!Region->IsInsideSpawnOutline(Candidate,S.Radius) || !Region->ClipGroundTrace(Candidate,4000,4000,Start,End)
                || !GetWorld()->LineTraceSingleByObjectType(Hit,Start,End,FCollisionObjectQueryParams(ECC_WorldStatic),Q)
                || Hit.ImpactNormal.Z<.6f || !Region->IsGroundHeightAllowed(Hit.ImpactPoint))continue;
            const FVector Ground=Hit.ImpactPoint;
            bool Safe=true;
            for(const auto& P:SafeCenters)if(FVector::DistSquared2D(P,Ground)<FMath::Square(S.SafeRadius+S.Radius)){Safe=false;break;}
            for(auto* P:Players)if(FVector::DistSquared2D(P->GetActorLocation(),Ground)<FMath::Square(FMath::Max(180.f,S.SafeRadius*.55f)+S.Radius)){Safe=false;break;}
            for(const auto& P:RecentImpacts)if(FVector::DistSquared2D(P,Ground)<FMath::Square(FMath::Max(S.ImpactSpacing,S.Radius*2.1f))){Safe=false;break;}
            if(!Safe)continue;
            SpawnEffect(ERogueBossAttackShape::Meteor,Ground,Downwind,true,S.FireSeconds);
            RecentImpacts.Add(Ground);break;
        }
    }
}
void URogueBossFrostPursuitAbility::OnSkillPulse_Implementation(const FRogueBossSkillState&)
{
    auto* E=GetSkillEnemy();if(!E)return;
    TArray<ARogueCharacter*,TInlineAllocator<4>> Players;RoguePlayerQuery::CollectAlivePlayers(GetWorld(),Players);
    for(auto* P:Players)
    {
        const FVector Feet=P->GetActorLocation()-FVector(0,0,P->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());FVector Ground;
        if(ARogueBossAttackActor::FindGround(GetWorld(),Feet,E,Ground))SpawnEffect(ERogueBossAttackShape::IceSpike,Ground,FVector::UpVector,true,.8f);
    }
}
