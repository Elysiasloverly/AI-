#include "Framework/RogueGameModeRules.h"

#include "Framework/RogueGameBalanceAsset.h"

namespace
{
	static const FRogueEnemyWaveEntryRow DefaultEnemyWaveEntries[] =
	{
		{ 1, ERogueEnemyType::Rifleman, 3 },
		{ 2, ERogueEnemyType::Assault, 1 },
		{ 2, ERogueEnemyType::Skirmisher, 1 },
		{ 3, ERogueEnemyType::Orbiter, 1 },
		{ 3, ERogueEnemyType::Marksman, 1 },
		{ 3, ERogueEnemyType::Skirmisher, 1 },
		{ 4, ERogueEnemyType::Juggernaut, 1 },
		{ 4, ERogueEnemyType::Scatterer, 1 },
		{ 5, ERogueEnemyType::Cannoneer, 1 },
		{ 5, ERogueEnemyType::Marksman, 1 },
		{ 5, ERogueEnemyType::Shotgunner, 1 },
		{ 6, ERogueEnemyType::Marksman, 1 },
		{ 6, ERogueEnemyType::Sniper, 1 },
		{ 6, ERogueEnemyType::Scatterer, 1 },
		{ 6, ERogueEnemyType::Orbiter, 1 },
		// 震荡柱从第 7 波开始进入默认加权池；资产配置了波次表时以资产为准。
		{ 7, ERogueEnemyType::ShockPillar, 1 },
		{ 7, ERogueEnemyType::Repeater, 1 },
		{ 8, ERogueEnemyType::Juggernaut, 1 },
		{ 8, ERogueEnemyType::Cannoneer, 1 },
		{ 8, ERogueEnemyType::Assault, 1 },
		{ 9, ERogueEnemyType::Ringcaster, 1 },
		{ 10, ERogueEnemyType::Cannoneer, 1 },
		{ 10, ERogueEnemyType::Scatterer, 1 },
		{ 10, ERogueEnemyType::Skirmisher, 1 },
		{ 13, ERogueEnemyType::ShockPillar, 1 }
	};

	static const FRogueEnemyBalanceRow DefaultEnemyBalanceRows[] =
	{
		{ ERogueEnemyType::Rifleman, 1.00f, 1.00f, 1.00f, 0, 1.00f, 1.00f, 1.00f },
		{ ERogueEnemyType::Assault, 0.95f, 1.28f, 1.25f, 1, 1.00f, 1.25f, 1.18f },
		{ ERogueEnemyType::Juggernaut, 2.10f, 0.72f, 1.55f, 2, 1.35f, 0.80f, 1.25f },
		{ ERogueEnemyType::Orbiter, 0.85f, 1.12f, 1.05f, 1, 0.92f, 1.15f, 1.12f },
		{ ERogueEnemyType::Marksman, 0.88f, 0.98f, 1.18f, 1, 1.05f, 1.05f, 1.22f },
		{ ERogueEnemyType::Skirmisher, 0.76f, 1.42f, 1.14f, 1, 0.90f, 1.32f, 1.16f },
		{ ERogueEnemyType::Cannoneer, 1.02f, 0.86f, 1.34f, 2, 1.14f, 0.92f, 1.35f },
		{ ERogueEnemyType::Scatterer, 0.82f, 1.08f, 1.10f, 1, 0.96f, 1.12f, 1.24f },
		{ ERogueEnemyType::ShockPillar, 3.35f, 0.0f, 1.24f, 2, 1.55f, 0.0f, 1.30f },
		{ ERogueEnemyType::Sniper, 1.05f, 0.90f, 1.35f, 1, 1.10f, 0.92f, 1.45f },
		{ ERogueEnemyType::Shotgunner, 0.90f, 1.05f, 1.05f, 1, 0.98f, 1.08f, 1.18f },
		{ ERogueEnemyType::Repeater, 0.78f, 1.18f, 0.90f, 1, 0.92f, 1.20f, 1.05f },
		{ ERogueEnemyType::Ringcaster, 1.15f, 0.95f, 1.00f, 1, 1.08f, 0.98f, 1.12f }
	};

	const TArray<FRogueEnemyWaveEntryRow>* GetWaveEntries(const URogueGameBalanceAsset* BalanceAsset)
	{
		return BalanceAsset != nullptr && BalanceAsset->EnemyWaveEntries.Num() > 0 ? &BalanceAsset->EnemyWaveEntries : nullptr;
	}

