#include "Enemies/Components/RogueEnemyPresentationComponent.h"
#include "Enemies/RogueEnemy.h"
#include "Enemies/Components/RogueEnemySupportComponent.h"
#include "Combat/Targeting/RogueEnemyTrackerSubsystem.h"
#include "Player/RogueCharacter.h"
#include "Combat/Pooling/RogueCombatPoolSubsystem.h"
#include "Combat/Visuals/RogueHitFlashComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Enemies/Visuals/RogueEnemyEffectVisual.h"
#include "Components/MeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
URogueEnemyPresentationComponent::URogueEnemyPresentationComponent()
{ PrimaryComponentTick.bCanEverTick=true;PrimaryComponentTick.bStartWithTickEnabled=false;PrimaryComponentTick.TickInterval=.05f; }
ARogueEnemyEffectVisual* URogueEnemyPresentationComponent::EnsureVisual(TObjectPtr<ARogueEnemyEffectVisual>& V,TSubclassOf<ARogueEnemyEffectVisual> C)
{
    if (!V&&C&&GetNetMode()!=NM_DedicatedServer)
    { FActorSpawnParameters P;P.Owner=GetOwner();P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;V=GetWorld()->SpawnActor<ARogueEnemyEffectVisual>(C,FTransform(),P); }
    return V;
}
void URogueEnemyPresentationComponent::ResetForPool()
{
    RestoreBossSurfaces();
    SetCrowdShade(false);
    RestoreVisualPose();Poses.Reset();SmoothedVelocity=FVector::ZeroVector;SmoothedTurn=0;
    LanternLift=0.f;
    AttackAge=HitAge=10.f;MotionPhase=0;PreviousYaw=0;LastBossCastTime=-1.e30;
    for (const auto& Item:Scales) if (auto* M=Item.Mesh.Get()) { M->SetRelativeScale3D(Item.Value); }
    Scales.Reset();bScaled=false;Generation=MAX_uint32;
    if (BossVisual) BossVisual->HideVisual();
    if (FlyingVisual) FlyingVisual->HideVisual();
    SetComponentTickEnabled(false);
}
void URogueEnemyPresentationComponent::RefreshRuntime()
{ if(GetNetMode()!=NM_DedicatedServer){SetComponentTickEnabled(true);TickComponent(0,LEVELTICK_All,nullptr);} }
void URogueEnemyPresentationComponent::TickComponent(float D,ELevelTick T,FActorComponentTickFunction* F)
{
    Super::TickComponent(D,T,F);
    auto* E=Cast<ARogueEnemy>(GetOwner());
    if (!E||E->IsAvailableInPool()||E->IsDead()) { ResetForPool();return; }
    if (Generation!=E->GetPresentationGeneration())
    {
        ResetForPool();Generation=E->GetPresentationGeneration();SetComponentTickEnabled(true);
        PreviousYaw=E->GetActorRotation().Yaw;
        // Independent cosmetic phase; never consume the gameplay random stream.
        MotionPhase=(GetTypeHash(E->GetUniqueID())%997)*(.0063f);
        bSkyfallPose = E->GetEnemyType() == ERogueEnemyType::Marksman;
        CacheVisualRoots();
    }
    if (E->IsBoss()&&!bScaled)
    {
        for (auto* M:TInlineComponentArray<UMeshComponent*>(E))
        {
            // 只将上一版外观加倍，碰撞胶囊与攻击法阵保持原尺寸。
            if (M->ComponentHasTag(TEXT("Rogue.DamageArea"))) continue;
            bool HasMeshParent=false;
            for (auto* P=M->GetAttachParent();P;P=P->GetAttachParent())
                if (Cast<UMeshComponent>(P)) { HasMeshParent=true;break; }
            if (HasMeshParent) continue;
            const float Prior=M->GetFName()==TEXT("ShockPillarMesh") ? 1.f : FMath::Clamp(BossModelScale,1.f,3.f);
            Scales.Add({M,M->GetRelativeScale3D()});
            M->SetRelativeScale3D(M->GetRelativeScale3D()*(2.f*Prior));
        }
        ApplyBossSurfaces();
        bScaled=true;
    }
    const auto* Tracker = GetWorld()->GetSubsystem<URogueEnemyTrackerSubsystem>();
    const auto* PC = GetWorld()->GetFirstPlayerController();
    const auto* Player = PC && PC->IsLocalController() ? Cast<ARogueCharacter>(PC->GetPawn()) : nullptr;
    const float ShadeRadius = bCrowdShaded ? 315.f : 260.f;
    SetCrowdShade(!E->IsBoss() && Player && Player->IsAlive() && Tracker && Tracker->ShouldCullCombatEffects()
        && FVector::DistSquared2D(Player->GetActorLocation(), E->GetActorLocation()) < FMath::Square(ShadeRadius));
    UpdateVisualMotion(D);
    const FVector Pos=E->GetActorLocation();
    const float Half=E->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    if (E->IsBoss())
    {
        float Top=Pos.Z+Half;
        for (auto* M:TInlineComponentArray<UMeshComponent*>(E))
            if (M->IsVisible()&&!M->bHiddenInGame) Top=FMath::Max(Top,float(M->Bounds.Origin.Z+M->Bounds.BoxExtent.Z));
        if (auto* V=EnsureVisual(BossVisual,BossVisualClass))
            V->ConfigureMarker(FVector(Pos.X,Pos.Y,Pos.Z-Half+5),FMath::Max(120.f,E->GetCapsuleComponent()->GetScaledCapsuleRadius()*2),Top+65);
    }
    else if (BossVisual) BossVisual->HideVisual();
    if (RogueEnemyArchetypes::IsFlyingType(E->GetEnemyType()))
    {
        if (auto* V=EnsureVisual(FlyingVisual,FlyingVisualClass))
            V->ConfigureCircle(FVector(Pos.X,Pos.Y,E->GetSpawnGroundZ()-Half+5),65);
    }
    else if (FlyingVisual) FlyingVisual->HideVisual();
}
void URogueEnemyPresentationComponent::EndPlay(const EEndPlayReason::Type R)
{
    RestoreBossSurfaces();
    if (BossVisual) BossVisual->Destroy();
    if (FlyingVisual) FlyingVisual->Destroy();
    Super::EndPlay(R);
}

