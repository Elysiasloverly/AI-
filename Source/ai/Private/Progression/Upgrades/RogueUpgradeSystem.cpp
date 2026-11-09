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
        case ERogueUpgradeType::FrostBladeCount:Option.Title=TEXT("冰刀·霜月");Option.Description=TEXT("升级树解锁后，每局自动装备唯一一把冰刀·霜月；悬于身后，自动横斩近处敌人后归位。此获取项不再进入选卡或商店。");Option.Magnitude=1;break;
        case ERogueUpgradeType::FrostBladeSize:Option.Title=TEXT("霜刃延展");Option.Description=TEXT("冰刀尺寸增加基础值的20%；横斩范围和伤害随尺寸提高，基础攻击加成也参与尺寸放大。");Option.Magnitude=0.2;break;
        case ERogueUpgradeType::FrostBladeWave:Option.Title=TEXT("霜月出鞘");Option.Description=TEXT("冰刀横斩时向前放出一道低伤害的浅弯月剑气；穿透敌人，碰地图或30秒后消失。");Option.Magnitude=1;break;
        case ERogueUpgradeType::FrostBladeWaveCount:Option.Title=TEXT("霜月重叠");Option.Description=TEXT("每次横斩额外发出1道剑气，向前呈小角度扇形散开。");Option.Magnitude=1;break;
        case ERogueUpgradeType::FrostBladeWaveSize:Option.Title=TEXT("霜月盈空");Option.Description=TEXT("剑气外观和命中范围增加基础尺寸的20%，不改变伤害与寿命。");Option.Magnitude=0.2;break;
		case ERogueUpgradeType::TalismanCount:
			Option.Title=TEXT("归心符阵"); Option.Description=TEXT("获得灵符·归心；已持有时，增加 1 张环绕灵符，独立出击并返回。"); Option.Magnitude=1; break;
		case ERogueUpgradeType::TalismanPierce:
			Option.Title=TEXT("灵符贯阵"); Option.Description=TEXT("每张灵符每次出击可穿刺的不同敌人数 +1；默认命中 1 名敌人后返回。"); Option.Magnitude=1; break;
		case ERogueUpgradeType::TalismanRepeat:
			Option.Title=TEXT("灵符重奏"); Option.Description=TEXT("每张灵符每次出击对每名敌人的穿刺次数 +1；每次穿刺造成完整伤害。"); Option.Magnitude=1; break;
		case ERogueUpgradeType::ProjectileBurst:
			Option.Title = TEXT("冰符连诵");
			Option.Description = TEXT("冰符·寒弹每轮攻击额外连射 1 次；每次连射都发出完整弹幕。");
			Option.Magnitude = 1.0f;
			break;
		case ERogueUpgradeType::RocketBlastRadius:
			Option.Title = TEXT("冰焰扩散");
			Option.Description = TEXT("冰焰球爆炸半径 +90 厘米，同时增大球体、焰尾与地面蓝焰的影响范围。");
			Option.Magnitude = 90.0f;
			break;
		case ERogueUpgradeType::HellTowerDamageRamp:
			Option.Title = TEXT("焰心递燃");
			Option.Description = TEXT("焰符·灼光每跳伤害递增值 +0.1；持续锁定同一目标时，伤害增长更快。");
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
			Option.Title = TEXT("霜玉连投");
			Option.Description = TEXT("获得冰符·霜玉；已持有时，每轮额外投出 1 枚冰符·霜玉。");
			Option.Magnitude = 1.0f;
			break;
		case ERogueUpgradeType::MortarBlastRadius:
			Option.Title = TEXT("霜玉扩散");
			Option.Description = TEXT("冰符·霜玉爆炸半径 +90 厘米，扩大落点周围的伤害区域。");
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
	RogueUpgradeSystemRules::BuildUpgradePool(Types, Character, RuleAsset, DefinitionAsset);
    const int32 OpeningLevels=RuleAsset?RuleAsset->OpeningOffenseLevels:8;
    if (Character->GetPlayerLevel()<=OpeningLevels)
    {
        // 前期至少一项能立即加强当前武器；其余位置仍随机且不重复。
        TArray<ERogueUpgradeType> Offensive;
        for (auto Type:Types)
            if (Type==ERogueUpgradeType::ProjectileCount || Type==ERogueUpgradeType::AttackPower || Type==ERogueUpgradeType::AttackSpeed)
                Offensive.Add(Type);
        if (!Offensive.IsEmpty())
        {
            const auto Type=Offensive[FMath::RandRange(0,Offensive.Num()-1)];
            OutOptions.Add(MakeUpgradeOption(Type));Types.Remove(Type);
        }
    }
	for (int32 Index = OutOptions.Num(); Index < DesiredCount && Types.Num() > 0; ++Index)
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

bool FRogueUpgradeSystem::IsUpgradeAvailable(const ARogueCharacter* Character, ERogueUpgradeType Type) const
{
    TArray<ERogueUpgradeType> Types;
    RogueUpgradeSystemRules::BuildUpgradePool(Types, Character, RuleAsset, DefinitionAsset);
    return Types.Contains(Type);
}
