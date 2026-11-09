#include "Combat/Projectiles/RogueReturningTalisman.h"
#include "Debug/RoguePerfStats.h"
#include "AbilitySystem/RogueAbilitySystemLibrary.h"
#include "AbilitySystem/RogueGameplayTags.h"
#include "Combat/Targeting/RogueEnemyTrackerSubsystem.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Enemies/RogueEnemy.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Framework/RogueGameState.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"
#include "Player/RogueCharacter.h"
#include "Combat/Weapons/RogueWeaponBase.h"
#include "UObject/ConstructorHelpers.h"

ARogueReturningTalisman::ARogueReturningTalisman()
{
    PrimaryActorTick.bCanEverTick=true;
    bReplicates=true; SetReplicateMovement(true); bNetUseOwnerRelevancy=true;
    NetUpdateFrequency=30; MinNetUpdateFrequency=15;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
    VisualRoot=CreateDefaultSubobject<USceneComponent>(TEXT("VisualRoot")); VisualRoot->SetupAttachment(RootComponent);
    Mesh=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TalismanMesh")); Mesh->SetupAttachment(VisualRoot);
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); Mesh->SetGenerateOverlapEvents(false);
    Mesh->SetCastShadow(false);
    // 原生类只提供占位形状；正式模型、材质和尺寸保存在 BP_RogueReturningTalisman。
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Shape(TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (Shape.Succeeded()) { Mesh->SetStaticMesh(Shape.Object); }
    Mesh->SetRelativeScale3D(FVector(.65f,.035f,1));
}

void ARogueReturningTalisman::BeginPlay()
{
    Super::BeginPlay();
    for(auto* Material:Mesh->GetMaterials())FlightMaterials.Add(Material);
    bFlightForceDisableNanite=Mesh->bForceDisableNanite;
    if (GetNetMode()!=NM_DedicatedServer && bEnableTrail)
    {
        UStaticMesh* Cylinder=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
        for (int32 I=0;I<16;++I)
        {
            auto* Segment=NewObject<USplineMeshComponent>(this);
            Segment->SetupAttachment(RootComponent); Segment->SetMobility(EComponentMobility::Movable);
            Segment->SetCollisionEnabled(ECollisionEnabled::NoCollision); Segment->SetGenerateOverlapEvents(false);
            Segment->SetStaticMesh(Cylinder); Segment->SetForwardAxis(ESplineMeshAxis::Z,false);
            Segment->SetCastShadow(false); if (TrailMaterial) { Segment->SetMaterial(0,TrailMaterial); }
            Segment->RegisterComponent(); Segment->SetVisibility(false); TrailSegments.Add(Segment);
        }
    }
}

void ARogueReturningTalisman::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ARogueReturningTalisman,Player);
    DOREPLIFETIME(ARogueReturningTalisman,Phase);
    DOREPLIFETIME(ARogueReturningTalisman,SlotIndex);
    DOREPLIFETIME(ARogueReturningTalisman,SlotCount);
}

void ARogueReturningTalisman::Configure(ARogueCharacter* InPlayer,int32 Index,int32 Count,float InDamage,
    float Range,float InCooldown,int32 Targets,int32 HitsPerTarget,const FRogueTalismanCombatSettings& Combat, ARogueWeaponBase* Weapon)
{
    if (!HasAuthority()) { return; }
    const bool bFirst=Player==nullptr;
    TargetingWeapon=Weapon;
    FlightSpeed=FMath::Max(100.f,Combat.FlightSpeed); ReturnSpeed=FMath::Max(100.f,Combat.ReturnSpeed);
    OvershootDistance=FMath::Max(40.f,Combat.OvershootDistance); HitRadius=FMath::Max(1.f,Combat.HitRadius);
    MaxSortieSeconds=FMath::Max(1.f,Combat.MaxSortieSeconds); MaxReturnSeconds=FMath::Max(.25f,Combat.MaxReturnSeconds);
    Player=InPlayer; SlotIndex=Index; SlotCount=FMath::Max(1,Count);
    Damage=FMath::Max(0.f,InDamage); SeekRange=FMath::Max(1.f,Range); Cooldown=FMath::Max(.05f,InCooldown);
    TargetBudget=FMath::Max(1,Targets); HitBudget=FMath::Max(1,HitsPerTarget);
    // 升级只作用于下一次完整出击，避免飞到一半更改本次命中额度。
    if (bFirst) { SetActorLocation(OrbitLocation()); NextLaunchTime=GetWorld()->GetTimeSeconds()+Index*.09f; ForceNetUpdate(); }
}

