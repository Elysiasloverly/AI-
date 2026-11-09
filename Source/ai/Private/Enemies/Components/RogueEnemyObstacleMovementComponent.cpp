#include "Enemies/Components/RogueEnemyObstacleMovementComponent.h"
#include "Enemies/Spawning/RogueEnemyNavigationBudget.h"
#include "Enemies/RogueEnemy.h"
#include "Enemies/Components/RogueEnemySupportComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"
#include "Debug/RoguePerfStats.h"

namespace RogueObstacleRouting
{
constexpr int32 MaxSearchNodes=1536;
constexpr int32 QueriesPerSearchSlice=48;
const FIntPoint Neighbors[]={{1,0},{0,1},{-1,0},{0,-1},{1,1},{-1,1},{-1,-1},{1,-1}};
}

URogueEnemyObstacleMovementComponent::URogueEnemyObstacleMovementComponent()
{ PrimaryComponentTick.bCanEverTick=false; }

void URogueEnemyObstacleMovementComponent::ClearSearch()
{
    bSearching=false;SearchNodes.Reset();SearchLookup.Reset();SearchOpen.Reset();
    ExpandingNode=INDEX_NONE;NextNeighbor=-1;bPendingGround=false;
}

void URogueEnemyObstacleMovementComponent::ResetForPool()
{
    bFollowingWall=false;CachedDirection=FVector::ZeroVector;
    NextDecision=NextPathRequest=NextSearchAllowed=0;PathPoints.Reset();PathIndex=0;
    LastObservation=-1;ProgressTime=0;EscapeUntil=0;SearchFailures=0;PenetrationEscape=FVector::ZeroVector;
    PathGoal=SearchOrigin=SearchGoal=PendingPosition=FVector::ZeroVector;ClearSearch();
}

bool URogueEnemyObstacleMovementComponent::SweepWall(const FVector& Start,const FVector& End,FHitResult& Hit) const
{
    const auto* E=Cast<ARogueEnemy>(GetOwner());
    if(!E)return false;
    const auto* C=E->GetCapsuleComponent();
    // 只留脚底查询余量，顶部与真实胶囊齐平，不能把低顶通道误判成可通过。
    const float Radius=FMath::Max(2.f,C->GetScaledCapsuleRadius());
    const float HalfHeight=FMath::Max(Radius,C->GetScaledCapsuleHalfHeight()-2);
    FCollisionQueryParams Q(SCENE_QUERY_STAT(EnemyObstacleProbe),false,E);
    FCollisionResponseParams Response;C->InitSweepCollisionParams(Q,Response);
    const bool HitSomething=GetWorld()->SweepSingleByChannel(Hit,Start+FVector(0,0,2),End+FVector(0,0,2),C->GetComponentQuat(),C->GetCollisionObjectType(),FCollisionShape::MakeCapsule(Radius,HalfHeight),Q,Response);
    return HitSomething && Hit.bBlockingHit && (Hit.bStartPenetrating || Hit.ImpactNormal.Z<E->GetCharacterMovement()->GetWalkableFloorZ());
}

bool URogueEnemyObstacleMovementComponent::QueryWall(const FVector& Start,const FVector& End,bool& bBlocked,FHitResult* OutHit) const
{
    // Budget exhaustion is unknown/blocked, never an assertion that a corridor is clear.
    bBlocked=true;
    if(!GetWorld()->GetSubsystem<URogueEnemyNavigationBudget>()->ConsumeCollisionQuery())return false;
    FHitResult Hit;bBlocked=SweepWall(Start,End,Hit);if(OutHit)*OutHit=Hit;return true;
}

bool URogueEnemyObstacleMovementComponent::FindSupport(const FVector& Center,FHitResult& Floor) const
{
    const auto* E=Cast<ARogueEnemy>(GetOwner());const auto* C=E->GetCapsuleComponent();
    const float Step=E->GetCharacterMovement()->MaxStepHeight+4;
    const FVector Foot=Center-FVector(0,0,C->GetScaledCapsuleHalfHeight());
    FCollisionQueryParams Q(SCENE_QUERY_STAT(EnemyRouteFloor),false,E);FCollisionResponseParams Response;C->InitSweepCollisionParams(Q,Response);
    return GetWorld()->LineTraceSingleByChannel(Floor,Foot+FVector(0,0,Step),Foot-FVector(0,0,300),C->GetCollisionObjectType(),Q,Response)
        &&Floor.ImpactNormal.Z>=E->GetCharacterMovement()->GetWalkableFloorZ();
}

