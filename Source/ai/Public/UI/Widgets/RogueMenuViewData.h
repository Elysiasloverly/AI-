#pragma once

#include "CoreMinimal.h"
#include "RogueMenuViewData.generated.h"

USTRUCT(BlueprintType)
struct FRogueDeathViewData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Death")
	bool bVisible = false;

	UPROPERTY(BlueprintReadOnly, Category = "Death")
	FText TitleText = FText::GetEmpty();

	UPROPERTY(BlueprintReadOnly, Category = "Death")
	FText SummaryText = FText::GetEmpty();
};

USTRUCT(BlueprintType)
struct FRoguePauseMenuViewData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Pause")
	bool bVisible = false;

	UPROPERTY(BlueprintReadOnly, Category = "Pause")
	FText TitleText = FText::GetEmpty();
};

USTRUCT(BlueprintType)
struct FRogueSettingsRowViewData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Settings")
	FText LabelText = FText::GetEmpty();

	UPROPERTY(BlueprintReadOnly, Category = "Settings")
	FText ValueText = FText::GetEmpty();

	UPROPERTY(BlueprintReadOnly, Category = "Settings")
	bool bUseAdjustButtons = false;
};

USTRUCT(BlueprintType)
struct FRogueSettingsMenuViewData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Settings")
	bool bVisible = false;

	UPROPERTY(BlueprintReadOnly, Category = "Settings")
	FText TitleText = FText::GetEmpty();

	UPROPERTY(BlueprintReadOnly, Category = "Settings")
	TArray<FRogueSettingsRowViewData> Rows;

	UPROPERTY(BlueprintReadOnly, Category = "Settings")
	FText StatusText = FText::GetEmpty();

	UPROPERTY(BlueprintReadOnly, Category = "Settings")
	bool bCanApply = false;
};

USTRUCT(BlueprintType)
struct FRogueShopOfferViewData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Shop")
	FText TitleText = FText::GetEmpty();

	UPROPERTY(BlueprintReadOnly, Category = "Shop")
	FText DescriptionText = FText::GetEmpty();

	UPROPERTY(BlueprintReadOnly, Category = "Shop")
	FText CostText = FText::GetEmpty();

	UPROPERTY(BlueprintReadOnly, Category = "Shop")
	bool bPurchased = false;

	UPROPERTY(BlueprintReadOnly, Category = "Shop")
	bool bAffordable = false;
};

USTRUCT(BlueprintType)
struct FRogueShopViewData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Shop")
	bool bVisible = false;

	UPROPERTY(BlueprintReadOnly, Category = "Shop")
	FText TitleText = FText::GetEmpty();

	UPROPERTY(BlueprintReadOnly, Category = "Shop")
	FText MoneyText = FText::GetEmpty();

	UPROPERTY(BlueprintReadOnly, Category = "Shop")
	FText HintText = FText::GetEmpty();

	UPROPERTY(BlueprintReadOnly, Category = "Shop")
	FText AutoRefreshText = FText::GetEmpty();

	UPROPERTY(BlueprintReadOnly, Category = "Shop")
	FText RefreshButtonText = FText::GetEmpty();

	UPROPERTY(BlueprintReadOnly, Category = "Shop")
	TArray<FRogueShopOfferViewData> Offers;
};

USTRUCT(BlueprintType)
struct FRogueUpgradeCardViewData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Upgrade")
	FText TitleText = FText::GetEmpty();

	UPROPERTY(BlueprintReadOnly, Category = "Upgrade")
	FText DescriptionText = FText::GetEmpty();
};

USTRUCT(BlueprintType)
struct FRogueUpgradeSelectionViewData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Upgrade")
	bool bVisible = false;

	UPROPERTY(BlueprintReadOnly, Category = "Upgrade")
	FText TitleText = FText::GetEmpty();

	UPROPERTY(BlueprintReadOnly, Category = "Upgrade")
	TArray<FRogueUpgradeCardViewData> Cards;
};

/** 复活倒计时界面的视图数据（C++ 每帧组装，蓝图只读） */
USTRUCT(BlueprintType)
struct FRogueReviveCountdownViewData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Revive")
	bool bVisible = false;

	UPROPERTY(BlueprintReadOnly, Category = "Revive")
	int32 SecondsRemaining = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Revive")
	FText HintText = FText::GetEmpty();
};