FVector ARogueReturningTalisman::OrbitLocation() const
{
    if (!Player) { return GetActorLocation(); }
    const AGameStateBase* State=GetWorld()->GetGameState();
    const double Time=State?State->GetServerWorldTimeSeconds():GetWorld()->GetTimeSeconds();
    const float Angle=FMath::DegreesToRadians(FMath::Fmod(Time*OrbitSpeed,360.)+360.f*SlotIndex/FMath::Max(1,SlotCount));
    return Player->GetActorLocation()+FVector(FMath::Cos(Angle)*OrbitRadius,FMath::Sin(Angle)*OrbitRadius,OrbitHeight);
}

bool ARogueReturningTalisman::IsUsable(const ARogueEnemy* Enemy,const FGuid& Cycle) const
{
    return IsValid(Enemy) && !Enemy->IsDead() && !Enemy->IsAvailableInPool()
        && Enemy->GetCurrentHealth()>0 && Enemy->GetSpawnCycleId()==Cycle;
}

ARogueEnemy* ARogueReturningTalisman::FindNextTarget() const
{
    // 先完成已经命中目标的重复穿刺。池复用后的新生命绝不继承旧目标的命中账本。
    for (const auto& Victim:Victims)
        if (Victim.Hits<SortieHitBudget && IsUsable(Victim.Enemy.Get(),Victim.Cycle)) { return Victim.Enemy.Get(); }
    if (Victims.Num()>=SortieTargetBudget) { return nullptr; }
    auto* Tracker=GetWorld()->GetSubsystem<URogueEnemyTrackerSubsystem>();
    if (!Tracker || !Player) { return nullptr; }
    TArray<ARogueEnemy*> Candidates;
    Tracker->CollectEnemiesInRange(Player->GetActorLocation(),SeekRange,Candidates,0,true);
    Candidates.RemoveAll([this](ARogueEnemy* Enemy)
    {
        return !IsUsable(Enemy,Enemy->GetSpawnCycleId()) || Victims.ContainsByPredicate(
            [Enemy](const FVictim& V){return V.Enemy==Enemy && V.Cycle==Enemy->GetSpawnCycleId();});
    });
    return Tracker->SelectWeaponTarget(TargetingWeapon.Get(), Candidates, this);
}

void ARogueReturningTalisman::StartPass(ARogueEnemy* Enemy)
{
    Target=Enemy; TargetCycle=Enemy->GetSpawnCycleId(); HitThisPass.Reset();
    if (auto* Tracker=GetWorld()->GetSubsystem<URogueEnemyTrackerSubsystem>())
    { Tracker->ReserveWeaponTarget(TargetingWeapon.Get(),this,Enemy); }
    FVector Direction=(Enemy->GetActorLocation()-GetActorLocation()).GetSafeNormal();
    if (Direction.IsNearlyZero()) { Direction=GetActorForwardVector(); }
    PassDirection=Direction;
    LegEnd=Enemy->GetActorLocation()+Direction*FMath::Max(40.f,OvershootDistance);
}

bool ARogueReturningTalisman::TryLaunch()
{
    if (!HasAuthority() || !Player || !Player->IsAlive() || Phase!=ERogueTalismanPhase::Orbiting
        || GetWorld()->GetTimeSeconds()<NextLaunchTime) { return false; }
    if (const auto* State=GetWorld()->GetGameState<ARogueGameState>(); State && State->IsGameOver()) { return false; }
    Victims.Reset(); SortieTargetBudget=TargetBudget; SortieHitBudget=HitBudget;
    ARogueEnemy* Enemy=FindNextTarget();
    if (!Enemy) { NextLaunchTime=GetWorld()->GetTimeSeconds()+.12f; return false; }
    Phase=ERogueTalismanPhase::Striking; SortieAge=0; StartPass(Enemy); ForceNetUpdate(); return true;
}

