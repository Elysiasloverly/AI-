#include "Core/RogueShopSystem.h"

#include "Core/RogueUpgradeSystem.h"
#include "Player/RogueCharacter.h"

void FRogueShopSystem::Reset()
{
	Offers.Reset();
	OfferTypePurchaseCounts.Reset();
	OfferCount = 6;
	BaseOfferCost = 22;
	BaseRefreshCost = 14;
	CurrentRefreshCost = BaseRefreshCost;
	AutoRefreshInterval = 60.0f;
	TimeUntilAutoRefresh = AutoRefreshInterval;
	bOpen = false;
	bAutoRefreshPending = false;
}

void FRogueShopSystem::Configure(int32 InOfferCount, int32 InOfferCost, int32 InRefreshCost, float InAutoRefreshInterval)
{
	OfferCount = FMath::Max(1, InOfferCount);
	BaseOfferCost = FMath::Max(1, InOfferCost);
	BaseRefreshCost = FMath::Max(0, InRefreshCost);
	AutoRefreshInterval = FMath::Max(5.0f, InAutoRefreshInterval);

	if (CurrentRefreshCost < BaseRefreshCost)
	{
		CurrentRefreshCost = BaseRefreshCost;
	}

	if (TimeUntilAutoRefresh < 0.0f || TimeUntilAutoRefresh > AutoRefreshInterval)
	{
		TimeUntilAutoRefresh = AutoRefreshInterval;
	}
}

void FRogueShopSystem::AdvanceAutoRefresh(float DeltaSeconds, const ARogueCharacter* Character, const FRogueUpgradeSystem& UpgradeSystem)
{
	if (AutoRefreshInterval <= 0.0f || DeltaSeconds <= 0.0f)
	{
		return;
	}

	TimeUntilAutoRefresh = FMath::Max(0.0f, TimeUntilAutoRefresh - DeltaSeconds);
	if (TimeUntilAutoRefresh > 0.0f)
	{
		return;
	}

	if (Character != nullptr)
	{
		BuildOffers(Character, UpgradeSystem, true);
		return;
	}

	bAutoRefreshPending = true;
}

bool FRogueShopSystem::OpenShop(const ARogueCharacter* Character, const FRogueUpgradeSystem& UpgradeSystem)
{
	if (Offers.Num() == 0 || bAutoRefreshPending)
	{
		if (!BuildOffers(Character, UpgradeSystem, true))
		{
			bOpen = false;
			return false;
		}
	}

	bOpen = true;
	return bOpen;
}

void FRogueShopSystem::CloseShop()
{
	bOpen = false;
}

bool FRogueShopSystem::TryRefresh(ARogueCharacter* Character, const FRogueUpgradeSystem& UpgradeSystem)
{
	if (!bOpen || Character == nullptr || !Character->TrySpendMoney(CurrentRefreshCost))
	{
		return false;
	}

	CurrentRefreshCost = GetDoubledCost(CurrentRefreshCost);
	return BuildOffers(Character, UpgradeSystem, false);
}

bool FRogueShopSystem::TryBuyOffer(int32 OfferIndex, ARogueCharacter* Character, FRogueUpgradeOption& OutUpgrade)
{
	if (!bOpen || Character == nullptr || !Offers.IsValidIndex(OfferIndex))
	{
		return false;
	}

	FRogueShopOffer& Offer = Offers[OfferIndex];
	if (Offer.bPurchased || !Character->TrySpendMoney(Offer.Cost))
	{
		return false;
	}

	Offer.bPurchased = true;
	OutUpgrade = Offer.Upgrade;
	int32& PurchaseCount = OfferTypePurchaseCounts.FindOrAdd(Offer.Upgrade.Type);
	++PurchaseCount;
	ApplyOfferCostForUpgradeType(Offer.Upgrade.Type);
	return true;
}

bool FRogueShopSystem::BuildOffers(const ARogueCharacter* Character, const FRogueUpgradeSystem& UpgradeSystem, bool bResetCycleState)
{
	if (Character == nullptr)
	{
		return false;
	}

	if (bResetCycleState)
	{
		ResetCycleState();
	}

	Offers.Reset();

	TArray<FRogueUpgradeOption> GeneratedOffers;
	UpgradeSystem.BuildRandomUpgradeOptions(Character, OfferCount, GeneratedOffers);
	for (const FRogueUpgradeOption& Upgrade : GeneratedOffers)
	{
		FRogueShopOffer& Offer = Offers.AddDefaulted_GetRef();
		Offer.Upgrade = Upgrade;
		Offer.Cost = GetOfferCostForUpgradeType(Upgrade.Type);
		Offer.bPurchased = false;
	}

	const bool bBuiltOffers = Offers.Num() > 0;
	if (bBuiltOffers)
	{
		ResetAutoRefreshTimer();
	}

	bOpen = bOpen && bBuiltOffers;
	return bBuiltOffers;
}

void FRogueShopSystem::ResetCycleState()
{
	CurrentRefreshCost = BaseRefreshCost;
}

void FRogueShopSystem::ResetAutoRefreshTimer()
{
	TimeUntilAutoRefresh = AutoRefreshInterval;
	bAutoRefreshPending = false;
}

void FRogueShopSystem::ApplyOfferCostForUpgradeType(ERogueUpgradeType UpgradeType)
{
	for (FRogueShopOffer& Offer : Offers)
	{
		if (!Offer.bPurchased && Offer.Upgrade.Type == UpgradeType)
		{
			Offer.Cost = GetOfferCostForUpgradeType(UpgradeType);
		}
	}
}

int32 FRogueShopSystem::GetOfferCostForUpgradeType(ERogueUpgradeType UpgradeType) const
{
	const int32 PurchaseCount = OfferTypePurchaseCounts.FindRef(UpgradeType);
	int64 Cost = BaseOfferCost;
	for (int32 Index = 0; Index < PurchaseCount; ++Index)
	{
		Cost = FMath::Min<int64>(Cost * 2, static_cast<int64>(MAX_int32));
	}

	return static_cast<int32>(Cost);
}

int32 FRogueShopSystem::GetDoubledCost(int32 Cost)
{
	return static_cast<int32>(FMath::Min<int64>(static_cast<int64>(Cost) * 2, static_cast<int64>(MAX_int32)));
}
