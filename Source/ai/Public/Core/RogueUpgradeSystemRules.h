#pragma once

#include "CoreMinimal.h"
#include "Core/RogueTypes.h"

class ARogueCharacter;
class URogueUpgradeRuleAsset;

namespace RogueUpgradeSystemRules
{
	AI_API void BuildUpgradePool(TArray<ERogueUpgradeType>& OutTypes, const ARogueCharacter* Character, const URogueUpgradeRuleAsset* RuleAsset = nullptr);
}
