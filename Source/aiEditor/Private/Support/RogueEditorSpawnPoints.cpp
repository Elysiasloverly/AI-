#include "RogueEditorAssetUtils.h"
#include "Components/CapsuleComponent.h"
#include "Enemies/RogueEnemy.h"
#include "Enemies/Spawning/RogueSpawnSubsystem.h"
#include "World/RogueEnemySpawnRegion.h"
#include "Engine/World.h"

bool URogueEditorAssetUtils::ValidateRangedSpawnSite(ARogueEnemy* Probe, ARogueEnemySpawnRegion* Region,
    FVector Candidate, FVector& OutLocation)
{
    if (!IsValid(Probe) || !IsValid(Region) || Probe->GetWorld() != Region->GetWorld()) { return false; }
    auto* Spawner = Probe->GetWorld()->GetSubsystem<URogueSpawnSubsystem>();
    return Spawner && Spawner->ProjectSpawnSite(Probe, Region, Candidate, OutLocation);
}

void URogueEditorAssetUtils::ConfigureEnemySpawnProbe(ARogueEnemy* Probe, ERogueEnemyType Type, bool bBoss)
{
    if (!Probe || !Probe->GetWorld() || Probe->GetWorld()->IsGameWorld()) { return; }
    const auto Archetype = RogueEnemyArchetypes::BuildEnemyArchetypeFromDataTable(Probe->GetEnemyArchetypeTable(), Type, bBoss);
    Probe->GetCapsuleComponent()->SetCapsuleSize(Archetype.Visual.CapsuleRadius, Archetype.Visual.CapsuleHalfHeight);
}


#include "Player/RogueCharacter.h"
#include "Framework/RogueGameModeRules.h"
#include "World/RogueRangedSpawnPoint.h"
#include "EngineUtils.h"

FString URogueEditorAssetUtils::VerifyPreviewRangedSpawns(ARogueCharacter* Player, ARogueEnemySpawnRegion* Region)
{
    if (!Player || !Region || Player->GetWorld()!=Region->GetWorld() ||
        Player->GetWorld()->WorldType!=EWorldType::PIE || !Player->HasAuthority()) return TEXT("Invalid preview world");
    auto* Spawner=Player->GetWorld()->GetSubsystem<URogueSpawnSubsystem>();
    if (!Spawner) return TEXT("No spawner");
    TArray<FVector> Sites;
    for (TActorIterator<ARogueRangedSpawnPoint> It(Player->GetWorld());It;++It)
        if (It->bEnabled && It->SpawnRegion==Region) Sites.Add(It->GetActorLocation());
    if (Sites.Num()!=12) return FString::Printf(TEXT("Expected twelve sites, found %d"),Sites.Num());
    for (int32 Group=0;Group<3;++Group)
    {
        TSet<int32> Visited;
        for (int32 I=0;I<Sites.Num();++I)
        {
            const auto Type=Group==1?ERogueEnemyType::SkyMoth:ERogueEnemyType::Marksman;
            auto Profile=Group==2?RogueGameModeRules::BuildBossProfile(40,Type,Spawner->BalanceAsset)
                :RogueGameModeRules::BuildEnemyProfile(40,Type,Spawner->BalanceAsset);
            auto* E=Spawner->SpawnConfiguredEnemy(Profile,Spawner->SpawnSettings.SpawnRadius,
                Spawner->SpawnSettings.EnemySpawnMinimumDistance,Region,Player);
            if (!E) return FString::Printf(TEXT("Spawn failed: group %d, index %d"),Group,I);
            const FVector At=E->GetActorLocation();
            int32 Site=INDEX_NONE;
            for (int32 J=0;J<Sites.Num();++J)
                if (FVector::DistSquared2D(Sites[J],At)<1.f) {Site=J;break;}
            const bool Ranged=E->HasRangedAttack();
            E->DeactivateToPool();
            const double Distance=FVector::Dist2D(Player->GetActorLocation(),At);
            const float MaxDistance=FMath::Max(Spawner->SpawnSettings.SpawnRadius*1.25f,Spawner->SpawnSettings.EnemySpawnMinimumDistance+300.f);
            if (!Ranged || Distance>MaxDistance+1.f) return TEXT("Ranged spawn escaped the active combat ring");
            Visited.Add(Site);
        }

    }
    return TEXT("PASS: 36 actual pooled spawns stay within the nearby combat ring");
}
