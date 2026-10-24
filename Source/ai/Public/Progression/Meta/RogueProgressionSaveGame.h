#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Progression/Meta/RogueProgressionTypes.h"
#include "RogueProgressionSaveGame.generated.h"

class URogueUpgradeTreeDefinition;

/** 每个本地玩家独立保存。事务先在副本执行，写盘成功后子系统才替换当前档案。 */
UCLASS()
class AI_API URogueProgressionSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	static constexpr int32 CurrentVersion = 2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "存档")
	int32 Version = CurrentVersion;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "存档")
	int32 IceBricks = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "存档")
	FRogueProgressionSnapshot Progression;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "存档")
	TArray<FGuid> AppliedBossRewards;

	/** v2 旧档为空时兼容；新提交校验业务字段完整性，不作为防作弊签名。 */
	UPROPERTY(SaveGame)
	FString IntegrityDigest;

	/** 只修改当前对象，便于事务提交以及不接触玩家存档的独立测试。 */
	bool TryPurchase(const URogueUpgradeTreeDefinition* Definition, FName NodeId, FText& OutFailure);
	bool AddBossReward(const FGuid& RewardId);

	/** v1 已付费的上层卡补齐新增的中间前置，保留余额与原升级。 */
	void UpgradeLegacyUnlocks(const URogueUpgradeTreeDefinition* Definition);
};
