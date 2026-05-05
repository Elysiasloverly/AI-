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
	if (!bOpen || Character == nullptr || Character->GetMoney() < CurrentRefreshCost)
	{
		return false;
	}

	TArray<FRogueShopOffer> PreviousOffers = Offers;
	const int32 PreviousRefreshCost = CurrentRefreshCost;
	const float PreviousTimeUntilAutoRefresh = TimeUntilAutoRefresh;
	const bool bPreviousAutoRefreshPending = bAutoRefreshPending;
	const bool bPreviousOpen = bOpen;
	if (!BuildOffers(Character, UpgradeSystem, false))
	{
		Offers = MoveTemp(PreviousOffers);
		CurrentRefreshCost = PreviousRefreshCost;
		TimeUntilAutoRefresh = PreviousTimeUntilAutoRefresh;
		bAutoRefreshPending = bPreviousAutoRefreshPending;
		bOpen = bPreviousOpen;
		return false;
	}

	if (!Character->TrySpendMoney(PreviousRefreshCost))
	{
		Offers = MoveTemp(PreviousOffers);
		CurrentRefreshCost = PreviousRefreshCost;
		TimeUntilAutoRefresh = PreviousTimeUntilAutoRefresh;
		bAutoRefreshPending = bPreviousAutoRefreshPending;
		bOpen = bPreviousOpen;
		return false;
	}

	CurrentRefreshCost = GetDoubledCost(PreviousRefreshCost);
	return true;
}

bool FRogueShopSystem::TryBuyOffer(int32 OfferIndex, ARogueCharacter* Character, FRogueUpgradeOption& OutUpgrade)
{
	if (!bOpen || Character == nullptr || !Offers.IsValidIndex(OfferIndex))
	{
		return false;
	}

	FRogueShopOffer& Offer = Offers[OfferIndex];
	if (!Offer.IsUpgradeOffer() || Offer.IsPurchased() || !Character->TrySpendMoney(Offer.GetCost()))
	{
		return false;
	}

	Offer.MarkPurchased();
	OutUpgrade = Offer.GetUpgrade();
	int32& PurchaseCount = OfferTypePurchaseCounts.FindOrAdd(Offer.GetUpgrade().Type);
	++PurchaseCount;
	ApplyOfferCostForUpgradeType(Offer.GetUpgrade().Type);
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
		Offer.Reward = FRogueRewardOffer::MakeUpgrade(Upgrade, ERogueRewardSource::Shop, GetOfferCostForUpgradeType(Upgrade.Type));
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
		if (Offer.IsUpgradeOffer() && !Offer.IsPurchased() && Offer.GetUpgrade().Type == UpgradeType)
		{
			Offer.SetCost(GetOfferCostForUpgradeType(UpgradeType));
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
