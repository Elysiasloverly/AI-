#pragma once

#include "CoreMinimal.h"
#include "Core/RogueTypes.h"

class ARogueCharacter;

struct AI_API FRogueUpgradeEffectApplier
{
	static void ApplyToCharacter(ARogueCharacter& Character, const FRogueUpgradeOption& Upgrade);
};