bool URogueEnemyObstacleMovementComponent::CanAdvanceOnGround(const FVector& Delta,FHitResult& OutEdge)
{
    const auto* E=Cast<ARogueEnemy>(GetOwner());
    if(RogueEnemyArchetypes::IsFlyingType(E->GetEnemyType())||E->GetCharacterMovement()->IsFalling())return true;
    const FVector Origin=E->GetActorLocation();
    const float Spacing=FMath::Max(16.f,E->GetCapsuleComponent()->GetScaledCapsuleRadius()*.75f);
    const int32 Samples=FMath::Max(1,FMath::CeilToInt(Delta.Size2D()/Spacing));
    FHitResult Floor;
    for(int32 I=1;I<=Samples;++I)
        if(!FindSupport(Origin+Delta*(float(I)/Samples),Floor))
        {
            OutEdge=FHitResult();OutEdge.bBlockingHit=true;OutEdge.ImpactNormal=-Delta.GetSafeNormal2D();OutEdge.Normal=OutEdge.ImpactNormal;
            return false;
        }
    return true;
}

bool URogueEnemyObstacleMovementComponent::QueryCorridor(const FVector& Start,const FVector& End,bool& bBlocked) const
{
    FHitResult DirectHit;
    if(!QueryWall(Start,End,bBlocked,&DirectHit))return false;
    const auto* E=Cast<ARogueEnemy>(GetOwner());
    if(RogueEnemyArchetypes::IsFlyingType(E->GetEnemyType()))return true;
    const bool DirectBlocked=bBlocked;
    const float Distance=FVector::Dist2D(Start,End);
    if(Distance>768){bBlocked=true;return true;}
    const auto* Movement=E->GetCharacterMovement();
    const float Half=E->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    const float Step=Movement->MaxStepHeight+4;
    // 高墙与初始穿透不能靠抬脚通过，立即拒绝，避免每个墙邻居都重复采样整段地面。
    if(DirectBlocked && (DirectHit.bStartPenetrating ||
        DirectHit.ImpactPoint.Z>FMath::Max(Start.Z,End.Z)-Half+Step))
        return true;
    const float Spacing=FMath::Max(24.f,E->GetCapsuleComponent()->GetScaledCapsuleRadius()*.75f);
    const int32 Samples=FMath::Max(1,FMath::CeilToInt(Distance/Spacing));
    auto* Budget=GetWorld()->GetSubsystem<URogueEnemyNavigationBudget>();
    if(!Budget->ConsumeCollisionQueries(Samples+2)){bBlocked=true;return false;}

    // 逐段跟随地面高度，不能始终在旧高度找地面，否则楼梯会被当作一面墙。
    TArray<FVector,TInlineAllocator<36>> Supported;
    FHitResult Floor;
    const FVector Edge=(End-Start).GetSafeNormal2D()*FMath::Min(2.f,Distance*.5f);
    if(!FindSupport(Start+Edge,Floor)){bBlocked=true;return true;}
    FVector Previous(Start.X,Start.Y,Floor.ImpactPoint.Z+Half+2);
    Supported.Add(Previous);
    bool HeightChanges=false;
    for(int32 I=1;I<=Samples;++I)
    {
        const FVector XY=FMath::Lerp(Start,End,float(I)/Samples);
        if(!FindSupport(FVector(XY.X,XY.Y,Previous.Z),Floor)){bBlocked=true;return true;}
        const FVector Next(XY.X,XY.Y,Floor.ImpactPoint.Z+Half+2);
        if(FMath::Abs(Next.Z-Previous.Z)>Step){bBlocked=true;return true;}
        HeightChanges|=FMath::Abs(Next.Z-Previous.Z)>1.f;
        Supported.Add(Next);Previous=Next;
    }
    // 同 XY 的楼梯下方并不等于到达楼梯上方；允许玩家与敌人的胶囊中心高度不同。
    if(FMath::Abs(Previous.Z-End.Z)>Step+32.f || !FindSupport(Previous-Edge,Floor))
    {bBlocked=true;return true;}
    if(!DirectBlocked){bBlocked=false;return true;}
    if(!HeightChanges){bBlocked=true;return true;}

    // 候选地面可逐级走上去后，再验证抬脚所需的完整胶囊空间，保留低顶/栏杆阻挡。
    for(int32 I=1;I<Supported.Num();++I)
    {
        FVector A=Supported[I-1],B=Supported[I];
        A.Z=B.Z=FMath::Max(A.Z,B.Z)+Step;
        if(!QueryWall(A,B,bBlocked))return false;
        if(bBlocked)return true;
    }
    bBlocked=false;return true;
}

