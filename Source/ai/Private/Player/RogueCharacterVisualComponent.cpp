#include "Player/RogueCharacterVisualComponent.h"
#include "Combat/Visuals/RogueGroundEffectFade.h"
#include "Player/RogueFloatingComponent.h"
#include "Player/RogueCharacter.h"
#include "Combat/Targeting/RogueEnemyTrackerSubsystem.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
URogueCharacterVisualComponent::URogueCharacterVisualComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;
    PrimaryComponentTick.TickInterval = .08f;
}
void URogueCharacterVisualComponent::BeginPlay()
{
    Super::BeginPlay();
    auto* Player = Cast<ARogueCharacter>(GetOwner());
    if (!Player || !FocusMarkerMaterial || GetNetMode() == NM_DedicatedServer) return;
    UStaticMesh* Plane = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
    if (!Plane) return;
    const auto MakeMarker = [&](FName Name, float Size, int32 SortPriority)
    {
        auto* Marker = NewObject<UStaticMeshComponent>(Player, Name);
        Marker->SetupAttachment(Player->GetCapsuleComponent());
        Marker->SetAbsolute(false, true, false);
        Marker->SetStaticMesh(Plane);
        Marker->SetMaterial(0, FocusMarkerMaterial);
        Marker->SetRelativeScale3D(FVector(Size, Size, 1));
        Marker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Marker->SetGenerateOverlapEvents(false);
        Marker->SetCanEverAffectNavigation(false);
        Marker->SetCastShadow(false);
        Marker->SetTranslucentSortPriority(SortPriority);
        Marker->SetVisibility(false);
        Marker->RegisterComponent();
        return Marker;
    };
    FocusMarker = MakeMarker(TEXT("PlayerFocusMarker"), 2.35f, -3);
    FocusCrest = MakeMarker(TEXT("PlayerFocusCrest"), 1.25f, 4);
    SetComponentTickEnabled(true);
}

void URogueCharacterVisualComponent::TickComponent(float DeltaSeconds, ELevelTick TickType, FActorComponentTickFunction* TickFunction)
{
    Super::TickComponent(DeltaSeconds, TickType, TickFunction);
    const auto* Player = Cast<ARogueCharacter>(GetOwner());
    const auto* Local = GetWorld()->GetFirstPlayerController();
    if (!Player || !Local || !Local->IsLocalController() || !Local->GetPawn() || !Player->IsAlive())
    {
        ARogueGroundEffectFade::CaptureComponents(GetWorld(),{FocusMarker.Get(),FocusCrest.Get()});
        FocusMarker->SetVisibility(false);
        FocusCrest->SetVisibility(false);
        return;
    }
    const FVector Feet = Player->GetActorLocation() - FVector(0, 0, Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
    FHitResult Ground;
    const bool bGrounded = GetWorld()->LineTraceSingleByObjectType(Ground, Feet + FVector(0, 0, 24), Feet - FVector(0, 0, 500),
        FCollisionObjectQueryParams(ECC_WorldStatic), FCollisionQueryParams(SCENE_QUERY_STAT(PlayerFocusGround), false, Player));
    // Some arenas have decorative paving above their simplified collision. The material allows shallow occlusion only.
    FocusMarker->SetVisibility(true);
    FocusMarker->SetWorldLocation((bGrounded ? Ground.ImpactPoint : Feet) + FVector(0, 0, 7));
    if (const auto* Camera = Local->PlayerCameraManager.Get())
    {
        const FVector Crest = Player->GetActorLocation() + FVector(0, 0, 139);
        // Plane 的 V 轴对应局部 Y。将它对齐镜头上方向，让头标保持竖直并向下指向玩家。
        // 使用完整镜头基向量，避免 LookAt 叠加固定旋转后将平面 UV 转歪。
        const FQuat CameraRotation = Camera->GetCameraRotation().Quaternion();
        const FQuat Facing = FRotationMatrix::MakeFromYZ(
            CameraRotation.GetUpVector(), -CameraRotation.GetForwardVector()).ToQuat();
        FocusCrest->SetWorldLocationAndRotation(Crest, Facing);
        FocusCrest->SetVisibility(true);
    }
    const bool bSelf = Local->GetPawn() == Player;
    const auto* Tracker = GetWorld()->GetSubsystem<URogueEnemyTrackerSubsystem>();
    const int32 Style = bSelf ? (Tracker && Tracker->ShouldCullCombatEffects() ? 2 : 0) : 1;
    if (MarkerStyle != Style)
    {
        MarkerStyle = Style;
        FocusMarker->SetCustomPrimitiveDataFloat(0, float(Style));
        FocusCrest->SetCustomPrimitiveDataFloat(0, float(Style + 3));
    }
}

void URogueCharacterVisualComponent::SetFloatingEnabled(bool bEnabled)
{
    if (GetOwner())
    {
        if (URogueFloatingComponent* Floating=GetOwner()->FindComponentByClass<URogueFloatingComponent>()) { Floating->SetFloatingEnabled(bEnabled); }
    }
}
