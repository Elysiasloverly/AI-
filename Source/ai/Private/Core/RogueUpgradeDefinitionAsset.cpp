#include "Core/RogueUpgradeDefinitionAsset.h"
#include "Engine/DataTable.h"

bool URogueUpgradeDefinitionAsset::FindDefinition(ERogueUpgradeType Type, FRogueUpgradeDefinitionRow& OutDefinition) const
{
	if (UpgradeDefinitionTable == nullptr)
	{
		return false;
	}

	// 遍历 DataTable 中所有行，查找匹配的升级类型
	const TMap<FName, uint8*>& RowMap = UpgradeDefinitionTable->GetRowMap();
	for (const auto& Pair : RowMap)
	{
		const FRogueUpgradeDefinitionRow* Row = reinterpret_cast<const FRogueUpgradeDefinitionRow*>(Pair.Value);
		if (Row != nullptr && Row->Type == Type)
		{
			OutDefinition = *Row;
			return true;
		}
	}

	return false;
}
