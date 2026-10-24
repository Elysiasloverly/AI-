#if WITH_DEV_AUTOMATION_TESTS

#include "UI/RogueHUD.h"
#include "UI/Widgets/RogueReviveCountdownWidgetBase.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueReviveCountdownVisibilityTest,
	"Rogue.UI.ReviveCountdown.MultipleHUDsAndVisibilityDrift",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FRogueReviveCountdownVisibilityTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const TSubclassOf<URogueReviveCountdownWidgetBase> WidgetClass =
		LoadClass<URogueReviveCountdownWidgetBase>(nullptr,
			TEXT("/Game/UMG/Menus/InGame/WBP_ResurrectionScreen.WBP_ResurrectionScreen_C"));
	if (!TestNotNull(TEXT("The actual resurrection Blueprint loads"), WidgetClass.Get()))
	{
		return false;
	}

	const UWorld::InitializationValues WorldSettings = UWorld::InitializationValues()
		.AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false)
		.CreateAISystem(false).ShouldSimulatePhysics(false);
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, FName(TEXT("ReviveCountdownTestWorld")),
		nullptr, true, ERHIFeatureLevel::Num, &WorldSettings);
	if (!TestNotNull(TEXT("The test world exists"), World))
	{
		return false;
	}
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ON_SCOPE_EXIT
	{
		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(false);
	};

	ARogueHUD* HUDs[2] = {World->SpawnActor<ARogueHUD>(), World->SpawnActor<ARogueHUD>()};
	URogueReviveCountdownWidgetBase* Widgets[2] = {
		CreateWidget<URogueReviveCountdownWidgetBase>(World, WidgetClass),
		CreateWidget<URogueReviveCountdownWidgetBase>(World, WidgetClass)};
	for (int32 Index = 0; Index < 2; ++Index)
	{
		if (!TestNotNull(TEXT("Each local HUD exists"), HUDs[Index])
			|| !TestNotNull(TEXT("Each local countdown widget exists"), Widgets[Index]))
		{
			return false;
		}
		HUDs[Index]->ReviveCountdownWidget = Widgets[Index];
	}

	// Reproduce the same-process PIE failure: a living teammate pushes hidden/0 first,
	// while another player's widget still contains the visible pre-revival countdown.
	Widgets[0]->UpdateReviveCountdown(0, false);
	Widgets[1]->UpdateReviveCountdown(3, true);
	TestTrue(TEXT("The pre-revival countdown has visible widget state"),
		Widgets[1]->GetVisibility() == ESlateVisibility::SelfHitTestInvisible);
	HUDs[0]->UpdateReviveCountdownWidget(nullptr);
	HUDs[1]->UpdateReviveCountdownWidget(nullptr);
	TestTrue(TEXT("The revived player's countdown collapses after the teammate updates"),
		Widgets[1]->GetVisibility() == ESlateVisibility::Collapsed);

	// Repeat with the opposite draw order, then exercise a second down/revive display cycle.
	Widgets[0]->UpdateReviveCountdown(2, true);
	Widgets[1]->UpdateReviveCountdown(2, true);
	HUDs[1]->UpdateReviveCountdownWidget(nullptr);
	HUDs[0]->UpdateReviveCountdownWidget(nullptr);
	TestTrue(TEXT("The first HUD hides independently in reverse draw order"),
		Widgets[0]->GetVisibility() == ESlateVisibility::Collapsed);
	TestTrue(TEXT("The second HUD hides independently in reverse draw order"),
		Widgets[1]->GetVisibility() == ESlateVisibility::Collapsed);

	Widgets[0]->SetVisibility(ESlateVisibility::Visible);
	HUDs[0]->UpdateReviveCountdownWidget(nullptr);
	TestTrue(TEXT("An unchanged hidden state repairs actual visibility drift"),
		Widgets[0]->GetVisibility() == ESlateVisibility::Collapsed);
	return true;
}

#endif
