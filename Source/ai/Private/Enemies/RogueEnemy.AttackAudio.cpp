#include "Enemies/RogueEnemy.h"
#include "Enemies/Components/RogueEnemyPresentationComponent.h"
#include "Audio/RogueCombatAudioSubsystem.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"

void ARogueEnemy::NotifyAttackFeedback(const FVector& Location)
{
    if (!HasAuthority() || bDead || bPoolAvailable) return;
    // One event, without an extra per-shot pose RPC. Each receiving system owns its configuration.
    Multicast_AttackFeedback(Location, HitFeedback.Generation);
}

void ARogueEnemy::Multicast_AttackFeedback_Implementation(FVector_NetQuantize Location, uint32 Generation)
{
#if !UE_BUILD_SHIPPING
    if (const auto* Debug = IConsoleManager::Get().FindConsoleVariable(TEXT("Rogue.Audio.Debug")); Debug && Debug->GetInt())
        UE_LOG(LogTemp, Display, TEXT("[AttackAudio] EnemyRPC Mode=%d Enemy=%s Gen=%u/%u Dead=%d Pool=%d"),
            int32(GetNetMode()), *GetName(), Generation, HitFeedback.Generation, bDead, bPoolAvailable);
#endif
    // 过期的上一轮对象池事件不应在新怪身上响起；晚加入也不回放历史音效。
    if (bDead || bPoolAvailable || Generation != HitFeedback.Generation) return;
    if (EnemyPresentation) EnemyPresentation->PlayAttackCue();
    if (AttackAudioGeneration != Generation)
    {
        AttackAudioState.Reset();
        AttackAudioGeneration = Generation;
    }
    if (auto* Audio = GetWorld()->GetSubsystem<URogueCombatAudioSubsystem>())
        Audio->PlayEnemyAttack(this, Location, AttackAudioState);
}
