#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Progression/Meta/RogueProgressionSubsystem.h"
#include "Progression/Meta/RogueProgressionSaveGame.h"
#include "Progression/Meta/RogueUpgradeTreeDefinition.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace RogueBrickDebugTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;
	struct FSlot
	{
		FString Name = TEXT("Automation_BrickDebug_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
		FString Path() const { return FPaths::ProjectSavedDir() / TEXT("SaveGames") / (Name + TEXT(".sav")); }
		~FSlot() { UGameplayStatics::DeleteGameInSlot(Name, 0); UGameplayStatics::DeleteGameInSlot(Name + TEXT("_backup"), 0); }
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueDebugBricksPreserveProgressTest, "Rogue.MetaProgression.DebugBricks.PreserveUnlocksAndBossReceipts", RogueBrickDebugTests::Flags)
bool FRogueDebugBricksPreserveProgressTest::RunTest(const FString& Parameters)
{
	RogueBrickDebugTests::FSlot Slot;
	const URogueUpgradeTreeDefinition* Tree = NewObject<URogueUpgradeTreeDefinition>();
	URogueProgressionSaveGame* Save = NewObject<URogueProgressionSaveGame>();
	Save->IceBricks = 7;
	Save->Progression.NodeRanks.Add(TEXT("Weapon.Scythe"), 1);
	Save->Progression.NodeRanks.Add(TEXT("Stat.AttackPower"), 3);
	const FGuid Receipt = FGuid::NewGuid();
	Save->AppliedBossRewards.Add(Receipt);
	if (!TestTrue(TEXT("Write an isolated starting profile"), UGameplayStatics::SaveGameToSlot(Save, Slot.Name, 0))) return false;
	FText Error;
	TestTrue(TEXT("Set an exact debug balance rather than adding currency"), URogueProgressionSubsystem::SetIceBricksInSlotForDebug(Slot.Name, 0, Tree, 42, Error));
	bool bWritable = false;
	URogueProgressionSaveGame* Loaded = URogueProgressionSubsystem::LoadProfileFromSlot(Slot.Name, 0, Tree, bWritable, Error);
	TestEqual(TEXT("The requested balance survives a reload"), Loaded->IceBricks, 42);
	TestEqual(TEXT("Weapon unlock survives"), Loaded->Progression.NodeRanks.FindRef(TEXT("Weapon.Scythe")), 1);
	TestEqual(TEXT("Permanent stat rank survives"), Loaded->Progression.NodeRanks.FindRef(TEXT("Stat.AttackPower")), 3);
	TestTrue(TEXT("Boss receipt survives without becoming claimable twice"), Loaded->AppliedBossRewards.Contains(Receipt));
	TestTrue(TEXT("Zero is a valid debug balance"), URogueProgressionSubsystem::SetIceBricksInSlotForDebug(Slot.Name, 0, Tree, 0, Error));
	Loaded = URogueProgressionSubsystem::LoadProfileFromSlot(Slot.Name, 0, Tree, bWritable, Error);
	TestEqual(TEXT("Resetting the currency to zero is persistent"), Loaded->IceBricks, 0);
	TestTrue(TEXT("Resetting currency did not reset receipts"), Loaded->AppliedBossRewards.Contains(Receipt));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueDebugBricksNegativeTest, "Rogue.MetaProgression.DebugBricks.NegativeInputDoesNotWrite", RogueBrickDebugTests::Flags)
bool FRogueDebugBricksNegativeTest::RunTest(const FString& Parameters)
{
	RogueBrickDebugTests::FSlot Slot;
	URogueProgressionSaveGame* Save = NewObject<URogueProgressionSaveGame>();
	Save->IceBricks = 7;
	if (!TestTrue(TEXT("Write an isolated starting profile"), UGameplayStatics::SaveGameToSlot(Save, Slot.Name, 0))) return false;
	TArray<uint8> Before, After;
	if (!TestTrue(TEXT("Read initial bytes"), FFileHelper::LoadFileToArray(Before, *Slot.Path()))) return false;
	FText Error;
	TestFalse(TEXT("Negative debug balance is rejected"), URogueProgressionSubsystem::SetIceBricksInSlotForDebug(Slot.Name, 0, NewObject<URogueUpgradeTreeDefinition>(), -1, Error));
	TestFalse(TEXT("Rejection explains the invalid request"), Error.IsEmpty());
	TestTrue(TEXT("Read remaining bytes"), FFileHelper::LoadFileToArray(After, *Slot.Path()));
	TestTrue(TEXT("Invalid request preserved the exact save"), Before == After);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueDebugBricksFutureSaveTest, "Rogue.MetaProgression.DebugBricks.FutureSaveIsNotOverwritten", RogueBrickDebugTests::Flags)
bool FRogueDebugBricksFutureSaveTest::RunTest(const FString& Parameters)
{
	RogueBrickDebugTests::FSlot Slot;
	URogueProgressionSaveGame* Save = NewObject<URogueProgressionSaveGame>();
	Save->Version = URogueProgressionSaveGame::CurrentVersion + 1;
	Save->IceBricks = 7;
	if (!TestTrue(TEXT("Write an isolated future-version profile"), UGameplayStatics::SaveGameToSlot(Save, Slot.Name, 0))) return false;
	TArray<uint8> Before, After;
	if (!TestTrue(TEXT("Read initial bytes"), FFileHelper::LoadFileToArray(Before, *Slot.Path()))) return false;
	FText Error;
	TestFalse(TEXT("Debug tools respect the existing read-only save protection"), URogueProgressionSubsystem::SetIceBricksInSlotForDebug(Slot.Name, 0, NewObject<URogueUpgradeTreeDefinition>(), 100, Error));
	TestFalse(TEXT("Read-only failure explains the problem"), Error.IsEmpty());
	TestFalse(TEXT("Progress reset also preserves future-version saves"), URogueProgressionSubsystem::ResetProgressionInSlotForDebug(Slot.Name, 0, NewObject<URogueUpgradeTreeDefinition>(), Error));
	TestTrue(TEXT("Read remaining bytes"), FFileHelper::LoadFileToArray(After, *Slot.Path()));
	TestTrue(TEXT("Future save bytes remain intact"), Before == After);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueDebugResetProgressTest, "Rogue.MetaProgression.DebugReset.RestoreInitialUnlocksAndPreserveCurrency", RogueBrickDebugTests::Flags)
bool FRogueDebugResetProgressTest::RunTest(const FString& Parameters)
{
	RogueBrickDebugTests::FSlot Slot;
	const URogueUpgradeTreeDefinition* Tree = NewObject<URogueUpgradeTreeDefinition>();
	URogueProgressionSaveGame* Save = NewObject<URogueProgressionSaveGame>();
	Save->IceBricks = 37;
	for (const FRogueUpgradeTreeNode& Node : Tree->Nodes)
	{
		if (!Node.bInitiallyUnlocked) Save->Progression.NodeRanks.Add(Node.NodeId, Tree->GetMaximumRank(Node));
	}
	const FGuid Receipt = FGuid::NewGuid();
	Save->AppliedBossRewards.Add(Receipt);
	if (!TestTrue(TEXT("Seed isolated profile with all paid nodes purchased"), UGameplayStatics::SaveGameToSlot(Save, Slot.Name, 0))) return false;
	FText Error;
	TestTrue(TEXT("Reset succeeds"), URogueProgressionSubsystem::ResetProgressionInSlotForDebug(Slot.Name, 0, Tree, Error));
	bool bWritable = false;
	URogueProgressionSaveGame* Loaded = URogueProgressionSubsystem::LoadProfileFromSlot(Slot.Name, 0, Tree, bWritable, Error);
	TestTrue(TEXT("Reloaded profile remains writable"), bWritable);
	TestEqual(TEXT("Purchased ranks are cleared on disk"), Loaded->Progression.NodeRanks.Num(), 0);
	TestEqual(TEXT("Remaining currency is preserved without a refund"), Loaded->IceBricks, 37);
	for (const FRogueUpgradeTreeNode& Node : Tree->Nodes)
	{
		TestEqual(*Node.NodeId.ToString(), Tree->GetNodeRank(Loaded->Progression, Node.NodeId), Node.bInitiallyUnlocked ? Tree->GetMaximumRank(Node) : 0);
	}
	TestFalse(TEXT("Armor is locked again"), Tree->IsArmorUnlocked(Loaded->Progression));
	TestEqual(TEXT("Permanent attack bonus is removed"), Tree->GetStatBonus(Loaded->Progression, ERogueMetaStat::AttackPower), 0.0f);
	TestTrue(TEXT("Boss reward receipt survives reset"), Loaded->AppliedBossRewards.Contains(Receipt));
	Loaded->AddBossReward(Receipt);
	TestEqual(TEXT("Same Boss reward cannot be claimed again"), Loaded->IceBricks, 37);
	TestTrue(TEXT("Repeated reset succeeds"), URogueProgressionSubsystem::ResetProgressionInSlotForDebug(Slot.Name, 0, Tree, Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueDebugResetInvalidInputTest, "Rogue.MetaProgression.DebugReset.InvalidDefinitionDoesNotWrite", RogueBrickDebugTests::Flags)
bool FRogueDebugResetInvalidInputTest::RunTest(const FString& Parameters)
{
	RogueBrickDebugTests::FSlot Slot;
	URogueProgressionSaveGame* Save = NewObject<URogueProgressionSaveGame>();
	Save->Progression.NodeRanks.Add(TEXT("Weapon.Scythe"), 1);
	if (!TestTrue(TEXT("Seed isolated profile"), UGameplayStatics::SaveGameToSlot(Save, Slot.Name, 0))) return false;
	TArray<uint8> Before, After;
	if (!TestTrue(TEXT("Read initial bytes"), FFileHelper::LoadFileToArray(Before, *Slot.Path()))) return false;
	FText Error;
	TestFalse(TEXT("Missing definition is rejected"), URogueProgressionSubsystem::ResetProgressionInSlotForDebug(Slot.Name, 0, nullptr, Error));
	TestFalse(TEXT("Invalid input explains the problem"), Error.IsEmpty());
	TestTrue(TEXT("Read remaining bytes"), FFileHelper::LoadFileToArray(After, *Slot.Path()));
	TestTrue(TEXT("Rejected reset preserves exact original save"), Before == After);
	return true;
}
#endif
