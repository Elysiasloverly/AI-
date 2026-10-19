#include "World/RogueEnemySpawnRegion.h"
#include "Components/BoxComponent.h"
#include "Components/SplineComponent.h"

namespace
{
    double Cross(const FVector2D& A, const FVector2D& B, const FVector2D& C)
    {
        return FVector2D::CrossProduct(B - A, C - A);
    }
    bool OnSegment(const FVector2D& A, const FVector2D& B, const FVector2D& P)
    {
        return FMath::Abs(Cross(A,B,P)) < 0.0001 && P.X >= FMath::Min(A.X,B.X)-0.0001 && P.X <= FMath::Max(A.X,B.X)+0.0001
            && P.Y >= FMath::Min(A.Y,B.Y)-0.0001 && P.Y <= FMath::Max(A.Y,B.Y)+0.0001;
    }
    bool EdgesIntersect(const FVector2D& A,const FVector2D& B,const FVector2D& C,const FVector2D& D)
    {
        return (Cross(A,B,C)*Cross(A,B,D) < 0.0 && Cross(C,D,A)*Cross(C,D,B) < 0.0)
            || OnSegment(A,B,C) || OnSegment(A,B,D) || OnSegment(C,D,A) || OnSegment(C,D,B);
    }
}

ARogueEnemySpawnRegion::ARogueEnemySpawnRegion()
{
    PrimaryActorTick.bCanEverTick = false;
    SpawnBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("SpawnBounds"));
    RootComponent = SpawnBounds;
    SpawnBounds->SetBoxExtent(FVector(6000.0f, 6000.0f, 1000.0f));
    SpawnBounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SpawnBounds->SetCollisionResponseToAllChannels(ECR_Ignore);
    SpawnBounds->SetGenerateOverlapEvents(false);
    SpawnBounds->SetCanEverAffectNavigation(false);
    SpawnBounds->SetHiddenInGame(true);
    SpawnBounds->ShapeColor = FColor::Green;
    SpawnBounds->SetLineThickness(3.0f);
    SpawnOutline = CreateDefaultSubobject<USplineComponent>(TEXT("SpawnOutline"));
    SpawnOutline->SetupAttachment(SpawnBounds);
    SpawnOutline->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SpawnOutline->SetGenerateOverlapEvents(false);
    SpawnOutline->SetCanEverAffectNavigation(false);
    SpawnOutline->SetHiddenInGame(true);
    SpawnOutline->ClearSplinePoints(false);
    for (const FVector& Point : { FVector(-3000,-3000,0), FVector(3000,-3000,0), FVector(3000,3000,0), FVector(-3000,3000,0) })
    {
        SpawnOutline->AddSplinePoint(Point, ESplineCoordinateSpace::Local, false);
        SpawnOutline->SetSplinePointType(SpawnOutline->GetNumberOfSplinePoints()-1, ESplinePointType::Linear, false);
    }
    SpawnOutline->SetClosedLoop(true, true);
    SpawnOutline->SetUnselectedSplineSegmentColor(FLinearColor(0.0f,0.8f,1.0f));
    SpawnOutline->SetSelectedSplineSegmentColor(FLinearColor(0.1f,1.0f,0.6f));
}

void ARogueEnemySpawnRegion::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    // 点位只在构造函数给默认值；重跑构造时保留关卡里编辑过的轮廓。
    for (int32 Index=0; Index<SpawnOutline->GetNumberOfSplinePoints(); ++Index)
    {
        SpawnOutline->SetSplinePointType(Index,ESplinePointType::Linear,false);
    }
    SpawnOutline->SetClosedLoop(true,true);
}

