#include "Player/RoguePlayerMotionComponent.h"
#include "Player/RogueCharacter.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/RogueGameplayTags.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Framework/RogueGameState.h"
#include "Combat/Pooling/RogueCombatPoolSubsystem.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

namespace RoguePlayerMotion
{
    constexpr float CastRiseTime = .12f;
    constexpr float CastDuration = .50f;
    constexpr float CastMinimumInterval = .70f;
}

URoguePlayerMotionComponent::URoguePlayerMotionComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    // 玩家只有少量外观节点，逐帧更新可避免动作在高帧率下仍按 30 Hz 跳变。
    PrimaryComponentTick.TickInterval = 0.f;
    PrimaryComponentTick.bAllowTickOnDedicatedServer=false;PrimaryComponentTick.TickGroup=TG_PostPhysics;
    SetIsReplicatedByDefault(false);
}
void URoguePlayerMotionComponent::BeginPlay()
{
    Super::BeginPlay();auto* Player=Cast<ARogueCharacter>(GetOwner());if(!Player)return;
    for(auto* Mesh:TInlineComponentArray<UStaticMeshComponent*>(Player))
    {
        if(!Mesh->GetStaticMesh()||Mesh->ComponentHasTag(TEXT("Rogue.NoVisualMotion")))continue;
        bool bUnderVisuals=false,bMeshParent=false;
        for(auto* Parent=Mesh->GetAttachParent();Parent;Parent=Parent->GetAttachParent())
        {if(Parent->GetFName()==TEXT("Visuals"))bUnderVisuals=true;if(Cast<UStaticMeshComponent>(Parent))bMeshParent=true;}
        if (bUnderVisuals && !bMeshParent)
        {
            const bool bSecondary = Mesh->ComponentHasTag(TEXT("Rogue.SecondaryMotion"));
            Poses.Add({Mesh, Mesh->GetRelativeLocation(), Mesh->GetRelativeRotation().Quaternion(), bSecondary});
            if (!bSecondary) Mesh->SetCustomPrimitiveDataFloat(25, 1.f);
        }
    }
}
void URoguePlayerMotionComponent::ResetPose()
{
    if(bApplied)for(const auto& P:Poses)if(auto* M=P.Mesh.Get())M->SetRelativeLocationAndRotation(P.Location,P.Rotation,false,nullptr,ETeleportType::TeleportPhysics);
    for (const auto& Pose : Poses)
        if (auto* Mesh = Pose.Mesh.Get(); Mesh && !Pose.bSecondary) Mesh->SetCustomPrimitiveDataFloat(24, 0.f);
    bApplied=false;Velocity=FVector::ZeroVector;CastAge=HitAge=10;DashAmount=0;LastHealth=-1;
    DashReleaseAge = 1.f;
    DownedAmount = VictoryAge = 0.f;
    bWasDashing = bRemoteDashing = bWasFalling = false;
    LastFallVelocityZ = 0.f;
}
void URoguePlayerMotionComponent::PlayCastCue()
{
    // 多把武器、连发和持续光束会密集触发。完整收势后再响应，避免中途归零产生抽动。
    if (bEnableMotion && bEnableCastMotion && GetNetMode() != NM_DedicatedServer &&
        CastAge >= RoguePlayerMotion::CastMinimumInterval)
    {
        CastAge = 0.f;
    }
}
void URoguePlayerMotionComponent::PlayDashPresentation(bool bStarting, const FVector& Position, const FVector& Direction)
{
    bRemoteDashing = bStarting;
    RemoteDashUntil = GetWorld()->GetTimeSeconds() + 1.f;
    if (auto* Pool = GetWorld()->GetSubsystem<URogueCombatPoolSubsystem>())
        Pool->PlayCombatAccent(bStarting ? DashLaunch : DashBrake, Position,
            bStarting ? Direction : -Direction, !bStarting);
}

