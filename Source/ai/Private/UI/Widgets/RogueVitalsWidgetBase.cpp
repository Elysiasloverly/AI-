#include "UI/Widgets/RogueVitalsWidgetBase.h"
#include "Components/Border.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "UI/RogueHUD.h"

void URogueVitalsWidgetBase::SetOwningRogueHUD(ARogueHUD* InOwningHUD){OwningHUD=InOwningHUD;}

void URogueVitalsWidgetBase::CacheFeedbackBase()
{
    if(bFeedbackBaseCached)return;
    if(HealthRow)HealthRowBase=HealthRow->GetRenderTransform();
    if(ArmorRow)ArmorRowBase=ArmorRow->GetRenderTransform();
    if(ExperienceRow)ExperienceRowBase=ExperienceRow->GetRenderTransform();
    if(HealthFeedback)HealthFeedbackColor=HealthFeedback->GetBrushColor();
    if(ArmorFeedback)ArmorFeedbackColor=ArmorFeedback->GetBrushColor();
    if(ExperienceFeedback)ExperienceFeedbackColor=ExperienceFeedback->GetBrushColor();
    if(HealthChangeText){HealthNumberBase=HealthChangeText->GetRenderTransform().Translation;HealthNumberColor=HealthChangeText->GetColorAndOpacity();}
    if(ArmorChangeText){ArmorNumberBase=ArmorChangeText->GetRenderTransform().Translation;ArmorNumberColor=ArmorChangeText->GetColorAndOpacity();}
    if(ExperienceChangeText){ExperienceNumberBase=ExperienceChangeText->GetRenderTransform().Translation;ExperienceNumberColor=ExperienceChangeText->GetColorAndOpacity();}
    bFeedbackBaseCached=true;
}

void URogueVitalsWidgetBase::ResetFeedback()
{
    HealthChange.Reset();ArmorChange.Reset();ExperienceChange.Reset();
    bExperienceWrapPending=false;ExperienceWrapRemaining=0;ExperienceFullHoldRemaining=0;FeedbackClock=0;
}

