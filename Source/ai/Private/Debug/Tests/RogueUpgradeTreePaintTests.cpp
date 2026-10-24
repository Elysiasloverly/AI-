#if WITH_DEV_AUTOMATION_TESTS
#include "UI/Widgets/RogueUpgradeTreePaintSpace.h"

#include "Components/CanvasPanel.h"
#include "Components/SizeBox.h"
#include "Input/HittestGrid.h"
#include "Misc/AutomationTest.h"
#include "Rendering/DrawElements.h"
#include "Types/PaintArgs.h"
#include "Widgets/SWindow.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueTreePaintSpaceTest, "Rogue.UI.UpgradeTree.ArrowsIgnoreDesktopOffsetAndFollowScroll",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FRogueTreePaintSpaceTest::RunTest(const FString& Parameters)
{
	// Arrange: real Slate paint passes with a separate window-to-desktop offset,
	// as in a moved PIE window. No native window, player save or map is created.
	const TSharedRef<SWindow> Window = SNew(SWindow).ClientSize(FVector2D(1920, 1080));
	UCanvasPanel* Canvas = NewObject<UCanvasPanel>();
	USizeBox* Viewport = NewObject<USizeBox>();
	const TSharedRef<SWidget> CanvasSlate = Canvas->TakeWidget();
	const TSharedRef<SWidget> ViewportSlate = Viewport->TakeWidget();
	const FVector2D DesktopOffsets[] = { FVector2D::ZeroVector, FVector2D(70, 120), FVector2D(840, 360) };
	const float Scales[] = { 1.0f, 0.85f, 1.25f };
	const float ScrollOffsets[] = { 0.0f, 300.0f };
	const FVector2D ParentAnchor(191, 292), ChildAnchor(191, 334);
	for (int32 Case = 0; Case < UE_ARRAY_COUNT(DesktopOffsets); ++Case)
	{
		for (float ScrollOffset : ScrollOffsets)
		{
			const FGeometry Root = FGeometry::MakeRoot(FVector2D(2400, 1600), FSlateLayoutTransform(Scales[Case]));
			const FGeometry ViewportGeometry = Root.MakeChild(FVector2D(1800, 700), FSlateLayoutTransform(FVector2D(36, 250)));
			const FGeometry CanvasGeometry = ViewportGeometry.MakeChild(FVector2D(2200, 1500), FSlateLayoutTransform(FVector2D(0, -ScrollOffset)));
			const FSlateRect CullingRect(0, 0, 4000, 3000);
			FHittestGrid Grid;
			Grid.SetHittestArea(FVector2D::ZeroVector, FVector2D(4000, 3000));
			const FPaintArgs Args(nullptr, Grid, DesktopOffsets[Case], 0.0, 0.0f);
			FSlateWindowElementList Elements(Window);
			ViewportSlate->SlatePrepass(Scales[Case]); CanvasSlate->SlatePrepass(Scales[Case]);
			ViewportSlate->Paint(Args, ViewportGeometry, CullingRect, Elements, 0, FWidgetStyle(), true);
			CanvasSlate->Paint(Args, CanvasGeometry, CullingRect, Elements, 0, FWidgetStyle(), true);

			// Act: use the same paint-space resolver as the real upgrade-tree arrows.
			const FRogueUpgradeTreePaintSpace Space = RogueUpgradeTreePaintSpace::Resolve(*Canvas, Viewport, CullingRect);
			// Assert: parent and child arrow anchors follow the cards, including scrolling/scale.
			TestTrue(TEXT("Parent arrow anchor remains in window paint coordinates"),
				FVector2D(Space.CanvasGeometry.LocalToAbsolute(ParentAnchor)).Equals(FVector2D(CanvasGeometry.LocalToAbsolute(ParentAnchor)), 0.01));
			TestTrue(TEXT("Child arrow anchor remains in window paint coordinates"),
				FVector2D(Space.CanvasGeometry.LocalToAbsolute(ChildAnchor)).Equals(FVector2D(CanvasGeometry.LocalToAbsolute(ChildAnchor)), 0.01));
			const FSlateRect ExpectedClip = CanvasGeometry.GetRenderBoundingRect().IntersectionWith(ViewportGeometry.GetRenderBoundingRect()).IntersectionWith(CullingRect);
			TestTrue(TEXT("Arrow clipping uses the same window coordinates as the scroll viewport"),
				FMath::IsNearlyEqual(Space.Clip.Left, ExpectedClip.Left) && FMath::IsNearlyEqual(Space.Clip.Top, ExpectedClip.Top)
				&& FMath::IsNearlyEqual(Space.Clip.Right, ExpectedClip.Right) && FMath::IsNearlyEqual(Space.Clip.Bottom, ExpectedClip.Bottom));
			if (!DesktopOffsets[Case].IsNearlyZero())
			{
				TestFalse(TEXT("Fixture reproduces the old bug: cached tick geometry adds a desktop offset"),
					FVector2D(Canvas->GetCachedGeometry().LocalToAbsolute(ParentAnchor)).Equals(FVector2D(CanvasGeometry.LocalToAbsolute(ParentAnchor)), 0.01));
			}
		}
	}
	Canvas->ReleaseSlateResources(true); Viewport->ReleaseSlateResources(true);
	return true;
}
#endif
