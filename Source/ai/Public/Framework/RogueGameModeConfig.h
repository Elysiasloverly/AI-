#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
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

	/** 敌人池预热量；默认 160 对应默认无 Boss 时第 16 波的存活上限（40 + 8×15）。
	 *  运行资产、Boss 调整与硬顶可改变实际需求。池耗尽时可扩容，具体启动成本和内存应实测。
	 *  用 FRogueCombatPools::LogPoolDiagnostics 查看池容量与运行时分配计数。 */
	UPROPERTY(EditAnywhere, Category = "Pools")
	int32 InitialEnemyPoolSize = 160;

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

	/** 第 1 波的基础存活上限；按波次增长后还会应用硬顶与 Boss 在场时的减量/下限。
	 *  参数在 GameBalanceAsset 配置，完整顺序见 BuildSpawnCadence。 */
	UPROPERTY(EditAnywhere, Category = "Spawning")
	int32 MaxAliveEnemies = 40;

	UPROPERTY(EditAnywhere, Category = "Spawning")
	float EnemySpawnMinimumDistance = 1100.0f;

	/** 旧资产兼容字段，不再参与计算；边缘留白在关卡刷怪框上配置。 */
	UPROPERTY()
	float ArenaSpawnPadding = 220.0f;

	UPROPERTY(EditAnywhere, Category = "Spawning|Ground", meta = (DisplayName = "地面射线向上起点距离", ToolTip = "从参照玩家高度向上偏移，再裁剪到关卡刷怪框内，向下寻找地面。", ClampMin = "0", Units = "cm"))
	float GroundTraceHeight = 2000.0f;

	UPROPERTY(EditAnywhere, Category = "Spawning|Ground", meta = (DisplayName = "地面射线向下检测距离", ToolTip = "射线终点相对参照玩家的向下距离；没有命中可站立地面时更换候选点。", ClampMin = "1", Units = "cm"))
	float GroundTraceDepth = 10000.0f;

	UPROPERTY(EditAnywhere, Category = "Spawning|Ground", meta = (DisplayName = "出生胶囊离地间隙", ToolTip = "在地面和实际胶囊尺寸计算出的高度上增加此间隙，避免初始穿插。", ClampMin = "0", Units = "cm"))
	float GroundClearance = 2.0f;

	UPROPERTY(EditAnywhere, Category = "Spawning|Ground", meta = (DisplayName = "地面射线检测通道", ToolTip = "地面需阻挡此通道，并能阻挡敌人胶囊；默认 Visibility。玩家和怪物不会被当成地面。"))
	TEnumAsByte<ECollisionChannel> GroundTraceChannel = ECC_Visibility;

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
