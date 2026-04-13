#include "Core/RogueGameModeRules.h"

#include "Core/RogueGameBalanceAsset.h"

namespace
{
	static const FRogueEnemyWaveEntryRow DefaultEnemyWaveEntries[] =
	{
		{ 1, ERogueEnemyType::Hunter, 3 },
		{ 2, ERogueEnemyType::Charger, 1 },
		{ 2, ERogueEnemyType::Skirmisher, 1 },
		{ 3, ERogueEnemyType::Orbiter, 1 },
		{ 3, ERogueEnemyType::Shooter, 1 },
		{ 3, ERogueEnemyType::Skirmisher, 1 },
		{ 4, ERogueEnemyType::Tank, 1 },
		{ 4, ERogueEnemyType::Spitter, 1 },
		{ 5, ERogueEnemyType::Artillery, 1 },
		{ 5, ERogueEnemyType::Shooter, 1 },
		{ 6, ERogueEnemyType::Shooter, 1 },
		{ 6, ERogueEnemyType::Spitter, 1 },
		{ 6, ERogueEnemyType::Orbiter, 1 },
		{ 8, ERogueEnemyType::Tank, 1 },
		{ 8, ERogueEnemyType::Artillery, 1 },
		{ 8, ERogueEnemyType::Charger, 1 },
		{ 10, ERogueEnemyType::Artillery, 1 },
		{ 10, ERogueEnemyType::Spitter, 1 },
		{ 10, ERogueEnemyType::Skirmisher, 1 }
	};

