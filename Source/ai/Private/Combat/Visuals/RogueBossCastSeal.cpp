#include "Combat/Visuals/RogueBossCastSeal.h"
#include "Combat/Visuals/RogueGroundEffectFade.h"

#include "AbilitySystem/Abilities/RogueBossAttackAbilities.h"
#include "Combat/Projectiles/RogueBossAttackActor.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Enemies/RogueEnemy.h"
#include "Engine/StaticMesh.h"
#include "Framework/RogueGameState.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Player/RogueCharacter.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
int32 ResolveSealStyle(const FRogueBossSkillRow* Skill)
{
    if (!Skill || !Skill->AbilityClass) return 0;
    const UClass* Ability = Skill->AbilityClass.Get();
    if (Ability->IsChildOf<URogueBossOctagramAbility>()) return 1;
    if (Ability->IsChildOf<URogueBossEndlessRingAbility>()) return 2;
    if (Ability->IsChildOf<URogueBossFlameCrescentsAbility>()) return 3;
    if (Ability->IsChildOf<URogueBossFrostPursuitAbility>()) return 4;
    if (Ability->IsChildOf<URogueBossLastBloomAbility>()) return 5;
    if (Ability->IsChildOf<URogueBossDiamondReversalAbility>()) return 6;
    if (Ability->IsChildOf<URogueBossScarletRainAbility>()) return 7;
    if (Ability->IsChildOf<URogueBossJudgmentRayAbility>()) return 8;
    return 0;
}
}

ARogueBossCastSeal::ARogueBossCastSeal()
{
    bReplicates = false;
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;
    SetActorEnableCollision(false);
    SetActorHiddenInGame(true);

    Seal = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Seal"));
    SetRootComponent(Seal);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Plane(TEXT("/Engine/BasicShapes/Plane.Plane"));
    Seal->SetStaticMesh(Plane.Object);
    Seal->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Seal->SetCastShadow(false);
    Seal->SetCanEverAffectNavigation(false);
    Seal->SetTranslucentSortPriority(3);

    Crown = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Crown"));
    Crown->SetupAttachment(Seal);
    Crown->SetStaticMesh(Plane.Object);
    Crown->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Crown->SetCastShadow(false);
    Crown->SetCanEverAffectNavigation(false);
    Crown->SetTranslucentSortPriority(1);
}

float ARogueBossCastSeal::ServerTime() const
{
    const UWorld* World = GetWorld();
    const auto* State = World->GetGameState<ARogueGameState>();
    return State ? State->GetServerWorldTimeSeconds() : World->GetTimeSeconds();
}

void ARogueBossCastSeal::ApplyState(const FRogueBossSkillState& State, UMaterialInterface* Material)
{
    if (State.Phase == ERogueBossSkillPhase::Idle || !Material)
    {
        HideSeal();
        return;
    }

    auto* Enemy = Cast<ARogueEnemy>(GetOwner());
    if (!Enemy) return;
    if (!SealMaterial)
    {
        SealMaterial = UMaterialInstanceDynamic::Create(Material, this);
        Seal->SetMaterial(0, SealMaterial);
        CrownMaterial = UMaterialInstanceDynamic::Create(Material, this);
        CrownMaterial->SetScalarParameterValue(TEXT("Stage"), 1.f);
        Crown->SetMaterial(0, CrownMaterial);
    }
    if (State.CastId != CastState.CastId || State.Generation != CastState.Generation)
    {
        const auto* Row = State.Skill.GetRow<FRogueBossSkillRow>(TEXT("Boss cast seal"));
        Style = ResolveSealStyle(Row);
        Spread = Row ? FMath::DegreesToRadians(Row->Attack.SpreadDegrees) : 0.f;
        const UCapsuleComponent* Capsule = Enemy->GetCapsuleComponent();
        CastRadius = FMath::Max(460.f, Capsule->GetScaledCapsuleRadius() * 4.1f);
        if (Style == 3) CastRadius = FMath::Max(CastRadius, 535.f);
        Ground = Enemy->GetActorLocation() - FVector(0, 0, Capsule->GetScaledCapsuleHalfHeight());
        FVector HitGround;
        bHasGround = ARogueBossAttackActor::FindGround(GetWorld(), Ground, Enemy, HitGround);
        if (bHasGround) Ground = HitGround;
        StartServerTime = State.PhaseStartServerTime;
        SealMaterial->SetScalarParameterValue(TEXT("Style"), Style);
        SealMaterial->SetScalarParameterValue(TEXT("Spread"), Spread);
        CrownMaterial->SetScalarParameterValue(TEXT("Style"), Style);
        CrownMaterial->SetScalarParameterValue(TEXT("Spread"), Spread);
    }

    CastState = State;
    SetActorHiddenInGame(false);
    SetActorTickEnabled(true);
    Tick(0.f);
}

