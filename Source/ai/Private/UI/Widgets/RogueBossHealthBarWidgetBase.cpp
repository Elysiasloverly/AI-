#include "UI/Widgets/RogueBossHealthBarWidgetBase.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"

void URogueBossHealthBarWidgetBase::UpdateBossHealth(const FRogueBossHealthView& View)
{
    if (!Animation.bInitialized || !CachedView.bVisible || CachedView.SpawnId != View.SpawnId
        || !FMath::IsNearlyEqual(CachedView.MaxHealth, View.MaxHealth)) { Animation.Snap(View.GetPercent()); }
    else { Animation.SetTarget(View.GetPercent(), DamageTrailDelay); }
    const bool bChanged = CachedView.bVisible != View.bVisible || CachedView.SpawnId != View.SpawnId
        || CachedView.CurrentHealth != View.CurrentHealth || CachedView.MaxHealth != View.MaxHealth;
    CachedView = View;
    SetVisibility(View.bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    if (bChanged && HealthText)
    {
        HealthText->SetText(FText::FromString(FString::Printf(TEXT("%.0f / %.0f"), View.CurrentHealth, View.MaxHealth)));
    }
    ApplyBars();
    if (bChanged) { OnBossHealthUpdated(View); }
}
void URogueBossHealthBarWidgetBase::NativeTick(const FGeometry& Geometry, float DeltaSeconds)
{
    Super::NativeTick(Geometry, DeltaSeconds);
    Animation.Step(DeltaSeconds, InterpolationSpeed, DamageTrailSpeed);
    ApplyBars();
}
void URogueBossHealthBarWidgetBase::ApplyBars()
{
    if (FillBar) { FillBar->SetPercent(Animation.Displayed); }
    if (DamageBar) { DamageBar->SetPercent(Animation.Trail); }
}
