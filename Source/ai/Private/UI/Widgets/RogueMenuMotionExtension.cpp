#include "UI/Widgets/RogueMenuMotionExtension.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
void URogueMenuMotionExtension::Attach(UUserWidget* Widget)
{if(Widget&&!Widget->GetExtension<URogueMenuMotionExtension>())Widget->AddExtension<URogueMenuMotionExtension>();}
void URogueMenuMotionExtension::Construct()
{
    Super::Construct();auto* Owner=GetUserWidget();if(!Owner||!Owner->WidgetTree)return;
    Buttons.Reset();TArray<UWidget*> Widgets;Owner->WidgetTree->GetAllWidgets(Widgets);
    for(auto* W:Widgets)if(auto* B=Cast<UButton>(W))
    {FButtonMotion M;M.Button=B;M.Authored=B->GetRenderTransform();M.Pivot=B->GetRenderTransformPivot();M.Opacity=B->GetRenderOpacity();Buttons.Add(M);}
    Content=Owner->GetWidgetFromName(TEXT("MenuContent"));bMain=Content.IsValid();
    if(auto* C=Content.Get()){ContentBase=C->GetRenderTransform();ContentOpacity=C->GetRenderOpacity();}
    Owner->OnNativeVisibilityChanged.AddUObject(this,&ThisClass::VisibilityChanged);Age=0;
}
void URogueMenuMotionExtension::Restore()
{
    for(auto& M:Buttons)if(auto* B=M.Button.Get()){B->SetRenderTransform(M.Authored);B->SetRenderTransformPivot(M.Pivot);B->SetRenderOpacity(M.Opacity);M.Scale=1;M.Y=0;M.bMoving=false;}
    if(auto* C=Content.Get()){C->SetRenderTransform(ContentBase);C->SetRenderOpacity(ContentOpacity);}
}
void URogueMenuMotionExtension::VisibilityChanged(ESlateVisibility Visibility)
{if(Visibility==ESlateVisibility::Hidden||Visibility==ESlateVisibility::Collapsed){Restore();Age=0;}}
void URogueMenuMotionExtension::Destruct()
{Restore();if(auto* Owner=GetUserWidget())Owner->OnNativeVisibilityChanged.RemoveAll(this);Buttons.Reset();Content.Reset();Super::Destruct();}
void URogueMenuMotionExtension::Tick(const FGeometry& Geometry,float Delta)
{
    Super::Tick(Geometry,Delta);if(!bEnabled){if(bWasEnabled)Restore();bWasEnabled=false;return;}bWasEnabled=true;
    const float DT=FMath::Clamp(Delta,0.f,.1f);Age+=DT;const float A=1-FMath::Exp(-18.f*DT);
    if(bMain&&Age<.9f)if(auto* C=Content.Get())
    {const float T=FMath::SmoothStep(0.f,.6f,Age);FWidgetTransform Transform=ContentBase;Transform.Translation.X-=12*(1-T);C->SetRenderTransform(Transform);C->SetRenderOpacity(ContentOpacity*T);}
    int32 I=0;
    for(auto& M:Buttons)
    {
        auto* B=M.Button.Get();if(!B)continue;
        const bool Active=B->GetIsEnabled()&&(B->IsHovered()||B->HasAnyUserFocus());const bool Pressed=B->GetIsEnabled()&&B->IsPressed();
        const float TargetScale=Pressed?.985f:Active?FMath::Clamp(HoverScale,1.f,1.05f):1.f;
        const float Enter=bMain?1-FMath::SmoothStep(.10f+I*.035f,.42f+I*.035f,Age):0.f;++I;
        const float TargetY=(Active&&!Pressed?-FMath::Clamp(HoverLift,0.f,12.f):0.f)+Enter*7;
        M.Scale=FMath::Lerp(M.Scale,TargetScale,A);M.Y=FMath::Lerp(M.Y,TargetY,A);
        const bool Settled=FMath::IsNearlyEqual(M.Scale,TargetScale,.0001f)&&FMath::IsNearlyEqual(M.Y,TargetY,.01f)&&Enter==0;
        if(!M.bMoving&&!Active&&!Pressed&&Settled)continue;
        if(Settled){M.Scale=TargetScale;M.Y=TargetY;}
        FWidgetTransform T=M.Authored;T.Scale*=M.Scale;T.Translation.Y+=M.Y;
        B->SetRenderTransformPivot(FVector2D(.5f,.5f));B->SetRenderTransform(T);
        if(bMain&&Age<1)B->SetRenderOpacity(M.Opacity*(1-Enter));
        M.bMoving=!Settled||Active||Pressed;
    }
}
