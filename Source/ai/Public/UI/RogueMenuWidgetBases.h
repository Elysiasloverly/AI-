#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RogueMenuWidgetBases.generated.h"

class ARogueHUD;
class UBorder;
class UButton;
class UHorizontalBox;
class UTextBlock;
class UUniformGridPanel;

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

UCLASS(Abstract, Blueprintable)
class AI_API URogueShopOfferCardWidgetBase : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;

	UFUNCTION(BlueprintCallable, Category = "Rogue|Shop")
	void InitializeOfferCard(ARogueHUD* InOwningHUD, int32 InOfferIndex);

	UFUNCTION(BlueprintCallable, Category = "Rogue|Shop")
	void UpdateOfferView(const FRogueShopOfferViewData& InViewData);

	UFUNCTION(BlueprintCallable, Category = "Rogue|Shop")
	void RequestPurchaseOffer();

	UFUNCTION(BlueprintPure, Category = "Rogue|Shop")
	int32 GetOfferIndex() const { return OfferIndex; }

protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "Rogue|Shop")
	void OnOfferViewUpdated(const FRogueShopOfferViewData& InViewData);

private:
	UFUNCTION()
	void HandleOfferButtonClicked();

	void ApplyManagedOfferView(const FRogueShopOfferViewData& InViewData);

	UPROPERTY(BlueprintReadOnly, Category = "Rogue|Shop", meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> Button_Offer = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Rogue|Shop", meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UBorder> Border_Card = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Rogue|Shop", meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> Text_Title = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Rogue|Shop", meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> Text_Desc = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Rogue|Shop", meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> Text_Cost = nullptr;

	TWeakObjectPtr<ARogueHUD> OwningRogueHUD;
	int32 OfferIndex = INDEX_NONE;
};

UCLASS(Abstract, Blueprintable)
class AI_API URogueUpgradeCardWidgetBase : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;

	UFUNCTION(BlueprintCallable, Category = "Rogue|Upgrade")
	void InitializeUpgradeCard(ARogueHUD* InOwningHUD, int32 InUpgradeIndex);

	UFUNCTION(BlueprintCallable, Category = "Rogue|Upgrade")
	void UpdateUpgradeCardView(const FRogueUpgradeCardViewData& InViewData);

	UFUNCTION(BlueprintCallable, Category = "Rogue|Upgrade")
	void RequestSelectUpgrade();

	UFUNCTION(BlueprintPure, Category = "Rogue|Upgrade")
	int32 GetUpgradeIndex() const { return UpgradeIndex; }

protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "Rogue|Upgrade")
	void OnUpgradeCardViewUpdated(const FRogueUpgradeCardViewData& InViewData);

private:
	UFUNCTION()
	void HandleSelectButtonClicked();

	void ApplyManagedUpgradeCardView(const FRogueUpgradeCardViewData& InViewData);

	UPROPERTY(BlueprintReadOnly, Category = "Rogue|Upgrade", meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> Button_Select = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Rogue|Upgrade", meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UBorder> Border_Card = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Rogue|Upgrade", meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> Text_Title = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Rogue|Upgrade", meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> Text_Desc = nullptr;

	TWeakObjectPtr<ARogueHUD> OwningRogueHUD;
	int32 UpgradeIndex = INDEX_NONE;
};

UCLASS(Abstract, Blueprintable)
class AI_API URoguePauseMenuWidgetBase : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetOwningRogueHUD(ARogueHUD* InOwningHUD);
	void UpdatePauseView(const FRoguePauseMenuViewData& InViewData);

	UFUNCTION(BlueprintCallable, Category = "Rogue|Pause")
	void RequestResumeGame();

	UFUNCTION(BlueprintCallable, Category = "Rogue|Pause")
	void RequestOpenSettings();

	UFUNCTION(BlueprintCallable, Category = "Rogue|Pause")
	void RequestQuitGame();

protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "Rogue|Pause")
	void OnPauseViewUpdated(const FRoguePauseMenuViewData& InViewData);

private:
	TWeakObjectPtr<ARogueHUD> OwningRogueHUD;
};

UCLASS(Abstract, Blueprintable)
class AI_API URogueUpgradeSelectionWidgetBase : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;

	void SetOwningRogueHUD(ARogueHUD* InOwningHUD);
	void UpdateUpgradeSelectionView(const FRogueUpgradeSelectionViewData& InViewData);

	UFUNCTION(BlueprintPure, Category = "Rogue|Upgrade")
	ARogueHUD* GetOwningRogueHUDObject() const { return OwningRogueHUD.Get(); }

protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "Rogue|Upgrade")
	void OnUpgradeSelectionViewUpdated(const FRogueUpgradeSelectionViewData& InViewData);

private:
	void ApplyManagedUpgradeSelectionView(const FRogueUpgradeSelectionViewData& InViewData);
	void EnsureUpgradeCardWidgets(int32 DesiredCount);
	TSubclassOf<URogueUpgradeCardWidgetBase> ResolveUpgradeCardWidgetClass();

	UPROPERTY(BlueprintReadOnly, Category = "Rogue|Upgrade", meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> Text_Title = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Rogue|Upgrade", meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UHorizontalBox> HorizontalBox_Cards = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Rogue|Upgrade")
	TSubclassOf<URogueUpgradeCardWidgetBase> ManagedUpgradeCardWidgetClass;

	UPROPERTY(Transient)
	TArray<TObjectPtr<URogueUpgradeCardWidgetBase>> ManagedUpgradeCardWidgets;

	TWeakObjectPtr<ARogueHUD> OwningRogueHUD;
};

