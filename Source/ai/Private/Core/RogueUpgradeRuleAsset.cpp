#include "Core/RogueUpgradeRuleAsset.h"

URogueUpgradeRuleAsset::URogueUpgradeRuleAsset()
{
	ResetToDefaultRules();
}

void URogueUpgradeRuleAsset::ResetToDefaultRules()
{
	BaseUpgradeTypes =
	{
		ERogueUpgradeType::MaxHealth,
		ERogueUpgradeType::ArmorCapacity,
		ERogueUpgradeType::MoveSpeed,
		ERogueUpgradeType::DashCooldown,
		ERogueUpgradeType::AttackPower,
		ERogueUpgradeType::AttackSpeed,
		ERogueUpgradeType::PickupRadius,
		ERogueUpgradeType::Recovery,
		ERogueUpgradeType::AttackRange,
		ERogueUpgradeType::ProjectileSpeed,
		ERogueUpgradeType::ProjectileCount,
		ERogueUpgradeType::Armor,
		ERogueUpgradeType::ExperienceGain
	};

	DefaultWeaponUpgradeTypes =
	{
		ERogueUpgradeType::ScytheCount,
		ERogueUpgradeType::RocketCount,
		ERogueUpgradeType::LaserCount,
		ERogueUpgradeType::HellTowerCount,
		ERogueUpgradeType::MortarCount
	};

	WeaponUpgradeRules =
	{
		{ ERogueWeaponUpgradeSource::Scythe, ERogueUpgradeType::ScytheCount },
		{ ERogueWeaponUpgradeSource::Rocket, ERogueUpgradeType::RocketCount },
		{ ERogueWeaponUpgradeSource::Laser, ERogueUpgradeType::LaserCount },
		{ ERogueWeaponUpgradeSource::HellTower, ERogueUpgradeType::HellTowerCount },
		{ ERogueWeaponUpgradeSource::Mortar, ERogueUpgradeType::MortarCount }
	};

	MinimumDashCooldown = 0.5f;
}
