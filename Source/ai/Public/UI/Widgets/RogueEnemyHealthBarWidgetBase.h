#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/Widgets/RogueHUDViewData.h"
#include "RogueEnemyHealthBarWidgetBase.generated.h"

class ARogueHUD;
class UProgressBar;

/**
 * 单个敌人血条基类。
 *
 * HUD 维护一个对象池（最多几十个），每帧把可见敌人的血条视图推给池中的 widget：
 *   · 位置来自屏幕投影，widget 以顶部中心对齐到该点；
 *   · 显示/隐藏由 bVisible 控制；
 *   · 尺寸由 WBP 的 BarSize 决定（默认 70×12），不要按敌人数量扩容。
 *
 * 用法：新建 WBP，父类选本类，设计血条外观并实现 OnBarUpdated；
 * 配到 BP_RogueHUD 的 EnemyHealthBarWidgetClass，或放到
 * /Game/UMG/HUD/WBP_EnemyHealthBar 让 HUD 默认加载。
 */
UCLASS(Abstract, Blueprintable)
class AI_API URogueEnemyHealthBarWidgetBase : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 由 HUD 每帧调用；负责可见性、位置和蓝图刷新。 */
	void UpdateBar(const FRogueEnemyHealthBarView& InView);

	/** 复用菜单 widget 的创建流程。 */
	void SetOwningRogueHUD(ARogueHUD* InOwningHUD);

	/** 血条在屏幕上的尺寸（像素），HUD 创建时用它设置 viewport 大小。 */
	UFUNCTION(BlueprintPure, Category = "Rogue|EnemyBar")
	FVector2D GetBarSize() const { return BarSize; }

	const FRogueEnemyHealthBarView& GetCachedBar() const { return CachedView; }

protected:
	/** 蓝图实现：按 HealthPercent / bBoss 刷新进度条与配色。 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Rogue|EnemyBar")
	void OnBarUpdated(const FRogueEnemyHealthBarView& View);

	/** 可选命名绑定：WBP 里放名为 FillBar 的 ProgressBar，C++ 会直接设百分比。 */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> FillBar;

	/** 在 WBP 类默认值里改；这是 viewport 布局尺寸，不是进度条本身的大小。 */
	UPROPERTY(EditDefaultsOnly, Category = "Rogue|EnemyBar", meta = (DisplayName = "血条屏幕尺寸"))
	FVector2D BarSize = FVector2D(70.0f, 12.0f);

private:
	UPROPERTY(Transient)
	TWeakObjectPtr<ARogueHUD> OwningHUD;

	UPROPERTY(Transient)
	FRogueEnemyHealthBarView CachedView;
};
