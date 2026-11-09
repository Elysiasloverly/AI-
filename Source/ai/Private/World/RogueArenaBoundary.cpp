#include "World/RogueArenaBoundary.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
ARogueArenaBoundary::ARogueArenaBoundary()
{
    PrimaryActorTick.bCanEverTick=false;
    RootComponent=CreateDefaultSubobject<USceneComponent>(TEXT("BoundaryRoot"));
}
void ARogueArenaBoundary::RebuildWalls()
{
    for (UBoxComponent* Wall : Walls)
        if (Wall) Wall->DestroyComponent();
    Walls.Reset();

    const double HalfThickness = FMath::Max(20.f, Thickness) * .5;
    const double Depth = FMath::Max(0.f, BelowGroundDepth);
    for (const auto& Loop : Loops)
    {
        const int32 Count = Loop.Points.Num();
        if (Count < (Loop.bClosed ? 3 : 2)) continue;
        const int32 SegmentCount = Loop.bClosed ? Count : Count - 1;
        for (int32 Index = 0; Index < SegmentCount; ++Index)
        {
            const FVector A = Loop.Points[Index];
            const FVector B = Loop.Points[(Index + 1) % Count];
            const FVector Delta(B.X - A.X, B.Y - A.Y, 0);
            if (A.ContainsNaN() || B.ContainsNaN() || Delta.SizeSquared() < 1.) continue;
            const double Bottom = FMath::Min(A.Z, B.Z) - Depth;
            const double Top = FMath::Max(A.Z, B.Z) + FMath::Max(500.f, Height) - 200.0;
            auto* Wall = NewObject<UBoxComponent>(this, NAME_None, RF_Transient);
            Wall->SetupAttachment(RootComponent);
            Wall->SetBoxExtent(FVector(Delta.Size() * .5 + HalfThickness, HalfThickness, (Top - Bottom) * .5));
            Wall->SetRelativeLocation(FVector((A.X + B.X) * .5, (A.Y + B.Y) * .5, (Bottom + Top) * .5));
            Wall->SetRelativeRotation(Delta.Rotation());
            Wall->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
            // Keep pawn-only barriers out of object-type ground and weapon queries.
            Wall->SetCollisionObjectType(CollisionChannel);
            Wall->SetCollisionResponseToAllChannels(ECR_Ignore);
            Wall->SetCollisionResponseToChannel(ECC_Pawn,ECR_Block);
            Wall->SetGenerateOverlapEvents(false);Wall->SetHiddenInGame(true);
            Wall->SetCanEverAffectNavigation(true);Wall->RegisterComponent();Walls.Add(Wall);
        }
    }
}
bool ARogueArenaBoundary::TryGetOuterRayDistance(const FVector& Origin, const FVector& Direction, double& OutDistance) const
{
    const FRogueBoundaryLoop* Outer = nullptr;
    double LargestArea = 0.;
    for (const FRogueBoundaryLoop& Loop : Loops)
    {
        if (!Loop.bClosed || Loop.Points.Num() < 3) continue;
        double TwiceArea = 0.;
        for (int32 Index = 0; Index < Loop.Points.Num(); ++Index)
        {
            const FVector& A = Loop.Points[Index];
            const FVector& B = Loop.Points[(Index + 1) % Loop.Points.Num()];
            TwiceArea += A.X * B.Y - A.Y * B.X;
        }
        const double Area = FMath::Abs(TwiceArea);
        if (Area > LargestArea) { LargestArea = Area; Outer = &Loop; }
    }

    const FVector2D Ray = FVector2D(Direction).GetSafeNormal();
    if (!Outer || Ray.IsNearlyZero()) return false;
    const FVector2D Start(Origin);
    const FTransform Transform = GetActorTransform();
    bool bInside = false;
    double First = TNumericLimits<double>::Max();
    double Last = -1.;
    for (int32 Index = 0; Index < Outer->Points.Num(); ++Index)
    {
        const FVector2D A(Transform.TransformPosition(Outer->Points[Index]));
        const FVector2D B(Transform.TransformPosition(Outer->Points[(Index + 1) % Outer->Points.Num()]));
        const FVector2D Edge = B - A;
        if ((A.Y > Start.Y) != (B.Y > Start.Y)
            && Start.X < A.X + (Start.Y - A.Y) * Edge.X / Edge.Y)
        {
            bInside = !bInside;
        }

        const double Cross = FVector2D::CrossProduct(Ray, Edge);
        if (FMath::IsNearlyZero(Cross)) continue;
        const FVector2D Offset = A - Start;
        const double AlongEdge = FVector2D::CrossProduct(Offset, Ray) / Cross;
        const double AlongRay = FVector2D::CrossProduct(Offset, Edge) / Cross;
        if (AlongEdge >= -UE_DOUBLE_SMALL_NUMBER && AlongEdge <= 1. + UE_DOUBLE_SMALL_NUMBER && AlongRay >= 0.)
        {
            First = FMath::Min(First, AlongRay);
            Last = FMath::Max(Last, AlongRay);
        }
    }
    if (Last < 0.) return false;
    OutDistance = bInside ? First : Last;
    return true;
}

void ARogueArenaBoundary::OnConstruction(const FTransform& Transform){Super::OnConstruction(Transform);RebuildWalls();}
void ARogueArenaBoundary::PostInitializeComponents(){Super::PostInitializeComponents();RebuildWalls();}
