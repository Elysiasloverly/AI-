#include "UI/Widgets/RogueUpgradeTreeLayout.h"

FRogueUpgradeTreeLayout RogueUpgradeTreeLayout::Build(const TArray<FRogueUpgradeTreeNode>& Nodes,
	const TMap<FName, FVector2D>& MeasuredSizes, FVector2D FallbackSize, FVector2D Spacing, FVector2D Padding)
{
	FRogueUpgradeTreeLayout Result;
	if (Nodes.IsEmpty()) return Result;
	FallbackSize.X = FMath::Max(1.0, FallbackSize.X);
	FallbackSize.Y = FMath::Max(1.0, FallbackSize.Y);
	Spacing.X = FMath::Max(0.0, Spacing.X); Spacing.Y = FMath::Max(0.0, Spacing.Y);
	Padding.X = FMath::Max(0.0, Padding.X); Padding.Y = FMath::Max(0.0, Padding.Y);
	TMap<FName, const FRogueUpgradeTreeNode*> ById;
	TMap<FName, int32> Depth;
	double MaximumWidth = 1;
	for (const FRogueUpgradeTreeNode& Node : Nodes)
	{
		if (Node.NodeId.IsNone() || ById.Contains(Node.NodeId)) continue;
		ById.Add(Node.NodeId, &Node);
		FVector2D Size = MeasuredSizes.FindRef(Node.NodeId);
		if (!FMath::IsFinite(Size.X) || Size.X <= 0) Size.X = FallbackSize.X;
		if (!FMath::IsFinite(Size.Y) || Size.Y <= 0) Size.Y = FallbackSize.Y;
		Result.Sizes.Add(Node.NodeId, Size);
		MaximumWidth = FMath::Max(MaximumWidth, Size.X);
		if (Node.Prerequisites.IsEmpty()) Depth.Add(Node.NodeId, 0);
	}
	// Resolve every parent before its child; array order only orders sibling branches.
	for (int32 Pass = 0; Pass < ById.Num(); ++Pass)
	{
		bool bChanged = false;
		for (const FRogueUpgradeTreeNode& Node : Nodes)
		{
			if (Depth.Contains(Node.NodeId) || !ById.Contains(Node.NodeId)) continue;
			bool bReady = true; int32 ChildDepth = 0;
			for (FName Parent : Node.Prerequisites)
			{
				const int32* ParentDepth = Depth.Find(Parent);
				if (ParentDepth == nullptr) { bReady = false; break; }
				ChildDepth = FMath::Max(ChildDepth, *ParentDepth + 1);
			}
			if (bReady) { Depth.Add(Node.NodeId, ChildDepth); bChanged = true; }
		}
		if (!bChanged) break;
	}
	int32 MaximumDepth = 0;
	for (const TPair<FName, int32>& Entry : Depth) MaximumDepth = FMath::Max(MaximumDepth, Entry.Value);
	TArray<FName> Roots;
	TMap<FName, TArray<FName>> PrimaryChildren;
	for (const FRogueUpgradeTreeNode& Node : Nodes)
	{
		if (!ById.Contains(Node.NodeId)) continue;
		if (!Depth.Contains(Node.NodeId))
		{
			Result.InvalidPrerequisites.AddUnique(Node.NodeId);
			Roots.AddUnique(Node.NodeId);
			continue;
		}
		if (Node.Prerequisites.IsEmpty()) Roots.AddUnique(Node.NodeId);
		else PrimaryChildren.FindOrAdd(Node.Prerequisites[0]).AddUnique(Node.NodeId);
	}
	for (FName Invalid : Result.InvalidPrerequisites) Depth.Add(Invalid, MaximumDepth + 1);
	if (!Result.InvalidPrerequisites.IsEmpty()) ++MaximumDepth;
	TMap<int32, double> RowHeights, RowPositions;
	for (const TPair<FName, FVector2D>& Entry : Result.Sizes)
	{
		const int32 Level = Depth.FindChecked(Entry.Key);
		RowHeights.FindOrAdd(Level) = FMath::Max(RowHeights.FindRef(Level), Entry.Value.Y);
	}
	for (int32 Level = 0; Level <= MaximumDepth; ++Level)
	{
		RowPositions.Add(Level, Level == 0 ? Padding.Y : RowPositions[Level - 1] + RowHeights.FindRef(Level - 1) + Spacing.Y);
	}
	// Forks reserve distinct horizontal lanes; straight chains keep a shared center line.
	TMap<FName, int32> SubtreeWidths;
	TFunction<int32(FName)> Measure = [&Measure, &PrimaryChildren, &SubtreeWidths](FName Id) -> int32
	{
		if (const int32* Known = SubtreeWidths.Find(Id)) return *Known;
		int32 Width = 0;
		if (const TArray<FName>* Children = PrimaryChildren.Find(Id))
		{
			for (FName Child : *Children) Width += Measure(Child);
		}
		Width = FMath::Max(1, Width);
		SubtreeWidths.Add(Id, Width);
		return Width;
	};
	const double CellWidth = MaximumWidth + Spacing.X;
	Result.ColumnWidth = CellWidth;
	Result.MaximumNodeWidth = MaximumWidth;
	Result.Padding = Padding;
	TFunction<void(FName, double)> Place = [&Place, &Result, &SubtreeWidths, &PrimaryChildren, &Depth, &RowPositions, Padding, CellWidth, MaximumWidth](FName Id, double BeginColumn)
	{
		const FVector2D Size = Result.Sizes.FindChecked(Id);
		const double Center = Padding.X + (BeginColumn + (SubtreeWidths[Id] - 1) * 0.5) * CellWidth + MaximumWidth * 0.5;
		const FVector2D Position(Center - Size.X * 0.5, RowPositions.FindChecked(Depth.FindChecked(Id)));
		Result.Positions.Add(Id, Position);
		const int32 FirstColumn = FMath::RoundToInt(BeginColumn);
		Result.ColumnSpans.Add(Id, FIntPoint(FirstColumn, FirstColumn + SubtreeWidths[Id]));
		Result.ContentSize.X = FMath::Max(Result.ContentSize.X, Position.X + Size.X + Padding.X);
		Result.ContentSize.Y = FMath::Max(Result.ContentSize.Y, Position.Y + Size.Y + Padding.Y);
		if (const TArray<FName>* Children = PrimaryChildren.Find(Id))
		{
			double ChildColumn = BeginColumn;
			for (FName Child : *Children) { Place(Child, ChildColumn); ChildColumn += SubtreeWidths[Child]; }
		}
	};
	double RootColumn = 0;
	for (FName Root : Roots)
	{
		const int32 Width = Measure(Root);
		Place(Root, RootColumn);
		RootColumn += Width;
	}
	Result.ColumnCount = FMath::RoundToInt(RootColumn);
	return Result;
}

