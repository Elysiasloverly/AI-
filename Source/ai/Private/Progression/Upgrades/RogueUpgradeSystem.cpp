#include "Progression/Upgrades/RogueUpgradeSystem.h"

#include "Progression/Upgrades/RogueUpgradeDefinitionAsset.h"
#include "Progression/Upgrades/RogueUpgradeRuleAsset.h"
#include "Progression/Upgrades/RogueUpgradeSystemRules.h"
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
		Option.FirstMagnitude = Definition.FirstMagnitude;
		Option.MaxMagnitude = Definition.MaxMagnitude;
		return Option;
	}

	/** DataTable 中未配置时使用的默认升级选项 */
	FRogueUpgradeOption MakeDefaultUpgradeOption(ERogueUpgradeType Type)
	{
		FRogueUpgradeOption Option;
		Option.Type = Type;
		switch (Type)
		{
		case ERogueUpgradeType::ProjectileBurst:
			Option.Title = TEXT("灵符连诵");
			Option.Description = TEXT("御札每轮攻击额外连射 1 次；每次连射都发出完整弹幕。");
			Option.Magnitude = 1.0f;
			break;
		case ERogueUpgradeType::RocketBlastRadius:
			Option.Title = TEXT("炉心扩散");
			Option.Description = TEXT("八卦炉爆炸半径 +90 厘米，扩大爆裂弹的伤害覆盖范围。");
			Option.Magnitude = 90.0f;
			break;
		case ERogueUpgradeType::HellTowerDamageRamp:
			Option.Title = TEXT("神威增幅");
			Option.Description = TEXT("御柱每跳伤害递增值 +0.1；持续锁定同一目标时，伤害增长更快。");
			Option.Magnitude = 0.1f;
			break;
		case ERogueUpgradeType::ScytheSize:
			Option.Title = TEXT("银刃巨化");
			Option.Description = TEXT("银刃外观与伤害范围均增加基础尺寸的 20%；环绕距离不变。");
			Option.Magnitude = 0.2f;
			break;
		case ERogueUpgradeType::ScytheLifesteal:
			Option.Title = TEXT("红雾吸血");
			Option.Description = TEXT("银刃造成伤害时回复生命。首次吸血率 20%，再次选择 +5 个百分点。");
			Option.Magnitude = 0.05f;
			Option.FirstMagnitude = 0.20f;
			Option.MaxMagnitude = 1.0f;
			break;
		case ERogueUpgradeType::MortarCount:
			Option.Title = TEXT("阴阳玉连投");
			Option.Description = TEXT("获得阴阳玉；已持有时，每轮额外投出 1 枚阴阳玉。");
			Option.Magnitude = 1.0f;
			break;
		case ERogueUpgradeType::MortarBlastRadius:
			Option.Title = TEXT("阴阳玉·扩散");
			Option.Description = TEXT("阴阳玉爆炸半径 +90 厘米，扩大落点周围的伤害区域。");
			Option.Magnitude = 90.0f;
			break;
		default:
			Option.Title = TEXT("未配置升级");
			Option.Description = TEXT("该升级尚未在 DataTable 中配置。");
			Option.Magnitude = 0.0f;
			break;
		}
		return Option;
	}
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
	UE_LOG(LogTemp, Verbose, TEXT("RogueUpgradeSystem: 升级类型 %d 未在 DataTable 中配置，使用默认值。"), static_cast<int32>(Type));
	return MakeDefaultUpgradeOption(Type);
}
