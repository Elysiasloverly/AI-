#pragma once

#include "NativeGameplayTags.h"
#include "Core/RogueTypes.h"
#include "Combat/RogueImpactEffect.h"

/**
 * 项目原生 GameplayTag 声明。
 *
 * 为什么用原生 Tag 而不是 .ini 里配：
 *   · 编译期就能拿到 FNativeGameplayTag 变量，C++ 侧不用写字符串、不怕拼错；
 *   · 不依赖 DefaultGameplayTags.ini 的加载时机（ini 里的 Tag 在资产加载早期可能还没注册）；
 *   · 重命名时编译器会报错，而字符串不会。
 *
 * 命名约定（沿用 Epic Lyra 的风格，用下划线代替点）：
 *   UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Dash)  ↔  "Ability.Dash"
 *
 * 注意：本文件只做"声明"，实际定义在 Private/Core/RogueGameplayTags.cpp。
 */

namespace RogueGameplayTags
{
	// ===========================================================
	//  技能（Ability）
	// ===========================================================

	/** 玩家冲刺。目前唯一的"主动"技能，也是第一个迁移到 GameplayAbility 的入口。 */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Dash);

	/** 武器自动攻击（被动型技能）。
	 *  六把武器都是 tick 驱动自动开火，没有"按下激活"这个动作，所以做成被动技能：
	 *  用这个 Tag 标识"它属于自动攻击"，后续按武器族挂子 Tag。 */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_WeaponAutoAttack);

	// ===========================================================
	//  状态（State）—— 用来阻塞技能、驱动表现
	// ===========================================================

	/** 死亡。原来靠 ARogueEnemy::IsDead() / ARogueCharacter::IsDead() 两个 bool，
	 *  迁移后统一用这个 Tag 查询（死亡还要顺带清掉所有升级效果）。 */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Dead);

	/** 冲刺中。用来在冲刺期间阻塞其它移动类技能。 */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Dashing);

	/** 无敌。冲刺无敌帧 / Boss 登场保护等都用它。 */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Invulnerable);

	/** 刚受过伤（短时状态，由一个带时长的 GE 授予）。
	 *  用途：护甲在"受伤后 N 秒"内不回复。
	 *  原来这靠 ARogueCharacter::ArmorRechargeDelayRemaining 这个 float 计时器，
	 *  迁移后由这个 Tag 承担 —— 这样"是否在冷却中"变成可查询的状态，
	 *  而不是散落在角色类里的一个私有计时器。 */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_RecentlyDamaged);

	// ===========================================================
	//  冷却（Cooldown）—— 由 GameplayEffect 授予
	// ===========================================================

	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Dash);

	/** 武器自动攻击的冷却。六把武器目前各有自己的 Config.Cooldown 计时器，
	 *  迁移后统一由这个 Tag 承担，升级改冷却就变成改 GE 的时长。 */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_WeaponAutoAttack);

	// ===========================================================
	//  SetByCaller 数值（Data）—— 让同一个 GE 资产承载不同数值
	// ===========================================================

	/** 伤害量。所有伤害 GE 都用它取量，这样一份 GE 资产能被六把武器复用。 */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Damage);

	/** 持续时间（升级卡的时长、状态的持续时间等）。 */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Duration);

	/** 通用幅值（升级卡的 Magnitude）。 */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Magnitude);

	// ===========================================================
	//  伤害来源（Damage.Source）—— 用于反馈分流与未来的免疫/减伤
	// ===========================================================
	//  这个轴是"怎么打出去的"，不是"什么元素"——本项目没有元素/抗性体系。
	//  它的直接用途：伤害飘字样式、命中特效、以及"某类来源免疫"这类需求。

	/** 接触伤害（敌人贴到玩家身上按距离结算的那种）。 */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage_Source_Contact);

	/** 单发/散射弹体（普通子弹、敌人弹体）。 */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage_Source_Projectile);

	/** 范围伤害（迫击炮爆炸、震荡柱地面波）。 */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage_Source_Area);

	/** 持续光束（激光、地狱塔）。 */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage_Source_Beam);

	/** 近身（环绕刀片）。 */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage_Source_Melee);

	// ===========================================================
	//  效果族（Effect）—— 用于批量查询/移除
	// ===========================================================

	/** 所有伤害 GE 的父 Tag。 */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Effect_Damage);

	/** 所有升级 GE 的父 Tag。
	 *  死亡重开时要按它一次性清空全部升级：RemoveActiveEffectsWithGrantedTags(Effect.Upgrade)。 */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Effect_Upgrade);

	/** 走属性系统的升级（最大生命/移速/护甲/冲刺冷却/拾取范围/回复/减伤/经验）。
	 *  自研系统里这 8 类是写进 UpgradeGroup 再整表重算，迁移后是挂这个 Tag 的 GE。 */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Effect_Upgrade_Attribute);

	/** 走武器分发的升级（攻击力/攻速/攻击范围/弹体速度/各武器数量）。 */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Effect_Upgrade_Weapon);

	// ===========================================================
	//  升级的"目标属性"（Upgrade.Attribute）—— 作为 GE 的载荷
	// ===========================================================
	//  与上面的 Effect_Upgrade_Attribute 区别：
	//    Effect.Upgrade.Attribute      —— GE 的**族标签**（"这是一个属性类升级"），用于批量查询/移除
	//    Upgrade.Attribute.*           —— **载荷**，告诉那个通用的升级 GE "这次要改哪个属性"
	//  这样只需要**一个** GE 类 + 一个 ExecutionCalculation 就能承载全部 8 种数值升级，
	//  不必为每个属性造一个 GE 资产。

	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Upgrade_Attribute_MaxHealth);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Upgrade_Attribute_MoveSpeed);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Upgrade_Attribute_MaxArmor);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Upgrade_Attribute_DashCooldown);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Upgrade_Attribute_PickupRadius);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Upgrade_Attribute_HealthRegen);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Upgrade_Attribute_DamageReduction);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Upgrade_Attribute_ExperienceGain);

	/** 升级类型 → 目标属性载荷 Tag。
	 *  只有 8 种"数值类"升级有映射；武器类升级返回空 Tag（它们不走 GE，直接分发给武器）。
	 *  放在这里而不是散在调用点，是为了让"哪些升级走属性、哪些走武器"一眼可见。 */
	AI_API FGameplayTag GetUpgradeAttributeTag(ERogueUpgradeType UpgradeType);

	// ===========================================================
	//  GameplayCue（表现层）—— 由 ASC 自动多播，项目不再手搓 RPC
	// ===========================================================
	//  为什么走 GameplayCue 而不是自己写 Multicast：
	//    · 多播/相关性/预测去重都由 AbilitySystem 负责，不用每加一种特效就加一条 RPC；
	//    · 敌人 ASC 是 Minimal 复制模式，而 Minimal 模式**天生就复制 GameplayCue**
	//      （这正是它的设计目的：只同步"属性 + 表现事件"，不同步 GE 细节）；
	//    · 特效与"谁造成的"绑定在一起（Instigator / EffectCauser），日后做击杀回放、
	//      命中反馈分流都是从参数里读，不需要再传一堆上下文。
	//
	//  子 Tag 直接对应 ERogueImpactVisualStyle 的三个风格 + 一个压扁变体。

	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Impact_Spark);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Impact_Laser);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Impact_Explosion);

	/** 冲击风格 → Cue Tag。无对应风格时返回空 Tag。 */
	AI_API FGameplayTag GetImpactCueTag(ERogueImpactVisualStyle Style);
}