void URogueVitalsWidgetBase::UpdateVitals(const FRoguePlayerVitalsView& InView)
{
    if(bHasView&&CachedView.Equals(InView))return;
    CacheFeedbackBase();
    const bool bLifeReset=!bHasView||CachedView.SourceActor!=InView.SourceActor||!CachedView.bVisible||!InView.bVisible
        ||CachedView.bDowned!=InView.bDowned||InView.bDowned||InView.Level<CachedView.Level;
    const bool bHealthCapacityChanged=!FMath::IsNearlyEqual(CachedView.MaxHealth,InView.MaxHealth);
    const bool bArmorCapacityChanged=CachedView.bHasArmor!=InView.bHasArmor||!FMath::IsNearlyEqual(CachedView.MaxArmor,InView.MaxArmor);
    const bool bLevelUp=!bLifeReset&&InView.Level>CachedView.Level;
    if(bLifeReset)ResetFeedback();
    if(bHealthCapacityChanged)HealthChange.Reset();
    if(bArmorCapacityChanged)ArmorChange.Reset();
    if(!bEnableChangeFeedback){HealthChange.Reset();ArmorChange.Reset();ExperienceChange.Reset();}
    else if(!bLifeReset)
    {
        if(!bHealthCapacityChanged)
        {
            const float Delta=InView.CurrentHealth-CachedView.CurrentHealth;
            if(Delta<-.01f)HealthChange.Trigger(ERogueVitalsChange::Damage,Delta,ChangeNumberDuration);
            else if(Delta>=MinimumRestoreFeedback)HealthChange.Trigger(ERogueVitalsChange::Restore,Delta,ChangeNumberDuration);
        }
        if(!bArmorCapacityChanged&&InView.bHasArmor)
        {
            const float Delta=InView.CurrentArmor-CachedView.CurrentArmor;
            if(Delta<-.01f)
            {
                const bool bBroke=CachedView.CurrentArmor>0&&InView.CurrentArmor<=KINDA_SMALL_NUMBER;
                ArmorChange.Trigger(bBroke?ERogueVitalsChange::ArmorBreak:ERogueVitalsChange::Damage,Delta,ChangeNumberDuration);
            }
            else if(Delta>=MinimumRestoreFeedback)ArmorChange.Trigger(ERogueVitalsChange::Restore,Delta,ChangeNumberDuration);
        }
        if(bLevelUp)
        {
            // One notification for the final level, even when one pickup grants several levels.
            ExperienceChange.Reset();ExperienceChange.Trigger(ERogueVitalsChange::LevelUp,InView.Level,FMath::Max(.9f,ChangeNumberDuration));
        }
        else if(InView.Experience>CachedView.Experience&&!(ExperienceChange.Active()&&ExperienceChange.Kind==ERogueVitalsChange::LevelUp))
            ExperienceChange.Trigger(ERogueVitalsChange::Experience,InView.Experience-CachedView.Experience,ChangeNumberDuration);
    }
    if(bLifeReset||bHealthCapacityChanged||!bSmoothBars)HealthAnimation.Snap(InView.HealthPercent);
    else HealthAnimation.SetTarget(InView.HealthPercent,DamageTrailDelay);
    if(bLifeReset||bArmorCapacityChanged||!bSmoothBars)ArmorAnimation.Snap(InView.ArmorPercent);
    else ArmorAnimation.SetTarget(InView.ArmorPercent,DamageTrailDelay);
    if(bLifeReset||!bSmoothBars)
    {
        ExperienceAnimation.Snap(InView.ExperiencePercent);bExperienceWrapPending=false;
    }
    else if(bLevelUp)
    {
        // Finish the old level first, then ease into the real ratio of the final new level.
        ExperienceWrapFrom=ExperienceAnimation.Displayed;ExperienceFullHoldRemaining=0;
        ExperienceAnimation.SetTarget(1,0);bExperienceWrapPending=true;
        ExperienceWrapRemaining=FMath::Max(.05f,ExperienceWrapDuration);ExperienceWrapTarget=InView.ExperiencePercent;
    }
    else if(bExperienceWrapPending)ExperienceWrapTarget=InView.ExperiencePercent;
    else if(InView.ExperiencePercent<CachedView.ExperiencePercent)ExperienceAnimation.Snap(InView.ExperiencePercent);
    else ExperienceAnimation.SetTarget(InView.ExperiencePercent,0);
    bHasView=true;CachedView=InView;
    SetVisibility(InView.bVisible?ESlateVisibility::HitTestInvisible:ESlateVisibility::Collapsed);
    const ESlateVisibility ArmorVisibility=InView.bHasArmor?ESlateVisibility::HitTestInvisible:ESlateVisibility::Collapsed;
    if(ArmorRow)ArmorRow->SetVisibility(ArmorVisibility);
    if(ArmorBar)ArmorBar->SetVisibility(ArmorVisibility);
    if(ArmorDamageBar)ArmorDamageBar->SetVisibility(ArmorVisibility);
    if(HealthText)HealthText->SetText(FText::FromString(FString::Printf(TEXT("生命 %.0f / %.0f"),InView.CurrentHealth,InView.MaxHealth)));
    if(ArmorText){ArmorText->SetText(FText::FromString(FString::Printf(TEXT("护甲 %.0f / %.0f"),InView.CurrentArmor,InView.MaxArmor)));ArmorText->SetVisibility(ArmorVisibility);}
    if(ExperienceText)ExperienceText->SetText(FText::FromString(FString::Printf(TEXT("经验 %d / %d"),InView.Experience,InView.ExperienceToNextLevel)));
    if(WaveText)WaveText->SetText(FText::FromString(FString::Printf(TEXT("第 %d 波"),InView.CurrentWave)));
    if(GoldText)GoldText->SetText(FText::FromString(FString::Printf(TEXT("金币 %d"),InView.Money)));
    ApplyAnimatedBars();OnVitalsUpdated(CachedView);ApplyFeedback();ApplyDashView();
}