	const FRogueEnemyBalanceRow* FindDefaultBalanceRow(ERogueEnemyType Type)
	{
		for (const FRogueEnemyBalanceRow& Row : DefaultEnemyBalanceRows)
		{
			if (Row.EnemyType == Type)
			{
				return &Row;
			}
		}

		return nullptr;
	}

	const FRogueEnemyBalanceRow* FindBalanceRow(const URogueGameBalanceAsset* BalanceAsset, ERogueEnemyType Type)
	{
		if (BalanceAsset != nullptr)
		{
			if (const FRogueEnemyBalanceRow* AssetRow = BalanceAsset->FindEnemyBalanceRow(Type))
			{
				return AssetRow;
			}
		}

		return FindDefaultBalanceRow(Type);
	}

	// 注意：权重求和已内联进 PickWeightedEnemyTypeInternal。
	// 因为"可用类型"（CanSpawnType）依赖运行时存活数，求和必须和抽取用同一套过滤条件，
	// 否则被上限挡住的类型仍会占权重 → 抽到它就会被丢弃 → 实际刷新变稀疏。
	template <typename EntryType>
	ERogueEnemyType PickWeightedEnemyTypeInternal(int32 Wave, const TArray<EntryType>& Entries,
		TFunctionRef<bool(ERogueEnemyType)> CanSpawnType)
	{
		int32 TotalWeight = 0;
		for (const EntryType& Entry : Entries)
		{
			if (Entry.Weight > 0 && Wave >= Entry.MinWave && CanSpawnType(Entry.EnemyType))
			{
				TotalWeight += Entry.Weight;
			}
		}

		if (TotalWeight <= 0)
		{
			return ERogueEnemyType::Rifleman;   // 兜底值；调用方需自行复核 CanSpawnType
		}

		int32 Pick = FMath::RandRange(1, TotalWeight);
		for (const EntryType& Entry : Entries)
		{
			if (Entry.Weight <= 0 || Wave < Entry.MinWave || !CanSpawnType(Entry.EnemyType))
			{
				continue;
			}

			Pick -= Entry.Weight;
			if (Pick <= 0)
			{
				return Entry.EnemyType;
			}
		}

		return ERogueEnemyType::Rifleman;
	}

	template <typename EntryType, uint32 N>
	ERogueEnemyType PickWeightedEnemyTypeInternal(int32 Wave, const EntryType (&Entries)[N],
		TFunctionRef<bool(ERogueEnemyType)> CanSpawnType)
	{
		int32 TotalWeight = 0;
		for (const EntryType& Entry : Entries)
		{
			if (Entry.Weight > 0 && Wave >= Entry.MinWave && CanSpawnType(Entry.EnemyType))
			{
				TotalWeight += Entry.Weight;
			}
		}

		if (TotalWeight <= 0)
		{
			return ERogueEnemyType::Rifleman;   // 兜底值；调用方需自行复核 CanSpawnType
		}

		int32 Pick = FMath::RandRange(1, TotalWeight);
		for (const EntryType& Entry : Entries)
		{
			if (Entry.Weight <= 0 || Wave < Entry.MinWave || !CanSpawnType(Entry.EnemyType))
			{
				continue;
			}

			Pick -= Entry.Weight;
			if (Pick <= 0)
			{
				return Entry.EnemyType;
			}
		}

		return ERogueEnemyType::Rifleman;
	}
}

int32 RogueGameModeRules::BuildWaveEnemyCount(int32 Wave, const URogueGameBalanceAsset* BalanceAsset)
{
    const FRogueSpawnBalanceSettings& Settings = BalanceAsset ? BalanceAsset->Spawn : GetDefault<URogueGameBalanceAsset>()->Spawn;
    int64 Count = static_cast<int64>(FMath::Max(1, Settings.BaseEnemiesPerWave))
        + static_cast<int64>(FMath::Max(0, Settings.EnemiesPerWaveIncrement)) * FMath::Max<int64>(0, static_cast<int64>(Wave) - 1);
    if (Settings.EnemiesPerWaveLimit > 0) Count = FMath::Min(Count, static_cast<int64>(Settings.EnemiesPerWaveLimit));
    return static_cast<int32>(FMath::Clamp<int64>(Count, 1, MAX_int32));
}

