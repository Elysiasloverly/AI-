#include "Core/RogueUpgradeSystem.h"

#include "Core/RogueUpgradeDefinitionAsset.h"
#include "Core/RogueUpgradeRuleAsset.h"
#include "Core/RogueUpgradeSystemRules.h"
#include "Player/RogueCharacter.h"

namespace
{
	FRogueUpgradeOption MakeUpgradeOptionData(const FRogueUpgradeDefinitionRow& Definition)
	{
		FRogueUpgradeOption Option;
		Option.Type = Definition.Type;
		Option.Title = Definition.Title;
		Option.Description = Definition.Description;
		Option.Magnitude = Definition.Magnitude;
		return Option;
	}

	/** DataTable 中未配置时使用的默认升级选项 */
	FRogueUpgradeOption MakeDefaultUpgradeOption(ERogueUpgradeType Type)
	{
		FRogueUpgradeOption Option;
		Option.Type = Type;
		Option.Title = TEXT("未配置升级");
		Option.Description = TEXT("该升级尚未在 DataTable 中配置。");
		Option.Magnitude = 0.0f;
		return Option;
	}
}

void FRogueUpgradeSystem::Reset()
{
	PendingUpgrades.Reset();
	QueuedSelections = 0;
	bAwaitingChoice = false;
}

void FRogueUpgradeSystem::QueueSelections(int32 Count)
{
	if (Count > 0)
	{
		QueuedSelections += Count;
	}
}

bool FRogueUpgradeSystem::OpenNextSelection(const ARogueCharacter* Character)
{
	if (Character == nullptr || Character->IsDead() || QueuedSelections <= 0)
	{
		Reset();
		return false;
	}

	BuildRandomUpgradeOptions(Character, 3, PendingUpgrades);

	if (PendingUpgrades.Num() != 3)
	{
		Reset();
		return false;
	}

	--QueuedSelections;
	bAwaitingChoice = true;
	return true;
}

void FRogueUpgradeSystem::BuildRandomUpgradeOptions(const ARogueCharacter* Character, int32 DesiredCount, TArray<FRogueUpgradeOption>& OutOptions) const
{
	OutOptions.Reset();
	if (Character == nullptr || Character->IsDead() || DesiredCount <= 0)
	{
		return;
	}

	TArray<ERogueUpgradeType> Types;
	RogueUpgradeSystemRules::BuildUpgradePool(Types, Character, RuleAsset);
	for (int32 Index = 0; Index < DesiredCount && Types.Num() > 0; ++Index)
	{
		const int32 PickedIndex = FMath::RandRange(0, Types.Num() - 1);
		OutOptions.Add(MakeUpgradeOption(Types[PickedIndex]));
		Types.RemoveAt(PickedIndex);
	}
}

bool FRogueUpgradeSystem::TryConsumeChoice(int32 UpgradeIndex, FRogueUpgradeOption& OutUpgrade)
{
	if (!bAwaitingChoice || !PendingUpgrades.IsValidIndex(UpgradeIndex))
	{
		return false;
	}

	OutUpgrade = PendingUpgrades[UpgradeIndex];
	PendingUpgrades.Reset();
	bAwaitingChoice = false;
	return true;
}

FRogueUpgradeOption FRogueUpgradeSystem::MakeUpgradeOption(ERogueUpgradeType Type) const
{
	if (DefinitionAsset != nullptr)
	{
		FRogueUpgradeDefinitionRow AssetDefinition;
		if (DefinitionAsset->FindDefinition(Type, AssetDefinition))
		{
			return MakeUpgradeOptionData(AssetDefinition);
		}
	}

	// DataTable 中未找到对应配置，返回默认值
	UE_LOG(LogTemp, Warning, TEXT("RogueUpgradeSystem: 升级类型 %d 未在 DataTable 中配置，使用默认值。"), static_cast<int32>(Type));
	return MakeDefaultUpgradeOption(Type);
}