void ARogueBossCastSeal::HideSeal()
{
    ARogueGroundEffectFade::CaptureActor(this);
    SetActorHiddenInGame(true);
    SetActorTickEnabled(false);
    CastState = {};
}

void ARogueBossCastSeal::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    const auto* Enemy = Cast<ARogueEnemy>(GetOwner());
    const auto* GameState = GetWorld()->GetGameState<ARogueGameState>();
    if (!Enemy || Enemy->IsDead() || Enemy->IsAvailableInPool() || !Enemy->IsBoss()
        || Enemy->GetPresentationGeneration() != CastState.Generation || (GameState && GameState->IsGameOver()))
    {
        HideSeal();
        return;
    }

    const float Now = ServerTime();
    const float PhaseAge = FMath::Max(0.f, Now - CastState.PhaseStartServerTime);
    const float PhaseLength = FMath::Max(.05f, CastState.PhaseEndServerTime - CastState.PhaseStartServerTime);
    const float Progress = FMath::Clamp(PhaseAge / PhaseLength, 0.f, 1.f);
    const bool bRecovering = CastState.Phase == ERogueBossSkillPhase::Recovery;
    if (bRecovering)
    {
        // 收招立即停止本体，由独立残影渐隐；保留 CastId，防止迟到快照重播。
        ARogueGroundEffectFade::CaptureActor(this);
        SetActorHiddenInGame(true);
        SetActorTickEnabled(false);
        return;
    }

    const float Age = FMath::Max(0.f, Now - StartServerTime);
    const float Pulse = CastState.PulseIndex > 0 ? FMath::Exp(-PhaseAge * 14.f) : 0.f;
    float Yaw = Style == 1 ? Enemy->GetActorRotation().Yaw : 0.f;
    if (Style == 3)
    {
        const FVector Aim = IsValid(CastState.Target) ? CastState.Target->GetActorLocation() : CastState.TargetLocation;
        Yaw = (Aim - Enemy->GetActorLocation()).Rotation().Yaw;
    }
    FVector Position = Enemy->GetActorLocation();
    Position.Z = Ground.Z + (bHasGround ? 9.f : 12.f);
    const float Size = CastRadius * (.93f + .07f * FMath::SmoothStep(0.f, .24f, Age));
    SetActorLocationAndRotation(Position, FRotator(0, Yaw, 0));
    SetActorScale3D(FVector(Size / 50.f, Size / 50.f, 1.f));

    const float Fade = bRecovering ? 1.f - FMath::SmoothStep(0.f, FMath::Min(.32f, PhaseLength), PhaseAge) : 1.f;
    SealMaterial->SetScalarParameterValue(TEXT("EffectAge"), Age);
    SealMaterial->SetScalarParameterValue(TEXT("Progress"), Progress);
    SealMaterial->SetScalarParameterValue(TEXT("Pulse"), Pulse);
    SealMaterial->SetScalarParameterValue(TEXT("Opacity"), Fade * FMath::SmoothStep(0.f, .11f, Age) * (Style==3 ? .55f : 1.f));

    // 每端面向本地镜头放置开口符阵，中心留空，让 Boss 的身体和动作仍然清晰。
    FVector CameraLocation;
    FRotator CameraRotation;
    FVector Facing(0, -1, 1);
    FVector CrownCenter(Position.X, Position.Y, Ground.Z + Size * .94f);
    if (const auto* Controller = GetWorld()->GetFirstPlayerController())
    {
        Controller->GetPlayerViewPoint(CameraLocation, CameraRotation);
        Facing = (CameraLocation - CrownCenter).GetSafeNormal();
    }
    const float CrownSize = Size * (.86f + .11f * Pulse) * (Style==3 ? .88f : 1.f);
    Crown->SetWorldLocationAndRotation(CrownCenter - Facing * 85.f, FRotationMatrix::MakeFromZ(Facing).Rotator());
    Crown->SetWorldScale3D(FVector(CrownSize / 50.f));
    CrownMaterial->SetScalarParameterValue(TEXT("EffectAge"), Age);
    CrownMaterial->SetScalarParameterValue(TEXT("Progress"), Progress);
    CrownMaterial->SetScalarParameterValue(TEXT("Pulse"), Pulse);
    CrownMaterial->SetScalarParameterValue(TEXT("Opacity"), Fade * FMath::SmoothStep(.05f, .36f, Age) * (Style==3 ? .42f : .82f));
}
