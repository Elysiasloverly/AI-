#if WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystem/RogueAbilitySystemComponent.h"
#include "AbilitySystem/RogueAbilitySystemLibrary.h"
#include "AbilitySystem/RogueAttributeSet.h"
#include "AbilitySystem/RogueGameplayTags.h"
#include "AbilitySystem/GameplayEffects/RogueStateEffects.h"
#include "Components/BoxComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Enemies/RogueEnemy.h"
#include "Framework/RogueGameMode.h"
#include "Framework/RogueGameState.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/WorldSettings.h"
#include "Misc/AutomationTest.h"
#include "Player/RogueCharacter.h"
#include "Player/RoguePlayerController.h"
#include "Player/RoguePlayerLifeState.h"
#include "Player/RoguePlayerQuery.h"
#include "Player/RoguePlayerState.h"
#include "Tests/AutomationCommon.h"
#include "UI/RogueHUD.h"
#include "UI/Widgets/RogueDeathScreenWidgetBase.h"
#include "UI/Widgets/RogueReviveCountdownWidgetBase.h"

// Access is limited to a received presentation snapshot and HUD inspection. Gameplay
// tests enter death through GAS and advance the real world/timer manager.
struct FRoguePlayerLifeCycleTestAccess
{
	static void ReceiveLifeState(ARogueCharacter* Character, ERoguePlayerLifePhase Phase, float Deadline)
	{
		Character->LifeState.Phase = Phase;
		Character->LifeState.ReviveServerTime = Deadline;
		Character->OnRep_LifeState();
	}

	static URogueReviveCountdownWidgetBase* GetResurrectionWidget(ARogueHUD* HUD)
	{
		return HUD->ReviveCountdownWidget;
	}

	static URogueDeathScreenWidgetBase* GetDeathWidget(ARogueHUD* HUD)
	{
		return HUD->DeathScreenWidget;
	}

	static void Refresh(ARogueHUD* HUD, const ARogueCharacter* Character)
	{
		HUD->UpdateReviveCountdownWidget(Character);
		HUD->UpdateDeathWidget();
	}
};

namespace RoguePlayerLifeCycleTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;

	struct FPrimitiveSnapshot
	{
		TWeakObjectPtr<UPrimitiveComponent> Component;
		ECollisionEnabled::Type Collision = ECollisionEnabled::NoCollision;
		bool bOverlap = false;
	};

	struct FTwoPlayerFixture
	{
		FTestConsoleVariable WeaponSelfTest{TEXT("Rogue.SelfTest.WeaponUpgradeChain")};
		FTestConsoleVariable EnemyStress{TEXT("Rogue.SelfTest.EnemyStress")};
		FTestWorldWrapper TestWorld;
		UWorld* World = nullptr;
		ARogueGameMode* GameMode = nullptr;
		ARogueGameState* GameState = nullptr;
		ARoguePlayerController* Controllers[2] = {};
		ARoguePlayerState* States[2] = {};
		ARogueCharacter* Pawns[2] = {};
		TArray<FPrimitiveSnapshot> InitialPrimitives[2];

		bool Initialize(FAutomationTestBase& Test)
		{
			WeaponSelfTest.Set(TEXT("0"));
			EnemyStress.Set(TEXT("0"));
			if (!TestWorld.CreateTestWorld(EWorldType::Game))
			{
				TestWorld.ForwardErrorMessages(&Test);
				return false;
			}
			World = TestWorld.GetTestWorld();
			World->GetWorldSettings()->DefaultGameMode = ARogueGameMode::StaticClass();
			if (!TestWorld.BeginPlayInTestWorld())
			{
				TestWorld.ForwardErrorMessages(&Test);
				return false;
			}
			GameMode = World->GetAuthGameMode<ARogueGameMode>();
			GameState = World->GetGameState<ARogueGameState>();
			if (!Test.TestNotNull(TEXT("The real authoritative GameMode exists"), GameMode)
				|| !Test.TestNotNull(TEXT("The real replicated GameState exists"), GameState))
			{
				return false;
			}

			// Spawn scheduling is called by GameMode::Tick. Its public death/team-wipe
			// entry points remain active; no unrelated enemies can kill the survivor.
			GameMode->SetActorTickEnabled(false);
			for (int32 Index = 0; Index < 2; ++Index)
			{
				Controllers[Index] = World->SpawnActor<ARoguePlayerController>();
				if (!Test.TestNotNull(TEXT("Each real PlayerController exists"), Controllers[Index]))
				{
					return false;
				}
				States[Index] = Controllers[Index]->GetPlayerState<ARoguePlayerState>();
				if (!Test.TestNotNull(TEXT("The controller creates a Rogue PlayerState"), States[Index]))
				{
					return false;
				}

				const FTransform Transform(FRotator::ZeroRotator, FVector(Index * 500.0f, 0.0f, 120.0f));
				Pawns[Index] = World->SpawnActorDeferred<ARogueCharacter>(ARogueCharacter::StaticClass(),
					Transform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
				if (!Test.TestNotNull(TEXT("Each real player Pawn exists"), Pawns[Index]))
				{
					return false;
				}
				// Explicitly retain GameMode ownership during fixture initialization.
				Pawns[Index]->AutoPossessPlayer = EAutoReceiveInput::Disabled;
				Pawns[Index]->FinishSpawning(Transform);
				Controllers[Index]->Possess(Pawns[Index]);
				// Isolate falling/physics while retaining Pawn, ASC and timer ticks.
				Pawns[Index]->GetCharacterMovement()->SetComponentTickEnabled(false);

				UAbilitySystemComponent* ASC = Pawns[Index]->GetAbilitySystemComponent();
				if (!Test.TestNotNull(TEXT("The player has a real GAS component"), ASC)
					|| !Test.TestNotNull(TEXT("The player has a real GAS AttributeSet"), States[Index]->GetAttributeSet()))
				{
					return false;
				}
				Test.TestTrue(TEXT("PC and Pawn are paired"), Controllers[Index]->GetPawn() == Pawns[Index]);
				Test.TestTrue(TEXT("Pawn and PlayerState are paired"), Pawns[Index]->GetPlayerState() == States[Index]);
				Test.TestTrue(TEXT("PlayerState knows its own Pawn"), States[Index]->GetPawn() == Pawns[Index]);
				Test.TestTrue(TEXT("ASC owner is its own PlayerState"), ASC->GetOwnerActor() == States[Index]);
				Test.TestTrue(TEXT("ASC avatar is its own Pawn"), ASC->GetAvatarActor() == Pawns[Index]);
				Test.TestTrue(TEXT("Both players start Alive"), Pawns[Index]->IsAlive());

				// Add a query component to exercise complete ghost collision handling,
				// rather than only the built-in capsule's collision switch.
				UBoxComponent* Supplemental = NewObject<UBoxComponent>(Pawns[Index]);
				Supplemental->SetupAttachment(Pawns[Index]->GetRootComponent());
				Supplemental->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
				Supplemental->SetGenerateOverlapEvents(true);
				Pawns[Index]->AddInstanceComponent(Supplemental);
				Supplemental->RegisterComponent();

				TInlineComponentArray<UPrimitiveComponent*> Primitives;
				Pawns[Index]->GetComponents(Primitives);
				for (UPrimitiveComponent* Primitive : Primitives)
				{
					InitialPrimitives[Index].Add({Primitive, Primitive->GetCollisionEnabled(), Primitive->GetGenerateOverlapEvents()});
				}
				Pawns[Index]->SetCurrentArmor(0.0f);
			}
			return !Test.HasAnyErrors();
		}

		void Advance(float Seconds)
		{
			// FTestWorldWrapper advances GFrameCounter after each tick. Without this,
			// FTimerManager silently skips repeated ticks in a synchronous test.
			while (Seconds > KINDA_SMALL_NUMBER)
			{
				const float Step = FMath::Min(Seconds, 0.02f);
				TestWorld.TickTestWorld(Step);
				Seconds -= Step;
			}
		}

		void Kill(FAutomationTestBase& Test, int32 Index, bool bAdvanceDeathFrame = true)
		{
			Test.TestTrue(TEXT("Real GAS lethal damage is accepted"), RogueAbilitySystem::ApplyDamageToTarget(
				Pawns[1 - Index], Pawns[Index], 1000000.0f, RogueGameplayTags::Damage_Source_Contact));
			if (bAdvanceDeathFrame)
			{
				Advance(0.04f);
			}
		}

		void CheckGhost(FAutomationTestBase& Test, int32 Index)
		{
			Test.TestFalse(TEXT("A ghost disables all actor collision"), Pawns[Index]->GetActorEnableCollision());
			Test.TestTrue(TEXT("A ghost has no movement mode"), Pawns[Index]->GetCharacterMovement()->MovementMode == MOVE_None);
			for (const FPrimitiveSnapshot& Snapshot : InitialPrimitives[Index])
			{
				if (UPrimitiveComponent* Primitive = Snapshot.Component.Get())
				{
					Test.TestTrue(FString::Printf(TEXT("Ghost primitive %s has no collision"), *Primitive->GetName()),
						Primitive->GetCollisionEnabled() == ECollisionEnabled::NoCollision);
					Test.TestFalse(TEXT("Ghost primitive disables overlap events"), Primitive->GetGenerateOverlapEvents());
				}
			}
		}

		void CheckRestored(FAutomationTestBase& Test, int32 Index)
		{
			Test.TestTrue(TEXT("Revival restores actor collision"), Pawns[Index]->GetActorEnableCollision());
			Test.TestTrue(TEXT("Revival restores walking"), Pawns[Index]->GetCharacterMovement()->MovementMode == MOVE_Walking);
			for (const FPrimitiveSnapshot& Snapshot : InitialPrimitives[Index])
			{
				if (UPrimitiveComponent* Primitive = Snapshot.Component.Get())
				{
					Test.TestTrue(FString::Printf(TEXT("Revival restores %s collision configuration"), *Primitive->GetName()),
						Primitive->GetCollisionEnabled() == Snapshot.Collision);
					Test.TestTrue(TEXT("Revival restores the original overlap configuration"), Primitive->GetGenerateOverlapEvents() == Snapshot.bOverlap);
				}
			}
			Test.TestTrue(TEXT("Revival keeps the same owning controller"), Controllers[Index]->GetPawn() == Pawns[Index]);
			Test.TestTrue(TEXT("Revival keeps the same GAS owner"), Pawns[Index]->GetAbilitySystemComponent()->GetOwnerActor() == States[Index]);
		}

		void CheckFinalWipe(FAutomationTestBase& Test)
		{
			Test.TestTrue(TEXT("The authoritative run ends immediately"), GameMode->IsGameOver());
			Test.TestTrue(TEXT("GameState publishes game over in the death frame"), GameState->IsGameOver());
			for (int32 Index = 0; Index < 2; ++Index)
			{
				Test.TestTrue(TEXT("Team wipe finalizes every player"), Pawns[Index]->IsEliminated());
				Test.TestFalse(TEXT("Team wipe cancels pending revival"), Pawns[Index]->IsRevivePending());
				Test.TestTrue(TEXT("Eliminated players have no revival countdown"), FMath::IsNearlyZero(Pawns[Index]->GetReviveSecondsRemaining()));
				CheckGhost(Test, Index);
			}
		}
	};
	
	bool IsShown(const UUserWidget* Widget)
	{
		return Widget->GetVisibility() != ESlateVisibility::Collapsed && Widget->GetVisibility() != ESlateVisibility::Hidden;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueGhostRevivalDeadlineTest,
	"Rogue.Gameplay.LifeCycle.TenSecondGhostAndRestore", RoguePlayerLifeCycleTests::Flags)

bool FRogueGhostRevivalDeadlineTest::RunTest(const FString& Parameters)
{
	using namespace RoguePlayerLifeCycleTests;
	(void)Parameters;
	FTwoPlayerFixture Fixture;
	if (!Fixture.Initialize(*this)) { return false; }
	FActorSpawnParameters EnemySpawn;
	EnemySpawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ARogueEnemy* Enemy = Fixture.World->SpawnActor<ARogueEnemy>(ARogueEnemy::StaticClass(),
		Fixture.Pawns[0]->GetActorLocation(), FRotator::ZeroRotator, EnemySpawn);
	if (!TestNotNull(TEXT("A real enemy exists for target/cache checks"), Enemy)) { return false; }
	FRogueEnemyProfile Profile;
	Profile.Health = 10000.0f;
	Profile.Speed = 0.0f;
	Profile.Damage = 0.0f;
	Profile.ExperienceReward = 0;
	Enemy->ActivatePooledEnemy(nullptr, Fixture.Pawns[0]->GetActorLocation(), FRotator::ZeroRotator);
	Enemy->InitializeEnemy(Profile);
	Enemy->SetActorTickEnabled(false);
	Enemy->GetCharacterMovement()->SetComponentTickEnabled(false);
	Enemy->Tick(0.02f);
	TestTrue(TEXT("The enemy initially acquires the nearby living player"), Enemy->GetPlayerCharacter() == Fixture.Pawns[0]);
	Fixture.Kill(*this, 0);
	TestTrue(TEXT("Lethal damage enters Downed"), Fixture.Pawns[0]->IsDowned());
	TestTrue(TEXT("The living teammate remains Alive"), Fixture.Pawns[1]->IsAlive());
	TestFalse(TEXT("One downed player does not end the run"), Fixture.GameMode->IsGameOver());
	TestTrue(TEXT("Revival is pending after lethal damage"), Fixture.Pawns[0]->IsRevivePending());
	TestTrue(TEXT("The countdown starts at ten seconds"), Fixture.Pawns[0]->GetReviveSecondsRemaining() > 9.9f);
	Fixture.CheckGhost(*this, 0);

	TArray<ARogueCharacter*, TInlineAllocator<4>> AlivePlayers;
	RoguePlayerQuery::CollectAlivePlayers(Fixture.World, AlivePlayers);
	TestTrue(TEXT("Target collection excludes the ghost"), AlivePlayers.Num() == 1 && AlivePlayers[0] == Fixture.Pawns[1]);
	TestTrue(TEXT("A nearby ghost is never the nearest alive target"),
		RoguePlayerQuery::FindNearestAlivePlayer(Fixture.World, Fixture.Pawns[0]->GetActorLocation()) == Fixture.Pawns[1]);
	TestNull(TEXT("An enemy's old cached target becomes invalid as soon as that player is Downed"), Enemy->GetPlayerCharacter());
	Enemy->Tick(0.3f);
	TestTrue(TEXT("The real enemy switches its target to the living teammate"), Enemy->GetPlayerCharacter() == Fixture.Pawns[1]);
	TestFalse(TEXT("The real contact-attack query ignores the overlapping ghost"), Enemy->HasTouchingPlayer());
	TestFalse(TEXT("Further damage cannot hit the ghost"), RogueAbilitySystem::ApplyDamageToTarget(
		Fixture.Pawns[1], Fixture.Pawns[0], 10.0f, RogueGameplayTags::Damage_Source_Contact));

	Fixture.Pawns[0]->Revive();
	Fixture.Pawns[0]->TryReviveIfExpired();
	TestTrue(TEXT("Direct early revival is rejected"), Fixture.Pawns[0]->IsDowned());
	Fixture.Pawns[0]->SetActorTickEnabled(false);
	Fixture.Advance(9.8f);
	TestTrue(TEXT("The player cannot revive before ten seconds"), Fixture.Pawns[0]->IsDowned());
	Fixture.Advance(0.3f);
	TestTrue(TEXT("The real world timer revives after ten seconds even without Pawn Tick"), Fixture.Pawns[0]->IsAlive());
	TestFalse(TEXT("Revival consumes the pending state"), Fixture.Pawns[0]->IsRevivePending());
	TestTrue(TEXT("Revival clears the countdown"), FMath::IsNearlyZero(Fixture.Pawns[0]->GetReviveSecondsRemaining()));
	TestTrue(TEXT("Revival restores positive GAS health"), Fixture.States[0]->GetAttributeSet()->GetHealth() > 0.0f);
	TestFalse(TEXT("Successful revival leaves the run active"), Fixture.GameMode->IsGameOver());
	Fixture.CheckRestored(*this, 0);
	Fixture.Pawns[0]->SetActorTickEnabled(true);
	Enemy->Tick(0.3f);
	TestTrue(TEXT("The revived player can be targeted again"), Enemy->GetPlayerCharacter() == Fixture.Pawns[0]);
	TestTrue(TEXT("The revived player participates in contact queries again"), Enemy->HasTouchingPlayer());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueRevivalMissingEffectTest,
	"Rogue.Gameplay.LifeCycle.RepeatedDeathAndMissingEffect", RoguePlayerLifeCycleTests::Flags)

bool FRogueRevivalMissingEffectTest::RunTest(const FString& Parameters)
{
	using namespace RoguePlayerLifeCycleTests;
	(void)Parameters;
	FTwoPlayerFixture Fixture;
	if (!Fixture.Initialize(*this)) { return false; }
	Fixture.Kill(*this, 0);
	Fixture.Advance(3.0f);
	const float Remaining = Fixture.Pawns[0]->GetReviveSecondsRemaining();
	Fixture.Pawns[0]->Die();
	Fixture.Pawns[0]->Die();
	TestTrue(TEXT("Repeated death cannot restart the ten-second deadline"),
		FMath::IsNearlyEqual(Fixture.Pawns[0]->GetReviveSecondsRemaining(), Remaining, 0.01f));

	FGameplayTagContainer DownedTags;
	DownedTags.AddTag(RogueGameplayTags::State_Downed);
	Fixture.Pawns[0]->GetAbilitySystemComponent()->RemoveActiveEffectsWithGrantedTags(DownedTags);
	TestFalse(TEXT("The Downed GE is genuinely absent"),
		Fixture.Pawns[0]->GetAbilitySystemComponent()->HasMatchingGameplayTag(RogueGameplayTags::State_Downed));
	Fixture.Pawns[0]->TryReviveIfExpired();
	Fixture.Pawns[0]->Revive();
	TestTrue(TEXT("Missing GE cannot authorize early revival"), Fixture.Pawns[0]->IsDowned());
	TestTrue(TEXT("Missing GE cannot erase the authoritative deadline"),
		FMath::IsNearlyEqual(Fixture.Pawns[0]->GetReviveSecondsRemaining(), Remaining, 0.01f));
	Fixture.Advance(6.8f);
	TestTrue(TEXT("Missing GE still respects the original deadline"), Fixture.Pawns[0]->IsDowned());
	Fixture.Advance(0.3f);
	TestTrue(TEXT("The authoritative deadline still completes without the GE"), Fixture.Pawns[0]->IsAlive());
	Fixture.CheckRestored(*this, 0);
	Fixture.Advance(2.0f); // Let the normal resurrection protection expire.
	Fixture.Kill(*this, 0);
	TestTrue(TEXT("A second death starts a fresh ten-second wait"), Fixture.Pawns[0]->IsDowned()
		&& Fixture.Pawns[0]->GetReviveSecondsRemaining() > 9.9f);
	Fixture.Advance(9.8f);
	TestTrue(TEXT("The second death cannot revive early"), Fixture.Pawns[0]->IsDowned());
	Fixture.Advance(0.3f);
	TestTrue(TEXT("Repeated cycles revive successfully"), Fixture.Pawns[0]->IsAlive());
	Fixture.CheckRestored(*this, 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueDelayedTeamWipeTest,
	"Rogue.Gameplay.LifeCycle.TeamWipeCancelsRevival", RoguePlayerLifeCycleTests::Flags)

bool FRogueDelayedTeamWipeTest::RunTest(const FString& Parameters)
{
	using namespace RoguePlayerLifeCycleTests;
	(void)Parameters;
	FTwoPlayerFixture Fixture;
	if (!Fixture.Initialize(*this)) { return false; }
	Fixture.Kill(*this, 0);
	Fixture.Advance(5.0f);
	TestTrue(TEXT("The first player is still Downed at five seconds"), Fixture.Pawns[0]->IsDowned());
	Fixture.Kill(*this, 1);
	Fixture.CheckFinalWipe(*this);
	Fixture.Advance(20.0f);
	for (ARogueCharacter* Pawn : Fixture.Pawns)
	{
		Pawn->TryReviveIfExpired();
		Pawn->Revive();
		Pawn->Die();
	}
	Fixture.GameMode->EvaluateTeamWipe();
	Fixture.CheckFinalWipe(*this);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueRevivalTeamWipeRaceTest,
	"Rogue.Gameplay.LifeCycle.TeamWipeAtRevivalDeadline", RoguePlayerLifeCycleTests::Flags)

bool FRogueRevivalTeamWipeRaceTest::RunTest(const FString& Parameters)
{
	using namespace RoguePlayerLifeCycleTests;
	(void)Parameters;
	FTwoPlayerFixture Fixture;
	if (!Fixture.Initialize(*this)) { return false; }
	Fixture.Kill(*this, 0);
	Fixture.Advance(9.9f);
	Fixture.Kill(*this, 1, false);
	// Both health values are lethal; Pawn B's deferred death and A's timer
	// compete in the same tick. A must check team wipe before reviving.
	Fixture.Advance(0.2f);
	Fixture.CheckFinalWipe(*this);
	Fixture.Advance(11.0f);
	Fixture.CheckFinalWipe(*this);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueSameFrameTeamWipeTest,
	"Rogue.Gameplay.LifeCycle.SameFrameLethalDamage", RoguePlayerLifeCycleTests::Flags)

bool FRogueSameFrameTeamWipeTest::RunTest(const FString& Parameters)
{
	using namespace RoguePlayerLifeCycleTests;
	(void)Parameters;
	FTwoPlayerFixture Fixture;
	if (!Fixture.Initialize(*this)) { return false; }
	Fixture.Kill(*this, 0, false);
	Fixture.Kill(*this, 1, false);
	TestTrue(TEXT("Both GAS health values are zero before queued death processing"),
		Fixture.States[0]->GetAttributeSet()->GetHealth() <= 0.0f
		&& Fixture.States[1]->GetAttributeSet()->GetHealth() <= 0.0f);
	Fixture.Advance(0.04f);
	Fixture.CheckFinalWipe(*this);
	Fixture.Advance(11.0f);
	Fixture.CheckFinalWipe(*this);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueLifeCycleWidgetOrderingTest,
	"Rogue.UI.LifeCycle.ReplicationOrderAndNoPawn", RoguePlayerLifeCycleTests::Flags)

bool FRogueLifeCycleWidgetOrderingTest::RunTest(const FString& Parameters)
{
	using namespace RoguePlayerLifeCycleTests;
	(void)Parameters;
	FTwoPlayerFixture Fixture;
	if (!Fixture.Initialize(*this)) { return false; }
	const TSubclassOf<URogueReviveCountdownWidgetBase> ResurrectionClass = LoadClass<URogueReviveCountdownWidgetBase>(nullptr,
		TEXT("/Game/UMG/Menus/WBP_ResurrectionScreen.WBP_ResurrectionScreen_C"));
	const TSubclassOf<URogueDeathScreenWidgetBase> DeathClass = LoadClass<URogueDeathScreenWidgetBase>(nullptr,
		TEXT("/Game/UMG/Menus/WBP_DeathScreen.WBP_DeathScreen_C"));
	if (!TestNotNull(TEXT("The actual resurrection Blueprint loads"), ResurrectionClass.Get())
		|| !TestNotNull(TEXT("The actual death Blueprint loads"), DeathClass.Get())) { return false; }

	ARogueHUD* HUDs[2] = {};
	URogueReviveCountdownWidgetBase* Resurrection[2] = {};
	URogueDeathScreenWidgetBase* Death[2] = {};
	for (int32 Index = 0; Index < 2; ++Index)
	{
		// Native HUD BeginPlay creates the actual WBP through its controller.
		// Supply real ULocalPlayer objects so UMG accepts those owning controllers.
		ULocalPlayer* LocalPlayer = NewObject<ULocalPlayer>(GEngine);
		Fixture.World->GetGameInstance()->AddLocalPlayer(LocalPlayer, FPlatformUserId::CreateFromInternalId(Index));
		Fixture.Controllers[Index]->SetPlayer(LocalPlayer);
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Owner = Fixture.Controllers[Index];
		HUDs[Index] = Fixture.World->SpawnActor<ARogueHUD>(ARogueHUD::StaticClass(), SpawnParameters);
		if (!TestNotNull(TEXT("Each player's own HUD exists"), HUDs[Index])) { return false; }
		Resurrection[Index] = FRoguePlayerLifeCycleTestAccess::GetResurrectionWidget(HUDs[Index]);
		Death[Index] = FRoguePlayerLifeCycleTestAccess::GetDeathWidget(HUDs[Index]);
		if (!TestNotNull(TEXT("Each actual resurrection widget exists"), Resurrection[Index])
			|| !TestNotNull(TEXT("Each actual death widget exists"), Death[Index])) { return false; }
		TestTrue(TEXT("Native HUD uses the real resurrection WBP"), Resurrection[Index]->IsA(ResurrectionClass.Get()));
		TestTrue(TEXT("Native HUD uses the real death WBP"), Death[Index]->IsA(DeathClass.Get()));
		TestTrue(TEXT("Resurrection belongs to the correct player"), Resurrection[Index]->GetOwningPlayer() == Fixture.Controllers[Index]);
		TestTrue(TEXT("Death screen belongs to the correct player"), Death[Index]->GetOwningPlayer() == Fixture.Controllers[Index]);
	}

	auto CheckViews = [this, &Resurrection, &Death](int32 Index, bool bResurrection, bool bDeath, const TCHAR* Stage)
	{
		TestTrue(FString::Printf(TEXT("%s: resurrection visibility"), Stage), IsShown(Resurrection[Index]) == bResurrection);
		TestTrue(FString::Printf(TEXT("%s: death visibility"), Stage), IsShown(Death[Index]) == bDeath);
		TestFalse(FString::Printf(TEXT("%s: both screens are never visible together"), Stage),
			IsShown(Resurrection[Index]) && IsShown(Death[Index]));
	};
	const float Deadline = Fixture.World->GetTimeSeconds() + 10.0f;
	FRoguePlayerLifeCycleTestAccess::ReceiveLifeState(Fixture.Pawns[0], ERoguePlayerLifePhase::Downed, Deadline);
	// No Downed GE has arrived at all. The atomic lifecycle snapshot alone must
	// open the resurrection screen, independent of the teammate's draw order.
	FRoguePlayerLifeCycleTestAccess::Refresh(HUDs[1], Fixture.Pawns[1]);
	FRoguePlayerLifeCycleTestAccess::Refresh(HUDs[0], Fixture.Pawns[0]);
	CheckViews(0, true, false, TEXT("Downed snapshot before any GE"));
	CheckViews(1, false, false, TEXT("Living teammate"));

	// An expired local countdown does not mean authoritative revival has arrived.
	FRoguePlayerLifeCycleTestAccess::ReceiveLifeState(Fixture.Pawns[0], ERoguePlayerLifePhase::Downed,
		Fixture.World->GetTimeSeconds() - 1.0f);
	FRoguePlayerLifeCycleTestAccess::Refresh(HUDs[0], Fixture.Pawns[0]);
	CheckViews(0, true, false, TEXT("Expired countdown while still Downed"));
	FRoguePlayerLifeCycleTestAccess::ReceiveLifeState(Fixture.Pawns[0], ERoguePlayerLifePhase::Alive, 0.0f);
	FRoguePlayerLifeCycleTestAccess::Refresh(HUDs[1], Fixture.Pawns[1]);
	FRoguePlayerLifeCycleTestAccess::Refresh(HUDs[0], Fixture.Pawns[0]);
	CheckViews(0, false, false, TEXT("Authoritative revival after teammate refresh"));
	Resurrection[0]->SetVisibility(ESlateVisibility::Visible);
	FRoguePlayerLifeCycleTestAccess::Refresh(HUDs[0], Fixture.Pawns[0]);
	CheckViews(0, false, false, TEXT("Visibility drift after revival"));

	// Exercise the opposite order with a real GAS effect: GE first, then Pawn
	// snapshot, and finally Alive while an old GE is still present locally.
	UAbilitySystemComponent* ASC = Fixture.Pawns[0]->GetAbilitySystemComponent();
	FGameplayEffectSpecHandle DownedSpec = ASC->MakeOutgoingSpec(URogueDownedEffect::StaticClass(), 1.0f, ASC->MakeEffectContext());
	if (!TestTrue(TEXT("A real Downed GE spec is constructed"), DownedSpec.IsValid())) { return false; }
	DownedSpec.Data->SetSetByCallerMagnitude(RogueGameplayTags::Data_Duration, 10.0f);
	const FActiveGameplayEffectHandle EarlyEffect = ASC->ApplyGameplayEffectSpecToSelf(*DownedSpec.Data.Get());
	if (!TestTrue(TEXT("The early Downed GE is genuinely active"), EarlyEffect.IsValid())) { return false; }
	FRoguePlayerLifeCycleTestAccess::Refresh(HUDs[0], Fixture.Pawns[0]);
	CheckViews(0, false, false, TEXT("GE arrives before Downed snapshot"));
	FRoguePlayerLifeCycleTestAccess::ReceiveLifeState(Fixture.Pawns[0], ERoguePlayerLifePhase::Downed, Deadline);
	FRoguePlayerLifeCycleTestAccess::Refresh(HUDs[0], Fixture.Pawns[0]);
	CheckViews(0, true, false, TEXT("Downed snapshot follows GE"));
	FRoguePlayerLifeCycleTestAccess::ReceiveLifeState(Fixture.Pawns[0], ERoguePlayerLifePhase::Alive, 0.0f);
	FRoguePlayerLifeCycleTestAccess::Refresh(HUDs[0], Fixture.Pawns[0]);
	CheckViews(0, false, false, TEXT("Alive arrives before old Downed GE disappears"));
	ASC->RemoveActiveGameplayEffect(EarlyEffect);

	// GameState may arrive before either Pawn's elimination snapshot. It has
	// priority even while one Pawn is Alive and the other remains Downed.
	FRoguePlayerLifeCycleTestAccess::ReceiveLifeState(Fixture.Pawns[1], ERoguePlayerLifePhase::Downed, Deadline);
	Fixture.GameState->SetRunStateSnapshot(1, 0, 5.0f, true, false);
	FRoguePlayerLifeCycleTestAccess::Refresh(HUDs[1], Fixture.Pawns[1]);
	FRoguePlayerLifeCycleTestAccess::Refresh(HUDs[0], Fixture.Pawns[0]);
	CheckViews(0, false, true, TEXT("GameOver arrives before Pawn state"));
	CheckViews(1, false, true, TEXT("GameOver overrides a pending countdown"));

	// Conversely, Eliminated can arrive before GameState in a different actor's
	// replication packet. Final player state should never reopen resurrection.
	Fixture.GameState->SetRunStateSnapshot(1, 0, 5.0f, false, false);
	FRoguePlayerLifeCycleTestAccess::ReceiveLifeState(Fixture.Pawns[0], ERoguePlayerLifePhase::Eliminated, 0.0f);
	FRoguePlayerLifeCycleTestAccess::Refresh(HUDs[0], Fixture.Pawns[0]);
	CheckViews(0, false, true, TEXT("Eliminated arrives before GameState"));

	Fixture.GameState->SetRunStateSnapshot(1, 0, 5.0f, true, false);
	Fixture.Controllers[0]->UnPossess();
	Resurrection[0]->SetVisibility(ESlateVisibility::Visible);
	Death[0]->SetVisibility(ESlateVisibility::Collapsed);
	HUDs[0]->DrawHUD();
	CheckViews(0, false, true, TEXT("GameOver UI survives missing Pawn and repairs drift"));
	Fixture.GameState->SetRunStateSnapshot(1, 0, 5.0f, false, false);
	HUDs[0]->DrawHUD();
	CheckViews(0, false, false, TEXT("Missing Pawn does not retain old screens in an active run"));
	FRoguePlayerLifeCycleTestAccess::ReceiveLifeState(Fixture.Pawns[0], ERoguePlayerLifePhase::Downed, Deadline);
	TestFalse(TEXT("Downed without a resolved controller still disables Pawn input"), Fixture.Pawns[0]->InputEnabled());
	FRoguePlayerLifeCycleTestAccess::ReceiveLifeState(Fixture.Pawns[0], ERoguePlayerLifePhase::Alive, 0.0f);
	TestTrue(TEXT("Alive before Controller resolves restores the Pawn input flag"), Fixture.Pawns[0]->InputEnabled());
	Fixture.Controllers[0]->Possess(Fixture.Pawns[0]);
	TestTrue(TEXT("Late controller ownership cannot leave the revived Pawn input disabled"), Fixture.Pawns[0]->InputEnabled());
	return true;
}

#endif
