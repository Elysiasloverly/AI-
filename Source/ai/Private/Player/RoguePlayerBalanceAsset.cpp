#include "Player/RoguePlayerBalanceAsset.h"

URoguePlayerBalanceAsset::URoguePlayerBalanceAsset()
{
	ResetToDefaults();
}

void URoguePlayerBalanceAsset::ResetToDefaults()
{
	BaseStats = FRoguePlayerBaseStatConfig();
	WeaponBonusCaps = FRogueWeaponBonusCaps();
	InitialWeaponBonuses = FRogueInitialWeaponBonusConfig();
	Progression = FRoguePlayerProgressionConfig();
	SpawnState = FRoguePlayerSpawnStateConfig();
	Revival = FRoguePlayerRevivalConfig();
}

int32 FRoguePlayerProgressionConfig::GetNextExperienceRequirement(int32 CurrentRequirement) const
{
	const float Multiplier = FMath::IsFinite(ExperienceGrowthMultiplier)
		? FMath::Max(0.0f, ExperienceGrowthMultiplier) : 1.30f;
	const float Requirement = static_cast<float>(FMath::Max(1, CurrentRequirement)) * Multiplier
		+ static_cast<float>(FMath::Max(0, ExperienceGrowthAddend));
	if (!FMath::IsFinite(Requirement) || Requirement >= static_cast<float>(MAX_int32))
	{
		return MAX_int32;
	}
	return FMath::Max(1, FMath::RoundToInt(Requirement));
}
