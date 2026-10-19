#include "Framework/RogueGameBalanceAsset.h"

#include "Serialization/CustomVersion.h"

namespace
{
	// 旧包没有这个版本；保存新布局后，清空规则列表也不会再次被旧字段填回。
	const FGuid GameBalanceLayoutVersion(0x7813CF72, 0x2EBB4E2A, 0xA614905F, 0xC30A8DD1);
	constexpr int32 GroupedLayout = 1;
	FCustomVersionRegistration RegisterGameBalanceLayout(GameBalanceLayoutVersion, GroupedLayout, TEXT("RogueGameBalanceLayout"));
}

void URogueGameBalanceAsset::Serialize(FArchive& Ar)
{
	Ar.UsingCustomVersion(GameBalanceLayoutVersion);
	Super::Serialize(Ar);
}

void URogueGameBalanceAsset::PostLoad()
{
	Super::PostLoad();
	if (GetLinkerCustomVersion(GameBalanceLayoutVersion) >= GroupedLayout)
	{
		return;
	}

	Spawn.BossAliveMinimumLimit = BossAliveMinMaxEnemies_DEPRECATED;
	Spawn.BossAliveReduction = BossAliveMaxEnemyReduction_DEPRECATED;
	Spawn.MaxAlivePerWave = MaxAliveEnemiesPerWave_DEPRECATED;
	Spawn.MaxAliveLimit = MaxAliveEnemiesHardLimit_DEPRECATED;
	Spawn.AccelerationPerWave = DifficultyScalePerWave_DEPRECATED;
	Spawn.MaxAcceleration = MaxDifficultyScale_DEPRECATED;
	Spawn.EarlyWaves.EndWave = EarlyWaveSlowdownEndWave_DEPRECATED;
	Spawn.EarlyWaves.FirstWaveIntervalMultiplier = EarlyWaveSpawnMultiplierAtWaveOne_DEPRECATED;
	Spawn.EarlyWaves.LastWaveIntervalMultiplier = EarlyWaveSpawnMultiplierAtEnd_DEPRECATED;
	Spawn.MinimumInterval = MinimumSpawnDelay_DEPRECATED;
	EnemyStats.Health.BaseValue = BaseEnemyHealth_DEPRECATED;
	EnemyStats.Health.PerWave = EnemyHealthPerWave_DEPRECATED;
	EnemyStats.Speed.BaseValue = BaseEnemySpeed_DEPRECATED;
	EnemyStats.Speed.PerWave = EnemySpeedPerWave_DEPRECATED;
	EnemyStats.Damage.BaseValue = BaseEnemyDamage_DEPRECATED;
	EnemyStats.Damage.PerWave = EnemyDamagePerWave_DEPRECATED;
	EnemyStats.MaxHealth = MaxEnemyHealth_DEPRECATED;
	BossStats.Health.BaseValue = BaseBossHealth_DEPRECATED;
	BossStats.Health.PerWave = BossHealthPerWave_DEPRECATED;
	BossStats.Speed.BaseValue = BaseBossSpeed_DEPRECATED;
	BossStats.Speed.PerWave = BossSpeedPerWave_DEPRECATED;
	BossStats.Damage.BaseValue = BaseBossDamage_DEPRECATED;
	BossStats.Damage.PerWave = BossDamagePerWave_DEPRECATED;
	BossStats.LateStageStartWave = LateEliteBossStartWave_DEPRECATED;
	BossStats.EarlyStage.HealthMultiplier = EarlyEliteBossHealthMultiplier_DEPRECATED;
	BossStats.LateStage.HealthMultiplier = LateEliteBossHealthMultiplier_DEPRECATED;
	BossStats.EarlyStage.DamageMultiplier = EarlyEliteBossDamageMultiplier_DEPRECATED;
	BossStats.LateStage.DamageMultiplier = LateEliteBossDamageMultiplier_DEPRECATED;
	BossStats.SpeedMultiplier = EliteBossSpeedMultiplier_DEPRECATED;
	Rewards.EnemyExperience.BaseAmount = BaseExperienceReward_DEPRECATED;
	Rewards.EnemyExperience.WaveInterval = ExperienceRewardWaveDivisor_DEPRECATED;
	Rewards.EnemyExperience.MaximumAmount = MaxExperienceReward_DEPRECATED;
	Rewards.BossExperience.PerWave = BossExperiencePerWave_DEPRECATED;
	Rewards.BossExperience.MinimumAmount = MinBossExperienceReward_DEPRECATED;
	Rewards.BossExperience.MaximumAmount = MaxBossExperienceReward_DEPRECATED;
	Rewards.EnemyGold.BaseAmount = EnemyCurrencyBaseReward_DEPRECATED;
	Rewards.EnemyGold.WaveInterval = EnemyCurrencyWaveDivisor_DEPRECATED;
	Rewards.EnemyGold.MaximumAmount = MaxEnemyCurrencyReward_DEPRECATED;
	Rewards.BossGold.BaseAmount = BossCurrencyBaseReward_DEPRECATED;
	Rewards.BossGold.PerWave = BossCurrencyPerWave_DEPRECATED;
	Rewards.BossGold.MaximumAmount = MaxBossCurrencyReward_DEPRECATED;
	Shop.OfferCount = ShopOfferCount_DEPRECATED;
	Shop.OfferCost = ShopOfferCost_DEPRECATED;
	Shop.RefreshCost = ShopRefreshCost_DEPRECATED;
	Shop.AutoRefreshInterval = ShopAutoRefreshInterval_DEPRECATED;
	Spawn.BatchRules = { {DoubleSpawnWave_DEPRECATED, DoubleSpawnSlack_DEPRECATED, 2}, {TripleSpawnWave_DEPRECATED, TripleSpawnSlack_DEPRECATED, 3} };
	EnemyStats.HealthGrowthStages = { {MidGameHealthStartWave_DEPRECATED, MidGameHealthPerWave_DEPRECATED, MidGameHealthCap_DEPRECATED}, {LateGameHealthStartWave_DEPRECATED, LateGameHealthPerWave_DEPRECATED, LateGameHealthCap_DEPRECATED} };
}

const FRogueEnemyBalanceRow* URogueGameBalanceAsset::FindEnemyBalanceRow(ERogueEnemyType Type) const
{
	for (const FRogueEnemyBalanceRow& Row : EnemyBalanceRows)
	{
		if (Row.EnemyType == Type)
		{
			return &Row;
		}
	}

	return nullptr;
}
