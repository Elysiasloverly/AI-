#include "AbilitySystem/GameplayEffects/RogueAttributeUpgradeEffect.h"

#include "AbilitySystem/RogueAttributeSet.h"
#include "AbilitySystem/RogueGameplayTags.h"

URogueAttributeUpgradeEffect::URogueAttributeUpgradeEffect()
{
	// 使用无周期 Infinite 保留可移除的升级贡献与层数。
	// Instant 不形成持续的活动效果栈；本效果的上限变化联动由角色属性委托处理。
	DurationPolicy = EGameplayEffectDurationType::Infinite;

	// 每个目标一份栈（升级来自商店/升级界面，没有有意义的"来源 Actor"，
	// 所以不该按来源分开叠）。
	StackingType = EGameplayEffectStackingType::AggregateByTarget;

	// 0 不限制层数；购买可选性和属性数值上限仍由升级规则/属性集单独限制。
	StackLimitCount = 0;
}

void URogueAttributeUpgradeEffect::ConfigureUpgrade(FGameplayAttribute TargetAttribute)
{
	// Additive 量级乘以栈数；同类升级的幅度需保持一致，才能得到 N 次购买的线性和。
	// 若未来每次传不同幅度，单栈并不逐次累加/保存每次的独立数值。
	FSetByCallerFloat MagnitudeFromCaller;
	MagnitudeFromCaller.DataTag = RogueGameplayTags::Data_Magnitude;

	FGameplayModifierInfo Modifier;
	Modifier.Attribute = TargetAttribute;
	Modifier.ModifierOp = EGameplayModOp::Additive;
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(MagnitudeFromCaller);
	Modifiers.Add(Modifier);
}

// ===================================================================
// 一种升级一个子类，固定目标属性；枚举到类的映射见本文件末尾。
// ===================================================================

// ---- 玩家核心属性（8 项）----
URogueUpgradeEffect_MaxHealth::URogueUpgradeEffect_MaxHealth()
{ ConfigureUpgrade(URogueAttributeSet::GetMaxHealthAttribute()); }

URogueUpgradeEffect_MoveSpeed::URogueUpgradeEffect_MoveSpeed()
{ ConfigureUpgrade(URogueAttributeSet::GetMoveSpeedAttribute()); }

URogueUpgradeEffect_DashCooldown::URogueUpgradeEffect_DashCooldown()
{ ConfigureUpgrade(URogueAttributeSet::GetDashCooldownDurationAttribute()); }

URogueUpgradeEffect_PickupRadius::URogueUpgradeEffect_PickupRadius()
{ ConfigureUpgrade(URogueAttributeSet::GetPickupRadiusAttribute()); }

// 注意：ERogueUpgradeType::Recovery 对应的是"每秒生命回复"，不是 ArmorRechargeRate
URogueUpgradeEffect_Recovery::URogueUpgradeEffect_Recovery()
{ ConfigureUpgrade(URogueAttributeSet::GetHealthRegenPerSecondAttribute()); }

// Armor 枚举对应伤害减免比例 DamageReductionPercent；ArmorCapacity 才是护甲上限。
URogueUpgradeEffect_Armor::URogueUpgradeEffect_Armor()
{ ConfigureUpgrade(URogueAttributeSet::GetDamageReductionPercentAttribute()); }

URogueUpgradeEffect_ArmorCapacity::URogueUpgradeEffect_ArmorCapacity()
{ ConfigureUpgrade(URogueAttributeSet::GetMaxArmorAttribute()); }

URogueUpgradeEffect_ExperienceGain::URogueUpgradeEffect_ExperienceGain()
{ ConfigureUpgrade(URogueAttributeSet::GetExperienceMultiplierAttribute()); }

// ---- 武器加成（14 项）----
URogueUpgradeEffect_AttackPower::URogueUpgradeEffect_AttackPower()
{ ConfigureUpgrade(URogueAttributeSet::GetAttackPowerBonusAttribute()); }

URogueUpgradeEffect_AttackSpeed::URogueUpgradeEffect_AttackSpeed()
{ ConfigureUpgrade(URogueAttributeSet::GetAttackSpeedBonusAttribute()); }

URogueUpgradeEffect_AttackRange::URogueUpgradeEffect_AttackRange()
{ ConfigureUpgrade(URogueAttributeSet::GetAttackRangeBonusAttribute()); }

URogueUpgradeEffect_ProjectileSpeed::URogueUpgradeEffect_ProjectileSpeed()
{ ConfigureUpgrade(URogueAttributeSet::GetProjectileSpeedBonusAttribute()); }

URogueUpgradeEffect_ProjectileCount::URogueUpgradeEffect_ProjectileCount()
{ ConfigureUpgrade(URogueAttributeSet::GetProjectileCountBonusAttribute()); }

URogueUpgradeEffect_ScytheCount::URogueUpgradeEffect_ScytheCount()
{ ConfigureUpgrade(URogueAttributeSet::GetScytheCountBonusAttribute()); }

URogueUpgradeEffect_RocketCount::URogueUpgradeEffect_RocketCount()
{ ConfigureUpgrade(URogueAttributeSet::GetRocketCountBonusAttribute()); }

URogueUpgradeEffect_LaserCount::URogueUpgradeEffect_LaserCount()
{ ConfigureUpgrade(URogueAttributeSet::GetLaserCountBonusAttribute()); }

URogueUpgradeEffect_HellTowerCount::URogueUpgradeEffect_HellTowerCount()
{ ConfigureUpgrade(URogueAttributeSet::GetHellTowerCountBonusAttribute()); }