void ARogueReturningTalisman::BeginReturn()
{
    if (auto* Tracker=GetWorld()->GetSubsystem<URogueEnemyTrackerSubsystem>()) { Tracker->ReleaseWeaponTarget(this); }
    Phase=ERogueTalismanPhase::Returning; ReturnAge=0; PreviousReturnHome=OrbitLocation(); Target.Reset(); HitThisPass.Reset(); ForceNetUpdate();
}

void ARogueReturningTalisman::SweepDamage(const FVector& Start,const FVector& End)
{
    CSV_SCOPED_TIMING_STAT(RoguePerf, TalismanSweep);
    auto* Tracker=GetWorld()->GetSubsystem<URogueEnemyTrackerSubsystem>();
    if (!Tracker || !HasAuthority() || !Player || !Player->IsAlive()) { return; }
    TArray<ARogueEnemy*> Candidates;
    // 预筛选使用空间索引，实际相交检查是飞行线段与敌人胶囊间的最短距离。
    Tracker->CollectEnemiesInRange((Start+End)*.5f,FVector::Distance(Start,End)*.5f+FMath::Max(1.f,HitRadius)+300,Candidates,0,false);
    Candidates.Sort([&](const ARogueEnemy& A,const ARogueEnemy& B)
    {return FVector::DistSquared(Start,A.GetActorLocation())<FVector::DistSquared(Start,B.GetActorLocation());});
    for (ARogueEnemy* Enemy:Candidates)
    {
        if (!IsUsable(Enemy,Enemy->GetSpawnCycleId()) || HitThisPass.Contains(Enemy)) { continue; }
        FVictim* Record=Victims.FindByPredicate([Enemy](const FVictim& V){return V.Enemy==Enemy && V.Cycle==Enemy->GetSpawnCycleId();});
        if ((Record && Record->Hits>=SortieHitBudget) || (!Record && Victims.Num()>=SortieTargetBudget)) { continue; }
        const UCapsuleComponent* Capsule=Enemy->GetCapsuleComponent();
        const FVector Center=Capsule->GetComponentLocation();
        const float Radius=Capsule->GetScaledCapsuleRadius(), Half=FMath::Max(0.f,Capsule->GetScaledCapsuleHalfHeight()-Radius);
        const FVector Axis=Capsule->GetUpVector()*Half;
        FVector OnFlight,OnCapsule;
        FMath::SegmentDistToSegmentSafe(Start,End,Center-Axis,Center+Axis,OnFlight,OnCapsule);
        if (FVector::DistSquared(OnFlight,OnCapsule)>FMath::Square(Radius+FMath::Max(1.f,HitRadius))) { continue; }
        if (!Record)
        {
            FVictim New; New.Enemy=Enemy; New.Cycle=Enemy->GetSpawnCycleId(); Victims.Add(New); Record=&Victims.Last();
        }
        // 即便此次被无敌阻挡，也消耗这次实际穿过的机会，不能停在同一位置反复提交伤害。
        ++Record->Hits; HitThisPass.Add(Enemy);
        RogueAbilitySystem::ApplyDamageToTarget(Player,Enemy,Damage,RogueGameplayTags::Damage_Source_Projectile,nullptr,uint8(HitVisualStyle));
    }
}

int32 ARogueReturningTalisman::GetSortieHitCount() const
{
    int32 Count=0; for (const auto& Victim:Victims) { Count+=Victim.Hits; } return Count;
}

