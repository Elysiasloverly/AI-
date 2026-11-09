#include "Enemies/Spawning/RogueEnemyNavigationBudget.h"
#include "Engine/World.h"
void URogueEnemyNavigationBudget::Refresh()
{
    const double Now=GetWorld()->GetTimeSeconds();
    if(Now!=FrameTime){FrameTime=Now;Decisions=Paths=CollisionQueries=0;}
}
bool URogueEnemyNavigationBudget::ConsumeDecision()
{
    Refresh();if(Decisions>=MaxDecisionsPerFrame)return false;++Decisions;return true;
}
bool URogueEnemyNavigationBudget::ConsumePath()
{
    Refresh();if(Paths>=MaxPathsPerFrame)return false;++Paths;return true;
}

bool URogueEnemyNavigationBudget::ConsumeCollisionQuery()
{
    return ConsumeCollisionQueries(1);
}
bool URogueEnemyNavigationBudget::ConsumeCollisionQueries(int32 Count)
{
    Refresh();if(Count<0||Count>MaxCollisionQueriesPerFrame-CollisionQueries)return false;
    CollisionQueries+=Count;return true;
}
