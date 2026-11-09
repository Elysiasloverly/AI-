#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/Widgets/RogueHUDViewData.h"
#include "UI/Widgets/RogueBarAnimation.h"
#include "UI/Widgets/RogueVitalsFeedback.h"
#include "RogueVitalsWidgetBase.generated.h"

class ARogueHUD;
class UProgressBar;
class UTextBlock;
class UBorder;
class URogueDashRingWidget;

/**
 * 本地玩家状态条基类（生命 / 护甲 / 经验）。
 *
 * 与菜单类 widget 的区别：
 *   · 它常驻视口，不参与输入模式切换，也不吃点击；
 *   · HUD 每帧推视图数据，基类做脏检查后只把**变化**转给蓝图；
 *   · 蓝图负责外观（进度条、颜色、动画），C++ 更新可选绑定控件及平滑比例，样式保存在 WBP。
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
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> PendingUpgradesText;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> PendingUpgradeHintText;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<URogueDashRingWidget> PlayerDashRing;
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Rogue|升级提示", meta=(DisplayName="呼吸周期",ClampMin="0.5",Units="s"))
    float PendingUpgradePulsePeriod = 1.8f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Rogue|升级提示", meta=(DisplayName="最低透明度",ClampMin="0.1",ClampMax="1"))
    float PendingUpgradeMinimumOpacity = .35f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Rogue|角色旁冲刺条", meta=(DisplayName="屏幕偏移",ToolTip="UI 坐标，X 为正时位于角色右侧；不随角色朝向旋转。"))
    FVector2D DashRingScreenOffset = FVector2D(24.f, 0.f);
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Rogue|角色旁冲刺条", meta=(DisplayName="世界高度偏移",Units="cm"))
    float DashRingWorldHeightOffset = 20.f;
    /** 本机玩家当前等级；样式保留在 WBP，不复用会淡出的升级提示。 */
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> LevelText;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> RemainingEnemiesText;
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Rogue|界面配色", meta=(DisplayName="冲刺就绪文字颜色"))
    FLinearColor DashReadyTextColor = FLinearColor(.38f,.82f,.96f,1.f);
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Rogue|界面配色", meta=(DisplayName="冲刺状态文字颜色",ToolTip="冷却、不可用等状态的文字颜色，与进度条独立。"))
    FLinearColor DashStatusTextColor = FLinearColor(.76f,.81f,.87f,1.f);
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Rogue|界面配色", meta=(DisplayName="回血数字颜色"))
    FLinearColor RestoreFeedbackTextColor = FLinearColor(.45f,1.f,.7f,1.f);
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Rogue|冲刺提示", meta=(DisplayName="冲刺就绪颜色"))
    FLinearColor DashReadyColor = FLinearColor(.38f, .82f, .96f, 1.f);
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Rogue|冲刺提示", meta=(DisplayName="冲刺冷却颜色"))
    FLinearColor DashCooldownColor = FLinearColor(.24f, .47f, .65f, 1.f);
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Rogue|冲刺提示", meta=(DisplayName="冲刺不可用颜色"))
    FLinearColor DashUnavailableColor = FLinearColor(.33f, .36f, .42f, 1.f);
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UWidget> DashCooldownRow;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UProgressBar> DashCooldownBar;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> DashCooldownText;
    virtual void NativeTick(const FGeometry& Geometry, float DeltaSeconds) override;
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Rogue|状态反馈", meta=(DisplayName="启用状态变化反馈"))
    bool bEnableChangeFeedback=true;
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Rogue|状态反馈", meta=(DisplayName="显示短暂变化数值"))
    bool bShowChangeNumbers=true;
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Rogue|状态反馈", meta=(DisplayName="低生命轻微呼吸提示"))
    bool bLowHealthBreathing=true;
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Rogue|状态反馈", meta=(DisplayName="行强调持续时间",ClampMin="0.05",Units="s"))
    float ChangeFeedbackDuration=.28f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Rogue|状态反馈", meta=(DisplayName="变化数值停留时间",ClampMin="0.1",Units="s"))
    float ChangeNumberDuration=.7f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Rogue|状态反馈", meta=(DisplayName="行强调缩放幅度",ClampMin="0",ClampMax="0.03"))
    float ChangeScaleAmount=.012f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Rogue|状态反馈", meta=(DisplayName="变化数值上浮距离",ClampMin="0",ClampMax="24"))
    float ChangeNumberRise=10.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Rogue|状态反馈", meta=(DisplayName="回血提示最小变化量",ClampMin="0.01"))
    float MinimumRestoreFeedback=1.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Rogue|状态反馈", meta=(DisplayName="低生命提示阈值",ClampMin="0",ClampMax="1"))
    float LowHealthThreshold=.25f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Rogue|血条动画", meta=(DisplayName="经验增长速度",ClampMin="0"))
    float ExperienceInterpolationSpeed=10.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Rogue|血条动画", meta=(DisplayName="升级满条过渡时间",ClampMin="0.05",Units="s"))
    float ExperienceWrapDuration=.24f;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UWidget> HealthRow;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UWidget> ArmorRow;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UWidget> ExperienceRow;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UBorder> HealthFeedback;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UBorder> ArmorFeedback;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UBorder> ExperienceFeedback;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> HealthChangeText;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> ArmorChangeText;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> ExperienceChangeText;
    /** 关闭时按真实比例即时显示；只影响外观，不影响数值及伤害结算。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Rogue|血条动画", meta=(DisplayName="启用平滑血条"))
    bool bSmoothBars = true;
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Rogue|血条动画", meta=(DisplayName="主血条变化速度", ClampMin="0"))
    float BarInterpolationSpeed = 14.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Rogue|血条动画", meta=(DisplayName="受击残影停留时间", ClampMin="0", Units="s"))
    float DamageTrailDelay = .2f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Rogue|血条动画", meta=(DisplayName="受击残影消退速度", ClampMin="0"))
    float DamageTrailSpeed = 4.f;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UProgressBar> HealthDamageBar;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UProgressBar> ArmorDamageBar;
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
    /** 可选分离数值绑定。存在时原 Text 仅显示名称；旧 WBP 仍显示完整的一行。 */
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> HealthValueText;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> ArmorValueText;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> ArmorUnavailableText;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> ExperienceValueText;

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

    friend struct FRogueUIPresentationTestAccess;
    friend struct FRogueVitalsFeedbackTestAccess;
    FRogueBarAnimation HealthAnimation, ArmorAnimation, ExperienceAnimation;
    FRogueVitalsFeedback HealthChange, ArmorChange, ExperienceChange;
    FWidgetTransform HealthRowBase, ArmorRowBase, ExperienceRowBase;
    FVector2D HealthNumberBase, ArmorNumberBase, ExperienceNumberBase;
    FLinearColor HealthFeedbackColor, ArmorFeedbackColor, ExperienceFeedbackColor;
    FSlateColor HealthNumberColor, ArmorNumberColor, ExperienceNumberColor;
    bool bFeedbackBaseCached=false, bExperienceWrapPending=false;
    float ExperienceWrapRemaining=0, ExperienceWrapTarget=0, ExperienceWrapFrom=0, ExperienceFullHoldRemaining=0, FeedbackClock=0;
    void ApplyAnimatedBars();
    void ApplyDashView();
    void UpdateDashRingPosition(const FGeometry& Geometry, float DeltaSeconds);
    void ApplyPendingUpgradeHint();
    float PendingUpgradePulseTime = 0.f;
    void ApplyFeedback();
    void ResetFeedback();
    void CacheFeedbackBase();
	bool bHasView = false;
};