void URogueEnemyObstacleMovementComponent::NotifyBlocked(const FHitResult& Hit,const FVector& Desired)
{
    const auto* E=Cast<ARogueEnemy>(GetOwner());
    if(!bEnabled||!E||!E->HasAuthority()||!Hit.bBlockingHit||Hit.ImpactNormal.Z>=E->GetCharacterMovement()->GetWalkableFloorZ())return;
    FVector N=Hit.ImpactNormal.GetSafeNormal2D();
    if(N.IsNearlyZero())N=-Desired.GetSafeNormal2D();
    if(N.IsNearlyZero())return;
    if(Hit.bStartPenetrating){PenetrationEscape=N;EscapeUntil=GetWorld()->GetTimeSeconds()+.2;}
    if(!bFollowingWall)
    {
        const FVector Tangent=N.RotateAngleAxis(90,FVector::UpVector);
        const float Alignment=FVector::DotProduct(Tangent,Desired);
        const float Side=FMath::Abs(Alignment)>.1f?(Alignment>0?1:-1):(E->GetUniqueID()%2?1:-1);
        CachedDirection=Tangent*Side;
    }
    bFollowingWall=true;
    // A collision invalidates a cached path, but repeated contacts must not restart an in-flight search.
    if(!PathPoints.IsEmpty()){++SearchFailures;InvalidateRoute(GetWorld()->GetTimeSeconds());}
}

bool URogueEnemyObstacleMovementComponent::TryNavigationPath(const FVector& Destination)
{
    auto* E=Cast<ARogueEnemy>(GetOwner());
    NextPathRequest=GetWorld()->GetTimeSeconds()+1.f+(E->GetUniqueID()%7)*.04f;
    // Ground NavMesh cannot describe a flying capsule's actual clearance.
    if(RogueEnemyArchetypes::IsFlyingType(E->GetEnemyType()))return false;
    auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
    if(!Nav||!Nav->GetNavDataForProps(E->GetNavAgentPropertiesRef(),E->GetActorLocation()))return false;
    if(!GetWorld()->GetSubsystem<URogueEnemyNavigationBudget>()->ConsumePath())return false;
    UNavigationPath* Path=UNavigationSystemV1::FindPathToLocationSynchronously(GetWorld(),E->GetNavAgentLocation(),Destination,E);
    if(!Path||!Path->IsValid()||Path->IsPartial()||Path->PathPoints.Num()<2)return false;
    PathPoints=Path->PathPoints;
    // NavMesh returns feet locations; capsule probes and waypoints use capsule centers.
    for(FVector& Point:PathPoints)Point.Z+=E->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+2;
    PathIndex=1;PathGoal=Destination;ProgressLocation=E->GetActorLocation();ProgressTime=GetWorld()->GetTimeSeconds();
    ClearSearch();return true;
}

void URogueEnemyObstacleMovementComponent::StartSearch(const FVector& Destination)
{
    const auto* E=Cast<ARogueEnemy>(GetOwner());ClearSearch();
    SearchOrigin=E->GetActorLocation();SearchGoal=Destination;
    PathGoal=Destination;SearchStarted=GetWorld()->GetTimeSeconds();
    const float BaseCell=FMath::Max(96.f,E->GetCapsuleComponent()->GetScaledCapsuleRadius()*2+20);
    const int32 Pass=SearchFailures%4;
    CellSize=BaseCell;
    if(SearchFailures>0 && Pass==1)CellSize=FMath::Max(24.f,E->GetCapsuleComponent()->GetScaledCapsuleRadius()*.5f);
    if(Pass==2)CellSize=FMath::Max(BaseCell,256.f);
    if(Pass==3)CellSize=FMath::Max(BaseCell,512.f);
    // 失败后既尝试细网格窄门，也扩大搜索范围绕长墙；不再永远困在同一个小方框。
    const float Expansion=Pass==2?4.f:(Pass==3?8.f:1.f);
    const float Margin=FMath::Min(12800.f,Expansion*FMath::Clamp(float(FVector::Dist2D(SearchOrigin,SearchGoal))*.6f,1200.f,3200.f));
    SearchBounds=FBox2D(ForceInit);SearchBounds+=FVector2D(SearchOrigin);SearchBounds+=FVector2D(SearchGoal);SearchBounds=SearchBounds.ExpandBy(Margin);
    SearchNodes.Add({FIntPoint::ZeroValue,SearchOrigin,0,INDEX_NONE,false});SearchLookup.Add(FIntPoint::ZeroValue,0);
    SearchOpen.HeapPush(FOpenNode{0,0,float(FVector::Dist2D(SearchOrigin,SearchGoal))});BestNode=0;bSearching=true;
}

