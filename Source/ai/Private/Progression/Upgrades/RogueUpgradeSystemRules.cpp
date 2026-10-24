#include "Progression/Upgrades/RogueUpgradeSystemRules.h"

#include "Progression/Upgrades/RogueUpgradeRuleAsset.h"
#include "Player/RogueCharacter.h"
#include "Player/RoguePlayerState.h"
#include "Progression/Meta/RogueUpgradeTreeDefinition.h"

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
		case ERogueWeaponUpgradeSource::Projectile:
			return Character.GetEffectiveProjectileCount();
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
		{ ERogueWeaponUpgradeSource::Projectile, ERogueUpgradeType::ProjectileBurst },
		{ ERogueWeaponUpgradeSource::Rocket, ERogueUpgradeType::RocketBlastRadius },
		{ ERogueWeaponUpgradeSource::HellTower, ERogueUpgradeType::HellTowerDamageRamp },

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

	int32 GetOwnedCountForUpgrade(const ARogueCharacter* Character, ERogueUpgradeType Type)
	{
		if (Character == nullptr)
		{
			return 0;
		}
		switch (Type)
		{
		case ERogueUpgradeType::ProjectileBurst:
		case ERogueUpgradeType::ProjectileCount: return Character->GetEffectiveProjectileCount();
		case ERogueUpgradeType::ScytheCount:
		case ERogueUpgradeType::ScytheSize:
		case ERogueUpgradeType::ScytheLifesteal: return Character->GetEffectiveScytheCount();
		case ERogueUpgradeType::RocketBlastRadius:
		case ERogueUpgradeType::RocketCount: return Character->GetEffectiveRocketLauncherCount();
		case ERogueUpgradeType::LaserCount:
		case ERogueUpgradeType::LaserRefraction: return Character->GetEffectiveLaserCannonCount();
		case ERogueUpgradeType::HellTowerDamageRamp:
		case ERogueUpgradeType::HellTowerCount: return Character->GetEffectiveHellTowerCount();
		case ERogueUpgradeType::MortarCount:
		case ERogueUpgradeType::MortarBlastRadius: return Character->GetEffectiveMortarCount();
		default: return 0;
		}
	}

	bool IsProgressionAndPrerequisitesAllowed(ERogueUpgradeType Type, const ARogueCharacter* Character)
	{
		// 卡池来自这个角色自己的 PlayerState；缺少档案时使用新存档权限，绝不借用房主档案。
		const ARoguePlayerState* State = Character != nullptr ? Character->GetPlayerState<ARoguePlayerState>() : nullptr;
		const FRogueProgressionSnapshot FreshProfile;
		const FRogueProgressionSnapshot& Profile = State != nullptr && State->IsProgressionReady()
			? State->GetProgressionSnapshot() : FreshProfile;
		const URogueUpgradeTreeDefinition* Tree = URogueUpgradeTreeDefinition::GetDefaultDefinition();
		const bool bOwned = GetOwnedCountForUpgrade(Character, Type) > 0;
		if (!Tree->IsUpgradeAvailable(Profile, Type, bOwned))
		{
			return false;
		}
		// 永久许可与本局前置分别验证；自定义基础池也不能提前塞进这些专属升级。
		switch (Type)
		{
		case ERogueUpgradeType::ProjectileBurst:
		case ERogueUpgradeType::RocketBlastRadius:
		case ERogueUpgradeType::HellTowerDamageRamp:
		case ERogueUpgradeType::ScytheSize:
		case ERogueUpgradeType::ScytheLifesteal:
		case ERogueUpgradeType::LaserRefraction:
		case ERogueUpgradeType::MortarBlastRadius:
			return bOwned;
		default:
			return true;
		}
	}

	void FilterProgressionAndPrerequisites(TArray<ERogueUpgradeType>& OutTypes, const ARogueCharacter* Character)
	{
		OutTypes.RemoveAll([Character](ERogueUpgradeType Type)
		{
			return !IsProgressionAndPrerequisitesAllowed(Type, Character);
		});
	}
}

bool RogueUpgradeSystemRules::IsUpgradeAllowedForProgression(ERogueUpgradeType Type, const ARogueCharacter* Character)
{
	return IsProgressionAndPrerequisitesAllowed(Type, Character);
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
		FilterProgressionAndPrerequisites(OutTypes, nullptr);
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

	// 升级选择与商店共用本入口，最终统一过滤永久解锁和本局武器前置。
	FilterProgressionAndPrerequisites(OutTypes, Character);
}