void ARogueReturningTalisman::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!Player) { return; } // 所有者引用可比 Actor 自身稍晚复制到客户端。
    if (HasAuthority())
    {
        if (!Player->IsAlive() || Player->IsActorBeingDestroyed()) { Destroy(); return; }
        const auto* State=GetWorld()->GetGameState<ARogueGameState>();
        if (State && State->IsGameOver()) { Destroy(); return; }
        if (Phase==ERogueTalismanPhase::Orbiting)
        {
            SetActorLocation(OrbitLocation()); SetActorRotation((GetActorLocation()-Player->GetActorLocation()).Rotation());
        }
        else
        {
            SortieAge+=DeltaSeconds;
            if (Phase==ERogueTalismanPhase::Striking)
            {
                if (SortieAge>=FMath::Max(1.f,MaxSortieSeconds)) { BeginReturn(); }
                else if (!IsUsable(Target.Get(),TargetCycle))
                {
                    if (auto* Next=FindNextTarget()) { StartPass(Next); } else { BeginReturn(); }
                }
                else
                {
                    // 接触前追踪移动目标；穿过后固定越过点，保证下一次是新的往返穿刺。
                    if (!HitThisPass.Contains(Target)) { LegEnd=Target->GetActorLocation()+PassDirection*FMath::Max(40.f,OvershootDistance); }
                    const FVector Old=GetActorLocation();
                    const FVector New=FMath::VInterpConstantTo(Old,LegEnd,DeltaSeconds,FMath::Max(100.f,FlightSpeed));
                    SetActorLocation(New); if (!New.Equals(Old)) { SetActorRotation((New-Old).Rotation()); }
                    SweepDamage(Old,New);
                    // 记满伤害次数后仍越过敌人再折返，避免刚碰到胶囊边缘就掉头。
                    if (New.Equals(LegEnd,2.f))
                    {
                        if (auto* Next=FindNextTarget()) { StartPass(Next); } else { BeginReturn(); }
                    }
                }
            }
            if (Phase==ERogueTalismanPhase::Returning)
            {
                const FVector Old=GetActorLocation(), Home=OrbitLocation();
                // 抵消玩家位移和轨道移动，再按武器表速度缩短相对距离，低速也能追上。
                ReturnAge+=DeltaSeconds;
                const FVector Carried=Old+(Home-PreviousReturnHome); PreviousReturnHome=Home;
                const bool bTimedOut=ReturnAge>=FMath::Max(.25f,MaxReturnSeconds);
                const FVector New=bTimedOut?Home:FMath::VInterpConstantTo(Carried,Home,DeltaSeconds,FMath::Max(100.f,ReturnSpeed));
                SetActorLocation(New); if (!New.Equals(Old)) { SetActorRotation((New-Old).Rotation()); }
                // 返回不再造成额外伤害，防止默认一次穿刺变成往返两次。
                if (New.Equals(Home,20.f))
                { Phase=ERogueTalismanPhase::Orbiting; NextLaunchTime=GetWorld()->GetTimeSeconds()+Cooldown; ForceNetUpdate(); }
            }
        }
    }
    UpdatePresentation(DeltaSeconds);
}

