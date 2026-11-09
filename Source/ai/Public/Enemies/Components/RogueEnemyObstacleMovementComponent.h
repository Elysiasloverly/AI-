#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RogueEnemyObstacleMovementComponent.generated.h"

/** Server-side, budgeted collision routing. Driven by movement calls, never by a component Tick. */
UCLASS(ClassGroup=(Enemy), meta=(BlueprintSpawnableComponent, DisplayName="敌人绕障移动"))
class AI_API URogueEnemyObstacleMovementComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    URogueEnemyObstacleMovementComponent();
    FVector ResolveDirection(const FVector& Desired, const FVector& Destination);
    void NotifyBlocked(const FHitResult& Hit, const FVector& Desired);
    void ResetForPool();
    bool IsDetouring() const { return bFollowingWall || !PathPoints.IsEmpty(); }
    float LimitMovementStep(float Requested) const;
    FString DescribeRoute() const;
    bool CanAdvanceOnGround(const FVector& Delta,FHitResult& OutEdge);

    UPROPERTY(EditDefaultsOnly, Category="绕障", meta=(DisplayName="启用绕障"))
    bool bEnabled=true;
    UPROPERTY(EditDefaultsOnly, Category="绕障", meta=(DisplayName="障碍探测距离",ClampMin="60",ClampMax="400",Units="cm"))
    float ProbeDistance=160;
    UPROPERTY(EditDefaultsOnly, Category="绕障", meta=(DisplayName="绕障决策间隔",ClampMin="0.08",ClampMax="0.5",Units="s"))
    float DecisionInterval=.12f;

private:
    struct FSearchNode
    {
        FIntPoint Cell;
        FVector Position;
        float Cost=0;
        int32 Parent=INDEX_NONE;
        bool bClosed=false;
    };
    struct FOpenNode
    {
        int32 Index;
        float Cost,Priority;
        // UE TArray heaps are min-heaps with the default predicate.
        bool operator<(const FOpenNode& Other) const { return Priority<Other.Priority; }
    };
    bool SweepWall(const FVector& Start,const FVector& End,FHitResult& Hit) const;
    bool QueryWall(const FVector& Start,const FVector& End,bool& bBlocked,FHitResult* OutHit=nullptr) const;
    bool QueryCorridor(const FVector& Start,const FVector& End,bool& bBlocked) const;
    bool FindSupport(const FVector& Center,FHitResult& Floor) const;
    bool TryNavigationPath(const FVector& Destination);
    void StartSearch(const FVector& Destination);
    void AdvanceSearch();
    void FinishSearch(int32 EndNode,bool bReachedGoal);
    void ClearSearch();
    void InvalidateRoute(double Now);
    FVector FollowPath(const FVector& Origin);

    bool bFollowingWall=false;
    FVector CachedDirection=FVector::ZeroVector,PathGoal=FVector::ZeroVector;
    double NextDecision=0,NextPathRequest=0,NextSearchAllowed=0;
    TArray<FVector> PathPoints;
    int32 PathIndex=0;

    // A* continues across frames; each collision query also consumes the shared world budget.
    TArray<FSearchNode> SearchNodes;
    TMap<FIntPoint,int32> SearchLookup;
    TArray<FOpenNode> SearchOpen;
    FVector SearchOrigin=FVector::ZeroVector,SearchGoal=FVector::ZeroVector,PendingPosition=FVector::ZeroVector;
    FBox2D SearchBounds;
    float CellSize=100;
    int32 ExpandingNode=INDEX_NONE,NextNeighbor=-1,BestNode=0;
    bool bSearching=false,bPendingGround=false;
    double SearchStarted=0;

    FVector LastLocation=FVector::ZeroVector,ProgressLocation=FVector::ZeroVector;
    double LastObservation=-1,ProgressTime=0;
    FVector PenetrationEscape=FVector::ZeroVector;
    double EscapeUntil=0;
    int32 SearchFailures=0;
};
