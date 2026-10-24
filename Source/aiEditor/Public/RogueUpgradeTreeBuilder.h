#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "RogueUpgradeTreeBuilder.generated.h"

class UWidgetBlueprint;

/** 只搭建升级树的两个新 WBP；已有主菜单仅允许定点更新按钮文字，不重建其设计器内容。 */
UCLASS()
class URogueUpgradeTreeBuilder : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category = "Rogue|Editor")
	static bool BuildUpgradeTree(UWidgetBlueprint* Blueprint);
	UFUNCTION(BlueprintCallable, Category = "Rogue|Editor")
	static bool BuildUpgradeTreeNode(UWidgetBlueprint* Blueprint);
	/** 只新增右下角翻页控件并为页脚预留位置，不重建已有页面或卡片。 */
	UFUNCTION(BlueprintCallable, Category = "Rogue|Editor")
	static bool AddPaginationControls(UWidgetBlueprint* Blueprint);
	/** 只改 SkillTreeButton 内现有 TextBlock 的文字，保留主菜单所有控件、布局、动画。 */
	UFUNCTION(BlueprintCallable, Category = "Rogue|Editor")
	static bool UpdateMainMenuEntry(UWidgetBlueprint* Blueprint);
};
