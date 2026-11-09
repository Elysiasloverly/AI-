#include "Framework/RogueGameMode.h"
#include "Framework/RogueGameState.h"
#include "Enemies/Spawning/RogueSpawnSubsystem.h"
#include "Enemies/RogueEnemy.h"
#include "Player/RogueCharacter.h"
#include "Player/RoguePlayerState.h"
#include "Combat/Weapons/RogueWeaponBase.h"
#include "Combat/Projectiles/RogueProjectile.h"
#include "Combat/Projectiles/RogueReturningTalisman.h"
#include "Combat/Projectiles/RogueRocketProjectile.h"
#include "Combat/Projectiles/RogueMortarProjectile.h"
#include "Combat/Projectiles/RogueEnemyProjectile.h"
#include "Combat/Projectiles/RogueBossAttackActor.h"
#include "Combat/Projectiles/RogueEnemySkyfallProjectile.h"
#include "Combat/Visuals/RogueLaserBeam.h"
#include "Combat/Projectiles/RogueOrbitingBlade.h"
#include "World/RogueExperiencePickup.h"
#include "World/RogueShopTerminal.h"
#include "EngineUtils.h"

bool ARogueGameMode::CompleteRunIfVictoryReached()
{
    if (!HasAuthority() || RunState.bGameOver || RunState.CurrentWave < VictoryWave) return false;
    // 最终波清场后关闭权威闸门；回收残余弹体与特效不重复结算奖励。
    RunState.CurrentWave = VictoryWave;
    RunState.bVictory = RunState.bGameOver = true;
    RunState.bBossAlive = false;
    RunState.SpawnTimer = 0;
    if (auto* Spawns=GetWorld()->GetSubsystem<URogueSpawnSubsystem>()) Spawns->SetSpawningSuspended(true);
    for (TActorIterator<ARogueCharacter> It(GetWorld()); It; ++It) It->FinalizeForVictory();
    for (TActorIterator<ARoguePlayerState> It(GetWorld()); It; ++It)
    {
        It->ResetUpgradeState(); It->GetShopRuntime().CloseShop();
        It->ApplyShopSnapshot(false, TArray<FRogueShopOffer>(), 0, 0.f); It->ForceNetUpdate();
    }
    RetireCombatActors();
    PushRunStateToGameState();
    if (auto* State=GetGameState<ARogueGameState>()) State->ForceNetUpdate();
    UE_LOG(LogTemp,Log,TEXT("[RogueVictory] Wave=%d Time=%.2f Defeated=%d"),RunState.CurrentWave,RunState.RunTimeSeconds,RunState.EnemiesDefeated);
    return true;
}

// 胜利和失败共用收尾；直接清退不会走敌人击杀奖励路径。
void ARogueGameMode::RetireCombatActors()
{
    RunState.SpawnTimer = 0;
    if (auto* Spawns = GetWorld()->GetSubsystem<URogueSpawnSubsystem>()) Spawns->SetSpawningSuspended(true);
    if (ARogueShopTerminal* Terminal = LevelShopTerminal.Get()) { Terminal->Destroy(); }
    TArray<AActor*> Retire;
    for (TActorIterator<AActor> It(GetWorld()); It; ++It)
    {
        AActor* A=*It;
        if (A->IsA<ARogueWeaponBase>() || A->IsA<ARogueEnemy>() || A->IsA<ARogueProjectile>()
            || A->IsA<ARogueRocketProjectile>() || A->IsA<ARogueMortarProjectile>()
            || A->IsA<ARogueBossAttackActor>() || A->IsA<ARogueEnemySkyfallProjectile>() || A->IsA<ARogueEnemyProjectile>() || A->IsA<ARogueLaserBeam>()
            || A->IsA<ARogueReturningTalisman>() || A->IsA<ARogueOrbitingBlade>() || A->IsA<ARogueExperiencePickup>()) Retire.Add(A);
    }
    for (AActor* A:Retire) if (IsValid(A)) A->Destroy();
}
