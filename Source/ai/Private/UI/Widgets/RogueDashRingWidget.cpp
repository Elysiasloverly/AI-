#include "UI/Widgets/RogueDashRingWidget.h"

#include "Brushes/SlateRoundedBoxBrush.h"
#include "Rendering/DrawElements.h"
#include "Widgets/SLeafWidget.h"

/** 用矢量绘制，缩放时保留圆润边缘，不依赖贴图分辨率或额外材质。 */
class SRogueDashCooldownArc final : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SRogueDashCooldownArc) {}
    SLATE_END_ARGS()
    void Construct(const FArguments&) {}

    void SetStyle(float InRadius, float InArc, float InThickness, FLinearColor InTrack,
        FLinearColor InFill, float InPercent)
    {
        const bool bSizeChanged = !FMath::IsNearlyEqual(Radius, InRadius);
        Radius = InRadius;
        ArcDegrees = InArc;
        Thickness = InThickness;
        Track = InTrack;
        Fill = InFill;
        Percent = InPercent;
        Invalidate(bSizeChanged ? EInvalidateWidgetReason::Layout : EInvalidateWidgetReason::Paint);
    }

    virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D((Radius + 10.f) * 2.f); }

    virtual int32 OnPaint(const FPaintArgs&, const FGeometry& Geometry, const FSlateRect&,
        FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle& Style, bool) const override
    {
        const FVector2D Center = Geometry.GetLocalSize() * .5f;
        const float DrawRadius = FMath::Min(Radius, float(FMath::Min(Center.X, Center.Y)) - Thickness - 2.f);
        if (DrawRadius <= 0.f) return Layer;
        const FLinearColor Tint = Style.GetColorAndOpacityTint();
        auto DrawArc = [&](float Fraction, FLinearColor Color, float Width, int32 DrawLayer)
        {
            if (Fraction <= KINDA_SMALL_NUMBER) return;
            const int32 Steps = FMath::Max(2, FMath::CeilToInt(ArcDegrees * Fraction / 3.f));
            TArray<FVector2D> Points;
            Points.Reserve(Steps + 1);
            for (int32 Index = 0; Index <= Steps; ++Index)
            {
                // 从圆弧底端向上恢复，空缺始终朝向角色左侧。
                const float Angle = FMath::DegreesToRadians(ArcDegrees * .5f - ArcDegrees * Fraction * Index / Steps);
                Points.Add(Center + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * DrawRadius);
            }
            FSlateDrawElement::MakeLines(Elements, DrawLayer, Geometry.ToPaintGeometry(), Points,
                ESlateDrawEffect::None, Color * Tint, true, Width);
            for (const auto& Point : {Points[0], Points.Last()})
                FSlateDrawElement::MakeBox(Elements, DrawLayer, Geometry.ToPaintGeometry(FVector2D(Width),
                    FSlateLayoutTransform(Point - FVector2D(Width * .5f))), &RoundTip, ESlateDrawEffect::None, Color * Tint);
        };
        // 深色细外沿保证在冰面、火焰和白色地面上也能识别。
        DrawArc(1.f, FLinearColor(.008f, .018f, .027f, .72f), Thickness + 3.f, Layer);
        DrawArc(1.f, Track, Thickness, Layer + 1);
        DrawArc(Percent, Fill, Thickness, Layer + 2);
        FLinearColor Core = FMath::Lerp(Fill, FLinearColor::White, .45f);
        Core.A = Fill.A * .55f;
        DrawArc(Percent, Core, FMath::Max(1.f, Thickness * .3f), Layer + 3);
        return Layer + 3;
    }

private:
    FSlateRoundedBoxBrush RoundTip = FSlateRoundedBoxBrush(FLinearColor::White, 100.f);
    float Radius = 44.f, ArcDegrees = 150.f, Thickness = 4.f, Percent = 0.f;
    FLinearColor Track, Fill;
};

TSharedRef<SWidget> URogueDashRingWidget::RebuildWidget()
{
    ArcWidget = SNew(SRogueDashCooldownArc);
    RefreshArc();
    return ArcWidget.ToSharedRef();
}

void URogueDashRingWidget::SynchronizeProperties()
{
    Super::SynchronizeProperties();
    RefreshArc();
}

void URogueDashRingWidget::ReleaseSlateResources(bool bReleaseChildren)
{
    Super::ReleaseSlateResources(bReleaseChildren);
    ArcWidget.Reset();
}

void URogueDashRingWidget::SetCooldown(float Percent, bool bReady, bool bUsable)
{
    if (bReady != bCooldownReady) ReadyElapsed = 0.f;
    CooldownPercent = FMath::IsFinite(Percent) ? FMath::Clamp(Percent, 0.f, 1.f) : 0.f;
    bCooldownReady = bReady;
    bCooldownUsable = bUsable;
    RefreshArc();
}

void URogueDashRingWidget::AdvancePresentation(float DeltaSeconds)
{
    // 就绪时短暂亮起，再回到低透明度；冷却期间保持清晰可读。
    if (!bCooldownReady || ReadyElapsed >= 2.f) return;
    ReadyElapsed = FMath::Min(2.f, ReadyElapsed + FMath::Max(0.f, DeltaSeconds));
    RefreshArc();
}

void URogueDashRingWidget::RefreshArc()
{
    if (!ArcWidget) return;
    FLinearColor Color = bCooldownReady ? ReadyColor : CooldownColor;
    if (!bCooldownUsable) Color.A *= .35f;
    else if (bCooldownReady)
        Color.A *= FMath::Lerp(FMath::Clamp(ReadyIdleOpacity, 0.f, 1.f), 1.f, FMath::Exp(-ReadyElapsed * 5.f));
    ArcWidget->SetStyle(FMath::Clamp(Radius, 12.f, 100.f), FMath::Clamp(ArcDegrees, 30.f, 360.f),
        FMath::Clamp(Thickness, 1.f, 12.f), TrackColor, Color, bCooldownUsable ? CooldownPercent : 0.f);
}
