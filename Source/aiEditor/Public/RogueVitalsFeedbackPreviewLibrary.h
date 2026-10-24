#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "RogueVitalsFeedbackPreviewLibrary.generated.h"

class AHUD;
class APlayerController;
class UTextBlock;
class URogueVitalsWidgetBase;
class URogueEnemyHealthBarWidgetBase;

/** 编辑器模块专用预览标签，不会进入打包游戏。 */
UCLASS(Transient)
class URogueVitalsPreviewLabels : public UUserWidget
{
	GENERATED_BODY()
public:
	void SetPhaseCaption(const FText& Caption);
protected:
	virtual void NativeOnInitialized() override;
private:
	UPROPERTY(Transient) TObjectPtr<UTextBlock> PhaseCaption;
};

/** 强引用仅保留当前预览控件；World / HUD / 原有 UI 都是弱引用。 */
UCLASS(Transient)
class URogueVitalsPreviewSession : public UObject
{
	GENERATED_BODY()
public:
	UPROPERTY(Transient) TWeakObjectPtr<UWorld> World;
	UPROPERTY(Transient) TWeakObjectPtr<APlayerController> Owner;
	UPROPERTY(Transient) TWeakObjectPtr<AHUD> HUD;
	UPROPERTY(Transient) TArray<TWeakObjectPtr<URogueVitalsWidgetBase>> OriginalVitals;
	UPROPERTY(Transient) TObjectPtr<URogueVitalsWidgetBase> Player;
	UPROPERTY(Transient) TArray<TObjectPtr<URogueEnemyHealthBarWidgetBase>> Enemies;
	UPROPERTY(Transient) TObjectPtr<URogueVitalsPreviewLabels> Labels;
	FGuid EnemyIds[3];
	uint32 EnemyGenerations[3] = { 1, 1, 1 };
	bool bOriginalHUDTick = false;
	bool bOriginalSpawningSuspended = false;
	int32 Phase = 0;
};

/** 本地样例 View 驱动，不生成怪物、不修改 GAS、地图或任何存档。 */
UCLASS()
class URogueVitalsFeedbackPreviewLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	/** 创建真正的 PlayerVitals / EnemyHealthBar WBP；NativeTick 正常运行。 */
	UFUNCTION(BlueprintCallable, Category = "Rogue|Editor|Preview")
	static UUserWidget* CreatePreview(APlayerController* Owner);
	/** 0 满状态；1 护甲受击；2 掉血与破甲；3 恢复；4 经验；5 升级；6 无护甲/敌条复用；7 濒死；8 复活。 */
	UFUNCTION(BlueprintCallable, Category = "Rogue|Editor|Preview")
	static bool SetPreviewPhase(APlayerController* Owner, int32 Phase);
	UFUNCTION(BlueprintCallable, Category = "Rogue|Editor|Preview")
	static FString DescribePreview(APlayerController* Owner);
	/** 清理预览并恢复原 HUD Tick 与原有玩家状态条。 */
	UFUNCTION(BlueprintCallable, Category = "Rogue|Editor|Preview")
	static void ClearPreview(APlayerController* Owner);
};