void URogueEnemyPresentationComponent::SetCrowdShade(bool bEnable)
{
    if (bCrowdShaded == bEnable) return;
    bCrowdShaded = bEnable;
    for (auto* Mesh : TInlineComponentArray<UMeshComponent*>(GetOwner()))
        if (!Mesh->ComponentHasTag(TEXT("Rogue.RoleAccessory")) && !Mesh->ComponentHasTag(TEXT("Rogue.DamageArea")))
            Mesh->SetCustomPrimitiveDataFloat(18, bEnable ? 1.f : 0.f);
}

void URogueEnemyPresentationComponent::ApplyBossSurfaces()
{
    if (!BossSurfaceMaterial || !GetOwner()) return;
    for (UMeshComponent* Mesh : TInlineComponentArray<UMeshComponent*>(GetOwner()))
    {
        if (!Mesh->IsVisible() || Mesh->bHiddenInGame || Mesh->ComponentHasTag(TEXT("Rogue.NoBossSurface")) ||
            Mesh->ComponentHasTag(TEXT("Rogue.DamageArea")) || Mesh->ComponentHasTag(TEXT("Rogue.RoleAccessory")) ||
            Mesh->ComponentHasTag(TEXT("Rogue.SecondaryMotion"))) continue;

        auto* StaticMesh = Cast<UStaticMeshComponent>(Mesh);
        const auto* SkeletalMesh = Cast<USkeletalMeshComponent>(Mesh);
        if ((!StaticMesh || !StaticMesh->GetStaticMesh()) && (!SkeletalMesh || !SkeletalMesh->GetSkeletalMeshAsset())) continue;

        auto& Snapshot = BossSurfaces.AddDefaulted_GetRef();
        Snapshot.Mesh = Mesh;
        Snapshot.PreviousOverlay.Reset(Mesh->GetOverlayMaterial());
        Snapshot.bForcedRaster = StaticMesh && !StaticMesh->bForceDisableNanite;
        if (Snapshot.bForcedRaster) StaticMesh->SetForceDisableNanite(true);
        Mesh->SetCustomPrimitiveDataFloat(20, -1000.f);
        Mesh->SetOverlayMaterial(BossSurfaceMaterial);
    }
}

void URogueEnemyPresentationComponent::RestoreBossSurfaces()
{
    if (BossSurfaces.IsEmpty()) return;
    // A hit flash can temporarily occupy the overlay slot. Restore its baseline before recycling the Boss.
    if (auto* Flash = GetOwner()->FindComponentByClass<URogueHitFlashComponent>()) Flash->ClearHitFlash();
    for (const auto& Snapshot : BossSurfaces)
    {
        auto* Mesh = Snapshot.Mesh.Get();
        if (!Mesh || Mesh->GetOverlayMaterial() != BossSurfaceMaterial) continue;
        Mesh->SetCustomPrimitiveDataFloat(20, -1000.f);
        Mesh->SetOverlayMaterial(Snapshot.PreviousOverlay.Get());
        if (Snapshot.bForcedRaster)
            if (auto* StaticMesh = Cast<UStaticMeshComponent>(Mesh)) StaticMesh->SetForceDisableNanite(false);
    }
    BossSurfaces.Reset();
}

