#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RogueUpgradeTreeNodeWidgetBase.generated.h"

class UBorder;
class UButton;
class UTextBlock;

UENUM(BlueprintType)
enum class ERogueUpgradeTreeNodeState : uint8
{
	Locked UMETA(DisplayName = "前置未解锁"),
	Available UMETA(DisplayName = "可以购买"),
	Unaffordable UMETA(DisplayName = "冰砖不足"),
	Unlocked UMETA(DisplayName = "已解锁，可继续升级"),
	Maxed UMETA(DisplayName = "已完成")
};

USTRUCT(BlueprintType)
struct AI_API FRogueUpgradeTreeNodeViewData
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FName NodeId;
	UPROPERTY(BlueprintReadOnly) FText Title;
	UPROPERTY(BlueprintReadOnly) FText Description;
	UPROPERTY(BlueprintReadOnly) FText Cost;
	UPROPERTY(BlueprintReadOnly) FText Rank;
	UPROPERTY(BlueprintReadOnly) FText StateLabel;
	UPROPERTY(BlueprintReadOnly) FText Prerequisites;
	UPROPERTY(BlueprintReadOnly) ERogueUpgradeTreeNodeState State = ERogueUpgradeTreeNodeState::Locked;
	UPROPERTY(BlueprintReadOnly) int32 CurrentRank = 0;
	UPROPERTY(BlueprintReadOnly) int32 MaximumRank = 1;
	UPROPERTY(BlueprintReadOnly) bool bCanPurchase = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FRogueUpgradeTreeNodePressed, FName, NodeId);

/** 单张节点的排版、字体、按钮、动画都保存在 WBP；此类只应用状态和转发购买意图。 */
UCLASS(Abstract, Blueprintable)
class AI_API URogueUpgradeTreeNodeWidgetBase : public UUserWidget
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category = "Rogue|升级树")
	void SetViewData(const FRogueUpgradeTreeNodeViewData& InViewData);

	/** 优先采用设计器自定义尺寸或根 SizeBox 的实际期望尺寸。 */
	UFUNCTION(BlueprintPure, Category = "Rogue|布局")
	FVector2D GetPreferredLayoutSize(FVector2D FallbackSize) const;

	/** 零表示自动测量；特殊自绘节点可以在本节点 WBP 中明确指定尺寸。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Rogue|布局", meta = (DisplayName = "节点尺寸覆盖（零为自动）"))
	FVector2D LayoutSizeOverride = FVector2D::ZeroVector;

	/** 设计器尺寸在保存时写入运行时数据，打包后仍能使用。 */
	UPROPERTY(VisibleDefaultsOnly, Category = "Rogue|布局")
	FVector2D SavedDesignerSize = FVector2D::ZeroVector;

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Rogue|布局")
	void CaptureDesignerSize();

#if WITH_EDITOR
	virtual void PreSave(FObjectPreSaveContext SaveContext) override;
#endif

	UPROPERTY(BlueprintReadOnly, Category = "Rogue|升级树")
	FRogueUpgradeTreeNodeViewData ViewData;

	UPROPERTY(BlueprintAssignable, Category = "Rogue|升级树")
	FRogueUpgradeTreeNodePressed OnNodePressed;

	/** 勾选后由 OnViewDataChanged 完全控制状态配色。文字和按钮可用性仍自动更新。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Rogue|样式")
	bool bBlueprintHandlesStateStyle = false;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Rogue|样式") FLinearColor LockedColor = FLinearColor(0.10f, 0.12f, 0.19f, 1.0f);
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Rogue|样式") FLinearColor AvailableColor = FLinearColor(0.08f, 0.29f, 0.34f, 1.0f);
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Rogue|样式") FLinearColor UnaffordableColor = FLinearColor(0.22f, 0.18f, 0.15f, 1.0f);
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Rogue|样式") FLinearColor UnlockedColor = FLinearColor(0.13f, 0.24f, 0.33f, 1.0f);
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Rogue|样式") FLinearColor CompletedColor = FLinearColor(0.17f, 0.32f, 0.23f, 1.0f);

	UFUNCTION(BlueprintImplementableEvent, Category = "Rogue|升级树")
	void OnViewDataChanged(const FRogueUpgradeTreeNodeViewData& Data);

protected:
	virtual void NativeConstruct() override;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UBorder> StateBorder;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> PurchaseButton;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> TitleText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> DescriptionText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> CostText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> RankText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> StateText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> PrerequisiteText;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> PurchaseLabel;

private:
	UFUNCTION() void HandlePurchaseClicked();
	void ApplyViewData();
};
