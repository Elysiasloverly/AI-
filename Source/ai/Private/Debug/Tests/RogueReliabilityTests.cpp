#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Framework/RogueGameMode.h"
#include "Player/RoguePlayerState.h"
#include "Player/RoguePlayerController.h"
#include "Progression/Meta/RogueProgressionSaveGame.h"
#include "Progression/Meta/RogueProgressionSubsystem.h"
#include "Progression/Meta/RogueUpgradeTreeDefinition.h"
#include "Combat/Pooling/RogueCombatPools.h"
#include "Enemies/RogueEnemy.h"
#include "Kismet/GameplayStatics.h"
#include "UI/RogueHUD.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

struct FRogueReliabilityTestAccess
{
	static void Repair(ARogueGameMode* Mode) { Mode->TickPawnOwnershipRepair(); }
	static double& Clock(ARogueGameMode* Mode) { return Mode->LastPawnRepairTime; }
	static void Fail(ARoguePlayerState* State) { State->FailProgressionHandshake(); }
	static void MakeWaiting(ARoguePlayerState* State) { State->bProgressionReady = false; }
	static void Expire(ARoguePlayerState* State) { for (int32 Index = 0; Index < 350; ++Index) { State->GetWorld()->Tick(LEVELTICK_TimeOnly, 0.1f); } State->bProgressionReady = false; State->ProgressionHandshakeStarted = 0.0; }
	static uint64 Refreshes(ARogueHUD* HUD) { return HUD->MenuRefreshCount; }
	static const FRoguePoolClassRuntime& Stats(FRogueCombatPools& Pools, UClass* Class) { return Pools.EnemyPoolRuntime.Classes.FindChecked(Class); }
};

