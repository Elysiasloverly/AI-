#pragma once

#include "CoreMinimal.h"
#include "Shared/RogueTypes.h"
#include "RogueUpgradeSystem.generated.h"

class ARogueCharacter;
class URogueUpgradeDefinitionAsset;
class URogueUpgradeRuleAsset;

/** 升级定义/规则资产的持有者和随机选项生成器，由服务端 GameMode 使用。
 *  待选卡、排队次数与选择状态位于各玩家 PlayerState，并按拥有者复制。
 *  生成过程读取角色/资产且使用全局随机数，因此不是数学意义的纯函数。
 */
USTRUCT()
struct AI_API FRogueUpgradeSystem
{
	GENERATED_BODY()

public:
	/** 从候选池随机抽取不重复类型，写入 OutOptions；不修改本对象配置，但会推进随机数状态。 */
	void BuildRandomUpgradeOptions(const ARogueCharacter* Character, int32 DesiredCount, TArray<FRogueUpgradeOption>& OutOptions) const;

	void SetDefinitionAsset(URogueUpgradeDefinitionAsset* InDefinitionAsset) { DefinitionAsset = InDefinitionAsset; }
	void SetRuleAsset(URogueUpgradeRuleAsset* InRuleAsset) { RuleAsset = InRuleAsset; }

private:
	FRogueUpgradeOption MakeUpgradeOption(ERogueUpgradeType Type) const;

	UPROPERTY()
	TObjectPtr<URogueUpgradeDefinitionAsset> DefinitionAsset;

	UPROPERTY()
	TObjectPtr<URogueUpgradeRuleAsset> RuleAsset;
};
