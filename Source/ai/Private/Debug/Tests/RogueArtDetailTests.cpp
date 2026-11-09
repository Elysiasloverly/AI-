#if WITH_DEV_AUTOMATION_TESTS
#include "UI/Widgets/RogueFloatingNumberMotion.h"
#include "UI/Widgets/RogueCardMotionWidgetBase.h"
#include "UI/Widgets/RogueUpgradeCardWidgetBase.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/SizeBox.h"
#include "Misc/AutomationTest.h"
struct FRogueCardMotionTestAccess
{
    static void Start(URogueCardMotionWidgetBase* W){W->NativeConstruct();}
    static void Stop(URogueCardMotionWidgetBase* W){W->NativeDestruct();}
    static void Step(URogueCardMotionWidgetBase* W,float D,bool H,bool P,bool E){W->ApplyMotion(D,H,P,E);}
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueCardMotionTest,"Rogue.Presentation.Detail.CardMotionPreservesLayoutAndDelegates",EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FRogueCardMotionTest::RunTest(const FString&)
{
    auto* Class=LoadClass<URogueCardMotionWidgetBase>(nullptr,TEXT("/Game/UMG/Menus/Upgrade/WBP_UpgradeCard.WBP_UpgradeCard_C"));if(!TestNotNull(TEXT("Saved card uses the shared cosmetic motion base"),Class))return false;
    auto* W=NewObject<URogueCardMotionWidgetBase>(GetTransientPackage(),Class);W->Initialize();auto* Root=W->GetRootWidget();if(!TestNotNull(TEXT("Authored widget tree survives native parent change"),Root))return false;
    FWidgetTransform Base;Base.Translation=FVector2D(3,7);Base.Scale=FVector2D(1.1f,.9f);Root->SetRenderTransform(Base);
    auto* Button=Cast<UButton>(W->WidgetTree->FindWidget(TEXT("Button_Select")));if(!Button)return false;
    FRogueCardMotionTestAccess::Start(W);const int Delegates=Button->OnClicked.GetAllObjects().Num();
    auto* Size=Cast<USizeBox>(Root);const float Width=Size?Size->GetWidthOverride():0;
    for(int I=0;I<20;++I)FRogueCardMotionTestAccess::Step(W,.016f,true,false,true);
    TestTrue(TEXT("Hover scales the render transform gently"),Root->GetRenderTransform().Scale.X>Base.Scale.X&&Root->GetRenderTransform().Scale.X<Base.Scale.X*1.061f);
    TestEqual(TEXT("Layout width does not grow or push adjacent cards"),Size?Size->GetWidthOverride():0,Width);
    for(int I=0;I<20;++I)FRogueCardMotionTestAccess::Step(W,.016f,true,true,true);
    TestTrue(TEXT("Press compresses gently"),Root->GetRenderTransform().Scale.X<Base.Scale.X);
    for(int I=0;I<40;++I)FRogueCardMotionTestAccess::Step(W,.016f,true,false,false);
    TestTrue(TEXT("Disabled cards settle to their authored size"),Root->GetRenderTransform().Scale.Equals(Base.Scale,.001f));
    TestEqual(TEXT("Hover/press never modifies purchase or selection delegates"),Button->OnClicked.GetAllObjects().Num(),Delegates);
    TestTrue(TEXT("Authored translation survives"),Root->GetRenderTransform().Translation.Equals(Base.Translation));
    FRogueCardMotionTestAccess::Stop(W);TestTrue(TEXT("Removal restores full authored transform"),Root->GetRenderTransform().Scale.Equals(Base.Scale));return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueNumberMotionTest,"Rogue.Presentation.Detail.FloatingNumbersHaveSmoothBoundedMotion",EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FRogueNumberMotionTest::RunTest(const FString&)
{
    auto Start=FRogueFloatingNumberMotion::Sample(0,.72,12,false,false),Mid=FRogueFloatingNumberMotion::Sample(.25,.72,12,false,false),End=FRogueFloatingNumberMotion::Sample(.72,.72,12,false,false);
    TestEqual(TEXT("Number begins without an opaque pop"),Start.Opacity,0.f);TestTrue(TEXT("Readable midlife opacity"),Mid.Opacity>.9f);TestEqual(TEXT("No abrupt opaque removal"),End.Opacity,0.f);
    TestTrue(TEXT("Offsets remain bounded near the target"),Mid.Offset.X>0&&End.Offset.X<=12.01&&End.Offset.Y>=-38.01);
    auto A=FRogueFloatingNumberMotion::Sample(.099,.72,12,false,false),B=FRogueFloatingNumberMotion::Sample(.101,.72,12,false,false);
    TestTrue(TEXT("Motion remains continuous across the projection cache boundary"),FVector2D::Distance(A.Offset,B.Offset)<1);
    auto Heal=FRogueFloatingNumberMotion::Sample(.2,.9,-8,true,false);TestTrue(TEXT("Healing can drift independently to the other side"),Heal.Offset.X<0&&Heal.Offset.Y<0);return true;
}
#endif
