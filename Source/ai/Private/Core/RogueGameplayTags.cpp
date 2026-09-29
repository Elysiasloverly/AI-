#include "Core/RogueGameplayTags.h"

// UE_DEFINE_GAMEPLAY_TAG 只能出现在 .cpp 里（宏内有 static_assert 强制检查文件后缀），
// 头文件里必须用 UE_DECLARE_GAMEPLAY_TAG_EXTERN。
// 两者都放在同一个命名空间下，这样声明与定义才是同一个符号。

namespace RogueGameplayTags
{
	// ---- 技能 ----
	UE_DEFINE_GAMEPLAY_TAG(Ability_Dash,                 "Ability.Dash");
	UE_DEFINE_GAMEPLAY_TAG(Ability_WeaponAutoAttack,     "Ability.Weapon.AutoAttack");

	// ---- 状态 ----
	UE_DEFINE_GAMEPLAY_TAG(State_Dead,                   "State.Dead");
	UE_DEFINE_GAMEPLAY_TAG(State_Dashing,                "State.Dashing");
	UE_DEFINE_GAMEPLAY_TAG(State_Invulnerable,           "State.Invulnerable");
	UE_DEFINE_GAMEPLAY_TAG(State_RecentlyDamaged,        "State.RecentlyDamaged");

	// ---- 冷却 ----
	UE_DEFINE_GAMEPLAY_TAG(Cooldown_Dash,                "Cooldown.Dash");
	UE_DEFINE_GAMEPLAY_TAG(Cooldown_WeaponAutoAttack,    "Cooldown.Weapon.AutoAttack");

	// ---- SetByCaller 数值 ----
	UE_DEFINE_GAMEPLAY_TAG(Data_Damage,                  "Data.Damage");
	UE_DEFINE_GAMEPLAY_TAG(Data_Duration,                "Data.Duration");
	UE_DEFINE_GAMEPLAY_TAG(Data_Magnitude,               "Data.Magnitude");

	// ---- 伤害来源 ----
	UE_DEFINE_GAMEPLAY_TAG(Damage_Source_Contact,        "Damage.Source.Contact");
	UE_DEFINE_GAMEPLAY_TAG(Damage_Source_Projectile,     "Damage.Source.Projectile");
	UE_DEFINE_GAMEPLAY_TAG(Damage_Source_Area,           "Damage.Source.Area");
	UE_DEFINE_GAMEPLAY_TAG(Damage_Source_Beam,           "Damage.Source.Beam");
	UE_DEFINE_GAMEPLAY_TAG(Damage_Source_Melee,          "Damage.Source.Melee");

	// ---- 效果族 ----
	UE_DEFINE_GAMEPLAY_TAG(Effect_Damage,                "Effect.Damage");
	UE_DEFINE_GAMEPLAY_TAG(Effect_Upgrade,               "Effect.Upgrade");
	UE_DEFINE_GAMEPLAY_TAG(Effect_Upgrade_Attribute,     "Effect.Upgrade.Attribute");
	UE_DEFINE_GAMEPLAY_TAG(Effect_Upgrade_Weapon,        "Effect.Upgrade.Weapon");

	// ---- 升级的目标属性（载荷）----
	UE_DEFINE_GAMEPLAY_TAG(Upgrade_Attribute_MaxHealth,        "Upgrade.Attribute.MaxHealth");
	UE_DEFINE_GAMEPLAY_TAG(Upgrade_Attribute_MoveSpeed,        "Upgrade.Attribute.MoveSpeed");
	UE_DEFINE_GAMEPLAY_TAG(Upgrade_Attribute_MaxArmor,         "Upgrade.Attribute.MaxArmor");
	UE_DEFINE_GAMEPLAY_TAG(Upgrade_Attribute_DashCooldown,     "Upgrade.Attribute.DashCooldown");
	UE_DEFINE_GAMEPLAY_TAG(Upgrade_Attribute_PickupRadius,     "Upgrade.Attribute.PickupRadius");
	UE_DEFINE_GAMEPLAY_TAG(Upgrade_Attribute_HealthRegen,      "Upgrade.Attribute.HealthRegen");
	UE_DEFINE_GAMEPLAY_TAG(Upgrade_Attribute_DamageReduction,  "Upgrade.Attribute.DamageReduction");
	UE_DEFINE_GAMEPLAY_TAG(Upgrade_Attribute_ExperienceGain,   "Upgrade.Attribute.ExperienceGain");

	// ---- GameplayCue（表现层）----
	// 注意根必须是 "GameplayCue"，引擎的 GameplayCueManager 按这个根做路由。
	UE_DEFINE_GAMEPLAY_TAG(GameplayCue_Impact_Spark,      "GameplayCue.Impact.Spark");
	UE_DEFINE_GAMEPLAY_TAG(GameplayCue_Impact_Laser,      "GameplayCue.Impact.Laser");
	UE_DEFINE_GAMEPLAY_TAG(GameplayCue_Impact_Explosion,  "GameplayCue.Impact.Explosion");
}

FGameplayTag RogueGameplayTags::GetUpgradeAttributeTag(ERogueUpgradeType UpgradeType)
{
	switch (UpgradeType)
	{
	case ERogueUpgradeType::MaxHealth:       return Upgrade_Attribute_MaxHealth;
	case ERogueUpgradeType::MoveSpeed:       return Upgrade_Attribute_MoveSpeed;
	case ERogueUpgradeType::ArmorCapacity:   return Upgrade_Attribute_MaxArmor;
	case ERogueUpgradeType::DashCooldown:    return Upgrade_Attribute_DashCooldown;
	case ERogueUpgradeType::PickupRadius:    return Upgrade_Attribute_PickupRadius;
	case ERogueUpgradeType::Recovery:        return Upgrade_Attribute_HealthRegen;
	// 注意：升级枚举里的 "Armor" 语义是**伤害减免**（对应 DamageReductionPercent），
	// 而 ArmorCapacity 才是护甲上限 —— 这是原项目的命名，容易看错，这里显式标出来。
	case ERogueUpgradeType::Armor:           return Upgrade_Attribute_DamageReduction;
	case ERogueUpgradeType::ExperienceGain:  return Upgrade_Attribute_ExperienceGain;

	// 武器类升级与其余类型：不走属性 GE
	default:                                 return FGameplayTag();
	}
}

FGameplayTag RogueGameplayTags::GetImpactCueTag(ERogueImpactVisualStyle Style)
{
	switch (Style)
	{
	case ERogueImpactVisualStyle::Laser:     return GameplayCue_Impact_Laser;
	case ERogueImpactVisualStyle::Explosion: return GameplayCue_Impact_Explosion;
	case ERogueImpactVisualStyle::Spark:
	default:                                 return GameplayCue_Impact_Spark;
	}
}
