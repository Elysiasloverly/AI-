#pragma once

#include "CoreMinimal.h"
#include "Core/RogueTypes.h"
#include "RogueRewardTypes.generated.h"

UENUM()
enum class ERogueRewardKind : uint8
{
	Upgrade,
	Relic
};

UENUM()
enum class ERogueRewardSource : uint8
{
	LevelUp,
	Boss,
	Shop,
	RelicShop
};

USTRUCT()
struct AI_API FRogueRewardOffer
{
	GENERATED_BODY()

	UPROPERTY()
	ERogueRewardKind Kind = ERogueRewardKind::Upgrade;

	UPROPERTY()
	ERogueRewardSource Source = ERogueRewardSource::LevelUp;

	UPROPERTY()
	FName RewardId = NAME_None;

	UPROPERTY()
	FRogueUpgradeOption Upgrade;

	UPROPERTY()
	int32 Cost = 0;

	UPROPERTY()
	bool bPurchased = false;

	bool IsUpgrade() const { return Kind == ERogueRewardKind::Upgrade; }

	static FRogueRewardOffer MakeUpgrade(const FRogueUpgradeOption& InUpgrade, ERogueRewardSource InSource, int32 InCost)
	{
		FRogueRewardOffer Offer;
		Offer.Kind = ERogueRewardKind::Upgrade;
		Offer.Source = InSource;
		Offer.RewardId = FName(*FString::Printf(TEXT("Upgrade_%d"), static_cast<int32>(InUpgrade.Type)));
		Offer.Upgrade = InUpgrade;
		Offer.Cost = InCost;
		return Offer;
	}
};
