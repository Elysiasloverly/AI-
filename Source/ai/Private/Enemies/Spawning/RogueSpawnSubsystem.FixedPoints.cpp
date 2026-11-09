#include "Enemies/Spawning/RogueSpawnSubsystem.h"
#include "World/RogueRangedSpawnPoint.h"
#include "Enemies/RogueEnemy.h"
#include "EngineUtils.h"
#include "CollisionQueryParams.h"

void URogueSpawnSubsystem::RegisterRangedSpawnPoint(ARogueRangedSpawnPoint* Point)
{
    if (IsValid(Point)) { RangedSpawnPoints.AddUnique(Point); }
}

void URogueSpawnSubsystem::UnregisterRangedSpawnPoint(ARogueRangedSpawnPoint* Point)
{
    RangedSpawnPoints.RemoveAll([Point](const auto& Entry) { return !Entry.IsValid() || Entry.Get() == Point; });
    NextRangedSpawnPoint = 0;
}

bool URogueSpawnSubsystem::ProjectSpawnSite(const ARogueEnemy* Enemy, const ARogueEnemySpawnRegion* Region,
    const FVector& Candidate, FVector& OutLocation) const
{
    FCollisionQueryParams Query(SCENE_QUERY_STAT(RogueFixedSpawnAuthoring), false, Enemy);
    for (TActorIterator<APawn> It(GetWorld()); It; ++It) { Query.AddIgnoredActor(*It); }
    return TryGroundSpawnLocation(Candidate, Enemy, Query, Region, OutLocation);
}