void URogueVitalsWidgetBase::ApplyDashView()
{
    const bool bUsable = CachedView.bDashUsable && !CachedView.bDowned;
    const bool bReady = bUsable && CachedView.bDashReady;
    if (DashCooldownRow) DashCooldownRow->SetRenderOpacity(bUsable ? 1.f : .5f);
    if (DashCooldownBar)
    {
        DashCooldownBar->SetPercent(bUsable ? FMath::Clamp(CachedView.DashCooldownPercent, 0.f, 1.f) : 0.f);
        DashCooldownBar->SetFillColorAndOpacity(!bUsable ? DashUnavailableColor : bReady ? DashReadyColor : DashCooldownColor);
    }
    if (DashCooldownText)
    {
        FString Status;
        if (!bUsable) Status = TEXT("冲刺不可用");
        else if (bReady) Status = TEXT("冲刺就绪");
        else if (CachedView.DashCooldownRemaining > 0.f)
        {
            const float Seconds = FMath::Max(.1f, FMath::CeilToFloat(CachedView.DashCooldownRemaining * 10.f) / 10.f);
            Status = FString::Printf(TEXT("冷却 %.1f 秒"), Seconds);
        }
        else Status = CachedView.bDashing ? TEXT("冲刺中") : TEXT("冲刺准备中");
        DashCooldownText->SetText(FText::FromString(Status));
        DashCooldownText->SetColorAndOpacity(bReady ? DashReadyColor : FLinearColor(.76f, .81f, .87f, 1.f));
    }
}

void URogueVitalsWidgetBase::NativeTick(const FGeometry& Geometry,float DeltaSeconds)
{
    Super::NativeTick(Geometry,DeltaSeconds);
    if(!bHasView||!CachedView.bVisible)return;
    const float Delta=FMath::Max(0.f,DeltaSeconds);
    HealthAnimation.Step(Delta,BarInterpolationSpeed,DamageTrailSpeed);
    ArmorAnimation.Step(Delta,BarInterpolationSpeed,DamageTrailSpeed);
    if(bExperienceWrapPending)
    {
        if(ExperienceWrapRemaining>0)
        {
            ExperienceWrapRemaining=FMath::Max(0.f,ExperienceWrapRemaining-Delta);
            const float Progress=1-ExperienceWrapRemaining/FMath::Max(.05f,ExperienceWrapDuration);
            ExperienceAnimation.Snap(FMath::Lerp(ExperienceWrapFrom,1.f,FMath::SmoothStep(0.f,1.f,Progress)));
            if(ExperienceWrapRemaining<=KINDA_SMALL_NUMBER)
            {
                // Render an exact full bar for at least one frame, including during frame hitches.
                ExperienceAnimation.Snap(1);ExperienceWrapRemaining=0;ExperienceFullHoldRemaining=.05f;
            }
        }
        else
        {
            ExperienceFullHoldRemaining=FMath::Max(0.f,ExperienceFullHoldRemaining-Delta);
            if(ExperienceFullHoldRemaining<=0)
            {
                ExperienceAnimation.Snap(0);ExperienceAnimation.SetTarget(ExperienceWrapTarget,0);bExperienceWrapPending=false;
            }
        }
    }
    else ExperienceAnimation.Step(Delta,ExperienceInterpolationSpeed,0);
    HealthChange.Step(Delta);ArmorChange.Step(Delta);ExperienceChange.Step(Delta);
    FeedbackClock=FMath::Fmod(FeedbackClock+Delta,16.f);
    ApplyAnimatedBars();ApplyFeedback();
}

void URogueVitalsWidgetBase::ApplyAnimatedBars()
{
    if(HealthBar)HealthBar->SetPercent(HealthAnimation.Displayed);
    if(HealthDamageBar)HealthDamageBar->SetPercent(HealthAnimation.Trail);
    if(ArmorBar)ArmorBar->SetPercent(ArmorAnimation.Displayed);
    if(ArmorDamageBar)ArmorDamageBar->SetPercent(ArmorAnimation.Trail);
    if(ExperienceBar)ExperienceBar->SetPercent(ExperienceAnimation.Displayed);
}

