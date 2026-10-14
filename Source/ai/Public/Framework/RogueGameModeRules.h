#pragma once

#include "CoreMinimal.h"
#include "Shared/RogueTypes.h"
#include "Templates/Function.h"

class URogueGameBalanceAsset;

namespace RogueGameModeRules
{
	/** 按波次权重抽一个敌人类型（不过滤任何上限）。
	 *  注意：这是"无约束"版本，Boss 生成走它；普通刷怪请用下面带 CanSpawnType 的重载。 */
	AI_API ERogueEnemyType PickWeightedEnemyTypeForWave(int32 Wave, const URogueGameBalanceAsset* BalanceAsset = nullptr);

	/** 按波次权重抽一个敌人类型，但跳过 CanSpawnType 返回 false 的类型
	 *  （例如本波已经刷满"每波刷新上限"的类型；被跳过的行权重也不计入总数）。
	 *  若所有候选都被跳过，返回值不再保证可用——调用方需要自己用 CanSpawnType 复核。 */
	AI_API ERogueEnemyType PickWeightedEnemyTypeForWave(int32 Wave, const URogueGameBalanceAsset* BalanceAsset,
		TFunctionRef<bool(ERogueEnemyType)> CanSpawnType);

	AI_API FRogueSpawnCadence BuildSpawnCadence(int32 Wave, int32 AliveEnemies, int32 MaxAliveEnemies, bool bBossAlive, float BaseSpawnInterval, const URogueGameBalanceAsset* BalanceAsset = nullptr);
	AI_API FRogueEnemyProfile BuildEnemyProfile(int32 Wave, ERogueEnemyType EnemyType, const URogueGameBalanceAsset* BalanceAsset = nullptr);
	AI_API FRogueEnemyProfile BuildBossProfile(int32 Wave, ERogueEnemyType EnemyType, const URogueGameBalanceAsset* BalanceAsset = nullptr);
	AI_API int32 BuildCurrencyReward(int32 Wave, bool bIsBoss, const URogueGameBalanceAsset* BalanceAsset = nullptr);
	AI_API int32 GetShopOfferCount(const URogueGameBalanceAsset* BalanceAsset = nullptr);
	AI_API int32 GetShopOfferCost(const URogueGameBalanceAsset* BalanceAsset = nullptr);
	AI_API int32 GetShopRefreshCost(const URogueGameBalanceAsset* BalanceAsset = nullptr);
	AI_API float GetShopAutoRefreshInterval(const URogueGameBalanceAsset* BalanceAsset = nullptr);
}