URogueUpgradeEffect_LaserRefraction::URogueUpgradeEffect_LaserRefraction()
{ ConfigureUpgrade(URogueAttributeSet::GetLaserRefractionBonusAttribute()); }

URogueUpgradeEffect_MortarCount::URogueUpgradeEffect_MortarCount()
{ ConfigureUpgrade(URogueAttributeSet::GetMortarCountBonusAttribute()); }

URogueUpgradeEffect_MortarBlastRadius::URogueUpgradeEffect_MortarBlastRadius()
{ ConfigureUpgrade(URogueAttributeSet::GetMortarBlastRadiusBonusAttribute()); }

URogueUpgradeEffect_ScytheSize::URogueUpgradeEffect_ScytheSize()
{ ConfigureUpgrade(URogueAttributeSet::GetScytheSizeBonusAttribute()); }

URogueUpgradeEffect_ScytheLifesteal::URogueUpgradeEffect_ScytheLifesteal()
{ ConfigureUpgrade(URogueAttributeSet::GetScytheLifestealBonusAttribute()); }

URogueUpgradeEffect_ProjectileBurst::URogueUpgradeEffect_ProjectileBurst()
{ ConfigureUpgrade(URogueAttributeSet::GetProjectileBurstBonusAttribute()); }

URogueUpgradeEffect_RocketBlastRadius::URogueUpgradeEffect_RocketBlastRadius()
{ ConfigureUpgrade(URogueAttributeSet::GetRocketBlastRadiusBonusAttribute()); }

URogueUpgradeEffect_HellTowerDamageRamp::URogueUpgradeEffect_HellTowerDamageRamp()
{ ConfigureUpgrade(URogueAttributeSet::GetHellTowerDamageRampBonusAttribute()); }

// ===================================================================
//  枚举 → GE 子类
// ===================================================================

TSubclassOf<UGameplayEffect> RogueUpgradeEffectMap::GetEffectClass(ERogueUpgradeType UpgradeType)
{
	switch (UpgradeType)
	{
	// ---- 玩家核心属性 ----
	case ERogueUpgradeType::MaxHealth:         return URogueUpgradeEffect_MaxHealth::StaticClass();
	case ERogueUpgradeType::MoveSpeed:         return URogueUpgradeEffect_MoveSpeed::StaticClass();
	case ERogueUpgradeType::DashCooldown:      return URogueUpgradeEffect_DashCooldown::StaticClass();
	case ERogueUpgradeType::PickupRadius:      return URogueUpgradeEffect_PickupRadius::StaticClass();
	case ERogueUpgradeType::Recovery:          return URogueUpgradeEffect_Recovery::StaticClass();
	case ERogueUpgradeType::Armor:             return URogueUpgradeEffect_Armor::StaticClass();
	case ERogueUpgradeType::ArmorCapacity:     return URogueUpgradeEffect_ArmorCapacity::StaticClass();
	case ERogueUpgradeType::ExperienceGain:    return URogueUpgradeEffect_ExperienceGain::StaticClass();

	// ---- 武器加成 ----
	case ERogueUpgradeType::AttackPower:         return URogueUpgradeEffect_AttackPower::StaticClass();
	case ERogueUpgradeType::AttackSpeed:         return URogueUpgradeEffect_AttackSpeed::StaticClass();
	case ERogueUpgradeType::AttackRange:         return URogueUpgradeEffect_AttackRange::StaticClass();
	case ERogueUpgradeType::ProjectileSpeed:     return URogueUpgradeEffect_ProjectileSpeed::StaticClass();
	case ERogueUpgradeType::ProjectileCount:     return URogueUpgradeEffect_ProjectileCount::StaticClass();
	case ERogueUpgradeType::ScytheCount:         return URogueUpgradeEffect_ScytheCount::StaticClass();
	case ERogueUpgradeType::RocketCount:         return URogueUpgradeEffect_RocketCount::StaticClass();
	case ERogueUpgradeType::LaserCount:          return URogueUpgradeEffect_LaserCount::StaticClass();
	case ERogueUpgradeType::HellTowerCount:      return URogueUpgradeEffect_HellTowerCount::StaticClass();
	case ERogueUpgradeType::LaserRefraction:     return URogueUpgradeEffect_LaserRefraction::StaticClass();
	case ERogueUpgradeType::MortarCount:         return URogueUpgradeEffect_MortarCount::StaticClass();
	case ERogueUpgradeType::MortarBlastRadius:   return URogueUpgradeEffect_MortarBlastRadius::StaticClass();
	case ERogueUpgradeType::ProjectileBurst: return URogueUpgradeEffect_ProjectileBurst::StaticClass();
	case ERogueUpgradeType::RocketBlastRadius: return URogueUpgradeEffect_RocketBlastRadius::StaticClass();
	case ERogueUpgradeType::HellTowerDamageRamp: return URogueUpgradeEffect_HellTowerDamageRamp::StaticClass();
	case ERogueUpgradeType::ScytheSize:          return URogueUpgradeEffect_ScytheSize::StaticClass();
	case ERogueUpgradeType::ScytheLifesteal:     return URogueUpgradeEffect_ScytheLifesteal::StaticClass();
	}

	// 走到这里说明枚举加了但映射忘了加。**不要静默 return nullptr 了事** ——
	// 调用方会据此打 Warning，否则表现是"卡牌能抽到但没效果"。
	return nullptr;
}
