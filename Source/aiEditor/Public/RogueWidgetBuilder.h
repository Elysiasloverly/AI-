#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "RogueWidgetBuilder.generated.h"

class UWidgetBlueprint;

/**
 * 编辑器侧的 UMG 占位构建工具。
 *
 * 为什么需要它：UE 5.5 的 Python API 能创建 WidgetBlueprint 资产，但 WidgetTree 属性是
 * protected，Python 读不到也无法往里加控件。这里用 C++ 在编辑器模块里完成建树，
 * Python 只负责创建资产和调用本类，样式仍可在 WBP 里继续改。
 */
UCLASS()
class URogueWidgetBuilder : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** 清空并重建玩家状态条：Canvas + 三条进度条、数值文字、左上角波数/金币。 */
	UFUNCTION(BlueprintCallable, Category = "Rogue|Editor")
	static bool BuildPlayerVitals(UWidgetBlueprint* Blueprint);

	/** 清空并重建敌人血条：Canvas + 背景 Border + FillBar。 */
	UFUNCTION(BlueprintCallable, Category = "Rogue|Editor")
	static bool BuildEnemyHealthBar(UWidgetBlueprint* Blueprint);

	/** 清空并重建受伤泛红占位：Canvas + 名为 FlashRoot 的全屏容器，内含中心底色与四条边缘。
	 *  FlashRoot 与 C++ 的可选绑定同名，所以不写事件图也能看到闪烁与淡出。 */
	UFUNCTION(BlueprintCallable, Category = "Rogue|Editor")
	static bool BuildDamageFlash(UWidgetBlueprint* Blueprint);

	/** 返回 "类名:控件名" 列表，便于脚本/日志核对生成结果。 */
	UFUNCTION(BlueprintCallable, Category = "Rogue|Editor")
	static FString DescribeWidgetTree(UWidgetBlueprint* Blueprint);

	/** 返回 TextBlock 名称与字体对象路径，便于核对中文是否用了 CJK 字体。 */
	UFUNCTION(BlueprintCallable, Category = "Rogue|Editor")
	static FString DescribeTextFonts(UWidgetBlueprint* Blueprint);
};
