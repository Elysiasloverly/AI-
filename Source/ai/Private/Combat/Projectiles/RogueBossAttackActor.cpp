#include "Combat/Projectiles/RogueBossAttackActor.h"
#include "Combat/Visuals/RogueGroundEffectFade.h"
#include "Enemies/RogueEnemy.h"
#include "Player/RogueCharacter.h"
#include "Player/RoguePlayerQuery.h"
#include "AbilitySystem/RogueAbilitySystemLibrary.h"
#include "AbilitySystem/RogueGameplayTags.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "World/RogueArenaBoundary.h"
#include "GameFramework/GameStateBase.h"
#include "Framework/RogueGameState.h"
#include "Net/UnrealNetwork.h"

ARogueBossAttackActor::ARogueBossAttackActor()
{
    bReplicates=true;bAlwaysRelevant=true;PrimaryActorTick.bCanEverTick=true;
    SetNetUpdateFrequency(15);SetActorEnableCollision(false);
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
    Body=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
    Glow=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Glow"));
    Warning=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Warning"));
    Trail=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Trail"));
    Accent=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Accent"));
    Glow->SetTranslucentSortPriority(1);
    Body->SetTranslucentSortPriority(2);
    Accent->SetTranslucentSortPriority(3);
    for (UStaticMeshComponent* Mesh : {Body.Get(), Glow.Get(), Warning.Get(), Trail.Get(), Accent.Get()})
    {
        Mesh->SetupAttachment(RootComponent);
        Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Mesh->SetCastShadow(false);
        Mesh->SetCanEverAffectNavigation(false);
        Mesh->SetVisibility(false);
    }
}
void ARogueBossAttackActor::BeginPlay()
{
    Super::BeginPlay();
    if (GetNetMode() == NM_DedicatedServer) return;
    BaseBodyScale = Body->GetRelativeScale3D();
    BaseGlowScale = Glow->GetRelativeScale3D();
    for (UStaticMeshComponent* Mesh : {Body.Get(), Glow.Get(), Warning.Get(), Trail.Get(), Accent.Get()})
        if (Mesh->GetStaticMesh() && Mesh->GetMaterial(0)) Mesh->CreateAndSetMaterialInstanceDynamic(0);
    bPresentationReady = true;
    if (bInitialized) UpdateVisuals(FMath::Max(0.f, ServerTime() - Payload.StartServerTime));
}
void ARogueBossAttackActor::RetireVisuals()
{
    if (bVisualsRetired) return;
    bVisualsRetired=true;
    ARogueGroundEffectFade::CaptureActor(this);
}
void ARogueBossAttackActor::EndPlay(const EEndPlayReason::Type Reason)
{
    if (Reason==EEndPlayReason::Destroyed) RetireVisuals();
    Super::EndPlay(Reason);
}
float ARogueBossAttackActor::ServerTime() const
{
    const auto* GS=GetWorld()?GetWorld()->GetGameState():nullptr;
    return GS?GS->GetServerWorldTimeSeconds():(GetWorld()?GetWorld()->GetTimeSeconds():0);
}
bool ARogueBossAttackActor::SourceStillValid() const
{
    const auto* E=Cast<ARogueEnemy>(GetOwner());
    // 客户端可能先收到危险区域，再收到远处 Boss 的 Actor 引用；预警本身可以独立显示。
    if (!HasAuthority() && !E) return true;
    return E && !E->IsDead() && !E->IsAvailableInPool() && E->GetPresentationGeneration()==Payload.Generation
        && (!HasAuthority() || (E->GetSpawnCycleId()==SourceCycle && E->GetCurrentHealth()>0));
}
bool ARogueBossAttackActor::FindGround(UWorld* W,const FVector& Near,AActor* Ignore,FVector& Ground)
{
    if (!W) return false;
    FCollisionQueryParams Q(SCENE_QUERY_STAT(BossSkillGround),false,Ignore);FHitResult Hit;
    if (!W->LineTraceSingleByObjectType(Hit,Near+FVector(0,0,120),Near-FVector(0,0,4000),FCollisionObjectQueryParams(ECC_WorldStatic),Q) || Hit.ImpactNormal.Z<.55f) return false;
    Ground=Hit.ImpactPoint;return true;
}
void ARogueBossAttackActor::InitializeAttack(ARogueEnemy* Source,ERogueBossAttackShape Shape,const FRogueBossAttackSettings& S,
    const FVector& Origin,const FVector& Direction,float Damage,float Seconds,bool bReleased,ARogueCharacter* AimTarget,float WarmupSeconds)
{
    if (!HasAuthority() || !IsValid(Source)) return;
    SetOwner(Source);SetInstigator(Source);SourceCycle=Source->GetSpawnCycleId();
    Payload.Shape=Shape;Payload.Settings=S;Payload.Origin=Origin;Payload.Direction=Direction.GetSafeNormal();
    if (Payload.Direction.IsNearlyZero()) Payload.Direction=FVector::ForwardVector;
    if (Shape==ERogueBossAttackShape::Beam)
    {
        Payload.Direction=Direction.GetSafeNormal2D();
        if (Payload.Direction.IsNearlyZero()) Payload.Direction=FVector::ForwardVector;
        if (bReleased) Payload.ReleasedBeamLength=ResolveBeamLength(Payload.Direction);
    }
    Payload.StartServerTime=ServerTime();Payload.Damage=FMath::Max(0.f,Damage);Payload.ActiveSeconds=FMath::Max(.1f,Seconds);
    Payload.Generation=Source->GetPresentationGeneration();Payload.bReleased=bReleased;
    Payload.AimTarget=Shape==ERogueBossAttackShape::Beam ? AimTarget : nullptr;
    Payload.WarmupSeconds=FMath::Max(.08f,WarmupSeconds);
    CacheVisualGround();
    bInitialized=true;SetActorLocation(Origin);
    NextDamageTime=Shape==ERogueBossAttackShape::Meteor
        ? Payload.StartServerTime+FMath::Max(.1f,S.WarningSeconds)+FMath::Max(.1f,S.BurnInterval)
        : Payload.StartServerTime;
    ForceNetUpdate();UpdateVisuals(0);
}
void ARogueBossAttackActor::Release(const FVector& Direction)
{
    if (!HasAuthority() || !bInitialized || !SourceStillValid()) return;
    Payload.bReleased=true;Payload.StartServerTime=ServerTime();Payload.Origin=GetActorLocation();
    Payload.Direction=Direction.GetSafeNormal();if(Payload.Direction.IsNearlyZero())Payload.Direction=FVector::ForwardVector;
    if (Payload.Shape==ERogueBossAttackShape::Beam)
    {
        Payload.Direction=Payload.Direction.GetSafeNormal2D();
        if (Payload.Direction.IsNearlyZero()) Payload.Direction=FVector::ForwardVector;
        Payload.ReleasedBeamLength=ResolveBeamLength(Payload.Direction);
    }
    Payload.AimTarget=nullptr;NextDamageTime=Payload.StartServerTime;ForceNetUpdate();
}
void ARogueBossAttackActor::OnRep_Payload()
{
    bInitialized=true;
    SetActorLocation(Payload.Origin);
    CacheVisualGround();
    UpdateVisuals(FMath::Max(0.f,ServerTime()-Payload.StartServerTime));
}

