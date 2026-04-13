#include "Core/RogueGameBalanceAsset.h"

const FRogueEnemyBalanceRow* URogueGameBalanceAsset::FindEnemyBalanceRow(ERogueEnemyType Type) const
{
	for (const FRogueEnemyBalanceRow& Row : EnemyBalanceRows)
	{
		if (Row.EnemyType == Type)
		{
			return &Row;
		}
	}

	return nullptr;
}
