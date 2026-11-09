#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RogueEnemyNavigationBudget.generated.h"

/** Shared per-world decision budget; clear movement does not request a decision. */
UCLASS()
class AI_API URogueEnemyNavigationBudget : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    static constexpr int32 MaxDecisionsPerFrame=16;
    static constexpr int32 MaxPathsPerFrame=2;
    static constexpr int32 MaxCollisionQueriesPerFrame=192;
    bool ConsumeDecision();
    bool ConsumePath();
    bool ConsumeCollisionQuery();
    bool ConsumeCollisionQueries(int32 Count);
    int32 GetDecisionsUsed() const { return Decisions; }
    int32 GetPathsUsed() const { return Paths; }
    int32 GetCollisionQueriesUsed() const { return CollisionQueries; }
private:
    void Refresh();
    double FrameTime=-1;
    int32 Decisions=0,Paths=0,CollisionQueries=0;
};
