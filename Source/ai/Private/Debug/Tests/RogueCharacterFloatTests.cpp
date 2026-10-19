#if WITH_DEV_AUTOMATION_TESTS
#include "Player/RogueCharacterVisualComponent.h"
#include "Player/RogueFloatingComponent.h"
#include "Player/RogueTurnSmoothingComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"

namespace RogueCharacterFloatTests
{
    constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;
    struct FFixture
    {
        FTestWorldWrapper TestWorld;
        AActor* Owner = nullptr;
        UCapsuleComponent* Capsule = nullptr;
        UCameraComponent* Camera = nullptr;
        URogueCharacterVisualComponent* Visuals = nullptr;
        UStaticMeshComponent* Mesh = nullptr;
        URogueFloatingComponent* Floating = nullptr;
        bool Initialize()
        {
            if (!TestWorld.CreateTestWorld(EWorldType::Game)) { return false; }
            Owner = TestWorld.GetTestWorld()->SpawnActor<AActor>();
            Capsule = NewObject<UCapsuleComponent>(Owner);
            Owner->SetRootComponent(Capsule); Capsule->RegisterComponent();
            Owner->SetActorLocation(FVector(1000, 2000, 300));
            Camera = NewObject<UCameraComponent>(Owner); Camera->SetupAttachment(Capsule); Camera->RegisterComponent();
            Camera->SetRelativeLocation(FVector(-500, 0, 700));
            Visuals = NewObject<URogueCharacterVisualComponent>(Owner); Visuals->SetupAttachment(Capsule); Visuals->RegisterComponent();
            Floating=NewObject<URogueFloatingComponent>(Owner);
            Floating->SetTargetComponent(Visuals); Floating->RegisterComponent(); Floating->Activate(true);
            Visuals->SetRelativeLocation(FVector(12, 23, 34));
            Mesh = NewObject<UStaticMeshComponent>(Owner); Mesh->SetupAttachment(Visuals); Mesh->RegisterComponent();
            Mesh->SetRelativeScale3D(FVector(.4, .8, 1.2));
            return true;
        }
        void Step(float Delta) { Floating->TickComponent(Delta, LEVELTICK_All, nullptr); }
    };
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueCharacterFloatBoundsTest,
    "Rogue.Cosmetics.CharacterFloat.BoundedMotionPreservesActorCameraAndMeshScale", RogueCharacterFloatTests::Flags)
bool FRogueCharacterFloatBoundsTest::RunTest(const FString& Parameters)
{
    using namespace RogueCharacterFloatTests;
    FFixture F; if (!F.Initialize()) { return false; }
    const FTransform ActorBefore = F.Owner->GetActorTransform();
    const FTransform CameraBefore = F.Camera->GetComponentTransform();
    const FVector Base = F.Visuals->GetRelativeLocation();
    F.Floating->FloatAmplitude = 4; F.Floating->FloatPeriod = 4;
    F.Step(1); TestTrue(TEXT("Quarter-cycle lifts the visual by four centimeters"), F.Visuals->GetRelativeLocation().Equals(Base + FVector(0,0,4), .001));
    F.Step(2); TestTrue(TEXT("Three-quarter-cycle lowers the visual by four centimeters"), F.Visuals->GetRelativeLocation().Equals(Base - FVector(0,0,4), .001));
    F.Step(1);
    for (int32 Index = 0; Index < 6000; ++Index)
    {
        F.Step(.016f);
        if (!TestTrue(TEXT("Bobbing stays inside configured amplitude without accumulating drift"),
            FMath::Abs(F.Visuals->GetRelativeLocation().Z - Base.Z) <= 4.001)) { return false; }
    }
    TestTrue(TEXT("Collision actor transform is unchanged"), F.Owner->GetActorTransform().Equals(ActorBefore));
    TestTrue(TEXT("Camera does not bob"), F.Camera->GetComponentTransform().Equals(CameraBefore));
    TestTrue(TEXT("Blueprint mesh proportions are unchanged"), F.Mesh->GetRelativeScale3D().Equals(FVector(.4,.8,1.2)));
    TestFalse(TEXT("Visual motion is not replicated each frame"), F.Floating->GetIsReplicated());
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueCharacterFloatResetTest,
    "Rogue.Cosmetics.CharacterFloat.DisableDeactivateAndUnregisterRestoreAuthoredPosition", RogueCharacterFloatTests::Flags)