void URoguePlayerMotionComponent::TickComponent(float Delta,ELevelTick Type,FActorComponentTickFunction* Fn)
{
    Super::TickComponent(Delta,Type,Fn);auto* P=Cast<ARogueCharacter>(GetOwner());
    if (!P || !bEnableMotion) { ResetPose(); return; }

    const float Dt = FMath::Clamp(Delta, 0.f, .1f);
    CastAge += Dt;
    HitAge += Dt;
    Phase += Dt * 3.f;
    const bool bAlive = P->IsAlive();
    const float Health = P->GetCurrentHealth();
    if (bAlive && LastHealth >= 0 && Health < LastHealth - .01f && HitAge >= .12f) HitAge = 0;
    LastHealth = bAlive ? Health : -1.f;

    const auto* State = GetWorld()->GetGameState<ARogueGameState>();
    const bool bVictory = bAlive && State && State->GetRunResult().Outcome == ERogueRunOutcome::Victory;
    const auto* ASC = P->GetAbilitySystemComponent();
    const bool bRemoteDash = bRemoteDashing && GetWorld()->GetTimeSeconds() < RemoteDashUntil;
    const bool bDashing = bAlive && !bVictory &&
        (bRemoteDash || (ASC && ASC->HasMatchingGameplayTag(RogueGameplayTags::State_Dashing)));
    const bool bFalling = bAlive && P->GetCharacterMovement()->IsFalling();
    if (bWasFalling && !bFalling && bAlive && !bVictory && !bDashing && LastFallVelocityZ < -270.f)
    {
        const FVector Feet = P->GetActorLocation() -
            FVector(0, 0, P->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() - 6.f);
        if (auto* Pool = GetWorld()->GetSubsystem<URogueCombatPoolSubsystem>())
            Pool->PlayCombatAccent(LandingFlakes, Feet, P->GetActorForwardVector(), true);
    }
    bWasFalling = bFalling;
    LastFallVelocityZ = bFalling ? P->GetVelocity().Z : 0.f;
    if (bWasDashing && !bDashing) DashReleaseAge = 0.f;
    DashReleaseAge += Dt;
    bWasDashing = bDashing;
    DashAmount = FMath::FInterpTo(DashAmount, bDashing ? 1.f : 0.f, Dt, bDashing ? 24.f : 13.f);
    DownedAmount = FMath::FInterpTo(DownedAmount, bAlive ? 0.f : 1.f, Dt, 7.f);
    VictoryAge = bVictory ? VictoryAge + Dt : 0.f;
    Velocity = FMath::VInterpTo(Velocity, bAlive && !bVictory ? P->GetVelocity() : FVector::ZeroVector, Dt, 8.f);
    const FVector Local = P->GetActorQuat().UnrotateVector(Velocity) / 600.f;

    // 单方向抬起后平滑归位，不再叠加快速反向摆动。攻击伤害和射速仍由原战斗逻辑控制。
    const float CastEnvelope = FMath::SmoothStep(0.f, RoguePlayerMotion::CastRiseTime, CastAge) *
        (1.f - FMath::SmoothStep(RoguePlayerMotion::CastRiseTime, RoguePlayerMotion::CastDuration, CastAge));
    const float Cast = bEnableCastMotion ? CastEnvelope * FMath::Clamp(CastMotionStrength, 0.f, 2.f) * (1.f - DashAmount) : 0.f;
    const float Hit = HitAge < .24f ? FMath::Sin(PI * HitAge / .24f) : 0.f;
    const float Brake = !bDashing && DashReleaseAge < .29f ? FMath::Sin(PI * DashReleaseAge / .29f) : 0.f;
    const float Cheer = bVictory ? FMath::SmoothStep(0.f, .25f, VictoryAge) : 0.f;
    const float CheerLift = bVictory && VictoryAge < .66f ? FMath::Sin(PI * VictoryAge / .66f) : 0.f;
    const float Active = (1.f - DownedAmount) * (1.f - Cheer);
    const float Pitch = Active * (-FMath::Clamp(Local.X, -1.f, 1.f) * MoveLean - DashAmount * DashLean +
        1.2f * Cast + 5.f * Brake) - 11.f * DownedAmount + 8.f * Cheer;
    const float Roll = Active * (-FMath::Clamp(Local.Y, -1.f, 1.f) * MoveLean + 3.5f * Hit) + 18.f * DownedAmount - 6.f * Cheer;
    const float Height = Active * (.8f * Cast - 5.f * Brake) - 18.f * DownedAmount + 9.f * Cheer + 12.f * CheerLift;

    // 24/25 驱动共享面部材质。三个眼部材质使用同一进度，避免眼白和瞳孔错开。
    const float BlinkClock = FMath::Fmod(Phase / 3.f, 3.8f);
    const float Blink = 1.f - FMath::SmoothStep(.03f, .13f, FMath::Abs(BlinkClock - .8f));
    for (const auto& Pose : Poses)
    {
        auto* Mesh = Pose.Mesh.Get();
        if (!Mesh) continue;
        if (!Pose.bSecondary) Mesh->SetCustomPrimitiveDataFloat(24, Blink);
        const float Follow = Pose.bSecondary ? AccessoryFollow : 0.f;
        const float Drag = Follow * Active * (DashAmount - .75f * Brake - .10f * Cast);
        const float BroomSway = Follow * Active * FMath::Sin(Phase - .8f);
        const FQuat Lean = FRotator(Pitch + Drag + .32f * BroomSway, -.55f * BroomSway,
            Roll + .5f * Drag - Follow * .4f * FMath::Clamp(Local.Y, -1.f, 1.f)).Quaternion();
        const FVector Offset(Active * (-1.5f * Hit), 0.f,
            Height + Follow * .3f * FMath::Sin(Phase - .8f) * Local.Size2D());
        Mesh->SetRelativeLocationAndRotation(Pose.Location + Offset, Lean * Pose.Rotation, false, nullptr, ETeleportType::TeleportPhysics);
    }
    bApplied = true;
}
void URoguePlayerMotionComponent::EndPlay(const EEndPlayReason::Type Reason)
{ResetPose();Super::EndPlay(Reason);}
