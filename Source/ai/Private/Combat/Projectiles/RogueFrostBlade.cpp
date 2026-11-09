#include "Combat/Projectiles/RogueFrostBlade.h"
#include "Combat/Projectiles/RogueFrostSwordWave.h"
#include "Combat/Weapons/RogueWeaponBase.h"
#include "Combat/Targeting/RogueEnemyTrackerSubsystem.h"
#include "AbilitySystem/RogueAbilitySystemLibrary.h"
#include "AbilitySystem/RogueGameplayTags.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Enemies/RogueEnemy.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"
#include "Player/RogueCharacter.h"

ARogueFrostBlade::ARogueFrostBlade()
{
    PrimaryActorTick.bCanEverTick=true;bReplicates=true;SetReplicateMovement(false);SetActorEnableCollision(false);NetUpdateFrequency=20;
    auto* Root=CreateDefaultSubobject<USceneComponent>(TEXT("Root"));SetRootComponent(Root);
    VisualRoot=CreateDefaultSubobject<USceneComponent>(TEXT("VisualRoot"));VisualRoot->SetupAttachment(Root);
    Mesh=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BladeMesh"));Mesh->SetupAttachment(VisualRoot);
    SlashTrail=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SlashTrail"));SlashTrail->SetupAttachment(VisualRoot);
    Afterimage=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Afterimage"));Afterimage->SetupAttachment(Root);Afterimage->SetAbsolute(true,true,true);
    IceMotes=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("IceMotes"));IceMotes->SetupAttachment(VisualRoot);
    for(auto* M:{Mesh.Get(),SlashTrail.Get(),Afterimage.Get(),IceMotes.Get()}){M->SetCollisionEnabled(ECollisionEnabled::NoCollision);M->SetGenerateOverlapEvents(false);M->SetCanEverAffectNavigation(false);M->SetCastShadow(false);}
    SlashTrail->SetVisibility(false);Afterimage->SetVisibility(false);
}
double ARogueFrostBlade::Now()const
{const auto* GS=GetWorld()->GetGameState();return GS?GS->GetServerWorldTimeSeconds():GetWorld()->GetTimeSeconds();}
FVector ARogueFrostBlade::Home()const
{
    const auto* Player=Cast<ARogueCharacter>(GetOwner());if(!Player)return GetActorLocation();
    FVector Local=HoverOffset;
    // Keep the enlarged blade above the owner's feet; this changes rest pose only.
    Local.Z=FMath::Max(Local.Z,Combat.BladeLength*BladeScale-Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+TipClearance);
    FVector Offset=Player->GetActorRotation().RotateVector(Local);Offset.Z+=HoverAmplitude*FMath::Sin(Now()*UE_TWO_PI*HoverFrequency);
    return Player->GetActorLocation()+Offset;
}
FVector ARogueFrostBlade::ClampToOwner(const FVector& Location)const
{
    const auto* Player=Cast<ARogueCharacter>(GetOwner());if(!Player)return Location;
    const FVector Center=Player->GetActorLocation();const FVector Offset=Location-Center;
    return Center+Offset.GetClampedToMaxSize(FMath::Max(50.f,Combat.MaxLeashDistance));
}
void ARogueFrostBlade::Configure(ARogueCharacter* Player,const FRogueFrostBladeCombatSettings& Settings,float Size)
{
    if(!HasAuthority())return;
    SetOwner(Player);SetInstigator(Player);Combat=Settings;BladeScale=FMath::Max(.1f,Size);
    if(!bPoseInitialized){SetActorLocation(Home());SetActorRotation(FRotator(-90,Player->GetActorRotation().Yaw,0));bPoseInitialized=true;}
}
bool ARogueFrostBlade::StartAttack(ARogueEnemy* Enemy,ARogueWeaponBase* Weapon,float InDamage,int32 Waves,float InWaveDamage,float WaveSize,float InWaveSpeed,TSubclassOf<ARogueFrostSwordWave> Class)
{
    auto* Player=Cast<ARogueCharacter>(GetOwner());
    if(!HasAuthority()||!Player||!Player->IsAlive()||State.Phase!=ERogueFrostBladePhase::Hover||!IsValid(Enemy)||Enemy->IsDead()||Enemy->IsAvailableInPool())return false;
    FCollisionQueryParams Sight(SCENE_QUERY_STAT(FrostBladeSight),false,this);Sight.AddIgnoredActor(Player);
    FHitResult Wall;
    if(GetWorld()->LineTraceSingleByObjectType(Wall,Player->GetActorLocation(),Enemy->GetActorLocation(),FCollisionObjectQueryParams(ECC_WorldStatic),Sight))return false;
    Target=Enemy;TargetCycle=Enemy->GetSpawnCycleId();SourceWeapon=Weapon;Damage=InDamage;WaveCount=FMath::Max(0,Waves);WaveDamage=InWaveDamage;WaveScale=WaveSize;WaveSpeed=InWaveSpeed;WaveClass=Class;
    FVector Direction=(Enemy->GetActorLocation()-Player->GetActorLocation()).GetSafeNormal2D();
    if(Direction.IsNearlyZero())Direction=Player->GetActorForwardVector();
    const float StandOff=Enemy->GetCapsuleComponent()->GetScaledCapsuleRadius()+Combat.BladeLength*BladeScale*.2f;
    State.Anchor=ClampToOwner(Enemy->GetActorLocation()-Direction*StandOff+FVector(0,0,Combat.SlashHeightOffset));
    State.AimYaw=Direction.Rotation().Yaw;SetPhase(ERogueFrostBladePhase::Approach);return true;
}
void ARogueFrostBlade::SetPhase(ERogueFrostBladePhase Phase)
{
    State.Start=GetActorLocation();State.Rotation=GetActorRotation();State.Phase=Phase;State.Born=Now();++State.Serial;
    if(Phase==ERogueFrostBladePhase::Slash){HitCycles.Reset();LastSweep=0;}
    OnRep_State();ForceNetUpdate();
    if(Phase==ERogueFrostBladePhase::Slash&&SourceWeapon.IsValid())
    {
        FRogueWeaponCosmeticFire Fire;Fire.Start=GetActorLocation();Fire.Direction=FRotator(0,State.AimYaw,0).Vector();Fire.End=Fire.Start+FVector(Fire.Direction)*Combat.BladeLength*BladeScale;
        Fire.bPlayAttackSound=true;Fire.bPlayFireAccent=true;SourceWeapon->BroadcastCosmeticFire(Fire);
    }
}
void ARogueFrostBlade::OnRep_State()
{
    if(GetNetMode()!=NM_DedicatedServer)
    {
        if(LastVisualPhase==ERogueFrostBladePhase::Slash&&State.Phase==ERogueFrostBladePhase::Return)
        {
            Afterimage->SetWorldTransform(SlashTrail->GetComponentTransform());AfterimageAge=0;
            Afterimage->SetCustomPrimitiveDataVector4(0,FVector4(0,1,1,1));Afterimage->SetVisibility(true);
        }
        SlashTrail->SetVisibility(State.Phase==ERogueFrostBladePhase::Slash);
    }
    LastVisualPhase=State.Phase;
}
void ARogueFrostBlade::UpdateArt(float Delta,float SwingProgress)
{
    if(GetNetMode()==NM_DedicatedServer)return;
    const bool Slashing=State.Phase==ERogueFrostBladePhase::Slash;
    Mesh->SetCustomPrimitiveDataVector4(0,FVector4(SwingProgress,Slashing?1.f:0.f,0,1));
    IceMotes->SetCustomPrimitiveDataVector4(0,FVector4(SwingProgress,Slashing?1.f:0.f,0,1));
    if(Slashing)
    {
        const float Visible=FMath::Clamp(Combat.SweepDegrees*SwingProgress/FMath::Max(10.f,TrailAngleDegrees),0.f,1.f);
        SlashTrail->SetCustomPrimitiveDataVector4(0,FVector4(SwingProgress,0,Visible,1));
    }
    if (Afterimage->IsVisible())
    {
        const auto* Tracker = GetWorld()->GetSubsystem<URogueEnemyTrackerSubsystem>();
        AfterimageAge += Delta * (Tracker && Tracker->ShouldCullCombatEffects() ? 1.9f : 1.f);
        const float Fade = FMath::Clamp(AfterimageAge / FMath::Max(.01f, AfterimageDuration), 0.f, 1.f);
        Afterimage->SetCustomPrimitiveDataFloat(0, Fade);
        if (Fade >= 1.f) Afterimage->SetVisibility(false);
    }
}
void ARogueFrostBlade::Tick(float Delta)
{
    Super::Tick(Delta);auto* Player=Cast<ARogueCharacter>(GetOwner());if(!Player)return;
    if(HasAuthority()&&!Player->IsAlive()){Destroy();return;}
    VisualRoot->SetRelativeScale3D(FVector(BladeScale));
    const float Age=FMath::Max(0.f,float(Now()-State.Born));
    const FRotator Rest(-90,Player->GetActorRotation().Yaw,0);
    if(State.Phase==ERogueFrostBladePhase::Hover)
    {
        const FVector Desired=Home();const bool Snap=!bPoseInitialized||FVector::DistSquared(GetActorLocation(),Player->GetActorLocation())>FMath::Square(Combat.MaxLeashDistance*1.5f);
        SetActorLocation(Snap||HoverFollowSpeed<=0?Desired:FMath::Lerp(GetActorLocation(),Desired,1-FMath::Exp(-HoverFollowSpeed*Delta)));bPoseInitialized=true;
        SetActorRotation(FRotator(Rest.Pitch+HoverSwayDegrees*FMath::Sin(Now()*1.7),Rest.Yaw+HoverSwayDegrees*FMath::Sin(Now()*1.1),0));UpdateArt(Delta,0);return;
    }
    const float Duration=State.Phase==ERogueFrostBladePhase::Approach?Combat.ApproachSeconds:State.Phase==ERogueFrostBladePhase::Slash?Combat.SlashSeconds:Combat.ReturnSeconds;
    const float T=FMath::Clamp(Age/FMath::Max(.03f,Duration),0.f,1.f),Ease=T*T*(3-2*T);
    UpdateArt(Delta,Ease);
    const FRotator BeginSlash(0,State.AimYaw-Combat.SweepDegrees*.5f,0);
    if(State.Phase==ERogueFrostBladePhase::Approach)
    {
        SetActorLocation(ClampToOwner(FMath::Lerp(FVector(State.Start),FVector(State.Anchor),Ease)));
        SetActorRotation(FQuat::Slerp(State.Rotation.Quaternion(),BeginSlash.Quaternion(),Ease));
        if(HasAuthority()&&T>=1)
        {
            if(!Target.IsValid()||Target->GetSpawnCycleId()!=TargetCycle||Target->IsDead()||Target->IsAvailableInPool()){SetPhase(ERogueFrostBladePhase::Return);return;}
            SetPhase(ERogueFrostBladePhase::Slash);
        }
    }
    else if(State.Phase==ERogueFrostBladePhase::Slash)
    {
        SetActorLocation(ClampToOwner(State.Anchor));SetActorRotation(FRotator(0,BeginSlash.Yaw+Combat.SweepDegrees*Ease,0));
        if(HasAuthority())
        {
            DamageSweep(LastSweep,Ease);
            if(LastSweep<.5f&&Ease>=.5f)EmitWaves();LastSweep=Ease;
            if(T>=1)SetPhase(ERogueFrostBladePhase::Return);
        }
    }
    else
    {
        SetActorLocation(FMath::Lerp(FVector(State.Start),Home(),Ease));
        SetActorRotation(FQuat::Slerp(State.Rotation.Quaternion(),Rest.Quaternion(),Ease));
        if(HasAuthority()&&T>=1)SetPhase(ERogueFrostBladePhase::Hover);
    }
}
void ARogueFrostBlade::DamageSweep(float Previous,float Current)
{
    // One area pulse at the confirmed middle of the swing, including on a long
    // frame. Enemy bodies never stop the query and shield enemies behind them.
    if(Previous>=.5f||Current<.5f)return;
    const FVector Origin=GetActorLocation();
    const float Length=FMath::Max(1.f,Combat.BladeLength*BladeScale);
    const float Width=FMath::Max(1.f,Combat.HitRadius*BladeScale);
    const float HalfAngle=FMath::Clamp(Combat.SweepDegrees*.5f,0.f,180.f);
    const FVector Aim=FRotator(0,State.AimYaw,0).Vector();
    const FVector Left=FRotator(0,State.AimYaw-HalfAngle,0).Vector()*Length;
    const FVector Right=FRotator(0,State.AimYaw+HalfAngle,0).Vector()*Length;
    const float CosHalf=FMath::Cos(FMath::DegreesToRadians(HalfAngle));
    FCollisionQueryParams Q(SCENE_QUERY_STAT(FrostBladeSlashArea),false,this);Q.AddIgnoredActor(GetOwner());
    TArray<FOverlapResult> Hits;
    GetWorld()->OverlapMultiByObjectType(Hits,Origin,FQuat::Identity,FCollisionObjectQueryParams(ECC_Pawn),FCollisionShape::MakeSphere(Length+Width),Q);
    for(const auto& Hit:Hits)
    {
        auto* Enemy=Cast<ARogueEnemy>(Hit.GetActor());if(!IsValid(Enemy)||Enemy->IsDead()||Enemy->IsAvailableInPool())continue;
        const FGuid Cycle=Enemy->GetSpawnCycleId();if(const auto* Old=HitCycles.Find(Enemy);Old&&*Old==Cycle)continue;
        const auto* Capsule=Enemy->GetCapsuleComponent();
        FVector Offset=Enemy->GetActorLocation()-Origin;
        if(FMath::Abs(Offset.Z)>Width+Capsule->GetScaledCapsuleHalfHeight())continue;
        Offset.Z=0;
        const float Radius=Width+Capsule->GetScaledCapsuleRadius();
        const float Distance=Offset.Size2D();
        if(Distance>Length+Radius)continue;
        const bool InAngle=Distance<=UE_SMALL_NUMBER||FVector::DotProduct(Offset/Distance,Aim)>=CosHalf;
        if(!InAngle&&FMath::Min(FMath::PointDistToSegmentSquared(Offset,FVector::ZeroVector,Left),FMath::PointDistToSegmentSquared(Offset,FVector::ZeroVector,Right))>FMath::Square(Radius))continue;
        FHitResult Wall;if(GetWorld()->LineTraceSingleByObjectType(Wall,Origin,Enemy->GetActorLocation(),FCollisionObjectQueryParams(ECC_WorldStatic),Q))continue;
        HitCycles.Add(Enemy,Cycle);
        RogueAbilitySystem::ApplyDamageToTarget(GetOwner(),Enemy,Damage,RogueGameplayTags::Damage_Source_Melee,nullptr,uint8(ERogueWeaponImpactStyle::FrostBlade));
    }
}
void ARogueFrostBlade::EmitWaves()
{
    for(int32 I=0;I<WaveCount;++I)
    {
        auto* Wave=GetWorld()->GetSubsystem<URogueFrostWavePool>()->Acquire(WaveClass);if(!Wave)continue;
        const float Step=WaveCount>1?FMath::Min(Combat.WaveSpreadStepDegrees,Combat.MaxWaveSpreadDegrees/(WaveCount-1)):0;
        const FVector Direction=FRotator(0,State.AimYaw+(I-(WaveCount-1)*.5f)*Step,0).Vector();
        Wave->Launch(Cast<ARogueCharacter>(GetOwner()),GetActorLocation()+Direction*Combat.WaveSpawnOffset,Direction,WaveSpeed,WaveDamage,WaveScale,Combat.WaveLifetime,Combat.WaveHalfWidth,Combat.WaveHalfThickness,Combat.WaveHalfHeight);
    }
}
void ARogueFrostBlade::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps)const
{Super::GetLifetimeReplicatedProps(OutLifetimeProps);DOREPLIFETIME(ARogueFrostBlade,State);DOREPLIFETIME(ARogueFrostBlade,Combat);DOREPLIFETIME(ARogueFrostBlade,BladeScale);}
