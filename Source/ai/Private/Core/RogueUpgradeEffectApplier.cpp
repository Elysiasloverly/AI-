#include "Core/RogueUpgradeEffectApplier.h"

#include "Player/RogueCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Player/RoguePlayerAttributeSet.h"

void FRogueUpgradeEffectApplier::ApplyToCharacter(ARogueCharacter& Character, const FRogueUpgradeOption& Upgrade)
{
	FRoguePlayerAttributeSystem* AttrSys = Character.GetAttributeSystem();

	switch (Upgrade.Type)
	{
	case ERogueUpgradeType::MaxHealth:
		Character.MaxHealth += Upgrade.Magnitude;
		Character.CurrentHealth = FMath::Min(Character.CurrentHealth + Upgrade.Magnitude, Character.MaxHealth);
		if (AttrSys) AttrSys->GetUpgradeGroup()->Add(Upgrade.Magnitude, EAttributesOperation::Additive, &FRoguePlayerAttributeSet::MaxHealth);
		break;
	case ERogueUpgradeType::MoveSpeed:
		Character.MoveSpeed += Upgrade.Magnitude;
		if (AttrSys) AttrSys->GetUpgradeGroup()->Add(Upgrade.Magnitude, EAttributesOperation::Additive, &FRoguePlayerAttributeSet::MoveSpeed);
		break;
	case ERogueUpgradeType::ArmorCapacity:
		Character.MaxArmor += Upgrade.Magnitude;
		Character.CurrentArmor = FMath::Min(Character.MaxArmor, Character.CurrentArmor + Upgrade.Magnitude);
		Character.ArmorRechargeDelayRemaining = 0.0f;
		if (AttrSys) AttrSys->GetUpgradeGroup()->Add(Upgrade.Magnitude, EAttributesOperation::Additive, &FRoguePlayerAttributeSet::MaxArmor);
		break;
	case ERogueUpgradeType::DashCooldown:
		Character.DashCooldownDuration = FMath::Max(Character.DashMinCooldown, Character.DashCooldownDuration - Upgrade.Magnitude);
		Character.DashCooldownRemaining = FMath::Min(Character.DashCooldownRemaining, Character.DashCooldownDuration);
		if (AttrSys) AttrSys->GetUpgradeGroup()->Add(-Upgrade.Magnitude, EAttributesOperation::Additive, &FRoguePlayerAttributeSet::DashCooldownDuration);
		break;
	case ERogueUpgradeType::AttackPower:
		Character.ApplySharedWeaponDamageBonus(Upgrade.Magnitude);
		break;
	case ERogueUpgradeType::AttackSpeed:
		Character.ApplySharedWeaponSpeedBonus(Upgrade.Magnitude);
		break;
	case ERogueUpgradeType::PickupRadius:
		Character.PickupRadius += Upgrade.Magnitude;
		if (AttrSys) AttrSys->GetUpgradeGroup()->Add(Upgrade.Magnitude, EAttributesOperation::Additive, &FRoguePlayerAttributeSet::PickupRadius);
		break;
	case ERogueUpgradeType::Recovery:
		Character.HealthRegenPerSecond += Upgrade.Magnitude;
		if (AttrSys) AttrSys->GetUpgradeGroup()->Add(Upgrade.Magnitude, EAttributesOperation::Additive, &FRoguePlayerAttributeSet::HealthRegenPerSecond);
		break;
	case ERogueUpgradeType::AttackRange:
		Character.ApplySharedWeaponRangeBonus(Upgrade.Magnitude);
		break;
	case ERogueUpgradeType::ProjectileSpeed:
		Character.DispatchWeaponUpgrade(Upgrade.Type, Upgrade.Magnitude);
		break;
	case ERogueUpgradeType::Armor:
		Character.DamageReductionPercent = FMath::Clamp(Character.DamageReductionPercent + Upgrade.Magnitude, 0.0f, 0.65f);
		if (AttrSys) AttrSys->GetUpgradeGroup()->Add(Upgrade.Magnitude, EAttributesOperation::Additive, &FRoguePlayerAttributeSet::DamageReductionPercent);
		break;
	case ERogueUpgradeType::ExperienceGain:
		Character.ExperienceMultiplier += Upgrade.Magnitude;
		if (AttrSys) AttrSys->GetUpgradeGroup()->Add(Upgrade.Magnitude, EAttributesOperation::Additive, &FRoguePlayerAttributeSet::ExperienceMultiplier);
		break;
	// 武器专属升级分发
	case ERogueUpgradeType::ProjectileCount:
	case ERogueUpgradeType::ScytheCount:
	case ERogueUpgradeType::RocketCount:
	case ERogueUpgradeType::LaserCount:
	case ERogueUpgradeType::HellTowerCount:
	case ERogueUpgradeType::LaserRefraction:
	case ERogueUpgradeType::MortarCount:
	case ERogueUpgradeType::MortarBlastRadius:
		Character.DispatchWeaponUpgrade(Upgrade.Type, Upgrade.Magnitude);
		break;
	default:
		break;
	}
}
