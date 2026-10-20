#pragma once

#include "CoreMinimal.h"
#include "Progression/RogueRewardTypes.h"
#include "Shared/RogueTypes.h"
#include "RogueShopSystem.generated.h"

class ARogueCharacter;
struct FRogueUpgradeSystem;

USTRUCT()
struct AI_API FRogueShopOffer
{
	GENERATED_BODY()

	UPROPERTY()
	FRogueRewardOffer Reward;

	const FRogueUpgradeOption& GetUpgrade() const { return Reward.Upgrade; }
	FRogueUpgradeOption& GetUpgrade() { return Reward.Upgrade; }
	int32 GetCost() const { return Reward.Cost; }
	bool IsPurchased() const { return Reward.bPurchased; }
	bool IsUpgradeOffer() const { return Reward.IsUpgrade(); }
	void SetCost(int32 NewCost) { Reward.Cost = NewCost; }
	void MarkPurchased() { Reward.bPurchased = true; }
};

/** 每玩家的服务端商店运行态；PlayerState 另行复制报价镜像。
 *  刷新失败回滚报价和周期状态；购买次数跨补货保留，影响该类型后续价格。 */
USTRUCT()
struct AI_API FRogueShopSystem
{
	GENERATED_BODY()

public:
	void Reset();
	void Configure(int32 InOfferCount, int32 InOfferCost, int32 InRefreshCost, float InAutoRefreshInterval);
	void AdvanceAutoRefresh(float DeltaSeconds, const ARogueCharacter* Character, const FRogueUpgradeSystem& UpgradeSystem);
	bool OpenShop(const ARogueCharacter* Character, const FRogueUpgradeSystem& UpgradeSystem);
	void CloseShop();
	bool TryRefresh(ARogueCharacter* Character, const FRogueUpgradeSystem& UpgradeSystem);
	bool TryBuyOffer(int32 OfferIndex, ARogueCharacter* Character, FRogueUpgradeOption& OutUpgrade);

	const TArray<FRogueShopOffer>& GetOffers() const { return Offers; }
	bool IsOpen() const { return bOpen; }
	int32 GetRefreshCost() const { return CurrentRefreshCost; }
	float GetSecondsUntilAutoRefresh() const { return bAutoRefreshPending ? 0.0f : FMath::Max(0.0f, TimeUntilAutoRefresh); }

private:
	bool BuildOffers(const ARogueCharacter* Character, const FRogueUpgradeSystem& UpgradeSystem, bool bResetCycleState);
	void ResetCycleState();
	void ResetAutoRefreshTimer();
	void ApplyOfferCostForUpgradeType(ERogueUpgradeType UpgradeType);
	int32 GetOfferCostForUpgradeType(ERogueUpgradeType UpgradeType) const;
	static int32 GetDoubledCost(int32 Cost);

	UPROPERTY()
	TArray<FRogueShopOffer> Offers;

	UPROPERTY()
	TMap<ERogueUpgradeType, int32> OfferTypePurchaseCounts;

	
	int32 OfferCount = 0;
	int32 BaseOfferCost = 0;
	int32 BaseRefreshCost = 0;
	int32 CurrentRefreshCost = 0;
	float AutoRefreshInterval = 0.0f;
	float TimeUntilAutoRefresh = 0.0f;
	bool bOpen = false;
	bool bAutoRefreshPending = false;
};
