// 敌人生成子系统 —— 实现
#include "Subsystems/RogueSpawnSubsystem.h"

#include "Subsystems/RogueCombatPoolSubsystem.h"
#include "Subsystems/RogueEnemyTrackerSubsystem.h"
#include "Enemies/RogueEnemy.h"
#include "Player/RogueCharacter.h"
#include "World/RogueArena.h"
#include "Core/RogueGameBalanceAsset.h"
#include "Core/RogueGameModeRules.h"
#include "Kismet/GameplayStatics.h"

void URogueSpawnSubsystem::Configure(
	TSubclassOf<ARogueEnemy> InEnemyClass,
	const FRogueSpawnSettings& InSpawnSettings,
	const FRogueBossSettings& InBossSettings,
	URogueGameBalanceAsset* InBalanceAsset)
{
	EnemyClass = InEnemyClass;
	SpawnSettings = InSpawnSettings;
	BossSettings = InBossSettings;
	BalanceAsset = InBalanceAsset;
}

void URogueSpawnSubsystem::TickSpawning(float DeltaSeconds, FRogueRunState& RunState, ARogueCharacter* Character, const ARogueArena* Arena)
{
	RunState.SpawnTimer -= DeltaSeconds;

	const int32 TargetWave = FMath::FloorToInt(RunState.RunTimeSeconds / SpawnSettings.WaveDuration) + 1;
	if (TargetWave != RunState.CurrentWave)
	{
		RunState.CurrentWave = TargetWave;
	}

	if (BossSettings.BossWaveInterval > 0 && RunState.CurrentWave > 0 && RunState.CurrentWave % BossSettings.BossWaveInterval == 0 && !RunState.bBossAlive && RunState.LastBossWaveSpawned != RunState.CurrentWave)
	{
		SpawnBoss(RunState, Character, Arena);
	}

	URogueEnemyTrackerSubsystem* Tracker = GetWorld()->GetSubsystem<URogueEnemyTrackerSubsystem>();
	if (Tracker == nullptr)
	{
		return;
	}

	int32 AliveEnemies = Tracker->GetActiveEnemyCount();
	const FRogueSpawnCadence SpawnCadence = RogueGameModeRules::BuildSpawnCadence(RunState.CurrentWave, AliveEnemies, SpawnSettings.MaxAliveEnemies, RunState.bBossAlive, SpawnSettings.SpawnInterval, BalanceAsset);
	if (RunState.SpawnTimer <= 0.0f && AliveEnemies < SpawnCadence.EffectiveMaxAliveEnemies)
	{
		for (int32 SpawnIndex = 0; SpawnIndex < SpawnCadence.SpawnBatchSize && AliveEnemies < SpawnCadence.EffectiveMaxAliveEnemies; ++SpawnIndex)
		{
			SpawnEnemy(RunState, Character, Arena);
			++AliveEnemies;
		}

		RunState.SpawnTimer = SpawnCadence.NextSpawnDelay;
	}
}

void URogueSpawnSubsystem::SpawnEnemy(FRogueRunState& RunState, ARogueCharacter* Character, const ARogueArena* Arena)
{
	if (!IsValid(Character) || Character->IsDead())
	{
		return;
	}

	SpawnConfiguredEnemy(RogueGameModeRules::BuildEnemyProfile(RunState.CurrentWave, PickEnemyTypeForCurrentWave(RunState.CurrentWave), BalanceAsset), SpawnSettings.SpawnRadius, SpawnSettings.EnemySpawnMinimumDistance, Arena, Character);
}

void URogueSpawnSubsystem::SpawnBoss(FRogueRunState& RunState, ARogueCharacter* Character, const ARogueArena* Arena)
{
	if (!IsValid(Character) || Character->IsDead())
	{
		return;
	}

	if (SpawnConfiguredEnemy(RogueGameModeRules::BuildBossProfile(RunState.CurrentWave, PickEnemyTypeForCurrentWave(RunState.CurrentWave), BalanceAsset), BossSettings.BossSpawnDistance, BossSettings.BossSpawnMinimumDistance, Arena, Character) != nullptr)
	{
		RunState.bBossAlive = true;
		RunState.LastBossWaveSpawned = RunState.CurrentWave;
	}
}