float ARogueBossAttackActor::ResolveBeamLength(const FVector& Direction)
{
    if (!bLookedUpBoundary)
    {
        for (TActorIterator<ARogueArenaBoundary> It(GetWorld()); It; ++It)
        {
            if (It->Loops.IsEmpty()) continue;
            ArenaBoundary=*It;
            break;
        }
        bLookedUpBoundary=true;
    }
    double Distance=0.;
    if (ArenaBoundary.IsValid() && ArenaBoundary->TryGetOuterRayDistance(Payload.Origin, Direction, Distance))
        return float(FMath::Max(1., Distance));
    return FMath::Max(1.f, Payload.Settings.BeamRange);
}

void ARogueBossAttackActor::CacheVisualGround()
{
    if (GetNetMode() == NM_DedicatedServer) return;
    if (Payload.Shape != ERogueBossAttackShape::Beam && Payload.Shape != ERogueBossAttackShape::HealAura
        && Payload.Shape != ERogueBossAttackShape::GoldenWard) return;
    VisualGroundZ = Payload.Origin.Z - 85.f;
    FVector Ground;
    if (FindGround(GetWorld(), Payload.Origin, this, Ground)) VisualGroundZ = Ground.Z;
}

void ARogueBossAttackActor::ApplyCircleDamage(const FVector& Center,float Radius,float Damage,float Interval,bool bOnce)
{
    if (!HasAuthority() || Damage<=0 || !SourceStillValid()) return;
    TArray<ARogueCharacter*,TInlineAllocator<4>> Players;RoguePlayerQuery::CollectAlivePlayers(GetWorld(),Players);
    const float Now=ServerTime();
    for (auto* P:Players)
    {
        const float Last=LastHits.Contains(P)?LastHits[P]:-MAX_flt;
        if (Now-Last<Interval || FMath::Abs(P->GetActorLocation().Z-Center.Z)>P->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+150
            || FVector::DistSquared2D(P->GetActorLocation(),Center)>FMath::Square(Radius+P->GetCapsuleComponent()->GetScaledCapsuleRadius())) continue;
        LastHits.Add(P,bOnce?MAX_flt:Now);
        RogueAbilitySystem::ApplyDamageToTarget(GetOwner(),P,Damage,RogueGameplayTags::Damage_Source_Area);
    }
}
float ARogueBossAttackActor::GetFlameWaveScale(const FVector& Position) const
{
    const auto& Settings=Payload.Settings;
    const float Distance=float(FVector::Dist2D(Position, Payload.Origin));
    const float Progress=FMath::Clamp(Distance / FMath::Max(1.f, Settings.WaveGrowthDistance), 0.f, 1.f);
    return FMath::Lerp(1.f, FMath::Clamp(Settings.WaveMaxScale, 1.f, 4.f), Progress);
}

