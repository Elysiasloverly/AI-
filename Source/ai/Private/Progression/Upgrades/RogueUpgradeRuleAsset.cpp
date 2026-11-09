#include "Progression/Upgrades/RogueUpgradeRuleAsset.h"
#include "Combat/Weapons/RogueWeaponCatalog.h"

URogueUpgradeRuleAsset::URogueUpgradeRuleAsset()
{
	ResetToDefaultRules();
}

void URogueUpgradeRuleAsset::ResetToDefaultRules()
{
	BaseUpgradeTypes = {ERogueUpgradeType::MaxHealth,		ERogueUpgradeType::ArmorCapacity,
						ERogueUpgradeType::MoveSpeed,		ERogueUpgradeType::DashCooldown,
						ERogueUpgradeType::AttackPower,		ERogueUpgradeType::AttackSpeed,
						ERogueUpgradeType::PickupRadius,	ERogueUpgradeType::Recovery,
						ERogueUpgradeType::AttackRange,		ERogueUpgradeType::ProjectileSpeed,
						ERogueUpgradeType::ProjectileCount, ERogueUpgradeType::Armor,
						ERogueUpgradeType::ExperienceGain};

	DefaultWeaponUpgradeTypes = {ERogueUpgradeType::ScytheCount, ERogueUpgradeType::RocketCount,
								 ERogueUpgradeType::LaserCount,	 ERogueUpgradeType::HellTowerCount,
								 ERogueUpgradeType::MortarCount, ERogueUpgradeType::TalismanCount};

	WeaponUpgradeRules.Reset();
	for (const auto &Rule : RogueWeaponCatalog::GetDefaultUpgradeRules())
		WeaponUpgradeRules.Add(Rule);

	MinimumDashCooldown = 0.5f;
}
