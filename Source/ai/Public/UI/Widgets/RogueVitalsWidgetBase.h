#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/Widgets/RogueHUDViewData.h"
#include "RogueVitalsWidgetBase.generated.h"

class ARogueHUD;
class UProgressBar;
class UTextBlock;

/**
 * 本地玩家状态条基类（生命 / 护甲 / 经验）。
 *
 * 与菜单类 widget 的区别：
 *   · 它常驻视口，不参与输入模式切换，也不吃点击；
 *   · HUD 每帧推视图数据，基类做脏检查后只把**变化**转给蓝图；
 *   · 蓝图负责外观（进度条、颜色、动画），C++ 不碰任何 Slate 组件。
 *
 * 用法：新建 WBP，父类选本类，实现 OnVitalsUpdated；把 WBP 配到 BP_RogueHUD 的
 * VitalsWidgetClass，或放到 /Game/UMG/HUD/WBP_PlayerVitals 让 HUD 默认加载。
 */
UCLASS(Abstract, Blueprintable)
class AI_API URogueVitalsWidgetBase : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 由 HUD 每帧调用；数值未变化时不触发蓝图事件。 */
	void UpdateVitals(const FRoguePlayerVitalsView& InView);

	/** 复用菜单 widget 的创建流程（CreateRogueMenuWidget 会对每个 widget 调它）。 */
	void SetOwningRogueHUD(ARogueHUD* InOwningHUD);

	/** 蓝图只读的最近一次视图数据（便于动画/延迟逻辑读取）。 */
	const FRoguePlayerVitalsView& GetCachedVitals() const { return CachedView; }

protected:
	/** 蓝图实现：刷新三条进度条与可选文字。 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Rogue|Vitals")
	void OnVitalsUpdated(const FRoguePlayerVitalsView& View);

	/** 可选的命名绑定：WBP 里放同名 ProgressBar 就会由 C++ 直接更新，
	 *  这样即使不写事件图，占位 WBP 也能跑；蓝图仍可通过 OnVitalsUpdated 追加样式。 */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> HealthBar;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> ArmorBar;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> ExperienceBar;

	/** 可选文字：沿用占位 WBP 的命名绑定，C++ 直接填数值。 */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> HealthText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ArmorText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ExperienceText;

	/** 左上角：波数与金币。 */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> WaveText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> GoldText;

private:
	UPROPERTY(Transient)
	TWeakObjectPtr<ARogueHUD> OwningHUD;

	UPROPERTY(Transient)
	FRoguePlayerVitalsView CachedView;

	bool bHasView = false;
};