FRogueUpgradeTreePage RogueUpgradeTreeLayout::Paginate(const FRogueUpgradeTreeLayout& FullLayout, int32 PageIndex, int32 ColumnsPerPage)
{
	FRogueUpgradeTreePage Result;
	const int32 Columns = FMath::Clamp(ColumnsPerPage, 1, 4);
	Result.PageCount = FMath::Max(1, FMath::DivideAndRoundUp(FullLayout.ColumnCount, Columns));
	Result.PageIndex = FMath::Clamp(PageIndex, 0, Result.PageCount - 1);
	FRogueUpgradeTreeLayout& Page = Result.Layout;
	Page.ColumnWidth = FullLayout.ColumnWidth;
	Page.MaximumNodeWidth = FullLayout.MaximumNodeWidth;
	Page.Padding = FullLayout.Padding;
	Page.InvalidPrerequisites = FullLayout.InvalidPrerequisites;
	const int32 First = Result.PageIndex * Columns;
	const int32 End = FMath::Min(First + Columns, FullLayout.ColumnCount);
	Page.ColumnCount = FMath::Max(0, End - First);
	for (const TPair<FName, FIntPoint>& Entry : FullLayout.ColumnSpans)
	{
		const int32 BeginColumn = FMath::Max(First, Entry.Value.X);
		const int32 EndColumn = FMath::Min(End, Entry.Value.Y);
		if (BeginColumn >= EndColumn) { continue; }
		const FVector2D Size = FullLayout.Sizes.FindChecked(Entry.Key);
		FVector2D Position = FullLayout.Positions.FindChecked(Entry.Key);
		const double CenterColumn = (BeginColumn + EndColumn - 1) * 0.5 - First;
		Position.X = Page.Padding.X + CenterColumn * Page.ColumnWidth + (Page.MaximumNodeWidth - Size.X) * 0.5;
		Page.Positions.Add(Entry.Key, Position);
		Page.Sizes.Add(Entry.Key, Size);
		Page.ColumnSpans.Add(Entry.Key, FIntPoint(BeginColumn - First, EndColumn - First));
		Page.ContentSize.X = FMath::Max(Page.ContentSize.X, Position.X + Size.X + Page.Padding.X);
		Page.ContentSize.Y = FMath::Max(Page.ContentSize.Y, Position.Y + Size.Y + Page.Padding.Y);
	}
	// Keep the same four-column display scale on the last page, even when only one or two lanes remain.
	if (FullLayout.ColumnCount > 0)
	{
		Page.ContentSize.X = FMath::Max(Page.ContentSize.X,
			Page.Padding.X * 2 + (Columns - 1) * Page.ColumnWidth + Page.MaximumNodeWidth);
	}
	return Result;
}
