#include "Progression/Meta/RogueProgressionSaveGame.h"
#include "Progression/Meta/RogueUpgradeTreeDefinition.h"

bool URogueProgressionSaveGame::TryPurchase(const URogueUpgradeTreeDefinition* Definition, FName NodeId, FText& OutFailure)
{
	if (!Definition)
	{
		OutFailure = FText::FromString(TEXT("升级树配置不可用。"));
		return false;
	}
	OutFailure = Definition->GetPurchaseFailure(Progression, IceBricks, NodeId);
	if (!OutFailure.IsEmpty()) { return false; }
	const FRogueUpgradeTreeNode* Node = Definition->FindNode(NodeId);
	check(Node);
	const int32 NewRank = Definition->GetNodeRank(Progression, NodeId) + 1;
	IceBricks -= FMath::Max(Node->Cost, 1);
	Progression.NodeRanks.Add(NodeId, NewRank);
	return true;
}

bool URogueProgressionSaveGame::AddBossReward(const FGuid& RewardId)
{
	if (!RewardId.IsValid()) { return false; }
	if (AppliedBossRewards.Contains(RewardId)) { return true; }
	if (IceBricks < 0 || IceBricks == MAX_int32) { return false; }
	++IceBricks;
	AppliedBossRewards.Add(RewardId);
	return true;
}


void URogueProgressionSaveGame::UpgradeLegacyUnlocks(const URogueUpgradeTreeDefinition* Definition)
{
	if (Version >= CurrentVersion || Definition == nullptr) return;
	struct FUpgrade { const TCHAR* Card; const TCHAR* Weapon; const TCHAR* Middle; };
	const FUpgrade Changes[] = {
		{ TEXT("Card.ScytheLifesteal"), TEXT("Weapon.Scythe"), TEXT("Card.ScytheSize") },
		{ TEXT("Card.ScytheLifesteal"), TEXT("Weapon.Scythe"), TEXT("Card.ScytheCount") },
		{ TEXT("Card.ScytheSize"), TEXT("Weapon.Scythe"), TEXT("Card.ScytheCount") },
		{ TEXT("Card.LaserRefraction"), TEXT("Weapon.Laser"), TEXT("Card.LaserCount") },
		{ TEXT("Card.MortarBlastRadius"), TEXT("Weapon.Mortar"), TEXT("Card.MortarCount") }
	};
	for (const FUpgrade& Change : Changes)
	{
		if (Progression.NodeRanks.FindRef(FName(Change.Card)) > 0
			&& Progression.NodeRanks.FindRef(FName(Change.Weapon)) > 0 && Definition->FindNode(FName(Change.Middle)) != nullptr)
		{
			Progression.NodeRanks.FindOrAdd(FName(Change.Middle)) = FMath::Max(1, Progression.NodeRanks.FindRef(FName(Change.Middle)));
		}
	}
	Version = CurrentVersion;
}
