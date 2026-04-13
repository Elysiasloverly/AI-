#include "Core/RogueUpgradeDefinitionAsset.h"

bool URogueUpgradeDefinitionAsset::FindDefinition(ERogueUpgradeType Type, FRogueUpgradeDefinitionRow& OutDefinition) const
{
	for (const FRogueUpgradeDefinitionRow& Definition : Definitions)
	{
		if (Definition.Type == Type)
		{
			OutDefinition = Definition;
			return true;
		}
	}

	return false;
}
