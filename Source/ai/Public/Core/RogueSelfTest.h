#pragma once

#include "CoreMinimal.h"

/**
 * 开发自检开关（默认全关）。
 *
 * 为什么要单独放一个头文件：这些开关要在**多个模块**（GameMode 的服务端自检、
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
	 *     客户端那行能打出来，就证明加成属性确实复制到位了（这是本次改造的关键前提）。 */
	AI_API bool IsWeaponUpgradeChainEnabled();
}
