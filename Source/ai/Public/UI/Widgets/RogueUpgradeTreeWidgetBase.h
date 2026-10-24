#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/Widgets/RogueUpgradeTreeNodeWidgetBase.h"
#include "RogueUpgradeTreeWidgetBase.generated.h"

class ARogueHUD;
class UButton;
class UCanvasPanel;
class USizeBox;
class UScrollBox;
class UTextBlock;
class URogueProgressionSubsystem;

/** 持久升级树视图。WBP 提供页面及节点外观，DataAsset 提供节点与坐标，本地子系统负责购买和存档。 */
UCLASS(Abstract, Blueprintable)
class AI_API URogueUpgradeTreeWidgetBase : public UUserWidget
{
	GENERATED_BODY()
public:
	void SetOwningRogueHUD(ARogueHUD* InOwningHUD);
	UFUNCTION(BlueprintCallable, Category = "Rogue|升级树") void RefreshTree();
	UFUNCTION(BlueprintCallable, Category = "Rogue|升级树") void ShowWeaponTree();
	UFUNCTION(BlueprintCallable, Category = "Rogue|升级树") void ShowAttributeTree();
	UFUNCTION(BlueprintCallable, Category = "Rogue|升级树") void PurchaseNode(FName NodeId);
	UFUNCTION(BlueprintCallable, Category = "Rogue|升级树") void CloseTree();
	UFUNCTION(BlueprintPure, Category = "Rogue|升级树") bool IsAttributeTree() const { return bAttributeTree; }
	UFUNCTION(BlueprintCallable, Category = "Rogue|升级树|翻页") void PreviousPage();
	UFUNCTION(BlueprintCallable, Category = "Rogue|升级树|翻页") void NextPage();
	/** 显示页码从 1 开始。两个分类分别记住自己的页码。 */
	UFUNCTION(BlueprintPure, Category = "Rogue|升级树|翻页") int32 GetCurrentPageNumber() const { return (bAttributeTree ? AttributePageIndex : WeaponPageIndex) + 1; }
	UFUNCTION(BlueprintPure, Category = "Rogue|升级树|翻页") int32 GetPageCount() const { return PageCount; }

	UFUNCTION(BlueprintPure, Category = "Rogue|布局")
	bool GetNodeLayoutBounds(FName NodeId, FVector2D& Position, FVector2D& Size) const;

	/** 节点只从此 WBP 创建；修改卡片样式无需改 C++。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|升级树")
	TSubclassOf<URogueUpgradeTreeNodeWidgetBase> NodeWidgetClass;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Rogue|布局", meta = (DisplayName = "无法测量时的备用节点尺寸")) FVector2D NodeSize = FVector2D(288.0f, 204.0f);
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Rogue|布局", meta = (DisplayName = "节点最小间距", ClampMin = "0")) FVector2D NodeSpacing = FVector2D(42.0f, 56.0f);
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Rogue|布局") FVector2D CanvasPadding = FVector2D(36.0f, 36.0f);
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Rogue|连线") FLinearColor LockedConnectionColor = FLinearColor(0.24f, 0.29f, 0.36f, 1.0f);
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Rogue|连线") FLinearColor UnlockedConnectionColor = FLinearColor(0.32f, 0.78f, 0.83f, 1.0f);
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Rogue|连线", meta = (ClampMin = "0.5")) float ConnectionThickness = 2.0f;
	UFUNCTION(BlueprintImplementableEvent, Category = "Rogue|升级树") void OnTreeRefreshed(int32 IceBricks, bool bShowingAttributes);
	UFUNCTION(BlueprintImplementableEvent, Category = "Rogue|升级树") void OnPurchaseResult(FName NodeId, bool bSucceeded, const FText& Message);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UCanvasPanel> TreeCanvas;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UScrollBox> TreeScrollBox;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<USizeBox> TreeContentSize;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> WeaponsTabButton;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> AttributesTabButton;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> CloseButton;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> IceBricksText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> StatusText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> SectionText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> PreviousPageButton;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> NextPageButton;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> PageNumberText;

private:
	UFUNCTION() void HandleProgressionChanged();
	void ChangePage(int32 Delta);
	void UpdatePageControls();
	URogueProgressionSubsystem* ResolveProgression();
	UPROPERTY(Transient) TArray<TObjectPtr<URogueUpgradeTreeNodeWidgetBase>> NodeWidgets;
	TWeakObjectPtr<ARogueHUD> OwningRogueHUD;
	TWeakObjectPtr<URogueProgressionSubsystem> Progression;
	TMap<FName, FVector2D> NodePositions;
	TMap<FName, FVector2D> NodeSizes;
	TMap<FName, TArray<FName>> NodePrerequisites;
	TSet<FName> PurchasedNodes;
	bool bAttributeTree = false;
	int32 WeaponPageIndex = 0;
	int32 AttributePageIndex = 0;
	int32 PageCount = 1;
};
