#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "Progression/Meta/RogueProgressionTypes.h"
#include "RogueProgressionSubsystem.generated.h"

class URogueProgressionSaveGame;
class URogueUpgradeTreeDefinition;
class APlayerController;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FRogueProgressionChanged);

/** 本地持久成长服务；只拥有本机玩家的存档，联机卡池使用其开局快照。 */
UCLASS()
class AI_API URogueProgressionSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintPure, Category = "升级树", meta = (DisplayName = "获取玩家升级树存档"))
	static URogueProgressionSubsystem* GetForPlayer(APlayerController* PlayerController);

	UPROPERTY(BlueprintAssignable, Category = "升级树")
	FRogueProgressionChanged OnProgressionChanged;

	UFUNCTION(BlueprintPure, Category = "升级树")
	FRogueProgressionSnapshot GetSnapshot() const;

	UFUNCTION(BlueprintPure, Category = "升级树")
	int32 GetIceBricks() const;

	UFUNCTION(BlueprintPure, Category = "升级树")
	const URogueUpgradeTreeDefinition* GetDefinition() const;

	UFUNCTION(BlueprintPure, Category = "升级树")
	bool IsSaveWritable() const { return bSaveWritable; }

	UFUNCTION(BlueprintPure, Category = "升级树")
	FText GetSaveError() const { return SaveError; }

	UFUNCTION(BlueprintPure, Category = "升级树")
	FString GetSaveSlotName() const { return SaveSlot; }

	UFUNCTION(BlueprintCallable, Category = "升级树")
	bool TryPurchaseNode(FName NodeId, FText& OutFailure);

	/** 同一个服务器奖励 ID 最多记一次；成功返回包含此前已经成功领取的情况。 */
	UFUNCTION(BlueprintCallable, Category = "升级树")
	bool ApplyBossReward(const FGuid& RewardId);

	/** 奖励移交后跨地图保留，直到事务成功；不计入已保存余额。 */
	void EnqueueBossReward(const FGuid& RewardId);
	bool FlushPendingBossRewards(float DeltaSeconds = 0.0f);
	UFUNCTION(BlueprintPure, Category = "升级树")
	int32 GetPendingBossRewardCount() const { return PendingBossRewards.Num(); }
	UFUNCTION(BlueprintPure, Category = "升级树")
	float GetLastSaveMilliseconds() const { return LastSaveMilliseconds; }

	/** Shared by normal loading and editor tools, including validated QA command-line overrides. */
	static bool ResolveSaveLocation(int32 LocalPlayerIndex, FString& OutSlot, int32& OutUserIndex, FText& OutError);
#if WITH_EDITOR
	/** Clear purchased ranks without refunding spent currency or replaying Boss rewards. */
	bool ResetProgressionForDebug(FText& OutError);
	static bool ResetProgressionInSlotForDebug(const FString& Slot, int32 UserIndex,
		const URogueUpgradeTreeDefinition* Definition, FText& OutError);
	bool SetIceBricksForDebug(int32 NewCount, FText& OutError);
	static bool SetIceBricksInSlotForDebug(const FString& Slot, int32 UserIndex,
		const URogueUpgradeTreeDefinition* Definition, int32 NewCount, FText& OutError);
#endif

	/** 显式槽名的存储接口也供隔离测试使用；不隐式访问实际玩家存档。 */
	static URogueProgressionSaveGame* LoadProfileFromSlot(const FString& SlotName, int32 UserIndex,
		const URogueUpgradeTreeDefinition* Definition, bool& bOutWritable, FText& OutError);
	static bool SaveProfileToSlot(const URogueProgressionSaveGame* Profile, const FString& SlotName, int32 UserIndex);

private:
#if WITH_DEV_AUTOMATION_TESTS
	friend class FRogueMetaSaveFailureRollbackTest;
	friend class FRoguePendingRewardTravelTest;
#endif

	TSet<FGuid> PendingBossRewards;
	FTSTicker::FDelegateHandle RewardRetryHandle;
	float LastSaveMilliseconds = 0.0f;

	bool CommitCandidate(URogueProgressionSaveGame* Candidate);

	UPROPERTY(Transient)
	TObjectPtr<URogueProgressionSaveGame> Profile;

	FString SaveSlot;
	int32 SaveUserIndex = 0;
	bool bSaveWritable = false;
	FText SaveError;
};
