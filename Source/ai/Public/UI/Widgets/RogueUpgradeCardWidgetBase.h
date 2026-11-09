#pragma once

#include "CoreMinimal.h"
#include "UI/Widgets/RogueCardMotionWidgetBase.h"
#include "UI/Widgets/RogueMenuViewData.h"
#include "RogueUpgradeCardWidgetBase.generated.h"

class ARogueHUD;
class UBorder;
class UButton;
class UTextBlock;

UCLASS(Abstract, Blueprintable)
class AI_API URogueUpgradeCardWidgetBase : public URogueCardMotionWidgetBase
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
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual UButton* GetMotionButton()const override{return Button_Select.Get();}
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Rogue|样式") FLinearColor CardTint = FLinearColor::White;
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
	FGuid SelectionId;
	bool bCanSelect = false;
};
