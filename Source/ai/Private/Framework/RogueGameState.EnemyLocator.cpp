#include "Framework/RogueGameState.h"
#include "Combat/Targeting/RogueEnemyTrackerSubsystem.h"
#include "Enemies/RogueEnemy.h"

void ARogueGameState::RefreshEnemyLocatorView()
{
    if (!HasAuthority()) return;
    FRogueEnemyLocatorView Next;
    const auto* Tracker = GetWorld() ? GetWorld()->GetSubsystem<URogueEnemyTrackerSubsystem>() : nullptr;
    const int32 TrackedCount=Tracker ? Tracker->GetActiveEnemyCount() : 0;
    const float Now=GetWorld()->GetTimeSeconds();
    if (!IsGameOver() && LastEnemyTrackerCount==TrackedCount && Now-LastEnemyLocatorTime<.1f) return;
    LastEnemyLocatorTime=Now; LastEnemyTrackerCount=TrackedCount;
    if (!IsGameOver() && Tracker)
    {
        for (const auto& Ref : Tracker->GetActiveEnemies())
        {
            const auto* Enemy = Ref.Get();
            if (!IsValid(Enemy) || Enemy->IsDead() || Enemy->IsAvailableInPool() || Enemy->GetCurrentHealth() <= 0) continue;
            ++Next.Remaining;
            if (Next.Remaining <= 5) Next.Locations.Add(Enemy->GetActorLocation());
        }
        if (Next.Remaining > 5) Next.Locations.Reset();
    }
    const bool bCountChanged = Next.Remaining != EnemyLocatorView.Remaining;
    EnemyLocatorView = MoveTemp(Next);
    if (bCountChanged) ForceNetUpdate();
}
