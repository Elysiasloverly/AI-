#pragma once

#include "CoreMinimal.h"
#include "Core/RogueTypes.h"

class ARogueCharacter;
class URogueUpgradeRuleAsset;

struct AI_API FRogueUpgradeOptionDefinition
{
	ERogueUpgradeType Type;
	const TCHAR* Title;
	const TCHAR* Description;
	float Magnitude;
};

namespace RogueUpgradeSystemRules
{
	AI_API void BuildUpgradePool(TArray<ERogueUpgradeType>& OutTypes, const ARogueCharacter* Character, const URogueUpgradeRuleAsset* RuleAsset = nullptr);
	AI_API const FRogueUpgradeOptionDefinition* FindUpgradeOptionDefinition(ERogueUpgradeType Type);
}