	static const FRogueEnemyBalanceRow DefaultEnemyBalanceRows[] =
	{
		{ ERogueEnemyType::Hunter, 1.00f, 1.00f, 1.00f, 0, 1.00f, 1.00f, 1.00f },
		{ ERogueEnemyType::Charger, 0.95f, 1.28f, 1.25f, 1, 1.00f, 1.25f, 1.18f },
		{ ERogueEnemyType::Tank, 2.10f, 0.72f, 1.55f, 2, 1.35f, 0.80f, 1.25f },
		{ ERogueEnemyType::Orbiter, 0.85f, 1.12f, 1.05f, 1, 0.92f, 1.15f, 1.12f },
		{ ERogueEnemyType::Shooter, 0.88f, 0.98f, 1.18f, 1, 1.05f, 1.05f, 1.22f },
		{ ERogueEnemyType::Skirmisher, 0.76f, 1.42f, 1.14f, 1, 0.90f, 1.32f, 1.16f },
		{ ERogueEnemyType::Artillery, 1.02f, 0.86f, 1.34f, 2, 1.14f, 0.92f, 1.35f },
		{ ERogueEnemyType::Spitter, 0.82f, 1.08f, 1.10f, 1, 0.96f, 1.12f, 1.24f }
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

	template <typename EntryType>
	int32 AccumulateWaveWeights(int32 Wave, const TArray<EntryType>& Entries)
	{
		int32 TotalWeight = 0;
		for (const EntryType& Entry : Entries)
		{
			if (Wave >= Entry.MinWave)
			{
				TotalWeight += Entry.Weight;
			}
		}
		return TotalWeight;
	}

	template <typename EntryType, uint32 N>
	int32 AccumulateWaveWeights(int32 Wave, const EntryType (&Entries)[N])
	{
		int32 TotalWeight = 0;
		for (const EntryType& Entry : Entries)
		{
			if (Wave >= Entry.MinWave)
			{
				TotalWeight += Entry.Weight;
			}
		}
		return TotalWeight;
	}

	template <typename EntryType>
	ERogueEnemyType PickWeightedEnemyTypeInternal(int32 Wave, const TArray<EntryType>& Entries)
	{
		int32 TotalWeight = AccumulateWaveWeights(Wave, Entries);
		if (TotalWeight <= 0)
		{
			return ERogueEnemyType::Hunter;
		}

		int32 Pick = FMath::RandRange(1, TotalWeight);
		for (const EntryType& Entry : Entries)
		{
			if (Wave < Entry.MinWave)
			{
				continue;
			}

			Pick -= Entry.Weight;
			if (Pick <= 0)
			{
				return Entry.EnemyType;
			}
		}

		return ERogueEnemyType::Hunter;
	}

	template <typename EntryType, uint32 N>
	ERogueEnemyType PickWeightedEnemyTypeInternal(int32 Wave, const EntryType (&Entries)[N])
	{
		int32 TotalWeight = AccumulateWaveWeights(Wave, Entries);
		if (TotalWeight <= 0)
		{
			return ERogueEnemyType::Hunter;
		}

		int32 Pick = FMath::RandRange(1, TotalWeight);
		for (const EntryType& Entry : Entries)
		{
			if (Wave < Entry.MinWave)
			{
				continue;
			}

			Pick -= Entry.Weight;
			if (Pick <= 0)
			{
				return Entry.EnemyType;
			}
		}

		return ERogueEnemyType::Hunter;
	}
}

ERogueEnemyType RogueGameModeRules::PickWeightedEnemyTypeForWave(int32 Wave, const URogueGameBalanceAsset* BalanceAsset)
{
	if (const TArray<FRogueEnemyWaveEntryRow>* AssetEntries = GetWaveEntries(BalanceAsset))
	{
		return PickWeightedEnemyTypeInternal(Wave, *AssetEntries);
	}

	return PickWeightedEnemyTypeInternal(Wave, DefaultEnemyWaveEntries);
}

FRogueSpawnCadence RogueGameModeRules::BuildSpawnCadence(int32 Wave, int32 AliveEnemies, int32 MaxAliveEnemies, bool bBossAlive, float BaseSpawnInterval, const URogueGameBalanceAsset* BalanceAsset)
{
	const int32 BossAliveMinMaxEnemies = BalanceAsset != nullptr ? BalanceAsset->BossAliveMinMaxEnemies : 20;
	const int32 BossAliveMaxEnemyReduction = BalanceAsset != nullptr ? BalanceAsset->BossAliveMaxEnemyReduction : 12;
	const int32 DoubleSpawnWave = BalanceAsset != nullptr ? BalanceAsset->DoubleSpawnWave : 5;
	const int32 DoubleSpawnSlack = BalanceAsset != nullptr ? BalanceAsset->DoubleSpawnSlack : 10;
	const int32 TripleSpawnWave = BalanceAsset != nullptr ? BalanceAsset->TripleSpawnWave : 10;
	const int32 TripleSpawnSlack = BalanceAsset != nullptr ? BalanceAsset->TripleSpawnSlack : 24;
	const float DifficultyScalePerWave = BalanceAsset != nullptr ? BalanceAsset->DifficultyScalePerWave : 0.21f;
	const float MaxDifficultyScale = BalanceAsset != nullptr ? BalanceAsset->MaxDifficultyScale : 4.8f;
	const int32 EarlyWaveSlowdownEndWave = BalanceAsset != nullptr ? BalanceAsset->EarlyWaveSlowdownEndWave : 4;
	const float EarlyWaveSpawnMultiplierAtWaveOne = BalanceAsset != nullptr ? BalanceAsset->EarlyWaveSpawnMultiplierAtWaveOne : 1.35f;
	const float EarlyWaveSpawnMultiplierAtEnd = BalanceAsset != nullptr ? BalanceAsset->EarlyWaveSpawnMultiplierAtEnd : 1.0f;
	const float MinimumSpawnDelay = BalanceAsset != nullptr ? BalanceAsset->MinimumSpawnDelay : 0.18f;

	FRogueSpawnCadence Cadence;
	Cadence.EffectiveMaxAliveEnemies = bBossAlive ? FMath::Max(BossAliveMinMaxEnemies, MaxAliveEnemies - BossAliveMaxEnemyReduction) : MaxAliveEnemies;
	Cadence.SpawnBatchSize = 1;

	if (Wave >= DoubleSpawnWave && AliveEnemies <= Cadence.EffectiveMaxAliveEnemies - DoubleSpawnSlack)
	{
		Cadence.SpawnBatchSize = 2;
	}

	if (Wave >= TripleSpawnWave && AliveEnemies <= Cadence.EffectiveMaxAliveEnemies - TripleSpawnSlack)
	{
		Cadence.SpawnBatchSize = 3;
	}

	const float DifficultyScale = 1.0f / FMath::Clamp(1.0f + static_cast<float>(Wave - 1) * DifficultyScalePerWave, 1.0f, MaxDifficultyScale);
	const float EarlyWaveSpawnMultiplier = Wave <= EarlyWaveSlowdownEndWave
		? FMath::GetMappedRangeValueClamped(FVector2D(1.0f, static_cast<float>(EarlyWaveSlowdownEndWave)), FVector2D(EarlyWaveSpawnMultiplierAtWaveOne, EarlyWaveSpawnMultiplierAtEnd), static_cast<float>(Wave))
		: 1.0f;
	Cadence.NextSpawnDelay = FMath::Max(MinimumSpawnDelay, BaseSpawnInterval * DifficultyScale * EarlyWaveSpawnMultiplier);
	return Cadence;
}

FRogueEnemyProfile RogueGameModeRules::BuildEnemyProfile(int32 Wave, ERogueEnemyType EnemyType, const URogueGameBalanceAsset* BalanceAsset)
{
	const float BaseEnemyHealth = BalanceAsset != nullptr ? BalanceAsset->BaseEnemyHealth : 35.0f;
	const float EnemyHealthPerWave = BalanceAsset != nullptr ? BalanceAsset->EnemyHealthPerWave : 10.0f;
	const float BaseEnemySpeed = BalanceAsset != nullptr ? BalanceAsset->BaseEnemySpeed : 240.0f;
	const float EnemySpeedPerWave = BalanceAsset != nullptr ? BalanceAsset->EnemySpeedPerWave : 12.0f;
	const float BaseEnemyDamage = BalanceAsset != nullptr ? BalanceAsset->BaseEnemyDamage : 8.0f;
	const float EnemyDamagePerWave = BalanceAsset != nullptr ? BalanceAsset->EnemyDamagePerWave : 1.5f;
	const int32 BaseExperienceReward = BalanceAsset != nullptr ? BalanceAsset->BaseExperienceReward : 1;
	const int32 ExperienceRewardWaveDivisor = BalanceAsset != nullptr ? BalanceAsset->ExperienceRewardWaveDivisor : 2;
	const int32 MaxExperienceReward = BalanceAsset != nullptr ? BalanceAsset->MaxExperienceReward : 5;
	const int32 MidGameHealthStartWave = BalanceAsset != nullptr ? BalanceAsset->MidGameHealthStartWave : 8;
	const float MidGameHealthPerWave = BalanceAsset != nullptr ? BalanceAsset->MidGameHealthPerWave : 0.08f;
	const float MidGameHealthCap = BalanceAsset != nullptr ? BalanceAsset->MidGameHealthCap : 0.72f;
	const int32 LateGameHealthStartWave = BalanceAsset != nullptr ? BalanceAsset->LateGameHealthStartWave : 16;
	const float LateGameHealthPerWave = BalanceAsset != nullptr ? BalanceAsset->LateGameHealthPerWave : 0.05f;
	const float LateGameHealthCap = BalanceAsset != nullptr ? BalanceAsset->LateGameHealthCap : 0.50f;

	FRogueEnemyProfile Profile;
	Profile.EnemyType = EnemyType;
	Profile.bIsBoss = false;

	const float WaveScale = static_cast<float>(Wave - 1);
	Profile.Health = BaseEnemyHealth + WaveScale * EnemyHealthPerWave;
	Profile.Speed = BaseEnemySpeed + WaveScale * EnemySpeedPerWave;
	Profile.Damage = BaseEnemyDamage + WaveScale * EnemyDamagePerWave;
	Profile.ExperienceReward = FMath::Clamp(BaseExperienceReward + Wave / FMath::Max(ExperienceRewardWaveDivisor, 1), BaseExperienceReward, MaxExperienceReward);

	if (Wave >= MidGameHealthStartWave)
	{
		const float MidGameHealthMultiplier = 1.0f + FMath::Min(static_cast<float>(Wave - (MidGameHealthStartWave - 1)) * MidGameHealthPerWave, MidGameHealthCap);
		Profile.Health *= MidGameHealthMultiplier;
	}

	if (Wave >= LateGameHealthStartWave)
	{
		const float LateGameHealthMultiplier = 1.0f + FMath::Min(static_cast<float>(Wave - (LateGameHealthStartWave - 1)) * LateGameHealthPerWave, LateGameHealthCap);
		Profile.Health *= LateGameHealthMultiplier;
	}

	if (const FRogueEnemyBalanceRow* BalanceRow = FindBalanceRow(BalanceAsset, EnemyType))
	{
		Profile.Health *= BalanceRow->HealthMultiplier;
		Profile.Speed *= BalanceRow->SpeedMultiplier;
		Profile.Damage *= BalanceRow->DamageMultiplier;
		Profile.ExperienceReward += BalanceRow->ExperienceRewardBonus;
	}

	return Profile;
}

FRogueEnemyProfile RogueGameModeRules::BuildBossProfile(int32 Wave, ERogueEnemyType EnemyType, const URogueGameBalanceAsset* BalanceAsset)
{
	const float BaseBossHealth = BalanceAsset != nullptr ? BalanceAsset->BaseBossHealth : 420.0f;
	const float BossHealthPerWave = BalanceAsset != nullptr ? BalanceAsset->BossHealthPerWave : 140.0f;
	const float BaseBossSpeed = BalanceAsset != nullptr ? BalanceAsset->BaseBossSpeed : 250.0f;
	const float BossSpeedPerWave = BalanceAsset != nullptr ? BalanceAsset->BossSpeedPerWave : 10.0f;
	const float BaseBossDamage = BalanceAsset != nullptr ? BalanceAsset->BaseBossDamage : 18.0f;
	const float BossDamagePerWave = BalanceAsset != nullptr ? BalanceAsset->BossDamagePerWave : 2.6f;
	const int32 BossExperiencePerWave = BalanceAsset != nullptr ? BalanceAsset->BossExperiencePerWave : 4;
	const int32 MinBossExperienceReward = BalanceAsset != nullptr ? BalanceAsset->MinBossExperienceReward : 30;
	const int32 MaxBossExperienceReward = BalanceAsset != nullptr ? BalanceAsset->MaxBossExperienceReward : 120;
	const int32 LateEliteBossStartWave = BalanceAsset != nullptr ? BalanceAsset->LateEliteBossStartWave : 20;
	const float EarlyEliteBossHealthMultiplier = BalanceAsset != nullptr ? BalanceAsset->EarlyEliteBossHealthMultiplier : 1.58f;
	const float LateEliteBossHealthMultiplier = BalanceAsset != nullptr ? BalanceAsset->LateEliteBossHealthMultiplier : 1.72f;
	const float EarlyEliteBossDamageMultiplier = BalanceAsset != nullptr ? BalanceAsset->EarlyEliteBossDamageMultiplier : 1.16f;
	const float LateEliteBossDamageMultiplier = BalanceAsset != nullptr ? BalanceAsset->LateEliteBossDamageMultiplier : 1.24f;
	const float EliteBossSpeedMultiplier = BalanceAsset != nullptr ? BalanceAsset->EliteBossSpeedMultiplier : 1.05f;

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
		const int32 BossCurrencyBaseReward = BalanceAsset != nullptr ? BalanceAsset->BossCurrencyBaseReward : 14;
		const int32 BossCurrencyPerWave = BalanceAsset != nullptr ? BalanceAsset->BossCurrencyPerWave : 1;
		const int32 MaxBossCurrencyReward = BalanceAsset != nullptr ? BalanceAsset->MaxBossCurrencyReward : 28;
		return FMath::Clamp(BossCurrencyBaseReward + Wave * BossCurrencyPerWave, BossCurrencyBaseReward, MaxBossCurrencyReward);
	}