void URogueEnemyObstacleMovementComponent::FinishSearch(int32 EndNode,bool bReachedGoal)
{
    PathPoints.Reset();PathIndex=0;
    const float StartDistance=FVector::Dist2D(SearchOrigin,SearchGoal);
    const bool UsefulPartial=EndNode!=INDEX_NONE&&FVector::Dist2D(SearchNodes[EndNode].Position,SearchGoal)<StartDistance-CellSize;
    if(bReachedGoal||UsefulPartial)
    {
        for(int32 I=EndNode;I!=INDEX_NONE;I=SearchNodes[I].Parent)PathPoints.Insert(SearchNodes[I].Position,0);
        if(bReachedGoal)PathPoints.Add(SearchGoal);
        PathIndex=PathPoints.Num()>1?1:0;
    }
    // Impossible goals settle at a reachable position and retry slowly, rather than circling forever.
    if(bReachedGoal)SearchFailures=0;else ++SearchFailures;
    NextSearchAllowed=GetWorld()->GetTimeSeconds()+(bReachedGoal?.2f:FMath::Min(5.f,1.5f*SearchFailures));
    ProgressLocation=GetOwner()->GetActorLocation();ProgressTime=GetWorld()->GetTimeSeconds();
    ClearSearch();
    if(PathPoints.IsEmpty())CachedDirection=FVector::ZeroVector;
}

