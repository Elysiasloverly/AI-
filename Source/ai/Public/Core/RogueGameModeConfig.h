#pragma once

#include "CoreMinimal.h"
#include "RogueGameModeConfig.generated.h"

USTRUCT(BlueprintType)
struct AI_API FRogueSceneOptimizationSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Scene")
	bool bOptimizeSceneForPerformance = true;

	UPROPERTY(EditAnywhere, Category = "Scene")
	float PrimaryDirectionalLightIntensity = 7.4f;

	UPROPERTY(EditAnywhere, Category = "Scene")
	float PrimarySkyLightIntensity = 1.05f;

	UPROPERTY(EditAnywhere, Category = "Scene")
	FRotator OptimizedSunRotation = FRotator(-48.0f, -32.0f, 0.0f);
};

USTRUCT(BlueprintType)
struct AI_API FRogueRuntimePerformanceSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Performance")
	int32 CombatEffectsCullEnemyCount = 52;

	UPROPERTY(EditAnywhere, Category = "Performance")
	float EnemySpatialCellSize = 650.0f;

	UPROPERTY(EditAnywhere, Category = "Performance")
	float EnemySpatialHashRefreshInterval = 0.15f;

	UPROPERTY(EditAnywhere, Category = "Performance")
	float EnemyRegistryCleanupInterval = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Performance")
	float PickupRegistryCleanupInterval = 1.5f;
};

USTRUCT(BlueprintType)
struct AI_API FRoguePoolSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Pools")
	int32 InitialPlayerProjectilePoolSize = 96;

	UPROPERTY(EditAnywhere, Category = "Pools")
	int32 InitialEnemyProjectilePoolSize = 96;

	UPROPERTY(EditAnywhere, Category = "Pools")
	int32 InitialEnemyPoolSize = 112;

	UPROPERTY(EditAnywhere, Category = "Pools")
	int32 InitialRocketProjectilePoolSize = 40;

	UPROPERTY(EditAnywhere, Category = "Pools")
	int32 InitialExperiencePickupPoolSize = 64;

	UPROPERTY(EditAnywhere, Category = "Pools")
	int32 InitialImpactEffectPoolSize = 48;

	UPROPERTY(EditAnywhere, Category = "Pools")
	int32 InitialLaserBeamPoolSize = 72;

	UPROPERTY(EditAnywhere, Category = "Pools")
	int32 InitialOrbitingBladePoolSize = 24;
};

USTRUCT(BlueprintType)
struct AI_API FRogueSpawnSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Spawning")
	float SpawnRadius = 2800.0f;

	UPROPERTY(EditAnywhere, Category = "Spawning")
	float SpawnInterval = 0.88f;

	UPROPERTY(EditAnywhere, Category = "Spawning")
	float WaveDuration = 25.0f;

	UPROPERTY(EditAnywhere, Category = "Spawning")
	int32 MaxAliveEnemies = 92;

	UPROPERTY(EditAnywhere, Category = "Spawning")
	float EnemySpawnMinimumDistance = 1100.0f;

	UPROPERTY(EditAnywhere, Category = "Spawning")
	float ArenaSpawnPadding = 220.0f;
};

USTRUCT(BlueprintType)
struct AI_API FRogueBossSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Boss")
	int32 BossWaveInterval = 10;

	UPROPERTY(EditAnywhere, Category = "Boss")
	int32 BossBonusSelections = 3;

	UPROPERTY(EditAnywhere, Category = "Boss")
	float BossSpawnDistance = 3600.0f;

	UPROPERTY(EditAnywhere, Category = "Boss")
	float BossSpawnMinimumDistance = 1800.0f;
};

USTRUCT(BlueprintType)
struct AI_API FRogueRunState
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category = "Run")
	float RunTimeSeconds = 0.0f;

	UPROPERTY(VisibleAnywhere, Category = "Run")
	int32 CurrentWave = 1;

	UPROPERTY(VisibleAnywhere, Category = "Run")
	int32 EnemiesDefeated = 0;

	UPROPERTY(VisibleAnywhere, Category = "Run")
	bool bGameOver = false;

	UPROPERTY(VisibleAnywhere, Category = "Run")
	bool bBossAlive = false;

	UPROPERTY(VisibleAnywhere, Category = "Run")
	float SpawnTimer = 0.0f;

	UPROPERTY(VisibleAnywhere, Category = "Run")
	int32 LastBossWaveSpawned = 0;
};
