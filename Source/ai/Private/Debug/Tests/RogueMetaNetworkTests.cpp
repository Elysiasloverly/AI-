#if WITH_DEV_AUTOMATION_TESTS

#include "Enemies/RogueEnemy.h"
#include "Combat/Weapons/RogueWeaponBase.h"
#include "Combat/Weapons/RogueWeaponBase.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "Framework/RogueGameMode.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/WorldSettings.h"
#include "Misc/AutomationTest.h"
#include "Player/RogueCharacter.h"
#include "Player/RoguePlayerBalanceAsset.h"
#include "Player/RoguePlayerController.h"
#include "Player/RoguePlayerState.h"
#include "Progression/Meta/RogueUpgradeTreeDefinition.h"
#include "Tests/AutomationCommon.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

namespace RogueMetaNetworkTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;

	FRogueProgressionSnapshot WeaponProfile(ERogueUpgradeType QuantityType)
	{
		FRogueProgressionSnapshot Snapshot;
		for (const FRogueUpgradeTreeNode& Node : URogueUpgradeTreeDefinition::GetDefaultDefinition()->GetNodes())
		{
			if (Node.Kind == ERogueMetaNodeKind::Weapon && Node.UpgradeType == QuantityType)
			{
				Snapshot.NodeRanks.Add(Node.NodeId, 1);
			}
		}
		return Snapshot;
	}

	struct FFixture
	{
		FTestConsoleVariable WeaponSelfTest{TEXT("Rogue.SelfTest.WeaponUpgradeChain")};
		FTestConsoleVariable EnemyStress{TEXT("Rogue.SelfTest.EnemyStress")};
		TStrongObjectPtr<URoguePlayerBalanceAsset> Balance{NewObject<URoguePlayerBalanceAsset>()};
		FTestWorldWrapper TestWorld;
		UWorld* World = nullptr;
		ARogueGameMode* Mode = nullptr;
		TArray<ARoguePlayerController*> Controllers;
		TArray<ARoguePlayerState*> States;
		TArray<ARogueCharacter*> Players;

		bool Initialize(FAutomationTestBase& Test, int32 ParticipantCount, bool bSpawnPawns)
		{
			WeaponSelfTest.Set(TEXT("0"));
			EnemyStress.Set(TEXT("0"));
			if (!TestWorld.CreateTestWorld(EWorldType::Game)) { return false; }
			World = TestWorld.GetTestWorld();
			World->GetWorldSettings()->DefaultGameMode = ARogueGameMode::StaticClass();
			if (!TestWorld.BeginPlayInTestWorld()) { return false; }
			Mode = World->GetAuthGameMode<ARogueGameMode>();
			if (!Test.TestNotNull(TEXT("Authoritative mode exists"), Mode)) { return false; }
			Mode->SetActorTickEnabled(false);
			for (int32 Index = 0; Index < ParticipantCount; ++Index)
			{
				ARoguePlayerController* Controller = World->SpawnActor<ARoguePlayerController>();
				if (!Test.TestNotNull(TEXT("Participant controller exists"), Controller)) { return false; }
				ARoguePlayerState* State = Controller->GetPlayerState<ARoguePlayerState>();
				if (!Test.TestNotNull(TEXT("Participant state exists"), State)) { return false; }
				// No local player and no connection: all RPC disk-writing paths stay unexecuted.
				if (!Test.TestNull(TEXT("Native fixture has no local saved profile"), Controller->GetLocalPlayer())) { return false; }
				Controllers.Add(Controller);
				States.Add(State);
				if (bSpawnPawns)
				{
					if (!Test.TestTrue(TEXT("Fresh in-memory snapshot is accepted before possession"), State->SetProgressionSnapshotForRun(FRogueProgressionSnapshot()))) { return false; }
					const FTransform Transform(FVector(Index * 500.0f, 0, 180));
					ARogueCharacter* Player = World->SpawnActorDeferred<ARogueCharacter>(ARogueCharacter::StaticClass(), Transform,
						nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
					if (!Test.TestNotNull(TEXT("Participant pawn exists"), Player)) { return false; }
					UDataTable* EmptyWeaponTable = NewObject<UDataTable>(World);
					EmptyWeaponTable->RowStruct = FRogueWeaponTableRow::StaticStruct();
					FObjectProperty* TableProperty = FindFProperty<FObjectProperty>(ARogueCharacter::StaticClass(), TEXT("WeaponDataTable"));
					FProperty* BalanceProperty = FindFProperty<FProperty>(ARogueCharacter::StaticClass(), TEXT("PlayerBalanceAsset"));
					if (!Test.TestNotNull(TEXT("Weapon configuration is reflected"), TableProperty)
						|| !Test.TestNotNull(TEXT("Player balance is reflected"), BalanceProperty)) { return false; }
					TableProperty->SetObjectPropertyValue_InContainer(Player, EmptyWeaponTable);
					*BalanceProperty->ContainerPtrToValuePtr<TSoftObjectPtr<URoguePlayerBalanceAsset>>(Player) = Balance.Get();
					Player->FinishSpawning(Transform);
					Controller->Possess(Player);
					Player->SetActorTickEnabled(false);
					Player->GetCharacterMovement()->SetComponentTickEnabled(false);
					Player->SetActorEnableCollision(false);
					Players.Add(Player);
					if (!Test.TestTrue(TEXT("Participant GAS baseline initialized"), State->IsBaseAttributesInitialized())) { return false; }
				}
			}
			return true;
		}

		ARogueEnemy* SpawnEnemy(bool bBoss)
		{
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			ARogueEnemy* Enemy = World->SpawnActor<ARogueEnemy>(ARogueEnemy::StaticClass(), FVector(0, 1000, 160), FRotator::ZeroRotator, Params);
			if (!Enemy) { return nullptr; }
			Enemy->ActivatePooledEnemy(Mode, Enemy->GetActorLocation(), FRotator::ZeroRotator);
			FRogueEnemyProfile Profile;
			Profile.bIsBoss = bBoss;
			Profile.Health = 100.0f;
			Profile.Speed = 0.0f;
			Profile.ExperienceReward = 0;
			Enemy->InitializeEnemy(Profile);
			Enemy->SetActorTickEnabled(false);
			Enemy->GetCharacterMovement()->SetComponentTickEnabled(false);
			return Enemy;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueMetaFrozenProfilesTest, "Rogue.Progression.Network.IndependentSnapshotsFreezeOnce", RogueMetaNetworkTests::Flags)
bool FRogueMetaFrozenProfilesTest::RunTest(const FString& Parameters)
{
	// Arrange: the native no-LocalPlayer fallback may become ready without freezing a profile.
	RogueMetaNetworkTests::FFixture Fixture;
	if (!Fixture.Initialize(*this, 2, false)) { return false; }
	const FRogueProgressionSnapshot Laser = RogueMetaNetworkTests::WeaponProfile(ERogueUpgradeType::LaserCount);
	const FRogueProgressionSnapshot Scythe = RogueMetaNetworkTests::WeaponProfile(ERogueUpgradeType::ScytheCount);
	const URogueUpgradeTreeDefinition* Definition = URogueUpgradeTreeDefinition::GetDefaultDefinition();

	// Act: each player submits one different profile, then tries to replace it mid-run.
	TestTrue(TEXT("First independent profile accepted"), Fixture.States[0]->SetProgressionSnapshotForRun(Laser));
	TestTrue(TEXT("Second independent profile accepted"), Fixture.States[1]->SetProgressionSnapshotForRun(Scythe));
	TestFalse(TEXT("A second submission cannot replace the first profile"), Fixture.States[0]->SetProgressionSnapshotForRun(Scythe));
	TestFalse(TEXT("A second submission cannot replace the second profile"), Fixture.States[1]->SetProgressionSnapshotForRun(Laser));

	// Assert: each authoritative PlayerState retains its own permissions.
	TestTrue(TEXT("First keeps laser"), Definition->IsWeaponUnlocked(Fixture.States[0]->GetProgressionSnapshot(), ERogueUpgradeType::LaserCount));
	TestFalse(TEXT("First does not borrow scythe"), Definition->IsWeaponUnlocked(Fixture.States[0]->GetProgressionSnapshot(), ERogueUpgradeType::ScytheCount));
	TestTrue(TEXT("Second keeps scythe"), Definition->IsWeaponUnlocked(Fixture.States[1]->GetProgressionSnapshot(), ERogueUpgradeType::ScytheCount));
	TestFalse(TEXT("Second does not borrow laser"), Definition->IsWeaponUnlocked(Fixture.States[1]->GetProgressionSnapshot(), ERogueUpgradeType::LaserCount));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueMetaBossParticipantsTest, "Rogue.Progression.Network.BossRewardsIncludeDownedExcludeSpectators", RogueMetaNetworkTests::Flags)
bool FRogueMetaBossParticipantsTest::RunTest(const FString& Parameters)
{
	// Arrange: two active participants, one of whom enters the real downed state, plus a spectator.
	RogueMetaNetworkTests::FFixture Fixture;
	if (!Fixture.Initialize(*this, 2, true)) { return false; }
	ARoguePlayerController* Spectator = Fixture.World->SpawnActor<ARoguePlayerController>();
	ARoguePlayerState* SpectatorState = Spectator ? Spectator->GetPlayerState<ARoguePlayerState>() : nullptr;
	if (!TestNotNull(TEXT("Spectator state exists"), SpectatorState)) { return false; }
	SpectatorState->SetIsOnlyASpectator(true);
	Fixture.Players[1]->Die();
	if (!TestTrue(TEXT("Second participant is downed"), Fixture.Players[1]->IsDowned())) { return false; }
	TestFalse(TEXT("Surviving teammate prevents game over"), Fixture.Mode->IsGameOver());
	ARogueEnemy* Normal = Fixture.SpawnEnemy(false);
	ARogueEnemy* Boss = Fixture.SpawnEnemy(true);
	if (!TestNotNull(TEXT("Normal enemy exists"), Normal) || !TestNotNull(TEXT("Boss exists"), Boss)) { return false; }

	// Act: normal kills do not award ice bricks; one boss activation can only reward once.
	Fixture.Mode->HandleEnemyKilled(Normal, Fixture.Players[0]);
	TestEqual(TEXT("Normal enemy gives no ice brick"), Fixture.States[0]->GetBossIceBricksEarnedThisRun(), 0);
	Fixture.Mode->HandleEnemyKilled(Boss, Fixture.Players[0]);
	Fixture.Mode->HandleEnemyKilled(Boss, Fixture.Players[0]);
	Fixture.States[0]->GrantBossIceBrick(Boss->GetSpawnCycleId());

	// Assert: server bookkeeping is independent and idempotent; no local save was accessed.
	TestEqual(TEXT("Alive participant receives exactly one"), Fixture.States[0]->GetBossIceBricksEarnedThisRun(), 1);
	TestEqual(TEXT("Downed participant receives exactly one"), Fixture.States[1]->GetBossIceBricksEarnedThisRun(), 1);
	TestEqual(TEXT("Spectator receives none"), SpectatorState->GetBossIceBricksEarnedThisRun(), 0);
	TestNull(TEXT("First participant still has no disk-backed local player"), Fixture.Controllers[0]->GetLocalPlayer());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueMetaBossPoolRewardTest, "Rogue.Progression.Network.PooledBossReactivationCreatesNewReward", RogueMetaNetworkTests::Flags)
bool FRogueMetaBossPoolRewardTest::RunTest(const FString& Parameters)
{
	// Arrange: reuse one real enemy actor across two separate boss activations.
	RogueMetaNetworkTests::FFixture Fixture;
	if (!Fixture.Initialize(*this, 2, false)) { return false; }
	ARogueEnemy* Boss = Fixture.SpawnEnemy(true);
	if (!TestNotNull(TEXT("Pooled boss exists"), Boss)) { return false; }
	const FGuid FirstCycle = Boss->GetSpawnCycleId();
	Fixture.Mode->HandleEnemyKilled(Boss, nullptr);

	// Act: returning to and leaving the pool must generate a new reward ID.
	Boss->DeactivateToPool();
	Boss->ActivatePooledEnemy(Fixture.Mode, FVector(0, 1000, 160), FRotator::ZeroRotator);
	FRogueEnemyProfile Profile;
	Profile.bIsBoss = true; Profile.Health = 100; Profile.Speed = 0; Profile.ExperienceReward = 0;
	Boss->InitializeEnemy(Profile);
	const FGuid SecondCycle = Boss->GetSpawnCycleId();
	Fixture.Mode->HandleEnemyKilled(Boss, nullptr);
	Fixture.Mode->HandleEnemyKilled(Boss, nullptr);

	// Assert: the new incarnation rewards every participant once, rather than being suppressed forever.
	TestTrue(TEXT("First cycle ID is valid"), FirstCycle.IsValid());
	TestTrue(TEXT("Second cycle ID is valid"), SecondCycle.IsValid());
	TestTrue(TEXT("Pool activation changes the ID"), FirstCycle != SecondCycle);
	TestEqual(TEXT("First participant receives both bosses exactly once"), Fixture.States[0]->GetBossIceBricksEarnedThisRun(), 2);
	TestEqual(TEXT("Second participant receives both bosses exactly once"), Fixture.States[1]->GetBossIceBricksEarnedThisRun(), 2);
	return true;
}

#endif
