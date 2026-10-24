#include "UI/Widgets/RogueEnemyHealthBarWidgetBase.h"
#include "Components/ProgressBar.h"
#include "UI/RogueHUD.h"

void URogueEnemyHealthBarWidgetBase::SetOwningRogueHUD(ARogueHUD* InOwningHUD)
{
	OwningHUD = InOwningHUD;
}

void URogueEnemyHealthBarWidgetBase::CaptureDesignerColor()
{
	if (!bCapturedDesignerColor && FillBar)
	{
		DesignerFillColor = FillBar->GetFillColorAndOpacity();
		bCapturedDesignerColor = true;
	}
}

void URogueEnemyHealthBarWidgetBase::ResetPresentation()
{
	Animation.Snap(0.0f);
	HitHighlightRemaining = 0.0f;
	CachedView = FRogueEnemyHealthBarView();
	ApplyBars();
}

void URogueEnemyHealthBarWidgetBase::UpdateBar(const FRogueEnemyHealthBarView& InView)
{
	CaptureDesignerColor();
	if (!InView.bVisible)
	{
		ResetPresentation();
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	FRogueEnemyHealthBarView View = InView;
	View.HealthPercent = FMath::Clamp(FMath::IsFinite(View.HealthPercent) ? View.HealthPercent : 0.0f, 0.0f, 1.0f);
	const bool bNewOccupant = !CachedView.bVisible || CachedView.SourceActor != View.SourceActor
		|| CachedView.SpawnId != View.SpawnId || CachedView.SpawnGeneration != View.SpawnGeneration
		|| CachedView.bBoss != View.bBoss;
	if (bNewOccupant || !bSmoothBars)
	{
		Animation.Snap(View.HealthPercent);
		HitHighlightRemaining = 0.0f;
	}
	else
	{
		if (View.HealthPercent < Animation.Target - KINDA_SMALL_NUMBER)
		{
			HitHighlightRemaining = FMath::Max(0.0f, HitHighlightDuration);
		}
		Animation.SetTarget(View.HealthPercent, DamageTrailDelay);
	}
	CachedView = View;
	SetVisibility(ESlateVisibility::HitTestInvisible);
	SetPositionInViewport(View.ScreenPosition, true);
	if (bNewOccupant)
	{
		SetDesiredSizeInViewport(FVector2D(BarSize.X * (View.bBoss ? BossWidthMultiplier : 1.0f), BarSize.Y));
	}
	OnBarUpdated(CachedView);
	ApplyBars();
}

void URogueEnemyHealthBarWidgetBase::NativeTick(const FGeometry& Geometry, float DeltaSeconds)
{
	Super::NativeTick(Geometry, DeltaSeconds);
	if (!CachedView.bVisible) { return; }
	const float Delta = FMath::IsFinite(DeltaSeconds) ? FMath::Max(0.0f, DeltaSeconds) : 0.0f;
	Animation.Step(Delta, InterpolationSpeed, DamageTrailSpeed);
	HitHighlightRemaining = FMath::Max(0.0f, HitHighlightRemaining - Delta);
	ApplyBars();
}

void URogueEnemyHealthBarWidgetBase::ApplyBars()
{
	if (FillBar)
	{
		FillBar->SetPercent(Animation.Displayed);
		const float Remaining = HitHighlightDuration > 0.0f
			? FMath::Clamp(HitHighlightRemaining / HitHighlightDuration, 0.0f, 1.0f) : 0.0f;
		// 单次快速提亮后平滑收回，不闪烁、不改变条的尺寸或屏幕位置。
		FillBar->SetFillColorAndOpacity(FMath::Lerp(DesignerFillColor, HitHighlightColor,
			FMath::Square(Remaining) * FMath::Clamp(HitHighlightStrength, 0.0f, 1.0f)));
	}
	if (DamageBar) { DamageBar->SetPercent(Animation.Trail); }
}

void URogueEnemyHealthBarWidgetBase::NativeDestruct()
{
	ResetPresentation();
	Super::NativeDestruct();
}