namespace RogueReliabilityTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;
	struct FSlot
	{
		FString Name = TEXT("Automation_Recovery_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
		~FSlot() { UGameplayStatics::DeleteGameInSlot(Name, 0); UGameplayStatics::DeleteGameInSlot(Name + TEXT("_backup"), 0); }
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueRecoveryBackupTest, "Rogue.Reliability.Save.CorruptPrimaryRecoversVerifiedBackup", RogueReliabilityTests::Flags)
bool FRogueRecoveryBackupTest::RunTest(const FString& Parameters)
{
	RogueReliabilityTests::FSlot Slot;
	URogueProgressionSaveGame* Profile = NewObject<URogueProgressionSaveGame>();
	Profile->IceBricks = 7;
	TestTrue(TEXT("Initial transaction commits"), URogueProgressionSubsystem::SaveProfileToSlot(Profile, Slot.Name, 0));
	Profile->IceBricks = 12;
	TestTrue(TEXT("Second transaction retains previous revision"), URogueProgressionSubsystem::SaveProfileToSlot(Profile, Slot.Name, 0));
	URogueProgressionSaveGame* Corrupted = Cast<URogueProgressionSaveGame>(UGameplayStatics::LoadGameFromSlot(Slot.Name, 0));
	if (!TestNotNull(TEXT("Primary is a standard UE save"), Corrupted)) { return false; }
	++Corrupted->IceBricks; // Container still parses, but checksum is inconsistent.
	TestTrue(TEXT("Only isolated fixture is damaged"), UGameplayStatics::SaveGameToSlot(Corrupted, Slot.Name, 0));
	bool bWritable = false; FText Error;
	URogueProgressionSaveGame* Recovered = URogueProgressionSubsystem::LoadProfileFromSlot(Slot.Name, 0, NewObject<URogueUpgradeTreeDefinition>(), bWritable, Error);
	TestEqual(TEXT("Previous valid balance recovered"), Recovered->IceBricks, 7);
	TestTrue(TEXT("Recovered profile can commit again"), bWritable);
	TestFalse(TEXT("Recovery is explained to UI"), Error.IsEmpty());
	TestTrue(TEXT("Recovery can replace corrupt primary without destroying good backup"), URogueProgressionSubsystem::SaveProfileToSlot(Recovered, Slot.Name, 0));
	Recovered = URogueProgressionSubsystem::LoadProfileFromSlot(Slot.Name, 0, nullptr, bWritable, Error);
	TestEqual(TEXT("Recovered commit survives reload"), Recovered->IceBricks, 7);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueRecoveryFutureTest, "Rogue.Reliability.Save.FuturePrimaryNeverDowngradesToBackup", RogueReliabilityTests::Flags)
bool FRogueRecoveryFutureTest::RunTest(const FString& Parameters)
{
	RogueReliabilityTests::FSlot Slot;
	URogueProgressionSaveGame* Profile = NewObject<URogueProgressionSaveGame>();
	Profile->IceBricks = 8;
	TestTrue(TEXT("First commits"), URogueProgressionSubsystem::SaveProfileToSlot(Profile, Slot.Name, 0));
	Profile->IceBricks = 9;
	TestTrue(TEXT("Backup exists"), URogueProgressionSubsystem::SaveProfileToSlot(Profile, Slot.Name, 0));
	Profile->Version += 10;
	UGameplayStatics::SaveGameToSlot(Profile, Slot.Name, 0);
	TArray<uint8> Before, After;
	UGameplayStatics::LoadDataFromSlot(Before, Slot.Name, 0);
	bool bWritable = true; FText Error;
	URogueProgressionSubsystem::LoadProfileFromSlot(Slot.Name, 0, nullptr, bWritable, Error);
	TestFalse(TEXT("Future version blocks writes despite valid old backup"), bWritable);
	Profile->Version = URogueProgressionSaveGame::CurrentVersion;
	TestFalse(TEXT("Direct commit also respects future primary"), URogueProgressionSubsystem::SaveProfileToSlot(Profile, Slot.Name, 0));
	UGameplayStatics::LoadDataFromSlot(After, Slot.Name, 0);
	TestTrue(TEXT("Future bytes unchanged"), Before == After);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRoguePendingRewardTravelTest, "Rogue.Reliability.Reward.PendingSurvivesPlayerStateWorldAndRetriesOnce", RogueReliabilityTests::Flags)
bool FRoguePendingRewardTravelTest::RunTest(const FString& Parameters)
{
	RogueReliabilityTests::FSlot Slot;
	ULocalPlayer* Player = NewObject<ULocalPlayer>(GEngine);
	URogueProgressionSubsystem* Meta = NewObject<URogueProgressionSubsystem>(Player);
	Meta->Profile = NewObject<URogueProgressionSaveGame>(Meta);
	Meta->bSaveWritable = true;
	Meta->SaveSlot.Reset(); // Deterministic storage rejection, no access to real player save.
	const FGuid Reward = FGuid::NewGuid();
	Meta->EnqueueBossReward(Reward);
	TestEqual(TEXT("Uncommitted balance stays unchanged"), Meta->GetIceBricks(), 0);
	TestEqual(TEXT("Reward remains pending"), Meta->GetPendingBossRewardCount(), 1);
	{
		FTestWorldWrapper OldWorld;
		OldWorld.CreateTestWorld(EWorldType::Game);
		ARoguePlayerState* OldState = OldWorld.GetTestWorld()->SpawnActor<ARoguePlayerState>();
		OldState->Destroy();
	} // World and PS end; LocalPlayer subsystem still owns pending identity.
	TestEqual(TEXT("World destruction does not own the queue"), Meta->GetPendingBossRewardCount(), 1);
	Meta->SaveSlot = Slot.Name;
	Meta->FlushPendingBossRewards();
	TestEqual(TEXT("Recovery adds one brick"), Meta->GetIceBricks(), 1);
	TestEqual(TEXT("Committed reward removed"), Meta->GetPendingBossRewardCount(), 0);
	Meta->EnqueueBossReward(Reward);
	TestEqual(TEXT("Repeated receipt remains idempotent"), Meta->GetIceBricks(), 1);
	Meta->Deinitialize();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueWorldClockTest, "Rogue.Reliability.Players.PawnRepairClocksArePerWorld", RogueReliabilityTests::Flags)
bool FRogueWorldClockTest::RunTest(const FString& Parameters)
{
	FTestWorldWrapper First, Second;
	if (!First.CreateTestWorld(EWorldType::Game) || !Second.CreateTestWorld(EWorldType::Game)) { return false; }
	ARogueGameMode* Old = First.GetTestWorld()->SpawnActor<ARogueGameMode>();
	ARogueGameMode* Fresh = Second.GetTestWorld()->SpawnActor<ARogueGameMode>();
	FRogueReliabilityTestAccess::Clock(Old) = 600.0;
	FRogueReliabilityTestAccess::Repair(Fresh);
	TestEqual(TEXT("Long prior world does not defer fresh world repair"), FRogueReliabilityTestAccess::Clock(Fresh), double(Second.GetTestWorld()->GetTimeSeconds()));
	TestEqual(TEXT("Fresh repair leaves other world untouched"), FRogueReliabilityTestAccess::Clock(Old), 600.0);
	FRogueReliabilityTestAccess::Repair(Old);
	TestEqual(TEXT("A reset world clock also recovers"), FRogueReliabilityTestAccess::Clock(Old), double(First.GetTestWorld()->GetTimeSeconds()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueHandshakeFailureTest, "Rogue.Reliability.Players.HandshakeFailureIsTerminalWithoutEmptyProfile", RogueReliabilityTests::Flags)
bool FRogueHandshakeFailureTest::RunTest(const FString& Parameters)
{
	FTestWorldWrapper Fixture;
	if (!Fixture.CreateTestWorld(EWorldType::Game)) { return false; }
	ARoguePlayerState* Pending = Fixture.GetTestWorld()->SpawnActor<ARoguePlayerState>();
	FRogueReliabilityTestAccess::MakeWaiting(Pending);
	FRogueReliabilityTestAccess::Fail(Pending);
	TestTrue(TEXT("Explicit timeout state"), Pending->HasProgressionFailed());
	TestTrue(TEXT("Failed participant no longer blocks team wipe"), Pending->IsOnlyASpectator());
	TestFalse(TEXT("No fabricated empty ready profile"), Pending->IsProgressionReady());
	FRogueProgressionSnapshot Legitimate; Legitimate.NodeRanks.Add(TEXT("Stat.AttackPower"), 1);
	TestFalse(TEXT("Late RPC cannot reactivate disconnected run"), Pending->SetProgressionSnapshotForRun(Legitimate));
	ARoguePlayerState* Expired = Fixture.GetTestWorld()->SpawnActor<ARoguePlayerState>();
	FRogueReliabilityTestAccess::Expire(Expired);
	TestFalse(TEXT("Late submission checks real deadline before timer dispatch"), Expired->SetProgressionSnapshotForRun(Legitimate));
	TestTrue(TEXT("Expired RPC enters explicit failed state"), Expired->HasProgressionFailed());
	ARoguePlayerState* Ready = Fixture.GetTestWorld()->SpawnActor<ARoguePlayerState>();
	TestTrue(TEXT("Legitimate profile accepted"), Ready->SetProgressionSnapshotForRun(Legitimate));
	FRogueReliabilityTestAccess::Fail(Ready);
	TestFalse(TEXT("Ready player cannot time out afterward"), Ready->HasProgressionFailed());
	TestEqual(TEXT("Legitimate progress stays intact"), Ready->GetProgressionSnapshot().NodeRanks.FindRef(TEXT("Stat.AttackPower")), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueHUDRefreshScheduleTest, "Rogue.Reliability.UI.CanvasDoesNotRepeatTickRefresh", RogueReliabilityTests::Flags)
bool FRogueHUDRefreshScheduleTest::RunTest(const FString& Parameters)
{
	FTestWorldWrapper Fixture;
	if (!Fixture.CreateTestWorld(EWorldType::Game)) { return false; }
	ARogueHUD* HUD = Fixture.GetTestWorld()->SpawnActor<ARogueHUD>();
	const uint64 Before = FRogueReliabilityTestAccess::Refreshes(HUD);
	HUD->Tick(0.0f);
	HUD->DrawHUD(); HUD->DrawHUD();
	TestEqual(TEXT("One Tick plus canvas passes refreshes once"), FRogueReliabilityTestAccess::Refreshes(HUD) - Before, uint64(1));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueClassPoolTest, "Rogue.Reliability.Pool.ExactClassIndexReusesAndSkipsOtherClasses", RogueReliabilityTests::Flags)
bool FRogueClassPoolTest::RunTest(const FString& Parameters)
{
	FTestWorldWrapper Fixture;
	if (!Fixture.CreateTestWorld(EWorldType::Game)) { return false; }
	UWorld* World = Fixture.GetTestWorld();
	UClass* Blueprint = LoadClass<ARogueEnemy>(nullptr, TEXT("/Game/BP/Enemy/BP_EnemySniper.BP_EnemySniper_C"));
	if (!TestNotNull(TEXT("Existing enemy BP class"), Blueprint)) { return false; }
	FRogueCombatPools Pools;
	ARogueEnemy* Native = Pools.AcquireEnemy(World, ARogueEnemy::StaticClass(), nullptr, FVector::ZeroVector, FRotator::ZeroRotator);
	ARogueEnemy* BP = Pools.AcquireEnemy(World, Blueprint, nullptr, FVector::ZeroVector, FRotator::ZeroRotator);
	Native->DeactivateToPool(); BP->DeactivateToPool();
	for (int32 Index = 0; Index < 128; ++Index)
	{
		ARogueEnemy* Acquired = Pools.AcquireEnemy(World, Blueprint, nullptr, FVector(Index, 0, 0), FRotator::ZeroRotator);
		TestTrue(TEXT("Exact requested BP is reused"), Acquired == BP);
	}
	const FRoguePoolClassRuntime& Stats = FRogueReliabilityTestAccess::Stats(Pools, Blueprint);
	TestEqual(TEXT("Only the requested bucket is scanned"), Stats.Scanned, uint64(128));
	TestEqual(TEXT("All warm acquisitions hit"), Stats.Hits, uint64(128));
	TestEqual(TEXT("Only initial allocation misses"), Stats.Misses, uint64(1));
	BP->Destroy();
	ARogueEnemy* Replacement = Pools.AcquireEnemy(World, Blueprint, nullptr, FVector::ZeroVector, FRotator::ZeroRotator);
	TestTrue(TEXT("Destroyed entry is replaced"), IsValid(Replacement) && Replacement != BP);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueAtomicTemporaryFailureTest, "Rogue.Reliability.Save.TemporaryWriteFailurePreservesPrimaryAndBackup", RogueReliabilityTests::Flags)
bool FRogueAtomicTemporaryFailureTest::RunTest(const FString& Parameters)
{
	RogueReliabilityTests::FSlot Slot;
	URogueProgressionSaveGame* Profile = NewObject<URogueProgressionSaveGame>();
	Profile->IceBricks = 4;
	if (!TestTrue(TEXT("First transaction"), URogueProgressionSubsystem::SaveProfileToSlot(Profile, Slot.Name, 0))) { return false; }
	Profile->IceBricks = 8;
	if (!TestTrue(TEXT("Second transaction"), URogueProgressionSubsystem::SaveProfileToSlot(Profile, Slot.Name, 0))) { return false; }
	TArray<uint8> Before, After;
	UGameplayStatics::LoadDataFromSlot(Before, Slot.Name, 0);
	const FString Temporary = FPaths::ProjectSavedDir() / TEXT("SaveGames") / (Slot.Name + TEXT(".sav.tmp"));
	// An empty isolated directory at the temp-file path reliably rejects file creation.
	IFileManager::Get().MakeDirectory(*Temporary);
	Profile->IceBricks = 80;
	TestFalse(TEXT("Failed temporary file does not commit"), URogueProgressionSubsystem::SaveProfileToSlot(Profile, Slot.Name, 0));
	IFileManager::Get().DeleteDirectory(*Temporary);
	UGameplayStatics::LoadDataFromSlot(After, Slot.Name, 0);
	TestTrue(TEXT("Primary bytes are preserved exactly"), Before == After);
	bool bWritable; FText Error;
	URogueProgressionSaveGame* Reloaded = URogueProgressionSubsystem::LoadProfileFromSlot(Slot.Name, 0, nullptr, bWritable, Error);
	TestEqual(TEXT("Old committed balance remains"), Reloaded->IceBricks, 8);
	TestTrue(TEXT("Storage can recover after obstruction clears"), URogueProgressionSubsystem::SaveProfileToSlot(Profile, Slot.Name, 0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueSaveMeasurementTest, "Rogue.Reliability.Save.MeasureReceiptGrowthWithoutPruningIdentity", RogueReliabilityTests::Flags)
bool FRogueSaveMeasurementTest::RunTest(const FString& Parameters)
{
	RogueReliabilityTests::FSlot Slot;
	URogueProgressionSaveGame* Profile = NewObject<URogueProgressionSaveGame>();
	for (int32 Count : { 0, 1000, 10000 })
	{
		while (Profile->AppliedBossRewards.Num() < Count) { Profile->AppliedBossRewards.Add(FGuid::NewGuid()); }
		const double Start = FPlatformTime::Seconds();
		TestTrue(TEXT("Measured isolated transaction commits"), URogueProgressionSubsystem::SaveProfileToSlot(Profile, Slot.Name, 0));
		const double Milliseconds = (FPlatformTime::Seconds() - Start) * 1000.0;
		TArray<uint8> Bytes; UGameplayStatics::LoadDataFromSlot(Bytes, Slot.Name, 0);
		AddInfo(FString::Printf(TEXT("StorageSample receipts=%d bytes=%d commit_ms=%.3f"), Count, Bytes.Num(), Milliseconds));
		bool bWritable; FText Error;
		URogueProgressionSaveGame* Reloaded = URogueProgressionSubsystem::LoadProfileFromSlot(Slot.Name, 0, nullptr, bWritable, Error);
		TestEqual(TEXT("No old reward identity is dropped"), Reloaded->AppliedBossRewards.Num(), Count);
	}
	return true;
}
#endif