ARogueEnemy* URogueSpawnSubsystem::SpawnConfiguredEnemy(const FRogueEnemyProfile& EnemyProfile, float DesiredDistance, float MinimumDistance, const ARogueArena* Arena, ARogueCharacter* Character)
{
	URogueCombatPoolSubsystem* Pools = GetWorld()->GetSubsystem<URogueCombatPoolSubsystem>();
	URogueEnemyTrackerSubsystem* Tracker = GetWorld()->GetSubsystem<URogueEnemyTrackerSubsystem>();
	if (Pools == nullptr || Tracker == nullptr)
	{
		return nullptr;
	}

	const FVector SpawnLocation = FindSpawnLocation(DesiredDistance, MinimumDistance, Arena, Character);
	UClass* EnemyToSpawn = EnemyClass ? EnemyClass.Get() : ARogueEnemy::StaticClass();
	ARogueEnemy* Enemy = Pools->AcquireEnemy(EnemyToSpawn, nullptr, SpawnLocation, FRotator::ZeroRotator);
	if (Enemy == nullptr)
	{
		return nullptr;
	}

	Enemy->ActivatePooledEnemy(nullptr, SpawnLocation, FRotator::ZeroRotator);
	Enemy->InitializeEnemy(EnemyProfile);
	Tracker->RegisterEnemy(Enemy);
	return Enemy;
}

FVector URogueSpawnSubsystem::FindSpawnLocation(float DesiredDistance, float MinimumDistance, const ARogueArena* Arena, ARogueCharacter* Character) const
{
	if (!IsValid(Character))
	{
		return FVector(0.0f, 0.0f, 60.0f);
	}

	const FVector PlayerLocation = Character->GetActorLocation();
	const float ArenaHalfExtent = IsValid(Arena) ? Arena->GetArenaHalfExtent() - SpawnSettings.ArenaSpawnPadding : FMath::Max(SpawnSettings.SpawnRadius, DesiredDistance) + 500.0f;

	for (int32 Attempt = 0; Attempt < 16; ++Attempt)
	{
		const FVector2D SpawnDirection = FVector2D(FMath::FRandRange(-1.0f, 1.0f), FMath::FRandRange(-1.0f, 1.0f)).GetSafeNormal();
		const float DistanceScale = FMath::FRandRange(0.82f, 1.15f);
		const FVector CandidateLocation = PlayerLocation + FVector(SpawnDirection.X * DesiredDistance * DistanceScale, SpawnDirection.Y * DesiredDistance * DistanceScale, 60.0f);
		if (FMath::Abs(CandidateLocation.X) <= ArenaHalfExtent && FMath::Abs(CandidateLocation.Y) <= ArenaHalfExtent && FVector::DistSquared2D(CandidateLocation, PlayerLocation) >= FMath::Square(MinimumDistance))
		{
			return CandidateLocation;
		}
	}

	for (int32 Attempt = 0; Attempt < 20; ++Attempt)
	{
		const FVector CandidateLocation(
			FMath::FRandRange(-ArenaHalfExtent, ArenaHalfExtent),
			FMath::FRandRange(-ArenaHalfExtent, ArenaHalfExtent),
			60.0f);

		if (FVector::DistSquared2D(CandidateLocation, PlayerLocation) >= FMath::Square(MinimumDistance))
		{
			return CandidateLocation;
		}
	}

	return FVector(
		FMath::Clamp(PlayerLocation.X + DesiredDistance * 0.5f, -ArenaHalfExtent, ArenaHalfExtent),
		FMath::Clamp(PlayerLocation.Y + DesiredDistance * 0.5f, -ArenaHalfExtent, ArenaHalfExtent),
		60.0f);
}

ERogueEnemyType URogueSpawnSubsystem::PickEnemyTypeForCurrentWave(int32 Wave) const
{
	return RogueGameModeRules::PickWeightedEnemyTypeForWave(Wave, BalanceAsset);
}