#include "UI/RogueHUD.h"
#include "RogueHUDPrivate.h"
#include "Framework/RogueGameState.h"
#include "Player/RogueCharacter.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/Canvas.h"
#include "GameFramework/PlayerController.h"

void ARogueHUD::DrawRemainingEnemyLocations()
{
    const auto* State = GetRogueGameState();
    const auto* Player = RogueHUDPrivate::GetLocalPlayerCharacter(this);
    const auto* PC = GetOwningPlayerController();
    if (!Canvas || !State || !PC || !PC->PlayerCameraManager || !Player || !Player->IsAlive()
        || State->IsGameOver() || IsCoveringBattleFeedback()) return;
    const auto& View = State->GetEnemyLocatorView();
    if (View.Remaining <= 0 || View.Remaining > 5) return;
    const float Scale = FMath::Clamp(Canvas->SizeY / 810.f, .65f, 1.6f);
    const FVector2D Center(Canvas->SizeX * .5f, Canvas->SizeY * .5f);
    const FVector2D Limit = Center - FVector2D(48, 125) * Scale;
    if (Limit.X <= 0 || Limit.Y <= 0) return;
    const FRotator CameraRotation = PC->PlayerCameraManager->GetCameraRotation();
    const FVector CameraLocation = PC->PlayerCameraManager->GetCameraLocation();
    const FRotationMatrix Basis(CameraRotation);
    const float Pulse = .5f + .5f * FMath::Sin(GetWorld()->GetTimeSeconds() * 4.f);
    const FLinearColor Gold(1.f,.78f,.25f,.75f + .2f * Pulse);
    const FLinearColor Shadow(.025f,.035f,.065f,.9f);
    auto Stroke = [&](FVector2D A, FVector2D B)
    {
        DrawLine(A.X,A.Y,B.X,B.Y,Shadow,5.f*Scale);
        DrawLine(A.X,A.Y,B.X,B.Y,Gold,2.f*Scale);
    };
    for (const auto& Location : View.Locations)
    {
        const FVector Target = FVector(Location) + FVector(0,0,100);
        const FVector Projected = Canvas->Project(Target);
        if (Projected.ContainsNaN()) continue;
        FVector2D Point(Projected.X,Projected.Y);
        const bool bOnscreen = Projected.Z > 0 && FMath::Abs(Point.X-Center.X) < Limit.X && FMath::Abs(Point.Y-Center.Y) < Limit.Y;
        if (bOnscreen)
        {
            const float Radius = (13.f + 3.f * Pulse) * Scale;
            const FVector2D Top = Point + FVector2D(0,-Radius), Bottom = Point + FVector2D(0,Radius);
            const FVector2D Left = Point + FVector2D(-Radius,0), Right = Point + FVector2D(Radius,0);
            Stroke(Top,Right);Stroke(Right,Bottom);Stroke(Bottom,Left);Stroke(Left,Top);
        }
        else
        {
            FVector2D Direction = Point-Center;
            if (Projected.Z <= 0)
            {
                const FVector Delta = Target-CameraLocation;
                Direction = FVector2D(FVector::DotProduct(Delta,Basis.GetUnitAxis(EAxis::Y)), -FVector::DotProduct(Delta,Basis.GetUnitAxis(EAxis::Z)));
            }
            if (!Direction.Normalize()) Direction = FVector2D(0,-1);
            const float Reach = 1.f / FMath::Max(FMath::Abs(Direction.X)/Limit.X,FMath::Abs(Direction.Y)/Limit.Y);
            Point = Center + Direction*Reach;
            const FVector2D Side(-Direction.Y,Direction.X);
            Stroke(Point-Direction*14.f*Scale+Side*8.f*Scale,Point);
            Stroke(Point,Point-Direction*14.f*Scale-Side*8.f*Scale);
        }
    }
}