float ARogueBossAttackActor::GetMovingAttackHeight(const FVector& Position) const
{
    const auto& Settings=Payload.Settings;
    if (Payload.Shape!=ERogueBossAttackShape::FlameWave) return FMath::Max(20.f,Settings.Radius);
    // 增高的部分向上展开，刀波下缘保持原来的离地距离。
    return FMath::Max(1.f,Settings.WaveHeightAboveGround)
        + FMath::Max(1.f,Settings.WaveHalfHeight)*(GetFlameWaveScale(Position)-1.f);
}

void ARogueBossAttackActor::ApplySweptDamage(const FVector& From,const FVector& To,float Width,float Damage,bool bOnce)
{
    if (!HasAuthority() || Damage<=0 || !SourceStillValid()) return;
    TArray<ARogueCharacter*,TInlineAllocator<4>> Players;RoguePlayerQuery::CollectAlivePlayers(GetWorld(),Players);
    const float Now=ServerTime();
    for (auto* P:Players)
    {
        const float Last=LastHits.Contains(P)?LastHits[P]:-MAX_flt;
        const float Interval=Payload.Shape==ERogueBossAttackShape::Beam?FMath::Max(.1f,Payload.Settings.DamageInterval):.75f;
        const FVector Position=P->GetActorLocation();
        const FVector Closest=FMath::ClosestPointOnSegment(Position,From,To);
        float EffectiveWidth=Width;
        float EffectiveHalfHeight=70.f;
        if (Payload.Shape==ERogueBossAttackShape::FlameWave)
        {
            const float Growth=GetFlameWaveScale(Closest);
            const float Along=FVector::DotProduct(Position-From,Payload.Direction);
            const float Padding=P->GetCapsuleComponent()->GetScaledCapsuleRadius()+35.f*Growth;
            if (Along < -Padding || Along > FVector::Dist2D(From,To)+Padding) continue;
            EffectiveWidth*=Growth;
            EffectiveHalfHeight=FMath::Max(1.f,Payload.Settings.WaveHalfHeight)*Growth;
        }
        if (Now-Last<Interval || FMath::Abs(P->GetActorLocation().Z-Closest.Z)>P->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+EffectiveHalfHeight
            || FVector::DistSquared2D(Closest,P->GetActorLocation())>FMath::Square(EffectiveWidth+P->GetCapsuleComponent()->GetScaledCapsuleRadius())) continue;
        LastHits.Add(P,bOnce?MAX_flt:Now);
        RogueAbilitySystem::ApplyDamageToTarget(GetOwner(),P,Damage,Payload.Shape==ERogueBossAttackShape::Beam?RogueGameplayTags::Damage_Source_Beam:RogueGameplayTags::Damage_Source_Projectile);
    }
}

