#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RogueReviveCountdownWidgetBase.generated.h"

class ARogueHUD;

/**
 * 复活倒计时界面基类（联机合作：默认等待 10 秒，实际时长由 DA_PlayerBalance.Revival 配置）。
 *
 * 与死亡结算界面**刻意分开**：
 *   · 这个界面表示"我暂时倒下、还会回来"——只显示倒计时与提示，不带结算/重开按钮；
 *   · WBP_DeathScreen 只在**全员同时倒下**（本局结束）时出现。
 * 可见性来自复制的生命状态；秒数按同一份状态的服务器复活截止时刻计算，蓝图负责样式与动画。
 */
UCLASS(Abstract, Blueprintable)
class AI_API URogueReviveCountdownWidgetBase : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 由 HUD 推送剩余秒数和可见性；隐藏后终止动画并强制收起。 */
	void UpdateReviveCountdown(int32 SecondsRemaining, bool bVisible);

	/** HUD 创建菜单 widget 的统一入口（CreateRogueMenuWidget 会对每个 widget 调它）。
	 *  倒计时界面不需要回指 HUD，这里保留空实现以复用同一套创建流程。 */
	void SetOwningRogueHUD(ARogueHUD* InOwningHUD) { (void)InOwningHUD; }

protected:
	/** 蓝图实现：刷新样式（大号秒数、入场动画等）。
	 *  刻意用**两个普通参数**而不是结构体：蓝图里直接就是两个引脚，
	 *  不需要先 Break Struct —— 少一步、也不容易接错。 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Rogue|Revive")
	void OnReviveCountdownUpdated(int32 SecondsRemaining, bool bVisible);
};