void URogueEnemyObstacleMovementComponent::AdvanceSearch()
{
    using namespace RogueObstacleRouting;
    auto* E=Cast<ARogueEnemy>(GetOwner());auto* Budget=GetWorld()->GetSubsystem<URogueEnemyNavigationBudget>();
    int32 Queries=0;
    while(bSearching&&Queries<QueriesPerSearchSlice)
    {
        if(ExpandingNode==INDEX_NONE)
        {
            while(!SearchOpen.IsEmpty())
            {
                FOpenNode Open;SearchOpen.HeapPop(Open,EAllowShrinking::No);
                if(!SearchNodes[Open.Index].bClosed&&Open.Cost<=SearchNodes[Open.Index].Cost+.01f){ExpandingNode=Open.Index;break;}
            }
            if(ExpandingNode==INDEX_NONE){FinishSearch(BestNode,false);break;}
            SearchNodes[ExpandingNode].bClosed=true;NextNeighbor=-1;bPendingGround=false;
            if(FVector::DistSquared2D(SearchNodes[ExpandingNode].Position,SearchGoal)<FVector::DistSquared2D(SearchNodes[BestNode].Position,SearchGoal))BestNode=ExpandingNode;
        }
        // Copy the node: adding a neighbor may reallocate SearchNodes.
        const FSearchNode Current=SearchNodes[ExpandingNode];
        if(NextNeighbor<0)
        {
            const int32 Before=Budget->GetCollisionQueriesUsed();
            bool Blocked;if(!QueryCorridor(Current.Position,SearchGoal,Blocked))break;Queries+=Budget->GetCollisionQueriesUsed()-Before;
            if(!Blocked){FinishSearch(ExpandingNode,true);break;}
            NextNeighbor=0;
        }
        if(NextNeighbor>=UE_ARRAY_COUNT(Neighbors)){ExpandingNode=INDEX_NONE;continue;}
        const FIntPoint Cell=Current.Cell+Neighbors[NextNeighbor];
        const int32* Found=SearchLookup.Find(Cell);
        const float Cost=Current.Cost+(NextNeighbor<4?CellSize:CellSize*1.41421356f);
        if(Found&&(SearchNodes[*Found].bClosed||SearchNodes[*Found].Cost<=Cost)){++NextNeighbor;bPendingGround=false;continue;}
        if(!bPendingGround)
        {
            PendingPosition=SearchOrigin+FVector(Cell.X*CellSize,Cell.Y*CellSize,0);PendingPosition.Z=Current.Position.Z;
            if(!SearchBounds.IsInsideOrOn(FVector2D(PendingPosition))){++NextNeighbor;continue;}
            if(!RogueEnemyArchetypes::IsFlyingType(E->GetEnemyType()))
            {
                if(!Budget->ConsumeCollisionQuery())break;++Queries;
                const float Half=E->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
                const float Step=E->GetCharacterMovement()->MaxStepHeight+4;
                const FVector Foot=PendingPosition-FVector(0,0,Half);
                FCollisionQueryParams Q(SCENE_QUERY_STAT(EnemyRouteFloor),false,E);
                FCollisionResponseParams Response;E->GetCapsuleComponent()->InitSweepCollisionParams(Q,Response);
                FHitResult Floor;
                if(!GetWorld()->LineTraceSingleByChannel(Floor,Foot+FVector(0,0,Step),Foot-FVector(0,0,Step),E->GetCapsuleComponent()->GetCollisionObjectType(),Q,Response)
                    ||Floor.ImpactNormal.Z<E->GetCharacterMovement()->GetWalkableFloorZ())
                {++NextNeighbor;continue;}
                PendingPosition.Z=Floor.ImpactPoint.Z+Half+2;
            }
            bPendingGround=true;
            if(Queries>=QueriesPerSearchSlice)break;
        }
        const int32 Before=Budget->GetCollisionQueriesUsed();
        bool Blocked;if(!QueryCorridor(Current.Position,PendingPosition,Blocked))break;Queries+=Budget->GetCollisionQueriesUsed()-Before;
        if(!Blocked)
        {
            const int32 Index=Found?*Found:SearchNodes.Num();
            if(!Found)
            {
                if(SearchNodes.Num()>=MaxSearchNodes){FinishSearch(BestNode,false);break;}
                SearchNodes.Add({Cell,PendingPosition,Cost,ExpandingNode,false});SearchLookup.Add(Cell,Index);
            }
            else {SearchNodes[Index].Cost=Cost;SearchNodes[Index].Parent=ExpandingNode;SearchNodes[Index].Position=PendingPosition;}
            const float H=FVector::Dist2D(PendingPosition,SearchGoal);
            SearchOpen.HeapPush(FOpenNode{Index,Cost,Cost+H});
        }
        ++NextNeighbor;bPendingGround=false;
    }
}

void URogueEnemyObstacleMovementComponent::InvalidateRoute(double Now)
{
    PathPoints.Reset();PathIndex=0;ClearSearch();CachedDirection=FVector::ZeroVector;
    NextSearchAllowed=FMath::Max(NextSearchAllowed,Now+.15);NextDecision=0;
}

FVector URogueEnemyObstacleMovementComponent::FollowPath(const FVector& Origin)
{
    const auto* E=Cast<ARogueEnemy>(GetOwner());
    const float Tolerance=3.f;
    while(PathIndex<PathPoints.Num()&&FVector::Dist2D(Origin,PathPoints[PathIndex])<Tolerance)++PathIndex;
    return PathIndex<PathPoints.Num()?(PathPoints[PathIndex]-Origin).GetSafeNormal2D():FVector::ZeroVector;
}

float URogueEnemyObstacleMovementComponent::LimitMovementStep(float Requested) const
{
    if(PathIndex<PathPoints.Num())return FMath::Min(Requested,float(FVector::Dist2D(GetOwner()->GetActorLocation(),PathPoints[PathIndex])));
    return Requested;
}

FString URogueEnemyObstacleMovementComponent::DescribeRoute() const
{
    return FString::Printf(TEXT("detour=%d searching=%d failures=%d cell=%.0f nodes=%d route=%d/%d goal=%s next=%s"),bFollowingWall,bSearching,SearchFailures,CellSize,SearchNodes.Num(),PathIndex,PathPoints.Num(),*PathGoal.ToCompactString(),PathPoints.IsValidIndex(PathIndex)?*PathPoints[PathIndex].ToCompactString():TEXT("none"));
}

