#pragma once

#include "Components/Widget.h"
#include "Layout/Geometry.h"

struct FRogueUpgradeTreePaintSpace
{
	FGeometry CanvasGeometry;
	FSlateRect Clip;
};

namespace RogueUpgradeTreePaintSpace
{
	/** Resolve after painting the children, so every geometry belongs to this window's paint pass. */
	inline FRogueUpgradeTreePaintSpace Resolve(const UWidget& Canvas, const UWidget* Viewport, const FSlateRect& CullingRect)
	{
		FRogueUpgradeTreePaintSpace Result{ Canvas.GetPaintSpaceGeometry(), CullingRect };
		Result.Clip = Result.Clip.IntersectionWith(Result.CanvasGeometry.GetRenderBoundingRect());
		if (Viewport != nullptr)
		{
			Result.Clip = Result.Clip.IntersectionWith(Viewport->GetPaintSpaceGeometry().GetRenderBoundingRect());
		}
		return Result;
	}
}
