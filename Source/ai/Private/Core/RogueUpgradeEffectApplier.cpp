#include "Core/RogueUpgradeEffectApplier.h"

#include "Player/RogueCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"

void FRogueUpgradeEffectApplier::ApplyToCharacter(ARogueCharacter& Character, const FRogueUpgradeOption& Upgrade)
{
	switch (Upgrade.Type)
	{
	case ERogueUpgradeType::MaxHealth:
		Character.MaxHealth += Upgrade.Magnitude;
		Character.CurrentHealth = FMath::Min(Character.CurrentHealth + Upgrade.Magnitude, Character.MaxHealth);
		break;
	case ERogueUpgradeType::MoveSpeed:
		Character.MoveSpeed += Upgrade.Magnitude;
		Character.GetCharacterMovement()->MaxWalkSpeed = Character.MoveSpeed;
		break;
	case ERogueUpgradeType::ArmorCapacity:
		Character.MaxArmor += Upgrade.Magnitude;
		Character.CurrentArmor = FMath::Min(Character.MaxArmor, Character.CurrentArmor + Upgrade.Magnitude);
		Character.ArmorRechargeDelayRemaining = 0.0f;
		break;
	case ERogueUpgradeType::DashCooldown:
		Character.DashCooldownDuration = FMath::Max(Character.DashMinCooldown, Character.DashCooldownDuration - Upgrade.Magnitude);
		Character.DashCooldownRemaining = FMath::Min(Character.DashCooldownRemaining, Character.DashCooldownDuration);
		break;
	case ERogueUpgradeType::AttackPower:
		Character.ApplySharedWeaponDamageBonus(Upgrade.Magnitude);
		break;
	case ERogueUpgradeType::AttackSpeed:
		Character.ApplySharedWeaponSpeedBonus(Upgrade.Magnitude);
		break;
	case ERogueUpgradeType::PickupRadius:
		Character.PickupRadius += Upgrade.Magnitude;
		break;
	case ERogueUpgradeType::Recovery:
		Character.HealthRegenPerSecond += Upgrade.Magnitude;
		break;
	case ERogueUpgradeType::AttackRange:
		Character.ApplySharedWeaponRangeBonus(Upgrade.Magnitude);
		break;
	case ERogueUpgradeType::ProjectileSpeed:
		// 弹速升级：分发给武器自行处理
		Character.DispatchWeaponUpgrade(Upgrade.Type, Upgrade.Magnitude);
		break;
	case ERogueUpgradeType::Armor:
		Character.DamageReductionPercent = FMath::Clamp(Character.DamageReductionPercent + Upgrade.Magnitude, 0.0f, 0.65f);
		break;
	case ERogueUpgradeType::ExperienceGain:
		Character.ExperienceMultiplier += Upgrade.Magnitude;
		break;
	// 所有武器专属升级统一分发
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
