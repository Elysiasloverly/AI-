#if WITH_DEV_AUTOMATION_TESTS
#include "Combat/Visuals/RogueCombatCameraModifier.h"
#include "Camera/CameraTypes.h"
#include "Misc/AutomationTest.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatFeelCameraRecovery,"Rogue.Presentation.CombatFeel.CameraPulseRecoversWithoutChangingBaseView",EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FCombatFeelCameraRecovery::RunTest(const FString&)
{
    auto* M=NewObject<URogueCombatCameraModifier>();
    FMinimalViewInfo Original;Original.FOV=80;Original.Location=FVector(12,34,56);Original.Rotation=FRotator(-45,120,0);
    auto V=Original;M->ModifyCamera(.02f,V);TestEqual(TEXT("Idle camera stays untouched"),V.FOV,Original.FOV);
    M->AddImpulse(.7f);V=Original;TestFalse(TEXT("Does not block other camera modifiers"),M->ModifyCamera(.04f,V));
    TestTrue(TEXT("Recoil is visible and bounded"),V.FOV<80 && V.FOV>79.15f);
    TestTrue(TEXT("Camera location remains authored"),V.Location.Equals(Original.Location));
    TestEqual(TEXT("Input yaw is not altered"),V.Rotation.Yaw,Original.Rotation.Yaw);
    V=Original;M->ModifyCamera(.4f,V);
    TestEqual(TEXT("FOV fully recovers"),V.FOV,Original.FOV);
    TestTrue(TEXT("Rotation fully recovers"),V.Rotation.Equals(Original.Rotation));
    M->AddImpulse(std::numeric_limits<float>::quiet_NaN());V=Original;M->ModifyCamera(.02f,V);
    TestEqual(TEXT("Invalid impulse cannot poison the camera"),V.FOV,Original.FOV);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatFeelCameraDensity,"Rogue.Presentation.CombatFeel.ConcurrentFinishesNeverStackOrProlongThePulse",EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FCombatFeelCameraDensity::RunTest(const FString&)
{
    auto* M=NewObject<URogueCombatCameraModifier>();M->AddImpulse(.5f);
    FMinimalViewInfo V;V.FOV=80;M->ModifyCamera(.1f,V);
    for(int I=0;I<100;++I)M->AddImpulse(10.f);
    V=FMinimalViewInfo();V.FOV=80;M->ModifyCamera(.1f,V);
    TestTrue(TEXT("Concurrent events remain below one degree of FOV"),V.FOV>=79.15f);
    V=FMinimalViewInfo();V.FOV=80;M->ModifyCamera(.13f,V);
    TestEqual(TEXT("Repeated events did not restart the deadline"),V.FOV,80.f);
    return true;
}
#endif
