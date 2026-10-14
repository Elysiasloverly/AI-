#include "Debug/RogueSelfTest.h"

#include "HAL/IConsoleManager.h"

namespace
{
	/** 开发自检：武器升级链路（默认关）。见 RogueSelfTest.h。 */
	static TAutoConsoleVariable<int32> CVarSelfTestWeaponUpgradeChain(
		TEXT("Rogue.SelfTest.WeaponUpgradeChain"), 0,
		TEXT("1 = 跑一次『升级 → GAS 加成属性 → 武器生效值』自检（服务端施加升级，各端打印读到的加成）"),
		ECVF_Default);

	/** 开发压测：一次性刷 N 只"很硬、伤害为 0"的敌人围在玩家周围（默认 0 = 关）。
	 *
	 *  为什么需要它：常规波次整局只维持个位数敌人（武器清怪快于刷怪），
	 *  于是"200+ 敌人时帧时有没有退化"这条红线**根本没有数据**。
	 *  这些压测怪伤害为 0 是刻意的：保证玩家不会被打倒、测试过程稳定可比。 */
	static TAutoConsoleVariable<int32> CVarSelfTestEnemyStress(
		TEXT("Rogue.SelfTest.EnemyStress"), 0,
		TEXT("N > 0 = 自检完成时刷 N 只压测敌人（很硬、伤害 0），用来量 200+ 敌人时的帧时"),
		ECVF_Default);

	/** 压测对照开关：1 = **不授予**敌人攻击能力（用来分离"GAS 给敌人攻击带来的开销"）。 */
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
