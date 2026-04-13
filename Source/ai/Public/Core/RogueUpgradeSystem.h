#pragma once

#include "CoreMinimal.h"
#include "Core/RogueTypes.h"
#include "RogueUpgradeSystem.generated.h"

class ARogueCharacter;
class URogueUpgradeDefinitionAsset;
class URogueUpgradeRuleAsset;

USTRUCT()
struct AI_API FRogueUpgradeSystem
{
	GENERATED_BODY()

public:
	const TArray<FRogueUpgradeOption>& GetPendingUpgrades() const { return PendingUpgrades; }
	bool IsAwaitingChoice() const { return bAwaitingChoice; }
	int32 GetQueuedSelections() const { return QueuedSelections; }

	void Reset();
	void QueueSelections(int32 Count);
	bool OpenNextSelection(const ARogueCharacter* Character);
	void BuildRandomUpgradeOptions(const ARogueCharacter* Character, int32 DesiredCount, TArray<FRogueUpgradeOption>& OutOptions) const;
	bool TryConsumeChoice(int32 UpgradeIndex, FRogueUpgradeOption& OutUpgrade);
	void CloseSelection() { bAwaitingChoice = false; }
	void SetDefinitionAsset(URogueUpgradeDefinitionAsset* InDefinitionAsset) { DefinitionAsset = InDefinitionAsset; }
	void SetRuleAsset(URogueUpgradeRuleAsset* InRuleAsset) { RuleAsset = InRuleAsset; }

private:
	FRogueUpgradeOption MakeUpgradeOption(ERogueUpgradeType Type) const;

	UPROPERTY()
	TObjectPtr<URogueUpgradeDefinitionAsset> DefinitionAsset;

	UPROPERTY()
	TObjectPtr<URogueUpgradeRuleAsset> RuleAsset;

	UPROPERTY()
	TArray<FRogueUpgradeOption> PendingUpgrades;

	int32 QueuedSelections = 0;
	bool bAwaitingChoice = false;
};