bool RogueGameModeRules::HasEligibleEnemyTypeForWave(int32 Wave, const URogueGameBalanceAsset* BalanceAsset,
    TFunctionRef<bool(ERogueEnemyType)> CanSpawnType)
{
    auto Eligible = [Wave, &CanSpawnType](const FRogueEnemyWaveEntryRow& Entry)
    {
        return Entry.Weight > 0 && Wave >= Entry.MinWave && CanSpawnType(Entry.EnemyType);
    };
    if (const TArray<FRogueEnemyWaveEntryRow>* Entries = GetWaveEntries(BalanceAsset))
        return Entries->ContainsByPredicate(Eligible);
    for (const FRogueEnemyWaveEntryRow& Entry : DefaultEnemyWaveEntries)
        if (Eligible(Entry)) return true;
    return false;
}

ERogueEnemyType RogueGameModeRules::PickWeightedEnemyTypeForWave(int32 Wave, const URogueGameBalanceAsset* BalanceAsset)
{
	// 无约束版本：Boss 生成走这条（Boss 不应该被"普通敌人的存活上限"挡住）。
	// TFunctionRef 不拥有回调；这里用具名 lambda 使其在本次调用中保持有效，不能将引用保存到回调生命期之外。
	auto AllowAll = [](ERogueEnemyType) { return true; };
	return PickWeightedEnemyTypeForWave(Wave, BalanceAsset, AllowAll);
}

ERogueEnemyType RogueGameModeRules::PickWeightedEnemyTypeForWave(int32 Wave, const URogueGameBalanceAsset* BalanceAsset,
	TFunctionRef<bool(ERogueEnemyType)> CanSpawnType)
{
	if (const TArray<FRogueEnemyWaveEntryRow>* AssetEntries = GetWaveEntries(BalanceAsset))
	{
		return PickWeightedEnemyTypeInternal(Wave, *AssetEntries, CanSpawnType);
	}

	return PickWeightedEnemyTypeInternal(Wave, DefaultEnemyWaveEntries, CanSpawnType);
}