void ARogueReturningTalisman::UpdatePresentation(float DeltaSeconds)
{
    CSV_SCOPED_TIMING_STAT(RoguePerf, TalismanPresentation);
    if (GetNetMode()==NM_DedicatedServer) { return; }
    const bool bStandby=Phase==ERogueTalismanPhase::Orbiting&&!OrbitMaterials.IsEmpty();
    if(bStandby!=bOrbitMaterialsApplied)
    {
        for(int32 I=0;I<FlightMaterials.Num();++I)
            Mesh->SetMaterial(I,bStandby&&OrbitMaterials.IsValidIndex(I)&&OrbitMaterials[I]?OrbitMaterials[I].Get():FlightMaterials[I].Get());
        Mesh->SetForceDisableNanite(bStandby||bFlightForceDisableNanite);bOrbitMaterialsApplied=bStandby;
    }
    if(bStandby)Mesh->SetCustomPrimitiveDataVector3(12,Player->GetActorLocation());
    FVector DisplayLocation=GetActorLocation();
    if (Phase==ERogueTalismanPhase::Orbiting) { DisplayLocation=OrbitLocation(); }
    else if (!HasAuthority() && bVisualInitialized)
    { DisplayLocation=FMath::VInterpTo(SmoothedLocation,DisplayLocation,DeltaSeconds,24.f); }
    const FVector DisplayMovement=bVisualInitialized?DisplayLocation-SmoothedLocation:FVector::ZeroVector;
    SmoothedLocation=DisplayLocation; bVisualInitialized=true;
    VisualRoot->SetWorldLocation(DisplayLocation);
    if (Phase==ERogueTalismanPhase::Orbiting)
    { VisualRoot->SetWorldRotation((DisplayLocation-Player->GetActorLocation()).Rotation()); }
    else
    {
        // 客户端按平滑后的可见轨迹朝向，避免复制位置与复制旋转稍有错位时横着飞。
        const FVector Direction=!HasAuthority() && !DisplayMovement.IsNearlyZero()
            ?DisplayMovement.GetSafeNormal():GetActorForwardVector();
        VisualRoot->SetWorldRotation(Direction.Rotation().Quaternion()*FlightRotationOffset.Quaternion());
    }
    VisualRoot->SetRelativeScale3D(FVector(Phase==ERogueTalismanPhase::Orbiting?FMath::Max(.05f,OrbitSizeMultiplier):1.f));
    if (LastVisualPhase!=Phase)
    {
        // Returning trails stay in world space and age out; only a new launch
        // resets history so it cannot bridge across the waiting orbit.
        if (LastVisualPhase==ERogueTalismanPhase::Orbiting) { TrailPoints.Reset(); }
        LastVisualPhase=Phase;
    }
    // 始终使用蓝图配置的拖尾时间；昼夜和敌人密度不再额外缩短或收细尾迹。
    const float Lifetime = FMath::Clamp(TrailLifetime, .03f, 1.f);
    for (auto& Point:TrailPoints) { Point.Age+=DeltaSeconds; }
    TrailPoints.RemoveAll([Lifetime](const FTrailPoint& P){return P.Age>Lifetime;});
    // 按时间均匀取历史点。逐帧取样再截断会使高帧率下的拖尾明显变短。
    const float SampleInterval=Lifetime/FMath::Max(1,TrailSegments.Num());
    if (bEnableTrail && Phase!=ERogueTalismanPhase::Orbiting
        && (TrailPoints.IsEmpty() || TrailPoints[0].Age>=SampleInterval))
    { FTrailPoint Point; Point.Location=DisplayLocation; TrailPoints.Insert(Point,0); }
    if (TrailPoints.Num()>TrailSegments.Num()) { TrailPoints.SetNum(TrailSegments.Num()); }
    for (int32 I=0;I<TrailSegments.Num();++I)
    {
        USplineMeshComponent* Segment=TrailSegments[I];
        const bool bVisible=bEnableTrail && I<TrailPoints.Num() && (Phase!=ERogueTalismanPhase::Orbiting||I>0);
        Segment->SetVisibility(bVisible);
        if (!bVisible) { continue; }
        const FVector Start=GetActorTransform().InverseTransformPosition(I==0?DisplayLocation:TrailPoints[I-1].Location);
        const FVector End=GetActorTransform().InverseTransformPosition(TrailPoints[I].Location);
        const FVector Tangent=End-Start;
        const float Width = FMath::Max(.2f, TrailWidth) / 100.f;
        const float A=Width*(I==0?1.f:1.f-TrailPoints[I-1].Age/Lifetime);
        const float B=Width*(1.f-TrailPoints[I].Age/Lifetime);
        Segment->SetStartScale(FVector2D(A),false); Segment->SetEndScale(FVector2D(B),false);
        Segment->SetStartAndEnd(Start,Tangent,End,Tangent,true);
    }
}

ARogueEnemy* ARogueReturningTalisman::GetCurrentTarget() const
{
    return IsUsable(Target.Get(),TargetCycle) ? Target.Get() : nullptr;
}

void ARogueReturningTalisman::EndPlay(const EEndPlayReason::Type Reason)
{
    if (auto* Tracker=GetWorld() ? GetWorld()->GetSubsystem<URogueEnemyTrackerSubsystem>() : nullptr)
    { Tracker->ReleaseWeaponTarget(this); }
    Super::EndPlay(Reason);
}
