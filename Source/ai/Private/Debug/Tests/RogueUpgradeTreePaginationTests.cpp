#if WITH_DEV_AUTOMATION_TESTS
#include "UI/Widgets/RogueUpgradeTreeLayout.h"
#include "Progression/Meta/RogueUpgradeTreeDefinition.h"
#include "Misc/AutomationTest.h"

namespace RogueTreePaginationTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;
	FRogueUpgradeTreeLayout Build(const TArray<FRogueUpgradeTreeNode>& Nodes, const TMap<FName,FVector2D>& Sizes = {})
	{
		return RogueUpgradeTreeLayout::Build(Nodes, Sizes, FVector2D(288,204), FVector2D(42,56), FVector2D(36,36));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueTreeActualPagesTest, "Rogue.UI.UpgradeTree.Pagination.ActualTreesKeepEachChainTogether", RogueTreePaginationTests::Flags)
bool FRogueTreeActualPagesTest::RunTest(const FString& Parameters)
{
	const auto* Definition=NewObject<URogueUpgradeTreeDefinition>();
	for (bool Attributes:{false,true})
	{
		TArray<FRogueUpgradeTreeNode> Nodes;
		for (const FRogueUpgradeTreeNode& Node:Definition->GetNodes())
		{
			const bool Attribute=Node.Kind==ERogueMetaNodeKind::BaseStat || Node.Kind==ERogueMetaNodeKind::ArmorUnlock;
			if (Attribute==Attributes) Nodes.Add(Node);
		}
		const auto Full=RogueTreePaginationTests::Build(Nodes);
		const int32 Count=RogueUpgradeTreeLayout::Paginate(Full,0).PageCount;
		const double PageWidth=RogueUpgradeTreeLayout::Paginate(Full,0).Layout.ContentSize.X;
		TestEqual(TEXT("Current default weapon and attribute trees each need two pages"),Count,2);
		TSet<FName> Seen;
		for (int32 Index=0;Index<Count;++Index)
		{
			const auto Page=RogueUpgradeTreeLayout::Paginate(Full,Index);
			TestTrue(TEXT("No page exceeds four actual columns"),Page.Layout.ColumnCount<=4);
			TestEqual(TEXT("Last page retains the same card display scale"),Page.Layout.ContentSize.X,PageWidth);
			TestTrue(TEXT("Page width fits four original size cards"),Page.Layout.ContentSize.X<=4*288+3*42+72);
			for (const auto& Entry:Page.Layout.Positions)
			{
				TestFalse(TEXT("Straight-chain nodes are not duplicated across pages"),Seen.Contains(Entry.Key));
				Seen.Add(Entry.Key);
				const auto* Node=Definition->FindNode(Entry.Key);
				for (FName Parent:Node->Prerequisites)
				{
					TestTrue(TEXT("The parent remains on the same page as its child"),Page.Layout.Positions.Contains(Parent));
					if (Page.Layout.Positions.Contains(Parent))
					{
						TestTrue(TEXT("The child still goes downward"),Entry.Value.Y>Page.Layout.Positions[Parent].Y);
						TestEqual(TEXT("Straight-chain centers still align"),Entry.Value.X,Page.Layout.Positions[Parent].X);
					}
				}
			}
		}
		TestEqual(TEXT("Every configured node is reachable by paging"),Seen.Num(),Nodes.Num());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueTreeForkPagesTest, "Rogue.UI.UpgradeTree.Pagination.WideForkRepeatsParentWithoutShrinkingCards", RogueTreePaginationTests::Flags)
bool FRogueTreeForkPagesTest::RunTest(const FString& Parameters)
{
	TArray<FRogueUpgradeTreeNode> Nodes;
	Nodes.AddDefaulted_GetRef().NodeId=TEXT("Root");
	TMap<FName,FVector2D> Sizes;
	Sizes.Add(TEXT("Root"),FVector2D(520,340));
	for (int32 Index=0;Index<6;++Index)
	{
		auto& Node=Nodes.AddDefaulted_GetRef();Node.NodeId=FName(*FString::Printf(TEXT("Child.%d"),Index));Node.Prerequisites.Add(TEXT("Root"));
		Sizes.Add(Node.NodeId,FVector2D(500,300));
	}
	const auto Full=RogueTreePaginationTests::Build(Nodes,Sizes);
	for (int32 Index=0;Index<2;++Index)
	{
		const auto Page=RogueUpgradeTreeLayout::Paginate(Full,Index);
		TestEqual(TEXT("Fork has two pages"),Page.PageCount,2);
		TestTrue(TEXT("Common prerequisite stays visible on each fork page"),Page.Layout.Positions.Contains(TEXT("Root")));
		TestEqual(TEXT("Custom root size is retained"),Page.Layout.Sizes[FName(TEXT("Root"))],FVector2D(520,340));
		TestEqual(TEXT("Only this page's fork children are shown"),Page.Layout.Positions.Num(),Index==0?5:3);
		for (const auto& A:Page.Layout.Positions)
		{
			const auto SizeA=Page.Layout.Sizes[A.Key];
			TestTrue(TEXT("Page includes the entire custom card"),A.Value.X>=0 && A.Value.X+SizeA.X<=Page.Layout.ContentSize.X);
			for (const auto& B:Page.Layout.Positions)
			{
				if (A.Key==B.Key) continue;
				const auto SizeB=Page.Layout.Sizes[B.Key];
				TestTrue(TEXT("Fork card rectangles never overlap"),A.Value.X+SizeA.X<=B.Value.X || B.Value.X+SizeB.X<=A.Value.X || A.Value.Y+SizeA.Y<=B.Value.Y || B.Value.Y+SizeB.Y<=A.Value.Y);
			}
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueTreePageBoundsTest, "Rogue.UI.UpgradeTree.Pagination.EmptyInvalidAndOutOfRangePagesStaySafe", RogueTreePaginationTests::Flags)
bool FRogueTreePageBoundsTest::RunTest(const FString& Parameters)
{
	const auto Empty=RogueUpgradeTreeLayout::Paginate({},999);
	TestEqual(TEXT("Empty tree has one empty page"),Empty.PageCount,1);
	TestEqual(TEXT("Empty tree starts at zero"),Empty.PageIndex,0);
	TArray<FRogueUpgradeTreeNode> Nodes;
	for (int32 I=0;I<9;++I) { auto& Node=Nodes.AddDefaulted_GetRef();Node.NodeId=FName(*FString::Printf(TEXT("N.%d"),I));Node.Prerequisites.Add(TEXT("Missing")); }
	const auto Full=RogueTreePaginationTests::Build(Nodes);
	const auto First=RogueUpgradeTreeLayout::Paginate(Full,-10,999);
	const auto Last=RogueUpgradeTreeLayout::Paginate(Full,999,999);
	TestEqual(TEXT("Invalid configuration remains diagnosable"),First.Layout.InvalidPrerequisites.Num(),9);
	TestEqual(TEXT("Columns are capped at four even for bad caller values"),First.Layout.ColumnCount,4);
	TestEqual(TEXT("First page clamps"),First.PageIndex,0);
	TestEqual(TEXT("Last page clamps"),Last.PageIndex,2);
	TestEqual(TEXT("Last remaining column remains accessible"),Last.Layout.ColumnCount,1);
	return true;
}
#endif