	const int32 EnemyCurrencyBaseReward = BalanceAsset != nullptr ? BalanceAsset->EnemyCurrencyBaseReward : 1;
	const int32 EnemyCurrencyWaveDivisor = BalanceAsset != nullptr ? BalanceAsset->EnemyCurrencyWaveDivisor : 6;
	const int32 MaxEnemyCurrencyReward = BalanceAsset != nullptr ? BalanceAsset->MaxEnemyCurrencyReward : 3;
	return FMath::Clamp(EnemyCurrencyBaseReward + Wave / FMath::Max(EnemyCurrencyWaveDivisor, 1), EnemyCurrencyBaseReward, MaxEnemyCurrencyReward);
}

int32 RogueGameModeRules::GetShopOfferCount(const URogueGameBalanceAsset* BalanceAsset)
{
	return BalanceAsset != nullptr ? FMath::Max(1, BalanceAsset->ShopOfferCount) : 6;
}

int32 RogueGameModeRules::GetShopOfferCost(const URogueGameBalanceAsset* BalanceAsset)
{
	return BalanceAsset != nullptr ? FMath::Max(1, BalanceAsset->ShopOfferCost) : 22;
}

int32 RogueGameModeRules::GetShopRefreshCost(const URogueGameBalanceAsset* BalanceAsset)
{
	return BalanceAsset != nullptr ? FMath::Max(0, BalanceAsset->ShopRefreshCost) : 14;
}

float RogueGameModeRules::GetShopAutoRefreshInterval(const URogueGameBalanceAsset* BalanceAsset)
{
	return BalanceAsset != nullptr ? FMath::Max(5.0f, BalanceAsset->ShopAutoRefreshInterval) : 60.0f;
}