void ARogueBossAttackActor::AdvanceMovingAttack(float Delta,float Age)
{
    const bool Wave=Payload.Shape==ERogueBossAttackShape::FlameWave;
    const auto& S=Payload.Settings;
    const FVector From=GetActorLocation();FVector To=From+Payload.Direction.GetSafeNormal2D()*FMath::Max(1.f,S.Speed)*Delta;
    FVector Ground;
    if (FindGround(GetWorld(),To,this,Ground)) To.Z=Ground.Z+GetMovingAttackHeight(To);
    FCollisionQueryParams Q(SCENE_QUERY_STAT(BossSkillWall),false,this);Q.AddIgnoredActor(GetOwner());TArray<FHitResult> Hits;
    const float Growth=Wave ? GetFlameWaveScale(To) : 1.f;
    const auto Shape=Wave ? FCollisionShape::MakeBox(FVector(22*Growth,FMath::Max(10.f,S.WaveHalfWidth)*Growth,FMath::Max(1.f,S.WaveHalfHeight)*Growth))
        : FCollisionShape::MakeSphere(FMath::Max(10.f,S.Radius*.8f));
    const bool Hit=GetWorld()->SweepMultiByObjectType(Hits,From,To,Payload.Direction.GetSafeNormal2D().Rotation().Quaternion(),FCollisionObjectQueryParams(ECC_WorldStatic),Shape,Q);
    bool Wall=false;
    if (Hit) for (const auto& H:Hits) if (H.bBlockingHit && H.ImpactNormal.Z<.55f)
    { To=H.Location;Wall=true;break; }
    ApplySweptDamage(From,To,Wave?S.WaveHalfWidth:S.Radius,Payload.Damage,Wave);
    SetActorLocation(To);
    if (Wall)
    {
        if (WallImpactClass) Multicast_WallImpact(To, Payload.Direction);
        Destroy();
    }
    else if (Age>=30.f) Destroy();
}

void ARogueBossAttackActor::Tick(float Delta)
{
    Super::Tick(Delta);if(!bInitialized)return;
    const auto* GS=GetWorld()->GetGameState<ARogueGameState>();
    if (!SourceStillValid() || (GS&&GS->IsGameOver()))
    {
        if (HasAuthority()) Destroy();
        else
        {
            RetireVisuals();
            for (UStaticMeshComponent* Mesh : {Body.Get(), Glow.Get(), Warning.Get(), Trail.Get(), Accent.Get()}) Mesh->SetVisibility(false);
        }
        return;
    }
    const float Age=FMath::Max(0.f,ServerTime()-Payload.StartServerTime);const auto& S=Payload.Settings;
    if (Payload.bReleased && (Payload.Shape==ERogueBossAttackShape::RollingBall || Payload.Shape==ERogueBossAttackShape::FlameWave))
    {
        if (HasAuthority()) AdvanceMovingAttack(Delta,Age);
        else
        {
            FVector P=Payload.Origin+Payload.Direction.GetSafeNormal2D()*FMath::Max(1.f,S.Speed)*Age;FVector Ground;
            if(FindGround(GetWorld(),P,this,Ground))P.Z=Ground.Z+GetMovingAttackHeight(P);
            SetActorLocation(P);
        }
    }
    if (HasAuthority())
    {
        if (Payload.Shape==ERogueBossAttackShape::Beam && Payload.bReleased)
        {
            if(Age>=Payload.ActiveSeconds) { Destroy();return; }
            if (ServerTime()>=NextDamageTime)
            {
                NextDamageTime=ServerTime()+FMath::Max(.1f,S.DamageInterval);
                const FVector From=Payload.Origin;
                const FVector End=From+Payload.Direction*Payload.ReleasedBeamLength;
                ApplySweptDamage(From,End,S.BeamHalfWidth,Payload.Damage,false);
            }
        }
        if (Payload.Shape==ERogueBossAttackShape::Meteor || Payload.Shape==ERogueBossAttackShape::IceSpike)
        {
            const float WarningTime=FMath::Max(.4f,S.WarningSeconds);
            if (Age>=WarningTime && !bImpacted)
            { bImpacted=true;ApplyCircleDamage(Payload.Origin,S.Radius,Payload.Damage,0,Payload.Shape==ERogueBossAttackShape::IceSpike); }
            if (Payload.Shape==ERogueBossAttackShape::Meteor && Age>=WarningTime && Age<WarningTime+S.FireSeconds && ServerTime()>=NextDamageTime)
            { NextDamageTime=ServerTime()+FMath::Max(.1f,S.BurnInterval);ApplyCircleDamage(Payload.Origin,S.Radius,Payload.Damage*FMath::Max(0.f,S.BurnDamageScale),FMath::Max(.1f,S.BurnInterval)); }
            if (Age>=WarningTime+(Payload.Shape==ERogueBossAttackShape::Meteor?FMath::Max(.1f,S.FireSeconds):.8f))Destroy();
        }
        if ((Payload.Shape==ERogueBossAttackShape::HealAura || Payload.Shape==ERogueBossAttackShape::GoldenWard) && Age>=Payload.ActiveSeconds+.5f)Destroy();
        if(!Payload.bReleased && Age>30)Destroy();
    }
    UpdateVisuals(Age);
}

void ARogueBossAttackActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{ Super::GetLifetimeReplicatedProps(OutLifetimeProps);DOREPLIFETIME(ARogueBossAttackActor,Payload); }
