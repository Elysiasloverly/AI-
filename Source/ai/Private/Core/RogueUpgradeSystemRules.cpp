#include "Core/RogueUpgradeSystemRules.h"

#include "Core/RogueUpgradeRuleAsset.h"
#include "Player/RogueCharacter.h"

namespace
{
	static constexpr float DefaultMinimumDashCooldown = 0.5f;

	template <uint32 N>
	void AppendUpgradeTypes(TArray<ERogueUpgradeType>& OutTypes, const ERogueUpgradeType (&UpgradeTypes)[N])
	{
		for (const ERogueUpgradeType UpgradeType : UpgradeTypes)
		{
			OutTypes.AddUnique(UpgradeType);
		}
	}

	void AppendUpgradeTypes(TArray<ERogueUpgradeType>& OutTypes, const TArray<ERogueUpgradeType>& UpgradeTypes)
	{
		for (const ERogueUpgradeType UpgradeType : UpgradeTypes)
		{
			OutTypes.AddUnique(UpgradeType);
		}
	}

	int32 GetWeaponSourceCount(const ARogueCharacter& Character, ERogueWeaponUpgradeSource Source)
	{
		switch (Source)
		{
		case ERogueWeaponUpgradeSource::Scythe:
			return Character.GetScytheCount();
		case ERogueWeaponUpgradeSource::Rocket:
			return Character.GetRocketLauncherCount();
		case ERogueWeaponUpgradeSource::Laser:
			return Character.GetLaserCannonCount();
		case ERogueWeaponUpgradeSource::HellTower:
			return Character.GetHellTowerCount();
		default:
			return 0;
		}
	}

	float ResolveMinimumDashCooldown(const URogueUpgradeRuleAsset* RuleAsset)
	{
		return RuleAsset != nullptr ? RuleAsset->MinimumDashCooldown : DefaultMinimumDashCooldown;
	}

	void AppendBaseUpgradeTypes(TArray<ERogueUpgradeType>& OutTypes, const URogueUpgradeRuleAsset* RuleAsset)
	{
		static constexpr ERogueUpgradeType BaseUpgradeTypes[] =
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

		if (RuleAsset != nullptr && RuleAsset->BaseUpgradeTypes.Num() > 0)
		{
			AppendUpgradeTypes(OutTypes, RuleAsset->BaseUpgradeTypes);
			return;
		}

		AppendUpgradeTypes(OutTypes, BaseUpgradeTypes);
	}

	void AppendDefaultWeaponUpgradeTypes(TArray<ERogueUpgradeType>& OutTypes, const URogueUpgradeRuleAsset* RuleAsset)
	{
		static constexpr ERogueUpgradeType DefaultWeaponUpgradeTypes[] =
		{
			ERogueUpgradeType::ScytheCount,
			ERogueUpgradeType::RocketCount,
			ERogueUpgradeType::LaserCount,
			ERogueUpgradeType::HellTowerCount
		};

		if (RuleAsset != nullptr && RuleAsset->DefaultWeaponUpgradeTypes.Num() > 0)
		{
			AppendUpgradeTypes(OutTypes, RuleAsset->DefaultWeaponUpgradeTypes);
			return;
		}

		AppendUpgradeTypes(OutTypes, DefaultWeaponUpgradeTypes);
	}

	void AppendDefaultWeaponRules(TArray<ERogueUpgradeType>& OutTypes, const ARogueCharacter& Character)
	{
		static const FRogueWeaponUpgradeRuleRow DefaultWeaponRules[] =
		{
			{ ERogueWeaponUpgradeSource::Scythe, ERogueUpgradeType::ScytheCount },
			{ ERogueWeaponUpgradeSource::Rocket, ERogueUpgradeType::RocketCount },
			{ ERogueWeaponUpgradeSource::Laser, ERogueUpgradeType::LaserCount },
			{ ERogueWeaponUpgradeSource::HellTower, ERogueUpgradeType::HellTowerCount }
		};

		for (const FRogueWeaponUpgradeRuleRow& Rule : DefaultWeaponRules)
		{
			OutTypes.AddUnique(Rule.UpgradeType);
			if (Rule.Source == ERogueWeaponUpgradeSource::Laser && GetWeaponSourceCount(Character, Rule.Source) > 0)
			{
				OutTypes.AddUnique(ERogueUpgradeType::LaserRefraction);
			}
		}
	}

	void AppendAssetWeaponRules(TArray<ERogueUpgradeType>& OutTypes, const ARogueCharacter& Character, const URogueUpgradeRuleAsset& RuleAsset)
	{
		for (const FRogueWeaponUpgradeRuleRow& Rule : RuleAsset.WeaponUpgradeRules)
		{
			OutTypes.AddUnique(Rule.UpgradeType);
			if (Rule.Source == ERogueWeaponUpgradeSource::Laser && GetWeaponSourceCount(Character, Rule.Source) > 0)
			{
				OutTypes.AddUnique(ERogueUpgradeType::LaserRefraction);
			}
		}
	}

	void AppendCharacterSpecificUpgradeTypes(TArray<ERogueUpgradeType>& OutTypes, const ARogueCharacter& Character, const URogueUpgradeRuleAsset* RuleAsset)
	{
		if (Character.GetDashCooldownDuration() <= ResolveMinimumDashCooldown(RuleAsset) + KINDA_SMALL_NUMBER)
		{
			OutTypes.Remove(ERogueUpgradeType::DashCooldown);
		}

		if (RuleAsset != nullptr && RuleAsset->WeaponUpgradeRules.Num() > 0)
		{
			AppendAssetWeaponRules(OutTypes, Character, *RuleAsset);
		}
		else
		{
			AppendDefaultWeaponRules(OutTypes, Character);
		}
	}
}

void RogueUpgradeSystemRules::BuildUpgradePool(TArray<ERogueUpgradeType>& OutTypes, const ARogueCharacter* Character, const URogueUpgradeRuleAsset* RuleAsset)
{
	OutTypes.Reset();
	AppendBaseUpgradeTypes(OutTypes, RuleAsset);

	if (Character == nullptr)
	{
		AppendDefaultWeaponUpgradeTypes(OutTypes, RuleAsset);
		return;
	}

	AppendCharacterSpecificUpgradeTypes(OutTypes, *Character, RuleAsset);
}


