#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Core/RogueTypes.h"
#include "RogueGameBalanceAsset.generated.h"

USTRUCT(BlueprintType)
struct AI_API FRogueEnemyWaveEntryRow
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (DisplayName = "最早出现波次"))
	int32 MinWave = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (DisplayName = "敌人类型"))
	ERogueEnemyType EnemyType = ERogueEnemyType::Hunter;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (DisplayName = "权重"))
	int32 Weight = 1;
};

USTRUCT(BlueprintType)
struct AI_API FRogueEnemyBalanceRow
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (DisplayName = "敌人类型"))
	ERogueEnemyType EnemyType = ERogueEnemyType::Hunter;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (DisplayName = "生命倍率"))
	float HealthMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (DisplayName = "速度倍率"))
	float SpeedMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (DisplayName = "伤害倍率"))
	float DamageMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (DisplayName = "额外经验奖励"))
	int32 ExperienceRewardBonus = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (DisplayName = "Boss生命倍率"))
	float BossHealthMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (DisplayName = "Boss速度倍率"))
	float BossSpeedMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (DisplayName = "Boss伤害倍率"))
	float BossDamageMultiplier = 1.0f;
};

UCLASS(BlueprintType)
class AI_API URogueGameBalanceAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	const FRogueEnemyBalanceRow* FindEnemyBalanceRow(ERogueEnemyType Type) const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "波次", meta = (DisplayName = "敌人波次权重"))
	TArray<FRogueEnemyWaveEntryRow> EnemyWaveEntries;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "波次", meta = (DisplayName = "敌人类型倍率"))
	TArray<FRogueEnemyBalanceRow> EnemyBalanceRows;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "刷怪", meta = (DisplayName = "Boss存活时最小敌人上限"))
	int32 BossAliveMinMaxEnemies = 20;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "刷怪", meta = (DisplayName = "Boss存活时敌人上限减少量"))
	int32 BossAliveMaxEnemyReduction = 12;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "刷怪", meta = (DisplayName = "双倍刷怪开始波次"))
	int32 DoubleSpawnWave = 5;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "刷怪", meta = (DisplayName = "双倍刷怪空余敌人数"))
	int32 DoubleSpawnSlack = 10;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "刷怪", meta = (DisplayName = "三倍刷怪开始波次"))
	int32 TripleSpawnWave = 10;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "刷怪", meta = (DisplayName = "三倍刷怪空余敌人数"))
	int32 TripleSpawnSlack = 24;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "刷怪", meta = (DisplayName = "每波难度缩放"))
	float DifficultyScalePerWave = 0.21f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "刷怪", meta = (DisplayName = "最大难度缩放"))
	float MaxDifficultyScale = 4.8f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "刷怪", meta = (DisplayName = "前期减速结束波次"))
	int32 EarlyWaveSlowdownEndWave = 4;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "刷怪", meta = (DisplayName = "第1波刷怪间隔倍率"))
	float EarlyWaveSpawnMultiplierAtWaveOne = 1.35f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "刷怪", meta = (DisplayName = "前期结束刷怪间隔倍率"))
	float EarlyWaveSpawnMultiplierAtEnd = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "刷怪", meta = (DisplayName = "最小刷怪间隔"))
	float MinimumSpawnDelay = 0.20f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "普通敌人", meta = (DisplayName = "基础生命值"))
	float BaseEnemyHealth = 35.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "普通敌人", meta = (DisplayName = "每波生命成长"))
	float EnemyHealthPerWave = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "普通敌人", meta = (DisplayName = "基础速度"))
	float BaseEnemySpeed = 240.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "普通敌人", meta = (DisplayName = "每波速度成长"))
	float EnemySpeedPerWave = 12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "普通敌人", meta = (DisplayName = "基础伤害"))
	float BaseEnemyDamage = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "普通敌人", meta = (DisplayName = "每波伤害成长"))
	float EnemyDamagePerWave = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "普通敌人", meta = (DisplayName = "基础经验奖励"))
	int32 BaseExperienceReward = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "普通敌人", meta = (DisplayName = "经验奖励波次除数"))
	int32 ExperienceRewardWaveDivisor = 2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "普通敌人", meta = (DisplayName = "最大经验奖励"))
	int32 MaxExperienceReward = 5;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "普通敌人", meta = (DisplayName = "中期生命强化开始波次"))
	int32 MidGameHealthStartWave = 8;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "普通敌人", meta = (DisplayName = "中期每波生命倍率成长"))
	float MidGameHealthPerWave = 0.08f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "普通敌人", meta = (DisplayName = "中期生命倍率上限"))
	float MidGameHealthCap = 0.72f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "普通敌人", meta = (DisplayName = "后期生命强化开始波次"))
	int32 LateGameHealthStartWave = 16;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "普通敌人", meta = (DisplayName = "后期每波生命倍率成长"))
	float LateGameHealthPerWave = 0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "普通敌人", meta = (DisplayName = "后期生命倍率上限"))
	float LateGameHealthCap = 0.50f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "首领", meta = (DisplayName = "基础生命值"))
	float BaseBossHealth = 420.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "首领", meta = (DisplayName = "每波生命成长"))
	float BossHealthPerWave = 140.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "首领", meta = (DisplayName = "基础速度"))
	float BaseBossSpeed = 250.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "首领", meta = (DisplayName = "每波速度成长"))
	float BossSpeedPerWave = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "首领", meta = (DisplayName = "基础伤害"))
	float BaseBossDamage = 18.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "首领", meta = (DisplayName = "每波伤害成长"))
	float BossDamagePerWave = 2.6f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "首领", meta = (DisplayName = "每波经验奖励"))
	int32 BossExperiencePerWave = 4;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "首领", meta = (DisplayName = "最小经验奖励"))
	int32 MinBossExperienceReward = 30;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "首领", meta = (DisplayName = "最大经验奖励"))
	int32 MaxBossExperienceReward = 120;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "首领", meta = (DisplayName = "后期精英Boss开始波次"))
	int32 LateEliteBossStartWave = 20;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "首领", meta = (DisplayName = "前期精英Boss生命倍率"))
	float EarlyEliteBossHealthMultiplier = 1.58f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "首领", meta = (DisplayName = "后期精英Boss生命倍率"))
	float LateEliteBossHealthMultiplier = 1.72f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "首领", meta = (DisplayName = "前期精英Boss伤害倍率"))
	float EarlyEliteBossDamageMultiplier = 1.16f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "首领", meta = (DisplayName = "后期精英Boss伤害倍率"))
	float LateEliteBossDamageMultiplier = 1.24f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "首领", meta = (DisplayName = "精英Boss速度倍率"))
	float EliteBossSpeedMultiplier = 1.05f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "经济", meta = (DisplayName = "普通敌人金币基础奖励"))
	int32 EnemyCurrencyBaseReward = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "经济", meta = (DisplayName = "普通敌人金币波次除数"))
	int32 EnemyCurrencyWaveDivisor = 6;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "经济", meta = (DisplayName = "普通敌人金币奖励上限"))
	int32 MaxEnemyCurrencyReward = 3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "经济", meta = (DisplayName = "Boss金币基础奖励"))
	int32 BossCurrencyBaseReward = 14;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "经济", meta = (DisplayName = "Boss每波金币成长"))
	int32 BossCurrencyPerWave = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "经济", meta = (DisplayName = "Boss金币奖励上限"))
	int32 MaxBossCurrencyReward = 28;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "商店", meta = (DisplayName = "商店卡牌数量"))
	int32 ShopOfferCount = 6;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "商店", meta = (DisplayName = "商店卡牌基础价格"))
	int32 ShopOfferCost = 22;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "商店", meta = (DisplayName = "商店刷新基础价格"))
	int32 ShopRefreshCost = 14;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "商店", meta = (DisplayName = "商店自动刷新间隔"))
	float ShopAutoRefreshInterval = 60.0f;
};
