#if WITH_DEV_AUTOMATION_TESTS
#include "UI/Widgets/RogueUpgradeTreeLayout.h"
#include "Progression/Meta/RogueProgressionSaveGame.h"
#include "Progression/Meta/RogueProgressionSubsystem.h"
#include "Progression/Meta/RogueUpgradeTreeDefinition.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"

namespace RogueTreeLayoutTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;
	struct FSlot
	{
		FString Name = TEXT("Automation_TreeLayout_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
		~FSlot() { UGameplayStatics::DeleteGameInSlot(Name, 0); }
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueTreeResizedBoundsTest, "Rogue.UI.UpgradeTree.ResizedCardsNeverOverlap", RogueTreeLayoutTests::Flags)
bool FRogueTreeResizedBoundsTest::RunTest(const FString& Parameters)
{
	// Arrange: four cards whose customized sizes exceed the old 288x204 grid.
	TArray<FRogueUpgradeTreeNode> Nodes;
	TMap<FName, FVector2D> Sizes;
	for (int32 Index = 0; Index < 4; ++Index)
	{
		FRogueUpgradeTreeNode& Node = Nodes.AddDefaulted_GetRef();
		Node.NodeId = FName(*FString::Printf(TEXT("Node.%d"), Index));
		Node.TreePosition = FVector2D((Index % 2) * 330, (Index / 2) * 260);
		if (Index >= 2) Node.Prerequisites.Add(FName(*FString::Printf(TEXT("Node.%d"), Index - 2)));
		Sizes.Add(Node.NodeId, Index % 2 == 0 ? FVector2D(550, 430) : FVector2D(410, 360));
	}
	// Act.
	const FRogueUpgradeTreeLayout Layout = RogueUpgradeTreeLayout::Build(Nodes, Sizes, FVector2D(288, 204), FVector2D(42, 56), FVector2D(36, 36));
	// Assert: actual sizes are retained, every pair is separated, and all fit inside the scroll content.
	for (int32 A = 0; A < Nodes.Num(); ++A)
	{
		const FName IdA = Nodes[A].NodeId;
		const FVector2D PositionA = Layout.Positions.FindChecked(IdA), SizeA = Layout.Sizes.FindChecked(IdA);
		TestTrue(TEXT("Measured size is not replaced by the old fixed cell"), SizeA.Equals(Sizes.FindChecked(IdA)));
		TestTrue(TEXT("Content includes every card"), PositionA.X + SizeA.X <= Layout.ContentSize.X && PositionA.Y + SizeA.Y <= Layout.ContentSize.Y);
		for (int32 B = A + 1; B < Nodes.Num(); ++B)
		{
			const FVector2D PositionB = Layout.Positions.FindChecked(Nodes[B].NodeId), SizeB = Layout.Sizes.FindChecked(Nodes[B].NodeId);
			TestTrue(TEXT("Resized card rectangles do not overlap"), PositionA.X + SizeA.X <= PositionB.X || PositionB.X + SizeB.X <= PositionA.X
				|| PositionA.Y + SizeA.Y <= PositionB.Y || PositionB.Y + SizeB.Y <= PositionA.Y);
		}
	}
	TestTrue(TEXT("The child row is generated below its configured parent"), Layout.Positions[Nodes[0].NodeId].Y < Layout.Positions[Nodes[2].NodeId].Y);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueTreeDuplicatePositionTest, "Rogue.UI.UpgradeTree.DuplicatePositionsRemainClickable", RogueTreeLayoutTests::Flags)
bool FRogueTreeDuplicatePositionTest::RunTest(const FString& Parameters)
{
	TArray<FRogueUpgradeTreeNode> Nodes;
	for (int32 Index = 0; Index < 3; ++Index)
	{
		FRogueUpgradeTreeNode& Node = Nodes.AddDefaulted_GetRef();
		Node.NodeId = FName(*FString::Printf(TEXT("Overlap.%d"), Index));
	}
	const FRogueUpgradeTreeLayout Layout = RogueUpgradeTreeLayout::Build(Nodes, {}, FVector2D(500, 300), FVector2D(42, 56), FVector2D(36, 36));
	TestEqual(TEXT("All authored nodes remain present"), Layout.Positions.Num(), 3);
	TestTrue(TEXT("Second duplicate is moved past the first card"), Layout.Positions[Nodes[1].NodeId].X >= Layout.Positions[Nodes[0].NodeId].X + 542);
	TestTrue(TEXT("Third duplicate has its own clickable area"), Layout.Positions[Nodes[2].NodeId].X >= Layout.Positions[Nodes[1].NodeId].X + 542);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueTreeSequentialPurchaseTest, "Rogue.MetaProgression.SequentialBranchesRejectSkipping", RogueTreeLayoutTests::Flags)
bool FRogueTreeSequentialPurchaseTest::RunTest(const FString& Parameters)
{
	// Arrange: enough currency cannot bypass a missing middle node.
	const URogueUpgradeTreeDefinition* Tree = NewObject<URogueUpgradeTreeDefinition>();
	URogueProgressionSaveGame* Save = NewObject<URogueProgressionSaveGame>();
	Save->IceBricks = 100;
	FText Failure;
	TestTrue(TEXT("Weapon can be purchased first"), Save->TryPurchase(Tree, TEXT("Weapon.Scythe"), Failure));
	// Act / assert: each step is mandatory, and rejection preserves the balance.
	TestFalse(TEXT("Size cannot skip the quantity card"), Save->TryPurchase(Tree, TEXT("Card.ScytheSize"), Failure));
	TestFalse(TEXT("Lifesteal cannot skip both middle cards"), Save->TryPurchase(Tree, TEXT("Card.ScytheLifesteal"), Failure));
	TestEqual(TEXT("Blocked upper cards did not charge"), Save->IceBricks, 95);
	TestTrue(TEXT("Quantity card is the next step"), Save->TryPurchase(Tree, TEXT("Card.ScytheCount"), Failure));
	TestFalse(TEXT("Lifesteal still requires size"), Save->TryPurchase(Tree, TEXT("Card.ScytheLifesteal"), Failure));
	TestTrue(TEXT("Size unlocks after quantity"), Save->TryPurchase(Tree, TEXT("Card.ScytheSize"), Failure));
	TestTrue(TEXT("Lifesteal unlocks after size"), Save->TryPurchase(Tree, TEXT("Card.ScytheLifesteal"), Failure));
	FRogueProgressionSnapshot Tampered;
	Tampered.NodeRanks.Add(TEXT("Weapon.Scythe"), 1);
	Tampered.NodeRanks.Add(TEXT("Card.ScytheLifesteal"), 1);
	TestFalse(TEXT("A raw upper rank without middle ranks grants no card permission"), Tree->IsUpgradeAvailable(Tampered, ERogueUpgradeType::ScytheLifesteal, true));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueTreeLegacySaveTest, "Rogue.MetaProgression.LegacyBranchesKeepPaidUnlocks", RogueTreeLayoutTests::Flags)
bool FRogueTreeLegacySaveTest::RunTest(const FString& Parameters)
{
	// Arrange: a legitimate v1 player purchased lifesteal while it only needed the weapon.
	RogueTreeLayoutTests::FSlot Slot;
	const URogueUpgradeTreeDefinition* Tree = NewObject<URogueUpgradeTreeDefinition>();
	URogueProgressionSaveGame* Old = NewObject<URogueProgressionSaveGame>();
	Old->Version = 1;
	Old->IceBricks = 7;
	Old->Progression.NodeRanks.Add(TEXT("Weapon.Scythe"), 1);
	Old->Progression.NodeRanks.Add(TEXT("Card.ScytheLifesteal"), 1);
	const FGuid Receipt = FGuid::NewGuid();
	Old->AppliedBossRewards.Add(Receipt);
	if (!TestTrue(TEXT("An isolated old-format save is written"), UGameplayStatics::SaveGameToSlot(Old, Slot.Name, 0))) return false;
	// Act: use the real loader, which migrates before sanitizing the new prerequisites.
	bool bWritable = false; FText Error;
	URogueProgressionSaveGame* Loaded = URogueProgressionSubsystem::LoadProfileFromSlot(Slot.Name, 0, Tree, bWritable, Error);
	if (!TestNotNull(TEXT("Old save loads"), Loaded)) return false;
	// Assert: no purchased card, money or receipt is lost; new profiles still pay for each step.
	TestTrue(TEXT("Migrated save stays writable"), bWritable);
	TestEqual(TEXT("Schema is upgraded"), Loaded->Version, URogueProgressionSaveGame::CurrentVersion);
	TestEqual(TEXT("Migration does not charge or reset money"), Loaded->IceBricks, 7);
	TestEqual(TEXT("New middle quantity prerequisite is completed for legacy ownership"), Tree->GetNodeRank(Loaded->Progression, TEXT("Card.ScytheCount")), 1);
	TestEqual(TEXT("New middle size prerequisite is completed for legacy ownership"), Tree->GetNodeRank(Loaded->Progression, TEXT("Card.ScytheSize")), 1);
	TestTrue(TEXT("The previously paid upper card remains available"), Tree->IsUpgradeAvailable(Loaded->Progression, ERogueUpgradeType::ScytheLifesteal, true));
	TestTrue(TEXT("Reward receipts survive the migration"), Loaded->AppliedBossRewards.Contains(Receipt));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueTreeTopologyTest, "Rogue.UI.UpgradeTree.PrerequisitesDefineRowsAndForks", RogueTreeLayoutTests::Flags)
bool FRogueTreeTopologyTest::RunTest(const FString& Parameters)
{
    // Arrange: descendants precede their parents in the data array; legacy coordinates contradict the hierarchy.
    TArray<FRogueUpgradeTreeNode> Nodes;
    auto Add = [&Nodes](const TCHAR* Id, TArray<FName> Parents)
    {
        FRogueUpgradeTreeNode& Node = Nodes.AddDefaulted_GetRef();
        Node.NodeId = FName(Id); Node.Prerequisites = Parents; Node.TreePosition = FVector2D(0, -1000);
    };
    Add(TEXT("Join"), { FName(TEXT("A")), FName(TEXT("B.Child")) });
    Add(TEXT("Leaf1"), { FName(TEXT("Join")) });
    Add(TEXT("Leaf2"), { FName(TEXT("Join")) });
    Add(TEXT("B.Child"), { FName(TEXT("B")) });
    Add(TEXT("A"), {}); Add(TEXT("B"), {});
    const FRogueUpgradeTreeLayout Layout = RogueUpgradeTreeLayout::Build(Nodes, {}, FVector2D(500, 360), FVector2D(42, 56), FVector2D(36, 36));
    TestTrue(TEXT("Unrelated roots occupy the same first-tier row"), FMath::IsNearlyEqual(Layout.Positions[FName(TEXT("A"))].Y, Layout.Positions[FName(TEXT("B"))].Y));
    TestTrue(TEXT("A child goes below its configured parent even when listed earlier"), Layout.Positions[FName(TEXT("B.Child"))].Y >= Layout.Positions[FName(TEXT("B"))].Y + 416);
    TestTrue(TEXT("Multiple prerequisites put the child below the deepest parent"), Layout.Positions[FName(TEXT("Join"))].Y >= Layout.Positions[FName(TEXT("B.Child"))].Y + 416);
    TestTrue(TEXT("Fork siblings share a stage"), FMath::IsNearlyEqual(Layout.Positions[FName(TEXT("Leaf1"))].Y, Layout.Positions[FName(TEXT("Leaf2"))].Y));
    TestTrue(TEXT("Fork siblings have separated clickable columns"), FMath::Abs(Layout.Positions[FName(TEXT("Leaf1"))].X - Layout.Positions[FName(TEXT("Leaf2"))].X) >= 542);
    TestTrue(TEXT("All valid nodes resolve without configuration errors"), Layout.InvalidPrerequisites.IsEmpty());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueTreeInvalidGraphTest, "Rogue.UI.UpgradeTree.InvalidPrerequisitesDoNotHangLayout", RogueTreeLayoutTests::Flags)
bool FRogueTreeInvalidGraphTest::RunTest(const FString& Parameters)
{
    TArray<FRogueUpgradeTreeNode> Nodes;
    for (const TCHAR* Id : { TEXT("A"), TEXT("B"), TEXT("Orphan") }) { Nodes.AddDefaulted_GetRef().NodeId = FName(Id); }
    Nodes[0].Prerequisites.Add(FName(TEXT("B")));
    Nodes[1].Prerequisites.Add(FName(TEXT("A")));
    Nodes[2].Prerequisites.Add(FName(TEXT("Missing")));
    const FRogueUpgradeTreeLayout Layout = RogueUpgradeTreeLayout::Build(Nodes, {}, FVector2D(500, 360), FVector2D(42, 56), FVector2D(36, 36));
    TestEqual(TEXT("Cycles and missing parents are reported"), Layout.InvalidPrerequisites.Num(), 3);
    TestEqual(TEXT("Invalid nodes remain visible for diagnosis"), Layout.Positions.Num(), 3);
    return true;
}
#endif
