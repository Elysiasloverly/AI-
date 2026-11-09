#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Shared/RogueTypes.h"
#include "RogueOwnedItemsWidget.generated.h"
class ARogueCharacter;
class ARoguePlayerState;
class URoguePresentationTheme;
class UCanvasPanel;
class UImage;
class UTextBlock;

USTRUCT(BlueprintType)
struct AI_API FRogueOwnedItemView
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) ERogueUpgradeType Type=ERogueUpgradeType::AttackPower;
    UPROPERTY(BlueprintReadOnly) bool bWeapon=false;
    /** Weapon entries show actual effective quantity; all other entries show successful pick count. */
    UPROPERTY(BlueprintReadOnly) int32 Count=0;
    UPROPERTY(BlueprintReadOnly) int32 PickCount=0;
    UPROPERTY(BlueprintReadOnly) FText Name;
};

namespace RogueOwnedItems
{
    AI_API bool IsWeaponType(ERogueUpgradeType Type);
    AI_API TArray<FRogueOwnedItemView> BuildView(const ARogueCharacter* Player,const ARoguePlayerState* State);
}

/** Saved WBP icon tile. Only the graphic hops; layout and count remain stable. */
UCLASS(Abstract,Blueprintable)
class AI_API URogueOwnedItemWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    void SetItem(const FRogueOwnedItemView& Item, const URoguePresentationTheme* Theme, int32 DisplayCount = INDEX_NONE);
    int32 GetDisplayedCount() const { return DisplayedCount; }
    void PlayAcquisitionPulse();
    UFUNCTION(BlueprintCallable,Category="收藏栏") void PlayAttackPulse();
    void AdvanceMotion(float Delta);
    UPROPERTY(BlueprintReadOnly,Category="收藏栏") FRogueOwnedItemView View;
    UPROPERTY(EditDefaultsOnly,BlueprintReadWrite,Category="收藏栏|动效",meta=(DisplayName="跳动高度",ClampMin="0",ClampMax="16")) float HopHeight=8;
    UPROPERTY(EditDefaultsOnly,BlueprintReadWrite,Category="收藏栏|动效",meta=(DisplayName="蓝图接管动效")) bool bBlueprintHandlesMotion=false;
protected:
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UImage> ItemIcon;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> StackText;
    UFUNCTION(BlueprintImplementableEvent,Category="收藏栏") void OnAttackPulse();
    UFUNCTION(BlueprintImplementableEvent,Category="收藏栏") void OnItemUpdated(const FRogueOwnedItemView& Item);
private:
    friend struct FRogueOwnedItemsTestAccess;
    float Offset=0,Velocity=0,AppearanceAge=0,CountAge=10;
    int32 DisplayedCount = INDEX_NONE;
    bool bInitialized=false;
    bool bAnimating=false;
};

/** One local player's run inventory. Layout is right to left, then upward, with bounded columns. */
UCLASS(Abstract,Blueprintable)
class AI_API URogueOwnedItemsWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    void RefreshItems();
    /** HUD 传入状态栏的实际右边缘，换行时为中间状态条留白。 */
    void SetVitalsExclusionRightEdge(float RightEdge);
    void NotifyWeaponAttack(ERogueUpgradeType Type);
    void RememberCardOrigin(const FGuid& ReceiptId, ERogueUpgradeType Type, const UWidget* Card);
    void ConfirmCardAcquired(ERogueUpgradeType Type, const FGuid& ReceiptId, int32 PickCount, int32 DisplayCount);
    UFUNCTION(BlueprintPure,Category="收藏栏") const TArray<FRogueOwnedItemView>& GetItems()const{return Items;}
    UFUNCTION(BlueprintPure,Category="收藏栏") float GetOccupiedHeight()const{return Items.IsEmpty()?0:OccupiedHeight+BottomPadding;}
    UPROPERTY(EditDefaultsOnly,BlueprintReadWrite,Category="收藏栏") TObjectPtr<URoguePresentationTheme> PresentationTheme;
    UPROPERTY(EditDefaultsOnly,BlueprintReadWrite,Category="收藏栏") TSubclassOf<URogueOwnedItemWidget> ItemWidgetClass;
    UPROPERTY(EditDefaultsOnly,BlueprintReadWrite,Category="收藏栏",meta=(ClampMin="32",ClampMax="80")) float IconSize=52;
    UPROPERTY(EditDefaultsOnly,BlueprintReadWrite,Category="收藏栏",meta=(ClampMin="0",ClampMax="20")) float Gap=7;
    UPROPERTY(EditDefaultsOnly,BlueprintReadWrite,Category="收藏栏",meta=(ClampMin="4",ClampMax="14")) int32 MaxColumns=12;
    UPROPERTY(EditDefaultsOnly,BlueprintReadWrite,Category="收藏栏") float RightPadding=24,BottomPadding=24;
    UPROPERTY(EditDefaultsOnly,BlueprintReadWrite,Category="收藏栏",meta=(DisplayName="左侧状态栏留白")) float ReservedLeftWidth=350;
protected:
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& Geometry,float Delta) override;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UCanvasPanel> ItemsCanvas;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UImage> AcquisitionIcon;
private:
    friend struct FRogueOwnedItemsTestAccess;
    UPROPERTY(Transient) TArray<TObjectPtr<URogueOwnedItemWidget>> Tiles;
    TArray<FRogueOwnedItemView> Items;
    TMap<ERogueUpgradeType,double> PendingAttacks;
    TWeakObjectPtr<ARoguePlayerState> SourceState;
    float RefreshAge=1,LastWidth=-1,OccupiedHeight=0,VitalsExclusionRightEdge=0;
    void ApplyItems(const TArray<FRogueOwnedItemView>& NewItems);
    void Arrange(float Width);
    void TickAcquisition(const FGeometry& Geometry, float Delta);
    struct FCardOrigin
    {
        ERogueUpgradeType Type;
        FVector2D NormalizedPosition;
        float Age = 0.f;
    };
    struct FCardFlight
    {
        ERogueUpgradeType Type;
        FVector2D NormalizedOrigin;
        int32 PickCount = 0;
        int32 DisplayCount = 0;
        float WaitAge = 0.f;
    };
    TMap<FGuid, FCardOrigin> CardOrigins;
    TMap<ERogueUpgradeType, int32> DeferredCounts;
    TSet<FGuid> ConfirmedReceipts;
    TArray<FCardFlight> AcquisitionQueue;
    TWeakObjectPtr<URogueOwnedItemWidget> AcquisitionTarget;
    float AcquisitionAge = 0.f;
    void FinishAcquisition(bool bArrived);
};
