#include "AbilitySystem/RogueGameplayTags.h"

// UE_DEFINE_GAMEPLAY_TAG 只能出现在 .cpp 里（宏内有 static_assert 强制检查文件后缀），
// 头文件里必须用 UE_DECLARE_GAMEPLAY_TAG_EXTERN。
// 两者都放在同一个命名空间下，这样声明与定义才是同一个符号。

namespace RogueGameplayTags
{
	// ---- 技能 ----
	UE_DEFINE_GAMEPLAY_TAG(Ability_Dash,                 "Ability.Dash");
	UE_DEFINE_GAMEPLAY_TAG(Ability_WeaponAutoAttack,     "Ability.Weapon.AutoAttack");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Enemy_Attack,         "Ability.Enemy.Attack");

	// ---- 状态 ----
	UE_DEFINE_GAMEPLAY_TAG(State_Dead,                   "State.Dead");
	UE_DEFINE_GAMEPLAY_TAG(State_Dashing,                "State.Dashing");
	UE_DEFINE_GAMEPLAY_TAG(State_Invulnerable,           "State.Invulnerable");
	UE_DEFINE_GAMEPLAY_TAG(State_RecentlyDamaged,        "State.RecentlyDamaged");
	UE_DEFINE_GAMEPLAY_TAG(State_Downed,                 "State.Downed");

	// ---- 冷却 ----
	UE_DEFINE_GAMEPLAY_TAG(Cooldown_Dash,                "Cooldown.Dash");
	UE_DEFINE_GAMEPLAY_TAG(Cooldown_WeaponAutoAttack,    "Cooldown.Weapon.AutoAttack");
	UE_DEFINE_GAMEPLAY_TAG(Cooldown_EnemyAttack,         "Cooldown.Enemy.Attack");

	// ---- SetByCaller 数值 ----
	UE_DEFINE_GAMEPLAY_TAG(Data_Damage,                  "Data.Damage");
	UE_DEFINE_GAMEPLAY_TAG(Data_Duration,                "Data.Duration");
	UE_DEFINE_GAMEPLAY_TAG(Data_Magnitude,               "Data.Magnitude");
	UE_DEFINE_GAMEPLAY_TAG(Data_Cooldown,                "Data.Cooldown");

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

	// 升级目标属性固定在各 GE 子类；不再使用历史 Upgrade.Attribute/Weapon 载荷标签。

	// ---- GameplayCue（表现层）----
	// 注意根必须是 "GameplayCue"，引擎的 GameplayCueManager 按这个根做路由。
	UE_DEFINE_GAMEPLAY_TAG(GameplayCue_Impact_Spark,      "GameplayCue.Impact.Spark");
	UE_DEFINE_GAMEPLAY_TAG(GameplayCue_Impact_Laser,      "GameplayCue.Impact.Laser");
	UE_DEFINE_GAMEPLAY_TAG(GameplayCue_Impact_Explosion,  "GameplayCue.Impact.Explosion");
	// 倒地表现：**Actor 型** cue，由 URogueDownedEffect 的 GameplayCues 数组驱动
	//（GE 生效 → OnActive，GE 移除 → OnRemove），见 RogueGameplayCue_Downed.h。
	UE_DEFINE_GAMEPLAY_TAG(GameplayCue_State_Downed,      "GameplayCue.State.Downed");
}

// 升级枚举与目标属性映射位于 GameplayEffects/RogueAttributeUpgradeEffect.cpp。

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
