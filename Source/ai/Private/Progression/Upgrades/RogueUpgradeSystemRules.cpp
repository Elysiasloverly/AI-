#include "Progression/Upgrades/RogueUpgradeSystemRules.h"

#include "Progression/Upgrades/RogueUpgradeRuleAsset.h"
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
		// 解锁奖励保存在 GAS 加成中；前置条件与实际开火共用合并并应用上限后的数量。
		switch (Source)
		{
		case ERogueWeaponUpgradeSource::Scythe:
			return Character.GetEffectiveScytheCount();
		case ERogueWeaponUpgradeSource::Rocket:
			return Character.GetEffectiveRocketLauncherCount();
		case ERogueWeaponUpgradeSource::Laser:
			return Character.GetEffectiveLaserCannonCount();
		case ERogueWeaponUpgradeSource::HellTower:
			return Character.GetEffectiveHellTowerCount();
		case ERogueWeaponUpgradeSource::Mortar:
			return Character.GetEffectiveMortarCount();
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
			ERogueUpgradeType::HellTowerCount,
			ERogueUpgradeType::MortarCount
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
			{ ERogueWeaponUpgradeSource::Scythe, ERogueUpgradeType::ScytheLifesteal },
			{ ERogueWeaponUpgradeSource::Rocket, ERogueUpgradeType::RocketCount },
			{ ERogueWeaponUpgradeSource::Laser, ERogueUpgradeType::LaserCount },
			{ ERogueWeaponUpgradeSource::HellTower, ERogueUpgradeType::HellTowerCount },
			{ ERogueWeaponUpgradeSource::Mortar, ERogueUpgradeType::MortarCount }
		};

		for (const FRogueWeaponUpgradeRuleRow& Rule : DefaultWeaponRules)
		{
			OutTypes.AddUnique(Rule.UpgradeType);
			if (Rule.Source == ERogueWeaponUpgradeSource::Laser && GetWeaponSourceCount(Character, Rule.Source) > 0)
			{
				OutTypes.AddUnique(ERogueUpgradeType::LaserRefraction);
			}
			else if (Rule.Source == ERogueWeaponUpgradeSource::Mortar && GetWeaponSourceCount(Character, Rule.Source) > 0)
			{
				OutTypes.AddUnique(ERogueUpgradeType::MortarBlastRadius);
			}
		}
	}

	void AppendAssetWeaponRules(TArray<ERogueUpgradeType>& OutTypes, const ARogueCharacter& Character, const URogueUpgradeRuleAsset& RuleAsset)
	{
		bool bHasMortarRule = false;

		for (const FRogueWeaponUpgradeRuleRow& Rule : RuleAsset.WeaponUpgradeRules)
		{
			bHasMortarRule = bHasMortarRule || Rule.Source == ERogueWeaponUpgradeSource::Mortar || Rule.UpgradeType == ERogueUpgradeType::MortarCount;
			OutTypes.AddUnique(Rule.UpgradeType);
			if (Rule.Source == ERogueWeaponUpgradeSource::Laser && GetWeaponSourceCount(Character, Rule.Source) > 0)
			{
				OutTypes.AddUnique(ERogueUpgradeType::LaserRefraction);
			}
			else if (Rule.Source == ERogueWeaponUpgradeSource::Mortar && GetWeaponSourceCount(Character, Rule.Source) > 0)
			{
				OutTypes.AddUnique(ERogueUpgradeType::MortarBlastRadius);
			}
		}

		if (!bHasMortarRule)
		{
			OutTypes.AddUnique(ERogueUpgradeType::MortarCount);
			if (GetWeaponSourceCount(Character, ERogueWeaponUpgradeSource::Mortar) > 0)
			{
				OutTypes.AddUnique(ERogueUpgradeType::MortarBlastRadius);
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
		// 镰刀专属升级需要实际武器，不能由无角色的通用池提供。
		OutTypes.Remove(ERogueUpgradeType::ScytheSize);
		OutTypes.Remove(ERogueUpgradeType::ScytheLifesteal);
		return;
	}

	AppendCharacterSpecificUpgradeTypes(OutTypes, *Character, RuleAsset);
	// 对默认规则和自定义资产共用同一前置，避免被误放进基础池后提前出现。
	if (Character->GetEffectiveScytheCount() > 0)
	{
		OutTypes.AddUnique(ERogueUpgradeType::ScytheSize);
		OutTypes.AddUnique(ERogueUpgradeType::ScytheLifesteal);
	}
	else
	{
		OutTypes.Remove(ERogueUpgradeType::ScytheSize);
		OutTypes.Remove(ERogueUpgradeType::ScytheLifesteal);
	}
}