UCLASS(Abstract, Blueprintable)
class AI_API URogueShopWidgetBase : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;

	void SetOwningRogueHUD(ARogueHUD* InOwningHUD);
	void UpdateShopView(const FRogueShopViewData& InViewData);

	UFUNCTION(BlueprintCallable, Category = "Rogue|Shop")
	void UpdateAutoRefreshText(const FText& InAutoRefreshText);

	UFUNCTION(BlueprintPure, Category = "Rogue|Shop")
	ARogueHUD* GetOwningRogueHUDObject() const { return OwningRogueHUD.Get(); }

	UFUNCTION(BlueprintCallable, Category = "Rogue|Shop")
	void RequestSelectOffer(int32 OfferIndex);

	UFUNCTION(BlueprintCallable, Category = "Rogue|Shop")
	void RequestRefreshShop();

	UFUNCTION(BlueprintCallable, Category = "Rogue|Shop")
	void RequestCloseShop();

protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "Rogue|Shop")
	void OnShopViewUpdated(const FRogueShopViewData& InViewData);

	UFUNCTION(BlueprintImplementableEvent, Category = "Rogue|Shop")
	void OnAutoRefreshTextUpdated(const FText& InAutoRefreshText);

private:
	UFUNCTION()
	void HandleRefreshButtonClicked();

	UFUNCTION()
	void HandleCloseButtonClicked();

	void ApplyManagedShopView(const FRogueShopViewData& InViewData);
	void EnsureOfferCardWidgets(int32 DesiredCount);
	TSubclassOf<URogueShopOfferCardWidgetBase> ResolveOfferCardWidgetClass();

	UPROPERTY(BlueprintReadOnly, Category = "Rogue|Shop", meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> Text_Title = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Rogue|Shop", meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> Text_Money = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Rogue|Shop", meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> Text_Hint = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Rogue|Shop", meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> Text_AutoRefresh = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Rogue|Shop", meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UTextBlock> Text_Refresh = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Rogue|Shop", meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UUniformGridPanel> UniformGridPanel_Offers = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Rogue|Shop", meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> Button_Refresh = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Rogue|Shop", meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
	TObjectPtr<UButton> Button_Close = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "Rogue|Shop")
	TSubclassOf<URogueShopOfferCardWidgetBase> ManagedOfferCardWidgetClass;

	UPROPERTY(Transient)
	TArray<TObjectPtr<URogueShopOfferCardWidgetBase>> ManagedOfferCardWidgets;

	TWeakObjectPtr<ARogueHUD> OwningRogueHUD;
};

UCLASS(Abstract, Blueprintable)
class AI_API URogueSettingsMenuWidgetBase : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetOwningRogueHUD(ARogueHUD* InOwningHUD);
	void UpdateSettingsView(const FRogueSettingsMenuViewData& InViewData);

	UFUNCTION(BlueprintCallable, Category = "Rogue|Settings")
	void RequestBackToPauseMenu();

	UFUNCTION(BlueprintCallable, Category = "Rogue|Settings")
	void RequestApplySettings();

	UFUNCTION(BlueprintCallable, Category = "Rogue|Settings")
	void RequestVolumeDown();

	UFUNCTION(BlueprintCallable, Category = "Rogue|Settings")
	void RequestVolumeUp();

	UFUNCTION(BlueprintCallable, Category = "Rogue|Settings")
	void RequestQualityDown();

	UFUNCTION(BlueprintCallable, Category = "Rogue|Settings")
	void RequestQualityUp();

	UFUNCTION(BlueprintCallable, Category = "Rogue|Settings")
	void RequestResolutionDown();

	UFUNCTION(BlueprintCallable, Category = "Rogue|Settings")
	void RequestResolutionUp();

	UFUNCTION(BlueprintCallable, Category = "Rogue|Settings")
	void RequestFrameLimitDown();

	UFUNCTION(BlueprintCallable, Category = "Rogue|Settings")
	void RequestFrameLimitUp();

	UFUNCTION(BlueprintCallable, Category = "Rogue|Settings")
	void RequestToggleDisplayMode();

protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "Rogue|Settings")
	void OnSettingsViewUpdated(const FRogueSettingsMenuViewData& InViewData);

private:
	TWeakObjectPtr<ARogueHUD> OwningRogueHUD;
};

UCLASS(Abstract, Blueprintable)
class AI_API URogueDeathScreenWidgetBase : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetOwningRogueHUD(ARogueHUD* InOwningHUD);
	void UpdateDeathView(const FRogueDeathViewData& InViewData);

	UFUNCTION(BlueprintCallable, Category = "Rogue|Death")
	void RequestRestartRound();

	UFUNCTION(BlueprintCallable, Category = "Rogue|Death")
	void RequestQuitGame();

protected:
	UFUNCTION(BlueprintImplementableEvent, Category = "Rogue|Death")
	void OnDeathViewUpdated(const FRogueDeathViewData& InViewData);

private:
	TWeakObjectPtr<ARogueHUD> OwningRogueHUD;
};
