#include "UI/Widgets/RogueCardMotionWidgetBase.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "UI/RoguePresentationTheme.h"
void URogueCardMotionWidgetBase::ApplyCardIcon(ERogueUpgradeType Type)
{
    if(auto* Icon=Cast<UImage>(GetWidgetFromName(TEXT("ArtIcon"))))
    {
        auto* Material=PresentationTheme?PresentationTheme->GetCardIcon(Type):nullptr;
        if(Material){Icon->SetBrushFromMaterial(Material);Icon->SetVisibility(ESlateVisibility::HitTestInvisible);}
        else Icon->SetVisibility(ESlateVisibility::Hidden);
    }
}
void URogueCardMotionWidgetBase::NativeConstruct()
{
    Super::NativeConstruct();
    if(auto* Root=GetRootWidget())
    {
        BaseTransform=Root->GetRenderTransform();BasePivot=Root->GetRenderTransformPivot();
        MotionScale=1;bCaptured=true;
    }
}
void URogueCardMotionWidgetBase::NativeDestruct()
{
    if(bCaptured)if(auto* Root=GetRootWidget()){Root->SetRenderTransform(BaseTransform);Root->SetRenderTransformPivot(BasePivot);}
    bCaptured=false;Super::NativeDestruct();
}
void URogueCardMotionWidgetBase::NativeTick(const FGeometry& G,float DT)
{
    Super::NativeTick(G,DT);auto* Button=GetMotionButton();
    ApplyMotion(DT,Button&&(Button->IsHovered()||Button->HasAnyUserFocus()),Button&&Button->IsPressed(),Button&&Button->GetIsEnabled());
}
void URogueCardMotionWidgetBase::ApplyMotion(float Delta,bool Hovered,bool Pressed,bool Enabled)
{
    auto* Root=GetRootWidget();if(!Root||!bCaptured)return;
    if(!bEnableCardMotion)
    {Root->SetRenderTransform(BaseTransform);Root->SetRenderTransformPivot(BasePivot);MotionScale=1;return;}
    const float Target=!Enabled?1.f:Pressed?FMath::Clamp(PressScale,.95f,1.f):Hovered?FMath::Clamp(HoverScale,1.f,1.06f):1.f;
    const float Alpha=1-FMath::Exp(-FMath::Clamp(MotionSpeed,1.f,40.f)*FMath::Max(0.f,Delta));
    MotionScale=FMath::Lerp(MotionScale,Target,Alpha);
    if(FMath::IsNearlyEqual(MotionScale,Target,.0001f))MotionScale=Target;
    FWidgetTransform T=BaseTransform;T.Scale*=MotionScale;
    Root->SetRenderTransformPivot(FVector2D(.5f,.5f));Root->SetRenderTransform(T);
}
