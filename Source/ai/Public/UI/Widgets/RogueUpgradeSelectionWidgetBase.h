#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/Widgets/RogueMenuViewData.h"
#include "UI/Widgets/RogueUpgradeCardWidgetBase.h"
#include "RogueUpgradeSelectionWidgetBase.generated.h"

class ARogueHUD;
class UHorizontalBox;
class UTextBlock;

UCLASS(Abstract, Blueprintable)
class AI_API URogueUpgradeSelectionWidgetBase : public UUserWidget
{
	GENERATED_BODY()

public:
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
