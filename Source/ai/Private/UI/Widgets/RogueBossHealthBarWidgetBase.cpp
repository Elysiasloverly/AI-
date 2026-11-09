#include "UI/Widgets/RogueBossHealthBarWidgetBase.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Engine/Texture2D.h"

void URogueBossHealthBarWidgetBase::UpdateBossHealth(const FRogueBossHealthView& View)
{
    if (!Animation.bInitialized || !CachedView.bVisible || CachedView.SpawnId != View.SpawnId
        || !FMath::IsNearlyEqual(CachedView.MaxHealth, View.MaxHealth)) { Animation.Snap(View.GetPercent()); }
    else { Animation.SetTarget(View.GetPercent(), DamageTrailDelay); }
    const bool bChanged = CachedView.bVisible != View.bVisible || CachedView.SpawnId != View.SpawnId
        || CachedView.CurrentHealth != View.CurrentHealth || CachedView.MaxHealth != View.MaxHealth;
    const bool bRebuild=CachedView.SpawnId!=View.SpawnId || CachedView.Skills!=View.Skills;
    const bool bSkillChanged=bRebuild || CachedView.ActiveSkill!=View.ActiveSkill || CachedView.SkillPhase!=View.SkillPhase;
    CachedView = View;
    if (bRebuild) RebuildSkillIcons(View);
    if (bSkillChanged) TintSkillIcons(View);
    SetVisibility(View.bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    if (bChanged && HealthText)
    {
        HealthText->SetText(FText::FromString(FString::Printf(TEXT("%.0f / %.0f"), View.CurrentHealth, View.MaxHealth)));
    }
    ApplyBars();
    if (bChanged || bSkillChanged) { OnBossHealthUpdated(View); }
}
void URogueBossHealthBarWidgetBase::RebuildSkillIcons(const FRogueBossHealthView& View)
{
    if (!BossSkillIcons || !WidgetTree) return;
    BossSkillIcons->ClearChildren(); SkillIconFrames.Reset();
    for (const auto& Skill:View.Skills)
    {
        const auto* Row=Skill.GetRow<FRogueBossSkillRow>(TEXT("Boss HUD"));
        auto* Size=WidgetTree->ConstructWidget<USizeBox>();
        Size->SetWidthOverride(SkillIconSize); Size->SetHeightOverride(SkillIconSize);
        auto* Frame=WidgetTree->ConstructWidget<UBorder>(); Frame->SetPadding(FMargin(3));
        auto* Image=WidgetTree->ConstructWidget<UImage>();
        if (Row && Row->Icon) Image->SetBrushFromTexture(Row->Icon);
        Frame->SetContent(Image); Size->SetContent(Frame);
        BossSkillIcons->AddChildToHorizontalBox(Size)->SetPadding(FMargin(4,0));
        SkillIconFrames.Add(Frame);
    }
}

void URogueBossHealthBarWidgetBase::TintSkillIcons(const FRogueBossHealthView& View)
{
    for (int32 I=0;I<SkillIconFrames.Num() && I<View.Skills.Num();++I)
    {
        const bool Active=View.ActiveSkill==View.Skills[I].RowName && View.SkillPhase!=ERogueBossSkillPhase::Recovery;
        SkillIconFrames[I]->SetBrush(FSlateRoundedBoxBrush(Active?ActiveIconBackground:SkillIconBackground,9.f,
            Active?FLinearColor(1.f,.7f,.36f,1.f):FLinearColor(.55f,.45f,.36f,.75f),1.3f));
    }
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
