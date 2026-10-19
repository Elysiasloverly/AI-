#if WITH_DEV_AUTOMATION_TESTS

#include "World/RogueExperiencePickup.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "Framework/RogueGameMode.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/WorldSettings.h"
#include "Misc/AutomationTest.h"
#include "Player/RogueCharacter.h"
#include "Player/RoguePlayerController.h"
#include "Tests/AutomationCommon.h"

namespace RogueExperiencePickupTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;

	struct FPickupFixture
	{
		FTestWorldWrapper TestWorld;
		UWorld* World = nullptr;
		ARogueGameMode* GameMode = nullptr;
		ARoguePlayerController* Controller = nullptr;
		ARogueCharacter* Player = nullptr;

		bool Initialize(FAutomationTestBase& Test, bool bEnablePlayerCollision = false)
		{
			if (!TestWorld.CreateTestWorld(EWorldType::Game)) { return false; }
			World = TestWorld.GetTestWorld();
			World->GetWorldSettings()->DefaultGameMode = ARogueGameMode::StaticClass();
			if (!TestWorld.BeginPlayInTestWorld()) { return false; }
			GameMode = World->GetAuthGameMode<ARogueGameMode>();
			if (!Test.TestNotNull(TEXT("The authoritative GameMode exists"), GameMode)) { return false; }
			GameMode->SetActorTickEnabled(false);
			Controller = World->SpawnActor<ARoguePlayerController>();
			Player = World->SpawnActorDeferred<ARogueCharacter>(ARogueCharacter::StaticClass(),
				FTransform(FVector(0.0f, 0.0f, 120.0f)), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
			if (!Test.TestNotNull(TEXT("The controller exists"), Controller)
				|| !Test.TestNotNull(TEXT("The player exists"), Player)) { return false; }
			Player->AutoPossessPlayer = EAutoReceiveInput::Disabled;
			Player->FinishSpawning(FTransform(FVector(0.0f, 0.0f, 120.0f)));
			Controller->Possess(Player);
			Player->SetActorTickEnabled(false);
			Player->GetCharacterMovement()->SetComponentTickEnabled(false);
			Player->SetActorEnableCollision(bEnablePlayerCollision);
			Player->GetCapsuleComponent()->SetGenerateOverlapEvents(bEnablePlayerCollision);
			if (bEnablePlayerCollision)
			{
				Player->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
				Player->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
			}
			return Test.TestTrue(TEXT("The player is alive"), Player->IsAlive());
		}

		ARogueExperiencePickup* SpawnPickup()
		{
			FActorSpawnParameters Parameters;
			Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			return World->SpawnActor<ARogueExperiencePickup>(ARogueExperiencePickup::StaticClass(),
				FVector(1000.0f, 0.0f, 120.0f), FRotator::ZeroRotator, Parameters);
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRoguePickupArrivalWithoutOverlapTest,
	"Rogue.Gameplay.ExperiencePickup.MagnetArrivalWithoutOverlap", RogueExperiencePickupTests::Flags)

bool FRoguePickupArrivalWithoutOverlapTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	RogueExperiencePickupTests::FPickupFixture Fixture;
	if (!Fixture.Initialize(*this)) { return false; }
	ARogueExperiencePickup* Pickup = Fixture.SpawnPickup();
	if (!TestNotNull(TEXT("The pickup exists"), Pickup)) { return false; }
	const int32 Before = Fixture.Player->GetCurrentExperience();
	Pickup->ActivatePooledPickup(Fixture.GameMode, Fixture.Player->GetActorLocation() + FVector(200.0f, 0.0f, 0.0f), 2);
	// Advance past auto-magnet delay while player collision remains disabled.
	Pickup->Tick(6.1f);
	TestTrue(TEXT("Arrival collects even when no overlap event can fire"), Pickup->IsAvailableInPool());
	TestTrue(TEXT("Collected pickup is hidden"), Pickup->IsHidden());
	TestEqual(TEXT("The configured experience is awarded"), Fixture.Player->GetCurrentExperience(), Before + 2);
	Pickup->Tick(0.1f);
	TestEqual(TEXT("A later tick cannot award it twice"), Fixture.Player->GetCurrentExperience(), Before + 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRoguePickupActivationOverlapTest,
	"Rogue.Gameplay.ExperiencePickup.ActivationInsidePlayerAndReuse", RogueExperiencePickupTests::Flags)

bool FRoguePickupActivationOverlapTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	RogueExperiencePickupTests::FPickupFixture Fixture;
	if (!Fixture.Initialize(*this, true)) { return false; }
	ARogueExperiencePickup* Pickup = Fixture.SpawnPickup();
	if (!TestNotNull(TEXT("The pickup exists"), Pickup)) { return false; }
	const int32 Before = Fixture.Player->GetCurrentExperience();
	Pickup->ActivatePooledPickup(Fixture.GameMode, Fixture.Player->GetActorLocation(), 2);
	TestTrue(TEXT("Activation inside the capsule collects immediately"), Pickup->IsAvailableInPool());
	TestEqual(TEXT("Activation uses the new value before enabling overlap"), Fixture.Player->GetCurrentExperience(), Before + 2);
	if (!Pickup->IsAvailableInPool()) { return false; }
	Pickup->ActivatePooledPickup(Fixture.GameMode, Fixture.Player->GetActorLocation(), 1);
	TestTrue(TEXT("A reused pickup also collects inside the capsule"), Pickup->IsAvailableInPool());
	TestEqual(TEXT("Reused pickup awards its own value exactly once"), Fixture.Player->GetCurrentExperience(), Before + 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRoguePooledPickupRelevancyTest,
	"Rogue.Gameplay.ExperiencePickup.InactiveStateRemainsRelevant", RogueExperiencePickupTests::Flags)

bool FRoguePooledPickupRelevancyTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	RogueExperiencePickupTests::FPickupFixture Fixture;
	if (!Fixture.Initialize(*this)) { return false; }
	ARogueExperiencePickup* Pickup = Fixture.SpawnPickup();
	if (!TestNotNull(TEXT("The pickup exists"), Pickup)) { return false; }
	Pickup->ActivatePooledPickup(Fixture.GameMode, Fixture.Player->GetActorLocation() + FVector(200.0f, 0.0f, 0.0f), 1);
	Pickup->DeactivateToPool();
	TestTrue(TEXT("A nearby client can receive the inactive state after hiding"),
		Pickup->IsNetRelevantFor(Fixture.Controller, Fixture.Player, Fixture.Player->GetActorLocation()));
	TestFalse(TEXT("Inactive pickups still obey the distance limit"),
		Pickup->IsNetRelevantFor(Fixture.Controller, Fixture.Player, FVector(1000000.0f, 0.0f, 0.0f)));
	return true;
}

#endif