FVector URogueEnemyObstacleMovementComponent::ResolveDirection(const FVector& Desired,const FVector& Destination)
{
    auto* E=Cast<ARogueEnemy>(GetOwner());
    if(!bEnabled||!E||!E->HasAuthority()||E->IsDead()||E->IsAvailableInPool())return Desired;
    const double Now=GetWorld()->GetTimeSeconds();const FVector Origin=E->GetActorLocation();
    if(LastObservation>=0&&FVector::Dist2D(Origin,LastLocation)>FMath::Max(240.,(Now-LastObservation)*E->GetMoveSpeed()*4+100))ResetForPool();
    LastLocation=Origin;LastObservation=Now;
    if(Now<EscapeUntil)return PenetrationEscape;
    const bool Healer=RogueEnemyArchetypes::IsHealerType(E->GetEnemyType());
    if(!IsDetouring()&&!Healer)return Desired;
    if(!IsDetouring()&&Now<NextDecision)return Desired;
    auto* Budget=GetWorld()->GetSubsystem<URogueEnemyNavigationBudget>();
    CSV_SCOPED_TIMING_STAT(RoguePerf, EnemyObstacleSteer);
    bool Decision=Now>=NextDecision&&Budget->ConsumeDecision();
    if(Decision)
    {
        NextDecision=Now+FMath::Clamp(DecisionInterval,.08f,.5f);
        bool Blocked;
        if(!bFollowingWall&&PathPoints.IsEmpty())
        {
            float Probe=FMath::Clamp(ProbeDistance,60.f,400.f);
            if(Healer)if(const auto* S=E->FindComponentByClass<URogueEnemySupportComponent>())Probe=S->Movement.Sanitized().ProbeDistance;
            if(!Budget->ConsumeCollisionQuery())return FVector::ZeroVector;
            FHitResult Hit;if(!SweepWall(Origin,Origin+Desired*Probe,Hit))return Desired;NotifyBlocked(Hit,Desired);
        }
        if(QueryCorridor(Origin,Destination,Blocked)&&!Blocked)
        {ResetForPool();return Desired;}
        // 玩家连续走动时先完成绕楼路线；250cm 就推倒重算会让长搜索永远无法完成。
        // 大幅换位仍立即重算；通路恢复畅通时，上方的直达检查会立即接回正常追击。
        const float ReplanDistance=FMath::Clamp(float(FVector::Dist2D(Origin,PathGoal))*.5f,1500.f,3000.f);
        if((bSearching||!PathPoints.IsEmpty())&&FVector::DistSquared2D(PathGoal,Destination)>FMath::Square(ReplanDistance))
        {InvalidateRoute(Now);NextSearchAllowed=Now;}
        if(!PathPoints.IsEmpty())
        {
            FollowPath(Origin);
            if(PathIndex>=PathPoints.Num())InvalidateRoute(Now);
            else
            {
                // Skip only forward waypoints that the full capsule can actually reach.
                for(int32 I=FMath::Min(PathIndex+5,PathPoints.Num()-1);I>PathIndex;--I)
                    if(QueryCorridor(Origin,PathPoints[I],Blocked)&&!Blocked){PathIndex=I;break;}
                if(QueryCorridor(Origin,PathPoints[PathIndex],Blocked)&&Blocked)InvalidateRoute(Now);
                else if(Now-ProgressTime>=1.)
                {
                    if(FVector::Dist2D(Origin,ProgressLocation)<FMath::Max(12.f,E->GetMoveSpeed()*.12f)){++SearchFailures;InvalidateRoute(Now);}
                    ProgressTime=Now;ProgressLocation=Origin;
                }
            }
        }
        if(PathPoints.IsEmpty()&&!bSearching&&Now>=NextSearchAllowed)
        {
            if(Now<NextPathRequest||!TryNavigationPath(Destination))StartSearch(Destination);
        }
        if(bSearching&&!CachedDirection.IsNearlyZero())
        {
            if(!QueryWall(Origin,Origin+CachedDirection*FMath::Clamp(ProbeDistance,60.f,400.f),Blocked)||Blocked)CachedDirection=FVector::ZeroVector;
        }
    }
    if(bSearching)AdvanceSearch();
    if(!PathPoints.IsEmpty())return FollowPath(Origin);
    // A stable tangent is allowed briefly while planning. Never rotate it repeatedly after losing a wall.
    return bSearching&&Now-SearchStarted<.6?CachedDirection:FVector::ZeroVector;
}