bool FRogueCharacterFloatResetTest::RunTest(const FString& Parameters)
{
    using namespace RogueCharacterFloatTests;
    FFixture F; if (!F.Initialize()) { return false; }
    const FVector Base = F.Visuals->GetRelativeLocation();
    F.Step(.5f); F.Visuals->AddRelativeLocation(FVector(10,20,30));
    F.Floating->SetFloatingEnabled(false);
    const FVector Expected = Base + FVector(10,20,30);
    TestTrue(TEXT("Disabling removes only the component's own offset"), F.Visuals->GetRelativeLocation().Equals(Expected,.001));
    F.Floating->SetFloatingEnabled(true); F.Step(.5f); F.Floating->Deactivate();
    TestTrue(TEXT("Deactivation restores the baseline"), F.Visuals->GetRelativeLocation().Equals(Expected,.001));
    F.Floating->Activate(); F.Step(.5f); F.Floating->UnregisterComponent();
    TestTrue(TEXT("Unregistering removes transient offsets"), F.Visuals->GetRelativeLocation().Equals(Expected,.001));
    F.Floating->RegisterComponent(); F.Floating->FloatPeriod = 0; F.Step(.5f);
    TestTrue(TEXT("An invalid period cannot generate invalid transforms"), F.Visuals->GetRelativeLocation().Equals(Expected,.001));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueFloatingTargetSwitchTest,
    "Rogue.Cosmetics.CharacterFloat.IndependentComponentSwitchesTargetsAndHandlesMissingTarget", RogueCharacterFloatTests::Flags)
bool FRogueFloatingTargetSwitchTest::RunTest(const FString& Parameters)
{
    using namespace RogueCharacterFloatTests;
    FFixture F; if (!F.Initialize()) { return false; }
    const FVector Base=F.Visuals->GetRelativeLocation();
    F.Step(.5f);
    TestTrue(TEXT("Independent floating component resolves its selected target"),F.Floating->GetTargetComponent()==F.Visuals);
    USceneComponent* Other=NewObject<USceneComponent>(F.Owner); Other->SetupAttachment(F.Capsule); Other->RegisterComponent();
    Other->SetRelativeLocation(FVector(0,0,70));
    F.Floating->SetTargetComponent(Other);
    TestTrue(TEXT("Previous target is restored when switching"),F.Visuals->GetRelativeLocation().Equals(Base,.001));
    F.Step(.5f); TestTrue(TEXT("New target floats"),Other->GetRelativeLocation().Z>70);
    F.Floating->SetTargetComponent(nullptr); F.Step(.5f);
    TestTrue(TEXT("Removing a target restores it without moving the owner root"),Other->GetRelativeLocation().Equals(FVector(0,0,70),.001));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueVisualTurningSmoothTest,
    "Rogue.Cosmetics.Turning.SmoothShortestRotationPreservesMovementAndFloating", RogueCharacterFloatTests::Flags)
bool FRogueVisualTurningSmoothTest::RunTest(const FString& Parameters)
{
    using namespace RogueCharacterFloatTests;
    FFixture F; if (!F.Initialize()) { return false; }
    URogueTurnSmoothingComponent* Turning=NewObject<URogueTurnSmoothingComponent>(F.Owner);
    Turning->SetTargetComponent(F.Visuals); Turning->RegisterComponent(); Turning->Activate(true);
    Turning->TickComponent(0,LEVELTICK_All,nullptr);
    F.Owner->SetActorRotation(FRotator(0,90,0)); F.Step(.5f);
    const FVector FloatLocation=F.Visuals->GetRelativeLocation();
    Turning->TickComponent(.016f,LEVELTICK_All,nullptr);
    const float FirstYaw=F.Visuals->GetComponentRotation().Yaw;
    TestTrue(TEXT("A sudden direction change receives a soft visual transition"),FirstYaw>0 && FirstYaw<30);
    for (int32 I=0;I<60;++I) { Turning->TickComponent(1.0f/60,LEVELTICK_All,nullptr); }
    TestTrue(TEXT("Visual facing converges to movement heading"),FMath::Abs(FMath::FindDeltaAngleDegrees(F.Visuals->GetComponentRotation().Yaw,90.0f))<.1);
    TestTrue(TEXT("Movement root retains its authoritative heading"),FMath::IsNearlyEqual(F.Owner->GetActorRotation().Yaw,90.0f,.01));
    TestTrue(TEXT("Turning does not overwrite floating position"),F.Visuals->GetRelativeLocation().Equals(FloatLocation,.001));
    Turning->Deactivate();
    TestTrue(TEXT("Disabling restores authored relative rotation"),F.Visuals->GetRelativeRotation().IsNearlyZero(.01));
    Turning->Activate(true); F.Owner->SetActorRotation(FRotator(0,179,0)); Turning->TickComponent(0,LEVELTICK_All,nullptr);
    F.Owner->SetActorRotation(FRotator(0,-179,0)); Turning->TickComponent(.016f,LEVELTICK_All,nullptr);
    TestTrue(TEXT("Wrapping at 180 degrees follows the short arc"),FMath::Abs(FMath::FindDeltaAngleDegrees(179.0f,F.Visuals->GetComponentRotation().Yaw))<2.1f);
    return true;
}

#endif