bool ARogueEnemySpawnRegion::CollectValidOutline(TArray<FVector2D>& OutVertices) const
{
    OutVertices.Reset();
    if (!SpawnOutline->IsClosedLoop() || SpawnOutline->GetNumberOfSplinePoints()<3) { return false; }
    for (int32 Index=0; Index<SpawnOutline->GetNumberOfSplinePoints(); ++Index)
    {
        const FVector Point=SpawnOutline->GetLocationAtSplinePoint(Index,ESplineCoordinateSpace::World);
        if (!FMath::IsFinite(Point.X) || !FMath::IsFinite(Point.Y)) { return false; }
        OutVertices.Add(FVector2D(Point.X,Point.Y));
    }
    const int32 Count=OutVertices.Num();
    double TwiceArea=0;
    for (int32 I=0; I<Count; ++I)
    {
        const FVector2D& A=OutVertices[I]; const FVector2D& B=OutVertices[(I+1)%Count];
        if (FVector2D::DistSquared(A,B)<0.0001) { return false; }
        TwiceArea+=FVector2D::CrossProduct(A,B);
        for (int32 J=I+1; J<Count; ++J)
        {
            if (J==I+1 || (I==0 && J==Count-1)) { continue; }
            if (EdgesIntersect(A,B,OutVertices[J],OutVertices[(J+1)%Count])) { return false; }
        }
    }
    return FMath::Abs(TwiceArea)>0.01;
}

bool ARogueEnemySpawnRegion::IsInsideSpawnOutline(const FVector& Point,float CapsuleRadius) const
{
    if (!bUseSpawnOutline) { return true; }
    TArray<FVector2D> Vertices;
    if (!CollectValidOutline(Vertices)) { return false; }
    const FVector2D P(Point.X,Point.Y);
    const double Margin=FMath::Max(0.0f,CapsuleRadius)+FMath::Max(0.0f,EdgePadding);
    bool bInside=false;
    for (int32 I=0,J=Vertices.Num()-1; I<Vertices.Num(); J=I++)
    {
        const FVector2D A=Vertices[J],B=Vertices[I],Edge=B-A;
        const double T=FMath::Clamp(FVector2D::DotProduct(P-A,Edge)/Edge.SizeSquared(),0.0,1.0);
        if (FVector2D::DistSquared(P,A+Edge*T)+0.0001<Margin*Margin) { return false; }
        if ((A.Y>P.Y)!=(B.Y>P.Y) && P.X<(B.X-A.X)*(P.Y-A.Y)/(B.Y-A.Y)+A.X) { bInside=!bInside; }
    }
    return bInside;
}

void ARogueEnemySpawnRegion::ConfigureGroundHeightRange(bool bEnabled, float MinLocalZ, float MaxLocalZ)
{
    bRestrictGroundHeight = bEnabled;
    GroundMinLocalZ = MinLocalZ;
    GroundMaxLocalZ = MaxLocalZ;
}

bool ARogueEnemySpawnRegion::IsGroundHeightAllowed(const FVector& GroundPoint) const
{
    if (!bRestrictGroundHeight) { return true; }
    if (!FMath::IsFinite(GroundMinLocalZ) || !FMath::IsFinite(GroundMaxLocalZ) || GroundMinLocalZ >= GroundMaxLocalZ) { return false; }
    const double LocalZ = SpawnBounds->GetComponentTransform().InverseTransformPosition(GroundPoint).Z;
    return LocalZ >= GroundMinLocalZ - 0.01 && LocalZ <= GroundMaxLocalZ + 0.01;
}

bool ARogueEnemySpawnRegion::ContainsCapsule(const FVector& Center, float Radius, float HalfHeight) const
{
    if (!FMath::IsFinite(Radius) || !FMath::IsFinite(HalfHeight) || Radius <= 0.0f || HalfHeight < Radius) { return false; }
    const FTransform Transform = SpawnBounds->GetComponentTransform();
    const FVector Scale = Transform.GetScale3D().GetAbs();
    if (Scale.GetMin() < KINDA_SMALL_NUMBER) { return false; }
    const FVector Local = Transform.InverseTransformPosition(Center);
    const FVector Extent = SpawnBounds->GetUnscaledBoxExtent();
    const FVector Axes[] = { FVector::ForwardVector, FVector::RightVector, FVector::UpVector };
    for (int32 Axis = 0; Axis < 3; ++Axis)
    {
        // 竖直胶囊在框各轴上的投影，允许框移动、旋转和非等比缩放。
        const float VerticalProjection = FMath::Abs(Transform.TransformVectorNoScale(Axes[Axis]).Z);
        const float Support = Radius + (HalfHeight - Radius) * VerticalProjection;
        const float Margin = Axis < 2 ? FMath::Max(0.0f, EdgePadding) : 0.0f;
        if (FMath::Abs(Local[Axis]) + (Support + Margin) / Scale[Axis] > Extent[Axis] + 0.01f) { return false; }
    }
    return IsInsideSpawnOutline(Center,Radius);
}

