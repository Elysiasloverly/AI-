#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/Widgets/RogueMenuViewData.h"
#include "RogueShopOfferCardWidgetBase.generated.h"

class ARogueHUD;
class UBorder;
class UButton;
class UTextBlock;

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
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Rogue|样式") FLinearColor NormalCardTint = FLinearColor::White;
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Rogue|样式") FLinearColor PurchasedCardTint = FLinearColor(.65f,.67f,.72f,1);
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Rogue|样式") FLinearColor UnaffordableCardTint = FLinearColor(1,.78f,.78f,1);
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
