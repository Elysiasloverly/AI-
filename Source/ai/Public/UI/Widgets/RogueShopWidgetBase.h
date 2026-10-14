#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/Widgets/RogueMenuViewData.h"
#include "UI/Widgets/RogueShopOfferCardWidgetBase.h"
#include "RogueShopWidgetBase.generated.h"

class ARogueHUD;
class UButton;
class UTextBlock;
class UUniformGridPanel;

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