bool ARogueEnemySpawnRegion::ClipGroundTrace(const FVector& Candidate, float UpDistance, float DownDistance,
    FVector& OutStart, FVector& OutEnd) const
{
    if (!IsInsideSpawnOutline(Candidate,0.0f)) { return false; }
    const FTransform Transform = SpawnBounds->GetComponentTransform();
    if (Transform.GetScale3D().GetAbs().GetMin() < KINDA_SMALL_NUMBER) { return false; }
    const FVector Local = Transform.InverseTransformPosition(Candidate);
    const FVector Direction = Transform.InverseTransformVector(FVector::UpVector);
    const FVector Extent = SpawnBounds->GetUnscaledBoxExtent();
    FVector Min = -Extent, Max = Extent;
    if (bRestrictGroundHeight)
    {
        if (!FMath::IsFinite(GroundMinLocalZ) || !FMath::IsFinite(GroundMaxLocalZ) || GroundMinLocalZ >= GroundMaxLocalZ) { return false; }
        Min.Z = FMath::Max(Min.Z, static_cast<double>(GroundMinLocalZ));
        Max.Z = FMath::Min(Max.Z, static_cast<double>(GroundMaxLocalZ));
        if (Min.Z >= Max.Z) { return false; }
    }
    double Low = -FMath::Max(1.0f, DownDistance), High = FMath::Max(0.0f, UpDistance);
    for (int32 Axis = 0; Axis < 3; ++Axis)
    {
        if (FMath::Abs(Direction[Axis]) < SMALL_NUMBER)
        {
            if (Local[Axis] < Min[Axis] || Local[Axis] > Max[Axis]) { return false; }
            continue;
        }
        const double A = (Min[Axis] - Local[Axis]) / Direction[Axis];
        const double B = (Max[Axis] - Local[Axis]) / Direction[Axis];
        Low = FMath::Max(Low, FMath::Min(A, B));
        High = FMath::Min(High, FMath::Max(A, B));
        if (Low >= High) { return false; }
    }
    OutStart = Candidate + FVector::UpVector * High;
    OutEnd = Candidate + FVector::UpVector * Low;
    return true;
}

FVector ARogueEnemySpawnRegion::GetRandomCandidate() const
{
    if (bUseSpawnOutline)
    {
        TArray<FVector2D> Vertices;
        if (!CollectValidOutline(Vertices)) { return SpawnBounds->GetComponentLocation(); }
        FBox2D Bounds(ForceInit);
        for (const FVector2D& Vertex : Vertices) { Bounds+=Vertex; }
        return FVector(FMath::FRandRange(Bounds.Min.X,Bounds.Max.X),FMath::FRandRange(Bounds.Min.Y,Bounds.Max.Y),SpawnBounds->GetComponentLocation().Z);
    }
    const FVector Extent = SpawnBounds->GetUnscaledBoxExtent();
    return SpawnBounds->GetComponentTransform().TransformPosition(FVector(
        FMath::FRandRange(-Extent.X, Extent.X), FMath::FRandRange(-Extent.Y, Extent.Y), 0.0f));
}

void ARogueEnemySpawnRegion::GetWorldCorners(TArray<FVector>& OutCorners) const
{
    OutCorners.Reset(8);
    const FVector Extent = SpawnBounds->GetUnscaledBoxExtent();
    for (int32 X : {-1, 1}) { for (int32 Y : {-1, 1}) { for (int32 Z : {-1, 1})
    {
        OutCorners.Add(SpawnBounds->GetComponentTransform().TransformPosition(Extent * FVector(X, Y, Z)));
    } } }
}
