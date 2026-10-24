#pragma once

#include "CoreMinimal.h"
#include "Shared/RogueTypes.h"

class ARogueCharacter;
class URogueUpgradeRuleAsset;

namespace RogueUpgradeSystemRules
{
	/** 查询当前玩家的永久许可与本局武器前置，供已有报价在扣费前复核；不改变卡池配置。 */
	AI_API bool IsUpgradeAllowedForProgression(ERogueUpgradeType Type, const ARogueCharacter* Character);

	AI_API void BuildUpgradePool(TArray<ERogueUpgradeType>& OutTypes, const ARogueCharacter* Character, const URogueUpgradeRuleAsset* RuleAsset = nullptr);
}
