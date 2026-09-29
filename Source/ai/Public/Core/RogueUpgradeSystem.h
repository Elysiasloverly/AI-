#pragma once

#include "CoreMinimal.h"
#include "Core/RogueTypes.h"
#include "RogueUpgradeSystem.generated.h"

class ARogueCharacter;
class URogueUpgradeDefinitionAsset;
class URogueUpgradeRuleAsset;

/**
 * 升级配置 + 纯算法。
 *
 * ⚠️ 这里**不再持有运行态**。原来的 `PendingUpgrades` / `QueuedSelections` / `bAwaitingChoice`
 * 都搬到了 `ARoguePlayerState`，原因是它们是**每玩家一份、且必须复制给客户端**的数据：
 *   · 单机版把它们放在 GameMode 上是可行的；
 *   · 联机下 GameMode 是服务端独占对象，客户端调 `GetAuthGameMode()` 得到 nullptr ——
 *     结果就是客户端玩家根本看不到升级界面（这是迁移前的真实故障）。
 * 现在这个结构体只承担"服务端配置"（升级定义表 + 规则资产），因此可以安全地留在 GameMode 上。
 */
USTRUCT()
struct AI_API FRogueUpgradeSystem
{
	GENERATED_BODY()

public:
	/** 构建随机升级选项（纯函数，不修改自身状态）。 */
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
