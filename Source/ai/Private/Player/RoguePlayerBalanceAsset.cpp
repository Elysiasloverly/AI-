#include "Player/RoguePlayerBalanceAsset.h"

URoguePlayerBalanceAsset::URoguePlayerBalanceAsset()
{
	ResetToDefaults();
}

void URoguePlayerBalanceAsset::ResetToDefaults()
{
	BaseStats = FRoguePlayerBaseStatConfig();
}
