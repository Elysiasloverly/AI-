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

	/** 敌人攻击（接触 / 远程 / 地面波三种共用这个族标签）。
	 *  AI（ARogueEnemy）只负责"决定要不要打"，真正的攻击由这个标签下的能力执行。 */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Enemy_Attack);

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

	/** 敌人攻击的冷却（三种攻击共用一个 GE 类，时长走 SetByCaller）。 */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_EnemyAttack);

	// ===========================================================
	//  SetByCaller 数值（Data）—— 让同一个 GE 资产承载不同数值
	// ===========================================================

	/** 伤害量。所有伤害 GE 都用它取量，这样一份 GE 资产能被六把武器复用。 */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Damage);

	/** 持续时间（升级卡的时长、状态的持续时间等）。 */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Duration);

	/** 通用幅值（升级卡的 Magnitude）。 */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Magnitude);

	/** 武器开火的冷却时长（秒）—— 由 URogueWeaponCooldownEffect 按 SetByCaller 取值。
	 *  为什么不复用 Data_Duration：两者语义不同（一个是"状态持续多久"，
	 *  一个是"这把武器多久能再开一枪"），共用会让同一个 GE 上出现歧义。 */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Cooldown);

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
	 *  自研系统里这 8 类是写进 UpgradeGroup 再整表重算，迁移后是挂这个 Tag 的 GE。
	 *  这也是**所有升级 GE 实际的族标签**（武器类升级现在也共用同一个 GE 类，见下）。 */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Effect_Upgrade_Attribute);

	/** 武器类升级的族标签。
	 *  ⚠️ 当前实现里它**没有被授予**：武器升级已经和属性升级合并到同一个 GE 类
	 *  （`URogueAttributeUpgradeEffect` + `URogueUpgradeExecution`），族标签统一是
	 *  Effect.Upgrade.Attribute，批量清除用的是父标签 Effect.Upgrade。
	 *  保留这个 Tag 是为了将来真需要"单独清除/抑制武器类升级"时可以直接用；
	 *  如果一直用不上，可以连同声明一起删掉。 */
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

	// ===========================================================
	//  升级的"武器加成属性"（Upgrade.Weapon）—— 同样是 GE 载荷
	// ===========================================================
	//  历史：这 12 类曾经**不走 GAS** —— 升级时改武器 Actor 上的配置结构体，
	//  再用一条 NetMulticast 让各端重放一遍（"各端自己把同一个数值加一遍"）。
	//  现在它们和属性升级走同一条路：改玩家 ASC 上 URogueAttributeSet 里的加成属性，
	//  武器开火时读"数据表基础值 + 属性加成"。
	//  好处：升级只在一处（服务端）结算、自动随属性复制、不需要广播重放、顺序问题自然消失。

	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Upgrade_Weapon_AttackPower);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Upgrade_Weapon_AttackSpeed);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Upgrade_Weapon_AttackRange);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Upgrade_Weapon_ProjectileSpeed);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Upgrade_Weapon_ProjectileCount);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Upgrade_Weapon_ScytheCount);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Upgrade_Weapon_RocketCount);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Upgrade_Weapon_LaserCount);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Upgrade_Weapon_HellTowerCount);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Upgrade_Weapon_LaserRefraction);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Upgrade_Weapon_MortarCount);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Upgrade_Weapon_MortarBlastRadius);

	/** 升级类型 → 目标属性载荷 Tag（属性类与武器类都走这里）。
	 *  放在这里而不是散在调用点，是为了让"哪个升级改哪个属性"一眼可见。
	 *  ⚠️ 返回无效 Tag = 该升级不通过 GE 落地，调用方应当报错而不是静默忽略。 */
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