void URogueVitalsWidgetBase::ApplyFeedback()
{
    const bool bActive=bEnableChangeFeedback&&CachedView.bVisible&&!CachedView.bDowned;
    auto Apply=[this,bActive](UWidget* Row,const FWidgetTransform& Base,UBorder* Flash,FLinearColor Accent,
        UTextBlock* Number,FVector2D NumberBase,FSlateColor NumberColor,const FRogueVitalsFeedback& Change,float Ambient)
    {
        const bool bChanging=bActive&&Change.Active();
        const float Emphasis=bChanging?FMath::Exp(-6.f*Change.Elapsed/FMath::Max(.05f,ChangeFeedbackDuration)):0;
        const bool bRestore=bChanging&&Change.Kind==ERogueVitalsChange::Restore;
        if(bRestore){const float Alpha=Accent.A;Accent=FMath::Lerp(Accent,FLinearColor(.2f,.95f,.55f,Alpha),.85f);}
        else if(bChanging&&Change.Kind==ERogueVitalsChange::ArmorBreak){const float Alpha=Accent.A;Accent=FMath::Lerp(Accent,FLinearColor(1,1,1,Alpha),.65f);}
        if(Flash){Flash->SetBrushColor(Accent);Flash->SetRenderOpacity(bActive?FMath::Clamp(FMath::Max(Emphasis,Ambient),0.f,1.f):0);}
        if(Row)
        {
            FWidgetTransform T=Base;
            const float Time=FMath::Clamp(Change.Elapsed/FMath::Max(.05f,ChangeFeedbackDuration),0.f,1.f);
            const float Scale=bChanging?1+ChangeScaleAmount*FMath::Sin(PI*Time):1;
            T.Scale*=Scale;Row->SetRenderTransform(T);
        }
        if(Number)
        {
            Number->SetRenderTranslation(NumberBase+FVector2D(0,bChanging?-ChangeNumberRise*FMath::SmoothStep(0.f,1.f,Change.Progress()):0));
            Number->SetRenderOpacity(bChanging&&bShowChangeNumbers?Change.Opacity():0);
            FSlateColor Color=NumberColor;
            if(bRestore)Color=FSlateColor(FLinearColor(.45f,1,.7f,1));
            Number->SetColorAndOpacity(Color);
            if(bChanging)
            {
                if(Change.Kind==ERogueVitalsChange::ArmorBreak)Number->SetText(FText::FromString(TEXT("护甲破裂")));
                else if(Change.Kind==ERogueVitalsChange::LevelUp)Number->SetText(FText::FromString(FString::Printf(TEXT("灵力提升 Lv.%d"),FMath::RoundToInt(Change.Amount))));
                else
                {
                    FNumberFormattingOptions Format;Format.MinimumFractionalDigits=0;Format.MaximumFractionalDigits=1;
                    const FString Value=FText::AsNumber(FMath::Abs(Change.Amount),&Format).ToString();
                    Number->SetText(FText::FromString((Change.Amount>=0?TEXT("+"):TEXT("-"))+Value+(Change.Kind==ERogueVitalsChange::Experience?TEXT(" 经验"):TEXT(""))));
                }
            }
        }
    };
    const bool bLow=bLowHealthBreathing&&CachedView.CurrentHealth>0&&CachedView.HealthPercent<=LowHealthThreshold;
    const float LowPulse=bLow?.07f+.07f*(.5f+.5f*FMath::Sin(FeedbackClock*PI*1.25f)):0;
    Apply(HealthRow,HealthRowBase,HealthFeedback,HealthFeedbackColor,HealthChangeText,HealthNumberBase,HealthNumberColor,HealthChange,LowPulse);
    Apply(ArmorRow,ArmorRowBase,ArmorFeedback,ArmorFeedbackColor,ArmorChangeText,ArmorNumberBase,ArmorNumberColor,ArmorChange,0);
    Apply(ExperienceRow,ExperienceRowBase,ExperienceFeedback,ExperienceFeedbackColor,ExperienceChangeText,ExperienceNumberBase,ExperienceNumberColor,ExperienceChange,0);
}