FRogueSpawnCadence RogueGameModeRules::BuildSpawnCadence(int32 Wave, int32 AliveEnemies, int32 MaxAliveEnemies, bool bBossAlive, float BaseSpawnInterval, const URogueGameBalanceAsset* BalanceAsset)
{
	//数据资产优先，代码兜底
	const int32 BossAliveMinMaxEnemies = BalanceAsset != nullptr ? BalanceAsset->Spawn.BossAliveMinimumLimit : 20;
	const int32 BossAliveMaxEnemyReduction = BalanceAsset != nullptr ? BalanceAsset->Spawn.BossAliveReduction : 12;
	const int32 MaxAliveEnemiesPerWave = BalanceAsset != nullptr ? BalanceAsset->Spawn.MaxAlivePerWave : 8;
	const int32 MaxAliveEnemiesHardLimit = BalanceAsset != nullptr ? BalanceAsset->Spawn.MaxAliveLimit : 0;
	const float DifficultyScalePerWave = BalanceAsset != nullptr ? BalanceAsset->Spawn.AccelerationPerWave : 0.21f;
	const float MaxDifficultyScale = BalanceAsset != nullptr ? BalanceAsset->Spawn.MaxAcceleration : 4.8f;
	const int32 EarlyWaveSlowdownEndWave = BalanceAsset != nullptr ? BalanceAsset->Spawn.EarlyWaves.EndWave : 4;
	const float EarlyWaveSpawnMultiplierAtWaveOne = BalanceAsset != nullptr ? BalanceAsset->Spawn.EarlyWaves.FirstWaveIntervalMultiplier : 1.35f;
	const float EarlyWaveSpawnMultiplierAtEnd = BalanceAsset != nullptr ? BalanceAsset->Spawn.EarlyWaves.LastWaveIntervalMultiplier : 1.0f;
	const float MinimumSpawnDelay = BalanceAsset != nullptr ? BalanceAsset->Spawn.MinimumInterval : 0.18f;

	FRogueSpawnCadence Cadence;

	// 存活上限随波次逐级增长： 基础上限 + 每波增长 × (波次 - 1)
	// HardLimit > 0 时才封顶；默认 0 = 不设上限（持续增长）。
	int32 WaveScaledMaxAlive = MaxAliveEnemies + MaxAliveEnemiesPerWave * FMath::Max<int64>(0, static_cast<int64>(Wave) - 1);
	if (MaxAliveEnemiesHardLimit > 0)
	{
		WaveScaledMaxAlive = FMath::Min(WaveScaledMaxAlive, MaxAliveEnemiesHardLimit);
	}

	// Boss 在场时仍然压低上限，给它留性能预算
	Cadence.EffectiveMaxAliveEnemies = bBossAlive
		? FMath::Max(BossAliveMinMaxEnemies, WaveScaledMaxAlive - BossAliveMaxEnemyReduction)
		: WaveScaledMaxAlive;
	
	// 规则按列表顺序覆盖，保留原先三只规则独立于两只规则的行为。
	Cadence.SpawnBatchSize = 1;
	const TArray<FRogueSpawnBatchRule>& BatchRules = BalanceAsset != nullptr
		? BalanceAsset->Spawn.BatchRules : GetDefault<URogueGameBalanceAsset>()->Spawn.BatchRules;
	for (const FRogueSpawnBatchRule& Rule : BatchRules)
	{
		if (Wave >= Rule.StartWave && AliveEnemies <= Cadence.EffectiveMaxAliveEnemies - Rule.RequiredFreeSlots)
		{
			Cadence.SpawnBatchSize = Rule.BatchSize;
		}
	}

	const float DifficultyScale = 1.0f / FMath::Clamp(1.0f + static_cast<float>(Wave - 1) * DifficultyScalePerWave, 1.0f, MaxDifficultyScale);
	const float EarlyWaveSpawnMultiplier = Wave <= EarlyWaveSlowdownEndWave
		? FMath::GetMappedRangeValueClamped(FVector2D(1.0f, static_cast<float>(EarlyWaveSlowdownEndWave)), 
			FVector2D(EarlyWaveSpawnMultiplierAtWaveOne, EarlyWaveSpawnMultiplierAtEnd), static_cast<float>(Wave))
		: 1.0f;
	Cadence.NextSpawnDelay = FMath::Max(MinimumSpawnDelay, BaseSpawnInterval * DifficultyScale * EarlyWaveSpawnMultiplier);
	return Cadence;
}

