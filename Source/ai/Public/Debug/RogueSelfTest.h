#pragma once

#include "CoreMinimal.h"

/**
 * 开发自检开关（默认全关）。
 *
 * 公共查询接口供同一 ai 模块的不同职责代码（GameMode 的服务端自检、
 * Character 的每台机器自检）里读同一份状态，而 CVar 不能散在各个 .cpp 里各定义一份
 * （那会变成两个互不相干的变量）。
 *
 * 用法（无头验证）：
 *   UnrealEditor.exe <uproject> <map>?listen -game -nullrhi
 *     -ExecCmds="Rogue.SelfTest.WeaponUpgradeChain 1" -log
 */
namespace RogueSelfTest
{
	/** 是否开启"武器升级链路"自检：
	 *   · 服务端（GameMode）：施加一组升级，打印加成属性与每把武器的生效数值；
	 *   · 每台机器（本地角色）：打印本机角色读到的加成属性 ——
	 *     需匹配服务端同一玩家、同一升级阶段的数值；单有日志不能证明复制成功。 */
	AI_API bool IsWeaponUpgradeChainEnabled();

	/** 压测敌人数（0 = 关）。武器链路自检启用后、玩家就绪时刷一批高生命且接触伤害为零的敌人。
	 *  实际数量受池获取结果影响；用于构造负载，仍需记录活跃敌人数与帧时。 */
	AI_API int32 GetEnemyStressCount();

	/** 压测对照：是否**不授予**敌人攻击能力。
	 *  对照会同时改变攻击执行、目标检测和表现负载，帧时差不能直接归因于 GAS 框架本身。 */
	AI_API bool AreEnemyAttackAbilitiesDisabled();
}
