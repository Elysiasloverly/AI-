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

const FRogueUpgradeOptionDefinition* RogueUpgradeSystemRules::FindUpgradeOptionDefinition(ERogueUpgradeType Type)
{
	static const FRogueUpgradeOptionDefinition Definitions[] =
	{
		{ ERogueUpgradeType::MaxHealth, TEXT("强化外壳"), TEXT("最大生命 +25，并立即回复 25 点生命。"), 25.0f },
		{ ERogueUpgradeType::MoveSpeed, TEXT("疾行伺服"), TEXT("移动速度 +90。"), 90.0f },
		{ ERogueUpgradeType::ArmorCapacity, TEXT("装甲扩容"), TEXT("最大护甲 +25，并立即恢复 25 点护甲。5 秒不受伤后会快速恢复。"), 25.0f },
		{ ERogueUpgradeType::DashCooldown, TEXT("相位推进"), TEXT("冲刺冷却 -0.5 秒，最低缩短到 0.5 秒。"), 0.5f },
		{ ERogueUpgradeType::AttackPower, TEXT("全武器增伤"), TEXT("子弹、镰刀、火箭、激光、地狱塔伤害全部 +8。"), 8.0f },
		{ ERogueUpgradeType::AttackSpeed, TEXT("联动加速"), TEXT("所有武器攻速同步提升。"), 0.12f },
		{ ERogueUpgradeType::PickupRadius, TEXT("磁吸核心"), TEXT("经验吸附半径 +180。"), 180.0f },
		{ ERogueUpgradeType::Recovery, TEXT("纳米修复"), TEXT("每秒生命恢复 +1.5。"), 1.5f },
		{ ERogueUpgradeType::AttackRange, TEXT("范围扩张"), TEXT("索敌、激光射程和爆炸范围同步扩大。"), 260.0f },
		{ ERogueUpgradeType::ProjectileSpeed, TEXT("弹道强化"), TEXT("子弹与火箭飞行速度提升。"), 450.0f },
		{ ERogueUpgradeType::ProjectileCount, TEXT("弹幕扩容"), TEXT("普通子弹数量 +1。"), 1.0f },
		{ ERogueUpgradeType::Armor, TEXT("偏转护甲"), TEXT("减伤 +8%。"), 0.08f },
		{ ERogueUpgradeType::ExperienceGain, TEXT("战斗心得"), TEXT("获得经验 +25%。"), 0.25f },
		{ ERogueUpgradeType::ScytheCount, TEXT("回旋镰刀"), TEXT("未持有时解锁回旋镰刀，已持有时镰刀数量 +1。"), 1.0f },
		{ ERogueUpgradeType::RocketCount, TEXT("火箭吊舱"), TEXT("未持有时解锁火箭炮，已持有时火箭数量 +1。"), 1.0f },
		{ ERogueUpgradeType::LaserCount, TEXT("激光阵列"), TEXT("未持有时解锁激光炮，已持有时激光数量 +1。"), 1.0f },
		{ ERogueUpgradeType::HellTowerCount, TEXT("地狱塔"), TEXT("未持有时解锁地狱塔，已持有时地狱塔数量 +1。"), 1.0f },
		{ ERogueUpgradeType::LaserRefraction, TEXT("棱镜折射"), TEXT("激光命中后会继续折射到最近敌人，每跳伤害减半，折射次数 +1。"), 1.0f }
	};

	for (const FRogueUpgradeOptionDefinition& Definition : Definitions)
	{
		if (Definition.Type == Type)
		{
			return &Definition;
		}
	}

	return nullptr;
}