FRogueEnemyProfile RogueGameModeRules::BuildEnemyProfile(int32 Wave, ERogueEnemyType EnemyType, const URogueGameBalanceAsset* BalanceAsset)
{
	//数据资产优先，代码兜底
	const float BaseEnemyHealth = BalanceAsset != nullptr ? BalanceAsset->EnemyStats.Health.BaseValue : 35.0f;
	const float EnemyHealthPerWave = BalanceAsset != nullptr ? BalanceAsset->EnemyStats.Health.PerWave : 10.0f;
	const float BaseEnemySpeed = BalanceAsset != nullptr ? BalanceAsset->EnemyStats.Speed.BaseValue : 240.0f;
	const float EnemySpeedPerWave = BalanceAsset != nullptr ? BalanceAsset->EnemyStats.Speed.PerWave : 12.0f;
	const float BaseEnemyDamage = BalanceAsset != nullptr ? BalanceAsset->EnemyStats.Damage.BaseValue : 8.0f;
	const float EnemyDamagePerWave = BalanceAsset != nullptr ? BalanceAsset->EnemyStats.Damage.PerWave : 1.5f;
	const int32 BaseExperienceReward = BalanceAsset != nullptr ? BalanceAsset->Rewards.EnemyExperience.BaseAmount : 1;
	const int32 ExperienceRewardWaveDivisor = BalanceAsset != nullptr ? BalanceAsset->Rewards.EnemyExperience.WaveInterval : 2;
	const int32 MaxExperienceReward = BalanceAsset != nullptr ? BalanceAsset->Rewards.EnemyExperience.MaximumAmount : 5;

	FRogueEnemyProfile Profile;
	Profile.EnemyType = EnemyType;
	Profile.bIsBoss = false;

	const float WaveScale = static_cast<float>(Wave - 1);
	Profile.Health = BaseEnemyHealth + WaveScale * EnemyHealthPerWave;
	Profile.Speed = BaseEnemySpeed + WaveScale * EnemySpeedPerWave;
	Profile.Damage = BaseEnemyDamage + WaveScale * EnemyDamagePerWave;
	Profile.ExperienceReward = FMath::Clamp(BaseExperienceReward + Wave / FMath::Max(ExperienceRewardWaveDivisor, 1), BaseExperienceReward, MaxExperienceReward);

	// 多个生命成长阶段可同时生效；每阶段从开始波当波起增加，并依次相乘。
	const TArray<FRogueHealthGrowthStage>& HealthStages = BalanceAsset != nullptr
		? BalanceAsset->EnemyStats.HealthGrowthStages : GetDefault<URogueGameBalanceAsset>()->EnemyStats.HealthGrowthStages;
	for (const FRogueHealthGrowthStage& Stage : HealthStages)
	{
		if (Wave >= Stage.StartWave)
		{
			const float RawBonus = static_cast<float>(Wave - (Stage.StartWave - 1)) * Stage.BonusPerWave;
			const float Bonus = Stage.MaxBonus > 0.0f ? FMath::Min(RawBonus, Stage.MaxBonus) : RawBonus;
			Profile.Health *= 1.0f + Bonus;
		}
	}

	if (const FRogueEnemyBalanceRow* BalanceRow = FindBalanceRow(BalanceAsset, EnemyType))
	{
		Profile.Health *= BalanceRow->HealthMultiplier;
		Profile.Speed *= BalanceRow->SpeedMultiplier;
		Profile.Damage *= BalanceRow->DamageMultiplier;
		Profile.ExperienceReward += BalanceRow->ExperienceRewardBonus;
	}

	// 普通敌人档案的最终生命钳制；任意负值表示关闭，>=0时包含类型倍率后再取最小值。
	// Boss 独立走 BuildBossProfile，不经过此分支。
	const float MaxEnemyHealth = BalanceAsset != nullptr ? BalanceAsset->EnemyStats.MaxHealth : -1.0f;
	if (MaxEnemyHealth >= 0.0f)
	{
		Profile.Health = FMath::Min(Profile.Health, MaxEnemyHealth);
	}

	return Profile;
}

FRogueEnemyProfile RogueGameModeRules::BuildBossProfile(int32 Wave, ERogueEnemyType EnemyType, const URogueGameBalanceAsset* BalanceAsset)
{
	const float BaseBossHealth = BalanceAsset != nullptr ? BalanceAsset->BossStats.Health.BaseValue : 420.0f;
	const float BossHealthPerWave = BalanceAsset != nullptr ? BalanceAsset->BossStats.Health.PerWave : 140.0f;
	const float BaseBossSpeed = BalanceAsset != nullptr ? BalanceAsset->BossStats.Speed.BaseValue : 250.0f;
	const float BossSpeedPerWave = BalanceAsset != nullptr ? BalanceAsset->BossStats.Speed.PerWave : 10.0f;
	const float BaseBossDamage = BalanceAsset != nullptr ? BalanceAsset->BossStats.Damage.BaseValue : 18.0f;
	const float BossDamagePerWave = BalanceAsset != nullptr ? BalanceAsset->BossStats.Damage.PerWave : 2.6f;
	const int32 BossExperiencePerWave = BalanceAsset != nullptr ? BalanceAsset->Rewards.BossExperience.PerWave : 4;
	const int32 MinBossExperienceReward = BalanceAsset != nullptr ? BalanceAsset->Rewards.BossExperience.MinimumAmount : 30;
	const int32 MaxBossExperienceReward = BalanceAsset != nullptr ? BalanceAsset->Rewards.BossExperience.MaximumAmount : 120;
	const int32 LateEliteBossStartWave = BalanceAsset != nullptr ? BalanceAsset->BossStats.LateStageStartWave : 20;
	const float EarlyEliteBossHealthMultiplier = BalanceAsset != nullptr ? BalanceAsset->BossStats.EarlyStage.HealthMultiplier : 1.58f;
	const float LateEliteBossHealthMultiplier = BalanceAsset != nullptr ? BalanceAsset->BossStats.LateStage.HealthMultiplier : 1.72f;
	const float EarlyEliteBossDamageMultiplier = BalanceAsset != nullptr ? BalanceAsset->BossStats.EarlyStage.DamageMultiplier : 1.16f;
	const float LateEliteBossDamageMultiplier = BalanceAsset != nullptr ? BalanceAsset->BossStats.LateStage.DamageMultiplier : 1.24f;
	const float EliteBossSpeedMultiplier = BalanceAsset != nullptr ? BalanceAsset->BossStats.SpeedMultiplier : 1.05f;

	FRogueEnemyProfile Profile;
	Profile.EnemyType = EnemyType;
	Profile.bIsBoss = true;

	const float WaveScale = static_cast<float>(Wave);
	Profile.Health = BaseBossHealth + WaveScale * BossHealthPerWave;
	Profile.Speed = BaseBossSpeed + WaveScale * BossSpeedPerWave;
	Profile.Damage = BaseBossDamage + WaveScale * BossDamagePerWave;
	Profile.ExperienceReward = FMath::Clamp(Wave * BossExperiencePerWave, MinBossExperienceReward, MaxBossExperienceReward);

	Profile.Health *= Wave >= LateEliteBossStartWave ? LateEliteBossHealthMultiplier : EarlyEliteBossHealthMultiplier;
	Profile.Damage *= Wave >= LateEliteBossStartWave ? LateEliteBossDamageMultiplier : EarlyEliteBossDamageMultiplier;
	Profile.Speed *= EliteBossSpeedMultiplier;

	if (const FRogueEnemyBalanceRow* BalanceRow = FindBalanceRow(BalanceAsset, EnemyType))
	{
		Profile.Health *= BalanceRow->BossHealthMultiplier;
		Profile.Speed *= BalanceRow->BossSpeedMultiplier;
		Profile.Damage *= BalanceRow->BossDamageMultiplier;
	}

	return Profile;
}

