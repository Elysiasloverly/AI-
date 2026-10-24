#if WITH_DEV_AUTOMATION_TESTS
#include "Networking/RogueLobby.h"
#include "Networking/RogueRoomSubsystem.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueRoomAddressTest,"Rogue.Networking.Rooms.ValidateDirectAddress",EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FRogueRoomAddressTest::RunTest(const FString& Parameters)
{
	FString Out;
	for (const TCHAR* Valid : {TEXT("127.0.0.1"),TEXT("192.168.1.10:7777"),TEXT("localhost:65535"),TEXT(" 10.0.0.2 ")})
		TestTrue(Valid,URogueRoomSubsystem::ValidateAddress(Valid,Out));
	for (const TCHAR* Invalid : {TEXT(""),TEXT("1.2.3"),TEXT("999.2.3.4"),TEXT("127.0.0.1:0"),TEXT("127.0.0.1:65536"),TEXT("127.0.0.1:abc"),TEXT("127.0.0.1:12.5"),TEXT("127.0.0.1:+12"),TEXT("127.0.0.1:-12"),TEXT("127.0.0.1:1e3"),TEXT("127.0.0.1?game=/Script/Engine.GameMode"),TEXT("localhost?Name=Other"),TEXT("127.0.0.1:7777?listen"),TEXT("1..2.3"),TEXT("/Game/Map/MainMenu")})
		TestFalse(Invalid,URogueRoomSubsystem::ValidateAddress(Invalid,Out));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueLobbyReadyGateTest,"Rogue.Networking.Lobby.AllPlayersMustBeReady",EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FRogueLobbyReadyGateTest::RunTest(const FString& Parameters)
{
	FTestWorldWrapper Fixture;
	if (!TestTrue(TEXT("Create isolated test world"),Fixture.CreateTestWorld(EWorldType::Game))) return false;
	UWorld* World=Fixture.GetTestWorld();
	ARogueLobbyGameState* Lobby=World->SpawnActor<ARogueLobbyGameState>();
	TestFalse(TEXT("Empty room cannot start"),Lobby->AreAllPlayersReady());
	ARogueLobbyPlayerState* Host=World->SpawnActor<ARogueLobbyPlayerState>();
	ARogueLobbyPlayerState* Guest=World->SpawnActor<ARogueLobbyPlayerState>();
	Lobby->PlayerArray={Host,Guest}; Host->bHost=true;
	Host->bReady=true;
	TestFalse(TEXT("Host readiness does not substitute for guest readiness"),Lobby->AreAllPlayersReady());
	Guest->bReady=true;
	TestTrue(TEXT("All participants ready"),Lobby->AreAllPlayersReady());
	Host->bReady=false;
	TestFalse(TEXT("Host also needs to prepare"),Lobby->AreAllPlayersReady());
	Host->bReady=true; Guest->bReady=false;
	TestFalse(TEXT("Cancelling readiness closes the start gate"),Lobby->AreAllPlayersReady());
	Lobby->PlayerArray.Remove(Guest);
	TestTrue(TEXT("Disconnected guest no longer blocks remaining prepared participants"),Lobby->AreAllPlayersReady());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueLobbyEditorTravelTest,"Rogue.Networking.Lobby.EditorSeamlessTravelEnabled",EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter)
bool FRogueLobbyEditorTravelTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("Lobby retains existing players during server travel"),GetDefault<ARogueLobbyGameMode>()->bUseSeamlessTravel);
	const IConsoleVariable* AllowPIE=IConsoleManager::Get().FindConsoleVariable(TEXT("net.AllowPIESeamlessTravel"));
	if (TestNotNull(TEXT("Engine supports seamless PIE travel"),AllowPIE))
		TestEqual(TEXT("Project enables seamless travel for editor multiplayer sessions"),AllowPIE->GetInt(),1);
	return true;
}
#endif
