#if WITH_DEV_AUTOMATION_TESTS

#include "Progression/Meta/RogueProgressionSaveGame.h"
#include "Progression/Meta/RogueProgressionSubsystem.h"
#include "Progression/Meta/RogueUpgradeTreeDefinition.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"

namespace RogueMetaProgressionTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;

	struct FIsolatedSlot
	{
		FString Name = TEXT("Automation_UpgradeTree_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
		~FIsolatedSlot() { UGameplayStatics::DeleteGameInSlot(Name, 0); UGameplayStatics::DeleteGameInSlot(Name + TEXT("_backup"), 0); }
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueMetaFreshProfileTest, "Rogue.MetaProgression.FreshProfileAndIndependentPlayers", RogueMetaProgressionTests::Flags)
bool FRogueMetaFreshProfileTest::RunTest(const FString& Parameters)
{
	// Arrange: two independent local profile objects, with no disk access.
	const URogueUpgradeTreeDefinition* Definition = NewObject<URogueUpgradeTreeDefinition>();
	URogueProgressionSaveGame* First = NewObject<URogueProgressionSaveGame>();
	URogueProgressionSaveGame* Second = NewObject<URogueProgressionSaveGame>();
	const FGuid Reward = FGuid::NewGuid();

	// Act: the same boss reward is delivered to both participants and duplicated for one.
	TestTrue(TEXT("First participant receives its reward"), First->AddBossReward(Reward));
	TestTrue(TEXT("Second participant receives its own reward"), Second->AddBossReward(Reward));
	TestTrue(TEXT("A repeated delivery is acknowledged"), First->AddBossReward(Reward));

	// Assert: each has exactly one brick, and only the talisman plus general cards are available.
	TestEqual(TEXT("First reward is idempotent"), First->IceBricks, 1);
	TestEqual(TEXT("Players do not share a balance"), Second->IceBricks, 1);
	TestTrue(TEXT("Talisman is free"), Definition->IsWeaponUnlocked(First->Progression, ERogueUpgradeType::ProjectileCount));
	TestTrue(TEXT("Talisman quantity card is free"), Definition->IsUpgradeAvailable(First->Progression, ERogueUpgradeType::ProjectileCount, true));
	TestTrue(TEXT("Generic health card remains available"), Definition->IsUpgradeAvailable(First->Progression, ERogueUpgradeType::MaxHealth, true));
	TestFalse(TEXT("Laser acquisition is locked"), Definition->IsUpgradeAvailable(First->Progression, ERogueUpgradeType::LaserCount, false));
	TestFalse(TEXT("Armor starts locked"), Definition->IsArmorUnlocked(First->Progression));
	TestFalse(TEXT("Armor recharge card is gated"), Definition->IsUpgradeAvailable(First->Progression, ERogueUpgradeType::Armor, true));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueMetaWeaponPurchaseTest, "Rogue.MetaProgression.WeaponAndIndividualCardPurchases", RogueMetaProgressionTests::Flags)
bool FRogueMetaWeaponPurchaseTest::RunTest(const FString& Parameters)
{
	// Arrange.
	const URogueUpgradeTreeDefinition* Definition = NewObject<URogueUpgradeTreeDefinition>();
	URogueProgressionSaveGame* Profile = NewObject<URogueProgressionSaveGame>();
	Profile->IceBricks = 20;
	FText Failure;

	// Act / assert: each dedicated card is a separate purchase behind its weapon.
	TestFalse(TEXT("A dedicated card requires the weapon"), Profile->TryPurchase(Definition, TEXT("Card.ScytheSize"), Failure));
	TestEqual(TEXT("Failed prerequisite does not spend bricks"), Profile->IceBricks, 20);
	TestTrue(TEXT("Weapon costs five"), Profile->TryPurchase(Definition, TEXT("Weapon.Scythe"), Failure));
	TestEqual(TEXT("Five bricks spent"), Profile->IceBricks, 15);
	TestTrue(TEXT("Initial acquisition card is now available"), Definition->IsUpgradeAvailable(Profile->Progression, ERogueUpgradeType::ScytheCount, false));
	TestFalse(TEXT("Repeated quantity card remains locked"), Definition->IsUpgradeAvailable(Profile->Progression, ERogueUpgradeType::ScytheCount, true));
	TestTrue(TEXT("Quantity card costs another five"), Profile->TryPurchase(Definition, TEXT("Card.ScytheCount"), Failure));
	TestTrue(TEXT("Size card costs another five"), Profile->TryPurchase(Definition, TEXT("Card.ScytheSize"), Failure));
	TestFalse(TEXT("Buying size does not also buy lifesteal"), Definition->IsUpgradeAvailable(Profile->Progression, ERogueUpgradeType::ScytheLifesteal, true));
	TestTrue(TEXT("Lifesteal has its own five brick purchase"), Profile->TryPurchase(Definition, TEXT("Card.ScytheLifesteal"), Failure));
	TestEqual(TEXT("All four separate purchases consumed twenty"), Profile->IceBricks, 0);
	TestFalse(TEXT("An already unlocked node cannot charge again"), Profile->TryPurchase(Definition, TEXT("Weapon.Scythe"), Failure));
	TestFalse(TEXT("Insufficient balance cannot buy another weapon"), Profile->TryPurchase(Definition, TEXT("Weapon.Laser"), Failure));
	TestEqual(TEXT("Rejected purchases preserve balance"), Profile->IceBricks, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueMetaRankAndArmorTest, "Rogue.MetaProgression.FiveRankLimitAndArmorPrerequisite", RogueMetaProgressionTests::Flags)
bool FRogueMetaRankAndArmorTest::RunTest(const FString& Parameters)
{
	// Arrange.
	const URogueUpgradeTreeDefinition* Definition = NewObject<URogueUpgradeTreeDefinition>();
	URogueProgressionSaveGame* Profile = NewObject<URogueProgressionSaveGame>();
	Profile->IceBricks = 30;
	FText Failure;

	// Act: a base stat can be upgraded exactly five times at three bricks each.
	for (int32 Rank = 1; Rank <= 5; ++Rank)
	{
		TestTrue(FString::Printf(TEXT("Attack rank %d purchase succeeds"), Rank), Profile->TryPurchase(Definition, TEXT("Stat.AttackPower"), Failure));
	}
	TestFalse(TEXT("Sixth purchase is rejected"), Profile->TryPurchase(Definition, TEXT("Stat.AttackPower"), Failure));
	TestEqual(TEXT("Five ranks cost fifteen bricks"), Profile->IceBricks, 15);
	TestEqual(TEXT("Attack bonus is fifteen"), Definition->GetStatBonus(Profile->Progression, ERogueMetaStat::AttackPower), 15.0f);
	TestFalse(TEXT("Armor ranks require unlocking armor first"), Profile->TryPurchase(Definition, TEXT("Stat.ArmorCapacity"), Failure));
	TestTrue(TEXT("Armor unlock succeeds for three"), Profile->TryPurchase(Definition, TEXT("Armor.Unlock"), Failure));
	TestTrue(TEXT("Armor rank now succeeds for three"), Profile->TryPurchase(Definition, TEXT("Stat.ArmorCapacity"), Failure));

	// Assert: the armor gate and its separately purchased bonus both take effect.
	TestEqual(TEXT("Armor unlock and rank cost six"), Profile->IceBricks, 9);
	TestTrue(TEXT("Armor gate opens"), Definition->IsArmorUnlocked(Profile->Progression));
	TestEqual(TEXT("One rank adds ten armor to the balance asset base value"), Definition->GetStatBonus(Profile->Progression, ERogueMetaStat::ArmorCapacity), 10.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueMetaSanitizeTest, "Rogue.MetaProgression.SanitizeUnknownOrphanAndOvercapRanks", RogueMetaProgressionTests::Flags)
bool FRogueMetaSanitizeTest::RunTest(const FString& Parameters)
{
	// Arrange: mimic malformed or old profile data and an untrusted network snapshot.
	URogueUpgradeTreeDefinition* Definition = NewObject<URogueUpgradeTreeDefinition>();
	FRogueProgressionSnapshot Dirty;
	Dirty.NodeRanks.Add(TEXT("Unknown.Node"), 100);
	Dirty.NodeRanks.Add(TEXT("Card.LaserRefraction"), 1);
	Dirty.NodeRanks.Add(TEXT("Stat.AttackPower"), 999);
	Dirty.NodeRanks.Add(TEXT("Stat.MaxHealth"), -4);
	Dirty.NodeRanks.Add(TEXT("Stat.ArmorCapacity"), 5);
	FRogueUpgradeTreeNode CycleA;
	CycleA.NodeId = TEXT("Cycle.A"); CycleA.Prerequisites.Add(TEXT("Cycle.B"));
	FRogueUpgradeTreeNode CycleB;
	CycleB.NodeId = TEXT("Cycle.B"); CycleB.Prerequisites.Add(TEXT("Cycle.A"));
	Definition->Nodes.Add(CycleA); Definition->Nodes.Add(CycleB);
	Dirty.NodeRanks.Add(CycleA.NodeId, 1); Dirty.NodeRanks.Add(CycleB.NodeId, 1);

	// Act.
	const FRogueProgressionSnapshot Clean = Definition->SanitizeSnapshot(Dirty);

	// Assert: only reachable, known positive ranks survive.
	TestEqual(TEXT("Only one legal paid node remains"), Clean.NodeRanks.Num(), 1);
	TestEqual(TEXT("Rank is capped at five"), Definition->GetNodeRank(Clean, TEXT("Stat.AttackPower")), 5);
	TestFalse(TEXT("Orphan dedicated card removed"), Definition->IsUpgradeAvailable(Clean, ERogueUpgradeType::LaserRefraction, true));
	TestEqual(TEXT("Armor bonus cannot bypass its unlock"), Definition->GetStatBonus(Clean, ERogueMetaStat::ArmorCapacity), 0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueMetaSaveRoundTripTest, "Rogue.MetaProgression.IsolatedSaveRoundTripAndRewardIdempotence", RogueMetaProgressionTests::Flags)
bool FRogueMetaSaveRoundTripTest::RunTest(const FString& Parameters)
{
	// Arrange: a unique automation-only slot, deleted even when an assertion fails.
	RogueMetaProgressionTests::FIsolatedSlot Slot;
	const URogueUpgradeTreeDefinition* Definition = NewObject<URogueUpgradeTreeDefinition>();
	URogueProgressionSaveGame* Profile = NewObject<URogueProgressionSaveGame>();
	Profile->IceBricks = 8;
	FText Failure;
	Profile->TryPurchase(Definition, TEXT("Weapon.Laser"), Failure);
	const FGuid Reward = FGuid::NewGuid();
	Profile->AddBossReward(Reward);

	// Act: persist, reload, then deliver the same boss reward again.
	if (!TestTrue(TEXT("Isolated profile saves"), URogueProgressionSubsystem::SaveProfileToSlot(Profile, Slot.Name, 0))) { return false; }
	bool bWritable = false;
	URogueProgressionSaveGame* Loaded = URogueProgressionSubsystem::LoadProfileFromSlot(Slot.Name, 0, Definition, bWritable, Failure);
	if (!TestNotNull(TEXT("Profile reloads"), Loaded)) { return false; }
	TestTrue(TEXT("Reloaded profile accepts writes"), bWritable);
	TestTrue(TEXT("Repeated reward is acknowledged after restart"), Loaded->AddBossReward(Reward));

	// Assert.
	TestEqual(TEXT("Balance survives and duplicate does not add another brick"), Loaded->IceBricks, 4);
	TestTrue(TEXT("Weapon unlock survives restart"), Definition->IsWeaponUnlocked(Loaded->Progression, ERogueUpgradeType::LaserCount));
	TestEqual(TEXT("Reward receipt survives restart"), Loaded->AppliedBossRewards.Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueMetaPreserveFutureSaveTest, "Rogue.MetaProgression.PreserveUnreadableOrFutureSave", RogueMetaProgressionTests::Flags)
bool FRogueMetaPreserveFutureSaveTest::RunTest(const FString& Parameters)
{
	// Arrange: a valid save container from a newer schema, in an isolated slot.
	RogueMetaProgressionTests::FIsolatedSlot Slot;
	const URogueUpgradeTreeDefinition* Definition = NewObject<URogueUpgradeTreeDefinition>();
	URogueProgressionSaveGame* Future = NewObject<URogueProgressionSaveGame>();
	Future->Version = URogueProgressionSaveGame::CurrentVersion + 10;
	Future->IceBricks = 123;
	if (!TestTrue(TEXT("Future fixture is written"), UGameplayStatics::SaveGameToSlot(Future, Slot.Name, 0))) { return false; }
	TArray<uint8> Before;
	UGameplayStatics::LoadDataFromSlot(Before, Slot.Name, 0);
	bool bWritable = true;
	FText Failure;

	// Act.
	URogueProgressionSaveGame* Safe = URogueProgressionSubsystem::LoadProfileFromSlot(Slot.Name, 0, Definition, bWritable, Failure);
	TArray<uint8> After;
	UGameplayStatics::LoadDataFromSlot(After, Slot.Name, 0);

	// Assert: fallback gameplay state is read-only, and the original bytes remain intact.
	TestNotNull(TEXT("A safe profile is available for presentation"), Safe);
	TestFalse(TEXT("Unreadable schema cannot be overwritten by the subsystem"), bWritable);
	TestFalse(TEXT("UI can explain why writes are blocked"), Failure.IsEmpty());
	TestTrue(TEXT("Future save bytes are preserved"), Before == After);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueMetaNetSerializationTest, "Rogue.MetaProgression.BoundedNetworkSnapshot", RogueMetaProgressionTests::Flags)
bool FRogueMetaNetSerializationTest::RunTest(const FString& Parameters)
{
	// Arrange: ordinary paid nodes and a prepopulated destination.
	FRogueProgressionSnapshot Source;
	Source.NodeRanks.Add(TEXT("Stat.AttackPower"), 5);
	Source.NodeRanks.Add(TEXT("Weapon.Scythe"), 1);
	TArray<uint8> Bytes;
	FMemoryWriter Writer(Bytes);
	bool bSuccess = false;
	TestTrue(TEXT("Snapshot serializes"), Source.NetSerialize(Writer, nullptr, bSuccess));
	TestTrue(TEXT("Writer reports success"), bSuccess);
	FRogueProgressionSnapshot Destination;
	Destination.NodeRanks.Add(TEXT("Stale.Node"), 1);
	FMemoryReader Reader(Bytes);

	// Act.
	TestTrue(TEXT("Snapshot deserializes"), Destination.NetSerialize(Reader, nullptr, bSuccess));

	// Assert: stale map data is cleared, values survive, and oversized writes are rejected.
	TestTrue(TEXT("Reader reports success"), bSuccess);
	TestEqual(TEXT("Exactly two entries restored"), Destination.NodeRanks.Num(), 2);
	TestEqual(TEXT("Five ranks restored"), Destination.NodeRanks.FindRef(TEXT("Stat.AttackPower")), 5);
	FRogueProgressionSnapshot Oversized;
	for (int32 Index = 0; Index < 129; ++Index) { Oversized.NodeRanks.Add(FName(*FString::Printf(TEXT("Node.%d"), Index)), 1); }
	TArray<uint8> OverflowBytes;
	FMemoryWriter OverflowWriter(OverflowBytes);
	TestFalse(TEXT("More than 128 entries are refused before serialization"), Oversized.NetSerialize(OverflowWriter, nullptr, bSuccess));
	TestFalse(TEXT("Oversized writer reports failure"), bSuccess);
	TestEqual(TEXT("Oversized data does not allocate output"), OverflowBytes.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueMetaSaveFailureRollbackTest, "Rogue.MetaProgression.FailedSaveRollsBackPurchaseAndReward", RogueMetaProgressionTests::Flags)
bool FRogueMetaSaveFailureRollbackTest::RunTest(const FString& Parameters)
{
	// Arrange: create an uninitialized isolated subsystem. An empty explicit slot deterministically
	// rejects every save before calling the platform, so this cannot touch the user's real profile.
	if (!TestNotNull(TEXT("Engine exists for the LocalPlayer outer"), GEngine)) { return false; }
	ULocalPlayer* LocalPlayer = NewObject<ULocalPlayer>(GEngine);
	URogueProgressionSubsystem* Subsystem = NewObject<URogueProgressionSubsystem>(LocalPlayer);
	Subsystem->Profile = NewObject<URogueProgressionSaveGame>(Subsystem);
	Subsystem->Profile->IceBricks = 100;
	Subsystem->bSaveWritable = true;
	Subsystem->SaveSlot.Reset();
	URogueProgressionSaveGame* Original = Subsystem->Profile;
	FText Failure;
	const FGuid RewardId = FGuid::NewGuid();

	// Act: both transactions can change their candidate but cannot commit to storage.
	TestFalse(TEXT("Purchase fails when storage rejects the save"), Subsystem->TryPurchaseNode(TEXT("Weapon.Scythe"), Failure));
	TestFalse(TEXT("Boss reward fails when storage rejects the save"), Subsystem->ApplyBossReward(RewardId));
#if WITH_EDITOR
	TestFalse(TEXT("Live debug balance fails atomically when storage rejects the save"), Subsystem->SetIceBricksForDebug(250, Failure));
	Original->Progression.NodeRanks.Add(TEXT("Stat.AttackPower"), 2);
	TestFalse(TEXT("Progress reset fails atomically when storage rejects the save"), Subsystem->ResetProgressionForDebug(Failure));
	TestEqual(TEXT("Failed reset preserves purchased rank"), Subsystem->Profile->Progression.NodeRanks.FindRef(TEXT("Stat.AttackPower")), 2);
#endif

	// Assert: the exact prior profile remains authoritative, including reward receipt bookkeeping.
	TestTrue(TEXT("The prior object is still installed"), Subsystem->Profile == Original);
	TestEqual(TEXT("Balance is unchanged"), Subsystem->GetIceBricks(), 100);
	TestFalse(TEXT("Failed purchase was not installed"), Subsystem->Profile->Progression.NodeRanks.Contains(TEXT("Weapon.Scythe")));
	TestFalse(TEXT("Failed reward is not marked as consumed"), Subsystem->Profile->AppliedBossRewards.Contains(RewardId));
	TestFalse(TEXT("The caller receives a useful save error"), Failure.IsEmpty());
	return true;
}

#endif