void URogueEnemyPresentationComponent::CacheVisualRoots()
{
    auto* E=Cast<ARogueEnemy>(GetOwner());if(!E)return;
    for(auto* M:TInlineComponentArray<UMeshComponent*>(E))
    {
        // Leave skeletal animation/network smoothing in charge of rigged meshes.
        const auto* Static=Cast<UStaticMeshComponent>(M);if(!Static||!Static->GetStaticMesh())continue;
        if(M->GetFName()==TEXT("ShockPillarMesh")||M->ComponentHasTag(TEXT("Rogue.DamageArea"))||M->ComponentHasTag(TEXT("Rogue.NoVisualMotion")))continue;
        bool ParentMoves=false;
        for(auto* P=M->GetAttachParent();P;P=P->GetAttachParent())
            if(Cast<UMeshComponent>(P)){ParentMoves=true;break;}
        if(!ParentMoves)Poses.Add({M,M->GetRelativeLocation(),M->GetRelativeRotation().Quaternion()});
    }
}
void URogueEnemyPresentationComponent::RestoreVisualPose()
{
    if(!bPoseApplied)return;
    for (const auto& P : Poses)
    {
        if (auto* M = P.Mesh.Get())
        {
            M->SetRelativeLocationAndRotation(P.Location, P.Rotation, false, nullptr, ETeleportType::TeleportPhysics);
            if (M->ComponentHasTag(TEXT("Rogue.SkyfallWings"))) M->SetCustomPrimitiveDataFloat(23, 0.f);
        }
    }
    bPoseApplied=false;
}
void URogueEnemyPresentationComponent::PlayAttackCue()
{
    auto* E=Cast<ARogueEnemy>(GetOwner());
    if(!E||E->IsDead()||E->IsAvailableInPool()||GetNetMode()==NM_DedicatedServer||!bEnableVisualMotion)return;
    // Repeated volleys refresh one bounded kick, rather than adding offsets.
    AttackAge=0;
    const float Now = GetWorld()->GetTimeSeconds();
    if (E->IsBoss() && Now - LastBossCastTime > .65f)
    {
        LastBossCastTime = Now;
        for (const auto& Surface : BossSurfaces)
            if (auto* Mesh = Surface.Mesh.Get()) Mesh->SetCustomPrimitiveDataFloat(20, Now);
        if (auto* Pool = GetWorld()->GetSubsystem<URogueCombatPoolSubsystem>())
            Pool->PlayCombatAccent(BossCastAccent, E->GetActorLocation(), E->GetActorForwardVector(), false);
    }
}
void URogueEnemyPresentationComponent::PlayHitCue()
{
    auto* E=Cast<ARogueEnemy>(GetOwner());
    if(!E||E->IsDead()||E->IsAvailableInPool()||GetNetMode()==NM_DedicatedServer||!bEnableVisualMotion)return;
    // Limit rapid-hit retriggers so a continuous laser cannot freeze the pose.
    if(HitAge<.10f)return;
    HitAge=0;HitSign=-HitSign;
}
void URogueEnemyPresentationComponent::UpdateVisualMotion(float Delta)
{
    auto* E=Cast<ARogueEnemy>(GetOwner());if(!E)return;
    const float Dt=FMath::Clamp(Delta,0.f,.15f);
    AttackAge+=Dt;HitAge+=Dt;MotionPhase+=Dt*3.4f;
    const float Yaw=E->GetActorRotation().Yaw;
    const float Turn=Dt>SMALL_NUMBER?FMath::FindDeltaAngleDegrees(PreviousYaw,Yaw)/Dt:0;
    PreviousYaw=Yaw;
    const FVector Velocity=E->GetVelocity();
    const auto* Support = E->GetEnemyType() == ERogueEnemyType::LinkHealer
        ? E->FindComponentByClass<URogueEnemySupportComponent>() : nullptr;
    const bool bLinkActive = Support && Support->GetActiveHealingLinkTarget();
    LanternLift = FMath::FInterpTo(LanternLift, bLinkActive ? 1.f : 0.f, Dt, 8.f);
    // No transform dirties for idle bodies: they retain their authored pose.
    if(Velocity.SizeSquared2D()<1.f&&SmoothedVelocity.SizeSquared2D()<1.f&&FMath::Abs(Turn)<.2f&&FMath::Abs(SmoothedTurn)<.002f&&AttackAge>=.48f&&HitAge>=.20f&&LanternLift<.015f)
    {
        static const FName BillboardTag(TEXT("Rogue.RoleBillboard"));
        const bool HasBillboard=Poses.ContainsByPredicate([](const FVisualPose& P){const auto* M=P.Mesh.Get();return M&&M->ComponentHasTag(BillboardTag);});
        if(!HasBillboard){RestoreVisualPose();return;}
    }
    bool bNear=true;FVector CameraLocation=FVector::ZeroVector;bool bHasCamera=false;
    if(const auto* PC=GetWorld()->GetFirstPlayerController();PC&&PC->PlayerCameraManager)
    {CameraLocation=PC->PlayerCameraManager->GetCameraLocation();bHasCamera=true;
        bNear=FVector::DistSquared(E->GetActorLocation(),CameraLocation)<FMath::Square(MotionDrawDistance);}
    if(bNear&&bHasCamera)
    {
        bNear=false;
        for(const auto& P:Poses)if(const auto* M=P.Mesh.Get();M&&M->WasRecentlyRendered(.3f)){bNear=true;break;}
    }
    if(!bEnableVisualMotion||!bNear||E->GetMovementModel()==ERogueEnemyMovementModel::Stationary)
    {RestoreVisualPose();return;}
    SmoothedVelocity=FMath::VInterpTo(SmoothedVelocity,Velocity,Dt,7.f);
    SmoothedTurn=FMath::FInterpTo(SmoothedTurn,FMath::Clamp(Turn/180.f,-1.f,1.f),Dt,8.f);
    const FVector Local=E->GetActorQuat().UnrotateVector(SmoothedVelocity)/FMath::Max(80.f,E->GetMoveSpeed());
    const float Speed=FMath::Clamp(SmoothedVelocity.Size2D()/FMath::Max(80.f,E->GetMoveSpeed()),0.f,1.f);
    auto Pulse=[](float Age,float Duration){return Age<Duration?FMath::Sin(PI*Age/Duration):0.f;};
    const float Kick=Pulse(AttackAge,.24f),Hit=Pulse(HitAge,.20f);
    const float Pitch=-FMath::Clamp(Local.X,-1.f,1.f)*MovementLeanDegrees+Kick*2.2f;
    const float Roll=-SmoothedTurn*TurnBankDegrees-FMath::Clamp(Local.Y,-1.f,1.f)*2.f+Hit*HitSign*HitSwayDegrees;
    const FVector Offset(-Kick*AttackRecoilDistance,Hit*HitSign*1.5f,
        FMath::Sin(MotionPhase)*MovementBobHeight*Speed);
    const FQuat Lean=FRotator(Pitch,0,Roll).Quaternion();
    for(const auto& P:Poses)if(auto* M=P.Mesh.Get())
    {
        FQuat Rotation=Lean*P.Rotation;
        if(M->ComponentHasTag(TEXT("Rogue.SecondaryMotion")))
            Rotation=FRotator(-SmoothedTurn*2.f,Speed*FMath::Sin(MotionPhase-.7f)*1.6f,Kick*3.f-SmoothedTurn*3.f).Quaternion()*Rotation;
        FVector LocalOffset = Offset;
        if (bSkyfallPose && M->ComponentHasTag(TEXT("Rogue.SkyfallWings")))
        {
            const float Fold = Pulse(AttackAge, .46f);
            Rotation = FRotator(Fold * 38.f, 0, Fold * 7.f).Quaternion() * Rotation;
            M->SetCustomPrimitiveDataFloat(23, Fold);
        }
        if (M->ComponentHasTag(TEXT("Rogue.LinkLantern")))
        {
            LocalOffset.Z += LanternLift * (22.f + 2.5f * FMath::Sin(MotionPhase * 1.2f));
            Rotation = FRotator(-14.f * LanternLift, 0.f, 4.f * LanternLift * FMath::Sin(MotionPhase)).Quaternion() * Rotation;
        }
        if(bHasCamera&&M->ComponentHasTag(TEXT("Rogue.RoleBillboard")))
        {
            const FVector Direction=E->GetActorQuat().UnrotateVector((CameraLocation-M->GetComponentLocation()).GetSafeNormal());
            Rotation=FQuat::FindBetweenNormals(FVector::ForwardVector,Direction)*P.Rotation;
        }
        M->SetRelativeLocationAndRotation(P.Location+LocalOffset,Rotation,false,nullptr,ETeleportType::TeleportPhysics);
    }
    bPoseApplied=true;
}
