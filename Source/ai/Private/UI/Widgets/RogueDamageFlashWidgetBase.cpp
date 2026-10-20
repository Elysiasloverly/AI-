#include "UI/Widgets/RogueDamageFlashWidgetBase.h"

#include "Components/Widget.h"
#include "UI/RogueHUD.h"

void FRogueDamageFlashState::Play(float Intensity, float MinIntensity)
{
	// 取峰值：连续受击时，弱的那一下不能把强的残留压下去。
	const float Requested = FMath::Clamp(Intensity, 0.0f, 1.0f);
	const float Floor = FMath::Clamp(MinIntensity, 0.0f, 1.0f);
	PeakStrength = FMath::Max(PeakStrength, FMath::Max(Requested, Floor));
	ElapsedSincePeak = 0.0f;
	bActive = PeakStrength > 0.0f;
}

float FRogueDamageFlashState::GetStrength(float HoldSeconds, float FadeOutSeconds) const
{
	if (!bActive || PeakStrength <= 0.0f)
	{
		return 0.0f;
	}

	if (ElapsedSincePeak <= HoldSeconds)
	{
		return PeakStrength;
	}

	const float FadeAlpha = 1.0f - (ElapsedSincePeak - HoldSeconds) / FMath::Max(FadeOutSeconds, KINDA_SMALL_NUMBER);
	return FMath::Clamp(PeakStrength * FadeAlpha, 0.0f, 1.0f);
}

float FRogueDamageFlashState::Advance(float DeltaSeconds, float HoldSeconds, float FadeOutSeconds)
{
	if (!bActive)
	{
		return 0.0f;
	}

	ElapsedSincePeak += FMath::Max(DeltaSeconds, 0.0f);
	const float Strength = GetStrength(HoldSeconds, FadeOutSeconds);

	if (Strength <= 0.0f)
	{
		// 淡出结束就失活并清零，下一次受击重新从当前伤害算强度。
		bActive = false;
		PeakStrength = 0.0f;
	}

	return Strength;
}

void URogueDamageFlashWidgetBase::SetOwningRogueHUD(ARogueHUD* InOwningHUD)
{
	OwningHUD = InOwningHUD;
	EnsureVisible();

	// 关键：把"未受伤"的逻辑状态同步到视图。
	// FlashRoot 在 WBP 里默认 RenderOpacity = 1（Border 自带颜色），而创建流程只会把它设成可见，
	// 不会碰透明度。少了这一句，整屏泛红会从进游戏一直挂着 —— 因为 AdvanceFlash 在
	// FlashState 未激活时直接 return，永远走不到归零那一句，只有真挨过一次伤害才恢复。
	ApplyFlashOpacity(0.0f);
}

void URogueDamageFlashWidgetBase::NativeConstruct()
{
	Super::NativeConstruct();

	// 重新加回视口（切关卡、读档、HUD 重建）时同样回到当前逻辑强度：
	// 正常是 0，若确实在淡出中则保留残影，不会把正在播放的效果抹掉。
	ApplyFlashOpacity(GetFlashStrength());
}

void URogueDamageFlashWidgetBase::ApplyFlashOpacity(float Strength)
{
	// 蓝图自己驱动透明度时交还控制权。这种情况下 WBP 必须保证未受伤时 FlashRoot 不可见，
	// 否则同样是"一进游戏就红"（生成脚本已把默认值设为 0）。
	if (bBlueprintHandlesOpacity || FlashRoot == nullptr)
	{
		return;
	}

	FlashRoot->SetRenderOpacity(FMath::Clamp(Strength, 0.0f, 1.0f) * MaxOpacity);
}

void URogueDamageFlashWidgetBase::EnsureVisible()
{
	// 菜单创建流程会在最后统一设为 Collapsed（菜单语义），而泛红层必须常驻可见：
	// 折叠的控件不会被绘制，透明度渐隐也就看不出来。
	// HitTestInvisible 保证它不参与鼠标/手柄命中，不会挡住任何操作。
	if (GetVisibility() == ESlateVisibility::Collapsed || GetVisibility() == ESlateVisibility::Hidden)
	{
		SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}

float URogueDamageFlashWidgetBase::GetFlashStrength() const
{
	return FlashState.GetStrength(HoldSeconds, FadeOutSeconds);
}

void URogueDamageFlashWidgetBase::PlayDamageFlash(float Intensity)
{
	const float Clamped = FMath::Clamp(Intensity, 0.0f, 1.0f);
	if (Clamped <= 0.0f)
	{
		return;
	}

	FlashState.Play(Clamped, MinIntensity);

	// 万一被外部折叠（切换界面、蓝图误改），第一次受击时恢复，避免"受伤完全没有反馈"。
	EnsureVisible();

	if (!bBlueprintHandlesOpacity && FlashRoot == nullptr && !bWarnedMissingFlashRoot)
	{
		bWarnedMissingFlashRoot = true;
		UE_LOG(LogTemp, Warning,
			TEXT("[Rogue] %s 没有绑定 FlashRoot：受伤泛红不会有可见效果。"
				 "在 WBP 里把承载泛红的容器命名为 FlashRoot，或勾上 bBlueprintHandlesOpacity 自行驱动。"),
			*GetNameSafe(this));
	}

	// 蓝图始终收到事件：即使 C++ 驱动透明度，也允许 WBP 叠音效、镜头噪点或自定义动画。
	OnDamageFlash(FlashState.GetStrength(HoldSeconds, FadeOutSeconds));
}

void URogueDamageFlashWidgetBase::AdvanceFlash(float DeltaSeconds)
{
	if (!FlashState.IsActive())
	{
		return;
	}

	const float Strength = FlashState.Advance(DeltaSeconds, HoldSeconds, FadeOutSeconds);

	// 淡出到 0 时 ApplyFlashOpacity 会顺手把透明度写成 0，不需要再单独判断一次失活。
	ApplyFlashOpacity(Strength);
}
