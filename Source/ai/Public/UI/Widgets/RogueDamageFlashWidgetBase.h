#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RogueDamageFlashWidgetBase.generated.h"

class ARogueHUD;
class UWidget;

/**
 * 一次受伤闪烁的强度状态：峰值 + 已过时间，纯值类型。
 *
 * 单独抽出来是为了让"闪多强、多久淡掉"能脱离 UMG 单独验证：
 * 它不碰 Slate、不依赖世界，可以在自动化测试里直接构造比较。
 */
USTRUCT()
struct FRogueDamageFlashState
{
	GENERATED_BODY()

	/** 触发一次闪烁。取峰值而不是覆盖：小伤害不该把大伤害留下的残影冲淡。 */
	void Play(float Intensity, float MinIntensity);

	/** 推进时间并返回当前强度（0~1）。淡出结束会自动失活并清零。 */
	float Advance(float DeltaSeconds, float HoldSeconds, float FadeOutSeconds);

	/** 只读当前强度，不推进时间。 */
	float GetStrength(float HoldSeconds, float FadeOutSeconds) const;

	bool IsActive() const { return bActive; }

private:
	float PeakStrength = 0.0f;
	float ElapsedSincePeak = 0.0f;
	bool bActive = false;
};

/**
 * 玩家受伤时的全屏反馈（屏幕四周泛红）。
 *
 * 分工：
 *   · C++ 只负责"什么时候闪、闪多强、多久淡出"，以及把强度转成根容器的 RenderOpacity；
 *   · WBP 负责外观 —— 渐变色、边缘宽度、是否有噪点/扭曲，都在控件树和材质里改。
 *
 * 用法：
 *   1. 新建 WBP，父类选本类；
 *   2. 在控件树里放一个覆盖全屏的容器，命名 FlashRoot（BindWidgetOptional）；
 *   3. 想自己用 UMG 动画控制透明度时，勾上 bBlueprintHandlesOpacity，
 *      C++ 就不再改 FlashRoot 的透明度，只广播 OnDamageFlash 供动画/音效使用。
 *
 * 强度是"本次伤害 / 最大生命"，0~1。连续受击时取峰值，不会因为小伤害打断大伤害的残留效果。
 */
UCLASS(Abstract, Blueprintable)
class AI_API URogueDamageFlashWidgetBase : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 由 HUD 在检测到本地玩家掉血时调用。Intensity 已按最大生命归一化。 */
	void PlayDamageFlash(float Intensity);

	/** 由 HUD 每帧推进淡出并写透明度。
	 *  没走 UMG 的 NativeTick：泛红层是常驻的全屏控件，由持有者按帧驱动，
	 *  时序与自己写的播放逻辑一致，也不依赖 Slate 是否给这个控件派发 Tick。 */
	void AdvanceFlash(float DeltaSeconds);

	/** 复用菜单 widget 的创建流程（CreateRogueMenuWidget 会对每个 widget 调它）。 */
	void SetOwningRogueHUD(ARogueHUD* InOwningHUD);

	/** 重新加回视口时把视图拉回"未受伤"：泛红层是常驻控件，切关卡/重建 HUD 都不该留下残影。 */
	virtual void NativeConstruct() override;

	/** 当前剩余强度 0~1，供蓝图做附加表现（例如低血量时叠一层常驻警示）。 */
	UFUNCTION(BlueprintPure, Category = "Rogue|Damage Flash")
	float GetFlashStrength() const;

protected:
	/** 蓝图实现：播动画、播放受击音效、触发材质参数。强度是本次伤害的比例。 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Rogue|Damage Flash")
	void OnDamageFlash(float Intensity);

	/** 泛红的载体，通常是一个覆盖全屏的 CanvasPanel / Border / Image。
	 *  命名绑上后，即使 WBP 不写任何动画也能看到闪烁与淡出。 */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> FlashRoot;

	/** 勾上后 C++ 不再改 FlashRoot 的透明度，由 WBP 动画全权控制。 */
	UPROPERTY(EditDefaultsOnly, Category = "Rogue|Damage Flash")
	bool bBlueprintHandlesOpacity = false;

	/** 保持最大不透明度的时长（秒），让瞬间伤害也看得见。 */
	UPROPERTY(EditDefaultsOnly, Category = "Rogue|Damage Flash", meta = (ClampMin = "0.0"))
	float HoldSeconds = 0.06f;

	/** 从峰值淡到 0 的时长（秒）。 */
	UPROPERTY(EditDefaultsOnly, Category = "Rogue|Damage Flash", meta = (ClampMin = "0.05"))
	float FadeOutSeconds = 0.55f;

	/** 满强度时的最大不透明度。泛红颜色越深，这个值应该越小。 */
	UPROPERTY(EditDefaultsOnly, Category = "Rogue|Damage Flash", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MaxOpacity = 0.9f;

	/** 极小伤害也保留一点反馈，避免"掉血了却什么都没发生"。 */
	UPROPERTY(EditDefaultsOnly, Category = "Rogue|Damage Flash", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MinIntensity = 0.2f;

private:
	void EnsureVisible();

	/** 把强度写进 FlashRoot 的透明度。
	 *  **初始状态也必须走这里写一次 0** —— WBP 里 FlashRoot 默认是 RenderOpacity=1 的不透明容器，
	 *  只靠"受伤时再改"会让整屏泛红从进游戏起一直挂着。 */
	void ApplyFlashOpacity(float Strength);

	FRogueDamageFlashState FlashState;

	/** FlashRoot 缺失只提醒一次，避免每帧刷日志。 */
	bool bWarnedMissingFlashRoot = false;

	UPROPERTY(Transient)
	TWeakObjectPtr<ARogueHUD> OwningHUD;
};
