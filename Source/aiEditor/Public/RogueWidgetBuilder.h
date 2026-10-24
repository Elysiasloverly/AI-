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
    UFUNCTION(BlueprintCallable, Category="Rogue|Editor")
    static bool BuildNightTransition(UWidgetBlueprint* Blueprint);
	/** Create the default editable multiplayer page (0 choice, 1 browser, 2 lobby). */
	UFUNCTION(BlueprintCallable, Category="Rogue|Editor")
	static bool BuildMultiplayerPage(UWidgetBlueprint* Blueprint, int32 Page);
	UFUNCTION(BlueprintCallable, Category="Rogue|Editor")
	static bool BuildRoomEntry(UWidgetBlueprint* Blueprint);
	/** Append the menu action to the existing death WBP without rebuilding its widget tree. */
	UFUNCTION(BlueprintCallable, Category = "Rogue|Editor")
	static bool AddDeathScreenMainMenuButton(UWidgetBlueprint* Blueprint);
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

	/** 清空并重建主菜单：背景 + 标题 + 6 个按钮 + 提示文字。
	 *  按钮名与 URogueMainMenuWidgetBase 的可选绑定一致；外观可直接在 WBP 里改。 */
	UFUNCTION(BlueprintCallable, Category = "Rogue|Editor")
	static bool BuildMainMenu(UWidgetBlueprint* Blueprint);

	/** 清空并重建教程页：背景 + 标题 + 可滚动正文 + 返回按钮。 */
	UFUNCTION(BlueprintCallable, Category = "Rogue|Editor")
	static bool BuildTutorial(UWidgetBlueprint* Blueprint);

	/** Update existing tutorial text only; preserve the designer hierarchy and styles. */
	UFUNCTION(BlueprintCallable, Category = "Rogue|Editor")
	static bool UpdateTutorialText(UWidgetBlueprint* Blueprint, const FString& Title, const FString& Body);

	/** 项目统一字体：DroidSansMono 为主 + DroidSansFallback 回退（中文）。
	 *  资产位于 /Game/UMG/Fonts/F_DroidMono_CJK，不存在时创建。 */
	UFUNCTION(BlueprintCallable, Category = "Rogue|Editor")
	static class UFont* GetOrCreateProjectFont();

	/** 把 WBP 里所有 TextBlock 的字体换成项目字体（保留原字号/字重）。 */
	UFUNCTION(BlueprintCallable, Category = "Rogue|Editor")
	static bool ApplyProjectFont(UWidgetBlueprint* Blueprint);

	/** 移动资产（会在旧路径留下 redirector）。整理目录后统一跑
	 *  `UnrealEditor-Cmd -run=ResavePackages -FixupRedirectors` 修引用并删除 redirector。
	 *  OldAssetPath 形如 /Game/UMG/Menus/WBP_X；NewAssetPath 形如 /Game/UMG/Menus/New/WBP_X。 */
	UFUNCTION(BlueprintCallable, Category = "Rogue|Editor")
	static bool MoveAsset(const FString& OldAssetPath, const FString& NewAssetPath);

	/** 返回 "类名:控件名" 列表，便于脚本/日志核对生成结果。 */
	UFUNCTION(BlueprintCallable, Category = "Rogue|Editor")
	static FString DescribeWidgetTree(UWidgetBlueprint* Blueprint);

	/** 返回 TextBlock 名称与字体对象路径，便于核对中文是否用了 CJK 字体。 */
	UFUNCTION(BlueprintCallable, Category = "Rogue|Editor")
	static FString DescribeTextFonts(UWidgetBlueprint* Blueprint);
};
