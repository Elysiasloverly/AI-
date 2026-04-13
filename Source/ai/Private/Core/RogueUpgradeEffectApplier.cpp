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
		Character.ProjectileWeapon.Speed += Upgrade.Magnitude;
		Character.RocketWeapon.Speed += Upgrade.Magnitude * 0.75f;
		break;
	case ERogueUpgradeType::ProjectileCount:
		Character.ProjectileWeapon.Count = FMath::Max(1, Character.ProjectileWeapon.Count + FMath::RoundToInt(Upgrade.Magnitude));
		break;
	case ERogueUpgradeType::Armor:
		Character.DamageReductionPercent = FMath::Clamp(Character.DamageReductionPercent + Upgrade.Magnitude, 0.0f, 0.65f);
		break;
	case ERogueUpgradeType::ExperienceGain:
		Character.ExperienceMultiplier += Upgrade.Magnitude;
		break;
	case ERogueUpgradeType::ScytheCount:
		Character.ScytheWeapon.Count = FMath::Max(1, Character.ScytheWeapon.Count + FMath::RoundToInt(Upgrade.Magnitude));
		break;
	case ERogueUpgradeType::RocketCount:
		Character.RocketWeapon.Count = FMath::Max(1, Character.RocketWeapon.Count + FMath::RoundToInt(Upgrade.Magnitude));
		break;
	case ERogueUpgradeType::LaserCount:
		Character.LaserWeapon.Count = FMath::Max(1, Character.LaserWeapon.Count + FMath::RoundToInt(Upgrade.Magnitude));
		break;
	case ERogueUpgradeType::HellTowerCount:
		Character.HellTowerWeapon.Count = FMath::Max(1, Character.HellTowerWeapon.Count + FMath::RoundToInt(Upgrade.Magnitude));
		break;
	case ERogueUpgradeType::LaserRefraction:
		Character.LaserWeapon.RefractionCount = FMath::Clamp(Character.LaserWeapon.RefractionCount + FMath::RoundToInt(Upgrade.Magnitude), 0, 8);
		break;
	default:
		break;
	}
}
