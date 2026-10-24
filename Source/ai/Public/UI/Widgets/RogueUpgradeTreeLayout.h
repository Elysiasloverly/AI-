#pragma once

#include "CoreMinimal.h"
#include "Progression/Meta/RogueProgressionTypes.h"

struct FRogueUpgradeTreeLayout
{
	TMap<FName, FVector2D> Positions;
	TMap<FName, FVector2D> Sizes;
	FVector2D ContentSize = FVector2D::ZeroVector;
	TArray<FName> InvalidPrerequisites;
	/** Primary subtree's [first column, exclusive end). Ancestors can span several lanes. */
	TMap<FName, FIntPoint> ColumnSpans;
	int32 ColumnCount = 0;
	double ColumnWidth = 1.0;
	double MaximumNodeWidth = 1.0;
	FVector2D Padding = FVector2D::ZeroVector;
};

struct FRogueUpgradeTreePage
{
	FRogueUpgradeTreeLayout Layout;
	int32 PageIndex = 0;
	int32 PageCount = 1;
};

namespace RogueUpgradeTreeLayout
{
	/** 按配置前置拓扑分层：无前置横排第一阶，后续节点向下展开；实际尺寸决定间距。 */
	AI_API FRogueUpgradeTreeLayout Build(const TArray<FRogueUpgradeTreeNode>& Nodes,
		const TMap<FName, FVector2D>& MeasuredSizes, FVector2D FallbackSize, FVector2D Spacing, FVector2D Padding);
	/** 每页最多四个实际列。直线分支整列保留；跨页分叉会重复显示共同前置。 */
	AI_API FRogueUpgradeTreePage Paginate(const FRogueUpgradeTreeLayout& FullLayout, int32 PageIndex, int32 ColumnsPerPage = 4);
}
