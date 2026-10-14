#include "AbilitySystem/GameplayEffects/RogueAttributeUpgradeEffect.h"

#include "AbilitySystem/RogueAttributeSet.h"
#include "AbilitySystem/RogueGameplayTags.h"

URogueAttributeUpgradeEffect::URogueAttributeUpgradeEffect()
{
	// **Infinite**：Stacking 必须驻留 —— Instant 的 GE 不会被保留，也就无法叠加。
	// 代价（见头文件）：只能改 CurrentValue，所以"升级最大生命/护甲上限时等量回复"
	// 那条联动已经搬到 ARogueCharacter 的属性变化委托上。
	DurationPolicy = EGameplayEffectDurationType::Infinite;

	// 每个目标一份栈（升级来自商店/升级界面，没有有意义的"来源 Actor"，
	// 所以不该按来源分开叠）。
	StackingType = EGameplayEffectStackingType::AggregateByTarget;

	// 0 = **不限制**层数（引擎只在 StackLimitCount > 0 时才钳制）——
	// 保持改造前"同类型可无限购买"的行为。
	// 想给某张卡设"最多买 N 次"，把它填成正整数即可，这是改造前做不到的。
	StackLimitCount = 0;
}

void URogueAttributeUpgradeEffect::ConfigureUpgrade(FGameplayAttribute TargetAttribute)
{
	// 量级走 SetByCaller：数值由施加方给出（冲刺冷却是负向升级，调用方传负数）。
	//
	// 关键：引擎会把它**乘以层数**
	//（GameplayEffectUtilities::ComputeStackedModifierMagnitude，Additive 的 OperationBias 为 0），
	// 所以买 N 次得到的增量 = N × 单次数值 —— 与改造前 Instant 连加 N 次逐项相等。
	FSetByCallerFloat MagnitudeFromCaller;
	MagnitudeFromCaller.DataTag = RogueGameplayTags::Data_Magnitude;

	FGameplayModifierInfo Modifier;
	Modifier.Attribute = TargetAttribute;
	Modifier.ModifierOp = EGameplayModOp::Additive;
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(MagnitudeFromCaller);
	Modifiers.Add(Modifier);
}

// ===================================================================
//  20 个子类 —— 每个只做一件事：把目标属性烤进去。
//  属性访问器与改造前 URogueUpgradeExecution 的映射表**逐项对应**，
//  迁移时逐个照搬，不要顺手改。
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

// ⚠️ 升级枚举里的 `Armor` 语义是**伤害减免**（DamageReductionPercent），
//    而 `ArmorCapacity` 才是护甲上限 —— 这是原项目的命名，极容易看错。
//
//    这条说明是从已删除的 `RogueUpgradeExecution` 里**抢救出来的**：
//    它原本是那张"载荷 Tag → 属性"映射表上唯一的提示，随死代码一起删掉后，
//    下一个人几乎必然会把它错映射成"护甲回复速率"（我自己第一次就映射错了，
//    而自检不覆盖这张卡，所以不会报错 —— 是删代码时才读到原注释发现的）。
URogueUpgradeEffect_Armor::URogueUpgradeEffect_Armor()
{ ConfigureUpgrade(URogueAttributeSet::GetDamageReductionPercentAttribute()); }

URogueUpgradeEffect_ArmorCapacity::URogueUpgradeEffect_ArmorCapacity()
{ ConfigureUpgrade(URogueAttributeSet::GetMaxArmorAttribute()); }

URogueUpgradeEffect_ExperienceGain::URogueUpgradeEffect_ExperienceGain()
{ ConfigureUpgrade(URogueAttributeSet::GetExperienceMultiplierAttribute()); }

// ---- 武器加成（12 项）----
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
	}

	// 走到这里说明枚举加了但映射忘了加。**不要静默 return nullptr 了事** ——
	// 调用方会据此打 Warning，否则表现是"卡牌能抽到但没效果"。
	return nullptr;
}
