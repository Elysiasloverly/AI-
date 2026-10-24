#include "Enemies/RogueEnemy.h"
#include "Audio/RogueCombatAudioSubsystem.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"

void ARogueEnemy::NotifyAttackAudio(const FVector& Location)
{
    if (!HasAuthority() || bDead || bPoolAvailable) return;
    // 只通知攻击事件；音效配置与选择全部由接收端音频系统负责。
    Multicast_AttackAudio(Location, HitFeedback.Generation);
}

void ARogueEnemy::Multicast_AttackAudio_Implementation(FVector_NetQuantize Location, uint32 Generation)
{
#if !UE_BUILD_SHIPPING
    if (const auto* Debug = IConsoleManager::Get().FindConsoleVariable(TEXT("Rogue.Audio.Debug")); Debug && Debug->GetInt())
        UE_LOG(LogTemp, Display, TEXT("[AttackAudio] EnemyRPC Mode=%d Enemy=%s Gen=%u/%u Dead=%d Pool=%d"),
            int32(GetNetMode()), *GetName(), Generation, HitFeedback.Generation, bDead, bPoolAvailable);
#endif
    // 过期的上一轮对象池事件不应在新怪身上响起；晚加入也不回放历史音效。
    if (bDead || bPoolAvailable || Generation != HitFeedback.Generation) return;
    if (AttackAudioGeneration != Generation)
    {
        AttackAudioState.Reset();
        AttackAudioGeneration = Generation;
    }
    if (auto* Audio = GetWorld()->GetSubsystem<URogueCombatAudioSubsystem>())
        Audio->PlayEnemyAttack(this, Location, AttackAudioState);
}
