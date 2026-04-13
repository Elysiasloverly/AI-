#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Core/RogueTypes.h"
#include "RogueGameBalanceAsset.generated.h"

USTRUCT(BlueprintType)
struct AI_API FRogueEnemyWaveEntryRow
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 MinWave = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	ERogueEnemyType EnemyType = ERogueEnemyType::Hunter;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 Weight = 1;
};

USTRUCT(BlueprintType)
struct AI_API FRogueEnemyBalanceRow
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	ERogueEnemyType EnemyType = ERogueEnemyType::Hunter;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float HealthMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float SpeedMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float DamageMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 ExperienceRewardBonus = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float BossHealthMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float BossSpeedMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float BossDamageMultiplier = 1.0f;
};

UCLASS(BlueprintType)
class AI_API URogueGameBalanceAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	const FRogueEnemyBalanceRow* FindEnemyBalanceRow(ERogueEnemyType Type) const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Waves")
	TArray<FRogueEnemyWaveEntryRow> EnemyWaveEntries;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Waves")
	TArray<FRogueEnemyBalanceRow> EnemyBalanceRows;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn")
	int32 BossAliveMinMaxEnemies = 20;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn")
	int32 BossAliveMaxEnemyReduction = 12;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn")
	int32 DoubleSpawnWave = 5;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn")
	int32 DoubleSpawnSlack = 10;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn")
	int32 TripleSpawnWave = 10;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn")
	int32 TripleSpawnSlack = 24;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn")
	float DifficultyScalePerWave = 0.21f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn")
	float MaxDifficultyScale = 4.8f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn")
	int32 EarlyWaveSlowdownEndWave = 4;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn")
	float EarlyWaveSpawnMultiplierAtWaveOne = 1.35f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn")
	float EarlyWaveSpawnMultiplierAtEnd = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawn")
	float MinimumSpawnDelay = 0.20f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy")
	float BaseEnemyHealth = 35.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy")
	float EnemyHealthPerWave = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy")
	float BaseEnemySpeed = 240.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy")
	float EnemySpeedPerWave = 12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy")
	float BaseEnemyDamage = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy")
	float EnemyDamagePerWave = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy")
	int32 BaseExperienceReward = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy")
	int32 ExperienceRewardWaveDivisor = 2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy")
	int32 MaxExperienceReward = 5;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy")
	int32 MidGameHealthStartWave = 8;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy")
	float MidGameHealthPerWave = 0.08f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy")
	float MidGameHealthCap = 0.72f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy")
	int32 LateGameHealthStartWave = 16;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy")
	float LateGameHealthPerWave = 0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Enemy")
	float LateGameHealthCap = 0.50f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss")
	float BaseBossHealth = 420.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss")
	float BossHealthPerWave = 140.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss")
	float BaseBossSpeed = 250.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss")
	float BossSpeedPerWave = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss")
	float BaseBossDamage = 18.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss")
	float BossDamagePerWave = 2.6f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss")
	int32 BossExperiencePerWave = 4;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss")
	int32 MinBossExperienceReward = 30;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss")
	int32 MaxBossExperienceReward = 120;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss")
	int32 LateEliteBossStartWave = 20;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss")
	float EarlyEliteBossHealthMultiplier = 1.58f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss")
	float LateEliteBossHealthMultiplier = 1.72f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss")
	float EarlyEliteBossDamageMultiplier = 1.16f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss")
	float LateEliteBossDamageMultiplier = 1.24f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss")
	float EliteBossSpeedMultiplier = 1.05f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Economy")
	int32 EnemyCurrencyBaseReward = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Economy")
	int32 EnemyCurrencyWaveDivisor = 6;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Economy")
	int32 MaxEnemyCurrencyReward = 3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Economy")
	int32 BossCurrencyBaseReward = 14;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Economy")
	int32 BossCurrencyPerWave = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Economy")
	int32 MaxBossCurrencyReward = 28;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop")
	int32 ShopOfferCount = 6;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop")
	int32 ShopOfferCost = 22;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop")
	int32 ShopRefreshCost = 14;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Shop")
	float ShopAutoRefreshInterval = 60.0f;
};
