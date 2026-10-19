#include "Debug/RogueSelfTest.h"

#include "HAL/IConsoleManager.h"

namespace
{
	/** 开发自检：武器升级链路（默认关）。见 RogueSelfTest.h。 */
	static TAutoConsoleVariable<int32> CVarSelfTestWeaponUpgradeChain(
		TEXT("Rogue.SelfTest.WeaponUpgradeChain"), 0,
		TEXT("1 = 跑一次『升级 → GAS 加成属性 → 武器生效值』自检（服务端施加升级，各端打印读到的加成）"),
		ECVF_Default);

	/** 压测敌人数开关；生成时机和成功数量见 RunWeaponUpgradeChainSelfTest/SpawnStressEnemies。
	 *  高生命、零接触伤害用于减少负载波动，不保证常规波次或其他伤害不会导致玩家倒地。 */
	static TAutoConsoleVariable<int32> CVarSelfTestEnemyStress(
		TEXT("Rogue.SelfTest.EnemyStress"), 0,
		TEXT("N > 0 = 自检完成时刷 N 只压测敌人（很硬、伤害 0），用来量 200+ 敌人时的帧时"),
		ECVF_Default);

	/** 压测对照开关：1 = 不授予敌人攻击能力；结果包含攻击整条路径的负载差异。 */
	static TAutoConsoleVariable<int32> CVarSelfTestDisableEnemyAttackAbilities(
		TEXT("Rogue.SelfTest.DisableEnemyAttackAbilities"), 0,
		TEXT("1 = 敌人不授予攻击能力（压测对照用：量 GAS 这部分到底贵多少）"),
		ECVF_Default);
}

bool RogueSelfTest::IsWeaponUpgradeChainEnabled()
{
	return CVarSelfTestWeaponUpgradeChain.GetValueOnGameThread() != 0;
}

int32 RogueSelfTest::GetEnemyStressCount()
{
	return FMath::Max(0, CVarSelfTestEnemyStress.GetValueOnGameThread());
}

bool RogueSelfTest::AreEnemyAttackAbilitiesDisabled()
{
	return CVarSelfTestDisableEnemyAttackAbilities.GetValueOnGameThread() != 0;
}
