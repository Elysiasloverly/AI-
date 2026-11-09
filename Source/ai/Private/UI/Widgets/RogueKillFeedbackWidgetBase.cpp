#include "UI/Widgets/RogueKillFeedbackWidgetBase.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Components/CanvasPanelSlot.h"
#include "Engine/World.h"
void URogueKillFeedbackWidgetBase::NativeConstruct()
{Super::NativeConstruct();SetVisibility(ESlateVisibility::HitTestInvisible);SetRenderOpacity(0);}
void URogueKillFeedbackWidgetBase::RegisterKills(int32 Count,bool bBoss)
{
    if(Count<=0)return;const float Now=GetWorld()?GetWorld()->GetTimeSeconds():0;
    if(Now-LastKillTime>FMath::Max(.5f,StreakWindow))StreakCount=0;
    StreakCount=FMath::Min(9999,StreakCount+FMath::Clamp(Count,1,9999));LastKillTime=Now;
    // Keep a boss confirmation from being immediately replaced by an ordinary kill in the same burst.
    bBossNotice=bBoss || (bBossNotice && Age<HoldDuration+.35f+FadeDuration);
    AttackStartOpacity=FMath::Clamp(GetRenderOpacity(),0.f,.92f);Age=0;
    if(KillText)KillText->SetText(FText::FromString(bBossNotice?TEXT("强敌退治"):TEXT("退治")));
    if(ChainText)ChainText->SetText(FText::FromString(FString::Printf(TEXT("连锁  × %d"),StreakCount)));
    OnKillFeedback(StreakCount,bBoss);
}
void URogueKillFeedbackWidgetBase::NativeTick(const FGeometry& G,float DT)
{
    Super::NativeTick(G,DT);Age+=FMath::Max(0.f,DT);
    if(bBlueprintHandlesMotion)return;
    const float Hold=FMath::Max(.1f,HoldDuration)+(bBossNotice?.35f:0),Fade=FMath::Max(.1f,FadeDuration);
    const float Attack=FMath::SmoothStep(0.f,.09f,Age),End=1.f-FMath::SmoothStep(Hold,Hold+Fade,Age);
    SetRenderOpacity(FMath::Lerp(AttackStartOpacity,.92f,Attack)*End);
    const float Settle=1-FMath::Exp(-Age*20);
    if(SpiritSeal)
    {
        SpiritSeal->SetRenderTransformPivot(FVector2D(.5f,.5f));
        SpiritSeal->SetRenderScale(FVector2D(FMath::Lerp(1.04f,1.f,Settle)));
        SpiritSeal->SetRenderTranslation(FVector2D(0,FMath::Lerp(5.f,0.f,Settle)));
    }
    if(Age>Hold+Fade)bBossNotice=false;
}

void URogueKillFeedbackWidgetBase::SetBottomInset(float Height)
{
    if(!SpiritSeal)return;
    if(auto* DockSlot=Cast<UCanvasPanelSlot>(SpiritSeal->Slot))
    {
        if(!bDockCaptured){AuthoredDock=DockSlot->GetPosition();bDockCaptured=true;}
        const FVector2D Target(AuthoredDock.X,FMath::Min(AuthoredDock.Y,-FMath::Max(0.f,Height)-18.f));
        if(!DockSlot->GetPosition().Equals(Target,.1))DockSlot->SetPosition(Target);
    }
}
