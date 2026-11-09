#include "Combat/Projectiles/RogueEnemyBulletSubsystem.h"
#include "Combat/Projectiles/RogueEnemyProjectile.h"
#include "Debug/RoguePerfStats.h"

bool URogueEnemyBulletSubsystem::FEntry::IsLive() const
{
    const auto* P=Projectile.Get();
    return IsValid(P) && !P->IsActorBeingDestroyed() && P->bBatchedMovement
        && !P->IsAvailableInPool() && P->BatchGeneration==Generation;
}

bool URogueEnemyBulletSubsystem::DoesSupportWorldType(EWorldType::Type Type) const
{
    return Type==EWorldType::Game || Type==EWorldType::PIE || Type==EWorldType::GamePreview;
}

bool URogueEnemyBulletSubsystem::IsTickable() const
{
    return !IsTemplate() && (!Active.IsEmpty() || !Pending.IsEmpty());
}

TStatId URogueEnemyBulletSubsystem::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(URogueEnemyBulletSubsystem,STATGROUP_Tickables);
}

void URogueEnemyBulletSubsystem::RegisterProjectile(ARogueEnemyProjectile* Projectile)
{
    FEntry Entry;Entry.Projectile=Projectile;Entry.Generation=Projectile->BatchGeneration;
    (bUpdating?Pending:Active).Add(Entry);
}

void URogueEnemyBulletSubsystem::Tick(float DeltaTime)
{
    CSV_SCOPED_TIMING_STAT(RoguePerf, EnemyBulletBatch);
    Super::Tick(DeltaTime);
    bUpdating=true;
    for(int32 I=0;I<Active.Num();)
    {
        const FEntry Entry=Active[I];
        if(Entry.IsLive())
        {
            auto* P=Entry.Projectile.Get();
            P->AdvanceBatchedMovement(DeltaTime*FMath::Max(0.f,P->CustomTimeDilation),ScratchHits);
        }
        // A hit can return/reuse/destroy the same Actor or spawn another bullet. Generation protects the old entry.
        if(!Entry.IsLive())Active.RemoveAtSwap(I,1,EAllowShrinking::No);
        else ++I;
    }
    bUpdating=false;
    for(const FEntry& Entry:Pending)if(Entry.IsLive())Active.Add(Entry);
    Pending.Reset();
    CSV_CUSTOM_STAT(RoguePerf, BatchedEnemyBullets, Active.Num(), ECsvCustomStatOp::Set);
}

void URogueEnemyBulletSubsystem::Deinitialize()
{
    Active.Reset();Pending.Reset();ScratchHits.Reset();
    Super::Deinitialize();
}
