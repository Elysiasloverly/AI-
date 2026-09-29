#include "Core/RogueSelfTest.h"

#include "HAL/IConsoleManager.h"

namespace
{
	/** 开发自检：武器升级链路（默认关）。见 RogueSelfTest.h。 */
	static TAutoConsoleVariable<int32> CVarSelfTestWeaponUpgradeChain(
		TEXT("Rogue.SelfTest.WeaponUpgradeChain"), 0,
		TEXT("1 = 跑一次『升级 → GAS 加成属性 → 武器生效值』自检（服务端施加升级，各端打印读到的加成）"),
		ECVF_Default);
}

bool RogueSelfTest::IsWeaponUpgradeChainEnabled()
{
	return CVarSelfTestWeaponUpgradeChain.GetValueOnGameThread() != 0;
}