int32 RogueGameModeRules::BuildCurrencyReward(int32 Wave, bool bIsBoss, const URogueGameBalanceAsset* BalanceAsset)
{
	if (bIsBoss)
	{
		const int32 BossCurrencyBaseReward = BalanceAsset != nullptr ? BalanceAsset->Rewards.BossGold.BaseAmount : 14;
		const int32 BossCurrencyPerWave = BalanceAsset != nullptr ? BalanceAsset->Rewards.BossGold.PerWave : 1;
		const int32 MaxBossCurrencyReward = BalanceAsset != nullptr ? BalanceAsset->Rewards.BossGold.MaximumAmount : 28;
		return FMath::Clamp(BossCurrencyBaseReward + Wave * BossCurrencyPerWave, BossCurrencyBaseReward, MaxBossCurrencyReward);
	}

	const int32 EnemyCurrencyBaseReward = BalanceAsset != nullptr ? BalanceAsset->Rewards.EnemyGold.BaseAmount : 1;
	const int32 EnemyCurrencyWaveDivisor = BalanceAsset != nullptr ? BalanceAsset->Rewards.EnemyGold.WaveInterval : 6;
	const int32 MaxEnemyCurrencyReward = BalanceAsset != nullptr ? BalanceAsset->Rewards.EnemyGold.MaximumAmount : 3;
	return FMath::Clamp(EnemyCurrencyBaseReward + Wave / FMath::Max(EnemyCurrencyWaveDivisor, 1), EnemyCurrencyBaseReward, MaxEnemyCurrencyReward);
}

int32 RogueGameModeRules::GetShopOfferCount(const URogueGameBalanceAsset* BalanceAsset)
{
	return BalanceAsset != nullptr ? FMath::Max(1, BalanceAsset->Shop.OfferCount) : 6;
}

int32 RogueGameModeRules::GetShopOfferCost(const URogueGameBalanceAsset* BalanceAsset)
{
	return BalanceAsset != nullptr ? FMath::Max(1, BalanceAsset->Shop.OfferCost) : 22;
}

int32 RogueGameModeRules::GetShopRefreshCost(const URogueGameBalanceAsset* BalanceAsset)
{
	return BalanceAsset != nullptr ? FMath::Max(0, BalanceAsset->Shop.RefreshCost) : 14;
}

float RogueGameModeRules::GetShopAutoRefreshInterval(const URogueGameBalanceAsset* BalanceAsset)
{
	return BalanceAsset != nullptr ? FMath::Max(5.0f, BalanceAsset->Shop.AutoRefreshInterval) : 60.0f;
}
