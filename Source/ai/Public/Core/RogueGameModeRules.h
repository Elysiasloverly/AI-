#pragma once

#include "CoreMinimal.h"
#include "Core/RogueTypes.h"

class URogueGameBalanceAsset;

namespace RogueGameModeRules
{
	AI_API ERogueEnemyType PickWeightedEnemyTypeForWave(int32 Wave, const URogueGameBalanceAsset* BalanceAsset = nullptr);
	AI_API FRogueSpawnCadence BuildSpawnCadence(int32 Wave, int32 AliveEnemies, int32 MaxAliveEnemies, bool bBossAlive, float BaseSpawnInterval, const URogueGameBalanceAsset* BalanceAsset = nullptr);
	AI_API FRogueEnemyProfile BuildEnemyProfile(int32 Wave, ERogueEnemyType EnemyType, const URogueGameBalanceAsset* BalanceAsset = nullptr);
	AI_API FRogueEnemyProfile BuildBossProfile(int32 Wave, ERogueEnemyType EnemyType, const URogueGameBalanceAsset* BalanceAsset = nullptr);
	AI_API int32 BuildCurrencyReward(int32 Wave, bool bIsBoss, const URogueGameBalanceAsset* BalanceAsset = nullptr);
	AI_API int32 GetShopOfferCount(const URogueGameBalanceAsset* BalanceAsset = nullptr);
	AI_API int32 GetShopOfferCost(const URogueGameBalanceAsset* BalanceAsset = nullptr);
	AI_API int32 GetShopRefreshCost(const URogueGameBalanceAsset* BalanceAsset = nullptr);
	AI_API float GetShopAutoRefreshInterval(const URogueGameBalanceAsset* BalanceAsset = nullptr);
}
