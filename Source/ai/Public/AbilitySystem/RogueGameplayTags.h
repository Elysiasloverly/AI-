#pragma once

#include "NativeGameplayTags.h"
#include "Shared/RogueTypes.h"
#include "Combat/Visuals/RogueImpactEffect.h"

/** 项目原生 GameplayTag 声明，定义位于 Private/AbilitySystem/RogueGameplayTags.cpp。
 * 通过变量引用可检查 C++ 符号拼写；Tag 路径字符串的正确性仍需审查与运行验证。 */

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

	/** 敌人攻击（接触 / 远程两种共用这个族标签）。
	 *  AI（ARogueEnemy）只负责"决定要不要打"，真正的攻击由这个标签下的能力执行。 */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Ability_Enemy_Attack);

	// ===========================================================
	//  状态（State）—— 用来阻塞技能、驱动表现
	// ===========================================================

	/** 致死信号/死亡阻塞标签。玩家是否 Alive/Downed/Eliminated 以 LifeState 为准。
	 * 本标签自身不会清理升级；清理策略由生命周期流程决定。 */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Dead);

	/** 冲刺中。用来在冲刺期间阻塞其它移动类技能。 */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Dashing);

	/** 无敌状态，当前由复活保护 GE 授予；伤害入口查询此标签拒绝伤害。 */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Invulnerable);

	/** 刚受过伤（短时状态，由一个带时长的 GE 授予）。
	 *  用途：护甲在"受伤后 N 秒"内不回复。
	 *  原来这靠 ARogueCharacter::ArmorRechargeDelayRemaining 这个 float 计时器，
	 *  迁移后由这个 Tag 承担 —— 这样"是否在冷却中"变成可查询的状态，
	 *  而不是散落在角色类里的一个私有计时器。 */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_RecentlyDamaged);

	/** 濒死镜像标签，由 Infinite URogueDownedEffect 授予。
	 * 复活截止时间来自角色 LifeState，HUD 不读取本效果剩余时间。
	 * 复活或团灭时显式移除镜像效果；标签本身不会触发定时复活。 */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Downed);

	// ===========================================================
	//  冷却（Cooldown）—— 由 GameplayEffect 授予
	// ===========================================================

	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Dash);

	/** 目标型武器冷却 GE 的共享标签。每个能力按自己的句柄查询，避免共享标签互相阻塞。
	 * 冷却秒数由武器计算并通过 Data.Cooldown 传入，升级不直接修改既有 GE 的时长。 */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_WeaponAutoAttack);

	/** 敌人攻击的冷却（接触/远程两种攻击共用一个 GE 类，时长走 SetByCaller）。 */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_EnemyAttack);

	// ===========================================================
	//  SetByCaller 数值（Data）—— 让同一个 GE 资产承载不同数值
	// ===========================================================

	/** 伤害量。所有伤害 GE 都用它取量，这样一份 GE 资产能被六把武器复用。 */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Damage);

	/** 状态持续秒数，当前复活无敌效果用此 SetByCaller 键；永久升级不读取它。 */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Duration);

	/** 通用幅值（升级卡的 Magnitude）。 */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Magnitude);

	/** 武器开火的冷却时长（秒）—— 由 URogueWeaponCooldownEffect 按 SetByCaller 取值。
	 *  为什么不复用 Data_Duration：两者语义不同（一个是"状态持续多久"，
	 *  一个是"这把武器多久能再开一枪"），共用会让同一个 GE 上出现歧义。 */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Cooldown);

	// 来源分类随伤害 Spec 携带；当前不据此自动实现元素抗性、免疫或反馈分流。

	/** 接触伤害（敌人贴到玩家身上按距离结算的那种）。 */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage_Source_Contact);

	/** 单发/散射弹体（普通子弹、敌人弹体）。 */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage_Source_Projectile);

	/** 范围伤害（迫击炮爆炸、震荡柱弹幕等）。 */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage_Source_Area);

	/** 持续光束（激光、地狱塔）。 */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage_Source_Beam);

	/** 近身（环绕刀片）。 */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage_Source_Melee);

	// ===========================================================
	//  保留的效果族（当前 GE 尚未配置这些标签）
	// ===========================================================

	/** 保留的伤害族标签；当前 URogueDamageEffect 未配置此资产/授予标签。 */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Effect_Damage);

	/** 保留的升级族标签；当前升级类未配置此资产/授予标签，不能据此查询并清空升级。 */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Effect_Upgrade);

	/** 保留的属性升级族标签；当前目标属性由各升级 GE 子类固定，不依赖此标签分流。 */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(Effect_Upgrade_Attribute);

		// 升级目标属性由 RogueAttributeUpgradeEffect 子类配置；这里不再声明历史载荷标签。
		// GE 合栈至少要求定义相同，AggregateBySource 还要求来源 ASC 相同。

	// GameplayCue 标签用于表现路由。传播依赖载体相关性与调用方式，属性复制另行配置。
	// 预测去重依赖匹配预测键，瞬时 Cue 不是可靠事件日志。
	// 三个 Impact 标签对应三种风格；压扁比例通过参数传递，没有独立标签。

	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Impact_Spark);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Impact_Laser);
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_Impact_Explosion);

	/** 持续濒死表现，由 URogueDownedEffect 的 Cue 生命周期驱动。
	 * GE 生效触发 OnActive/WhileActive，移除触发 OnRemove；角色仍需在复活/团灭时移除 GE。 */
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(GameplayCue_State_Downed);

	/** 冲击风格映射到 Cue Tag；未知枚举同样回退到 Spark。 */
	AI_API FGameplayTag GetImpactCueTag(ERogueImpactVisualStyle Style);
}
