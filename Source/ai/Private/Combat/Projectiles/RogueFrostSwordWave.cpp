#include "Combat/Projectiles/RogueFrostSwordWave.h"
#include "AbilitySystem/RogueAbilitySystemLibrary.h"
#include "AbilitySystem/RogueGameplayTags.h"
#include "Combat/Visuals/RogueCombatAccentEffect.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Enemies/RogueEnemy.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"
#include "Player/RogueCharacter.h"
#include "Combat/Pooling/RogueCombatPoolSubsystem.h"

ARogueFrostSwordWave* URogueFrostWavePool::Acquire(TSubclassOf<ARogueFrostSwordWave> Class)
{
    if(!Class)Class=ARogueFrostSwordWave::StaticClass();
    Waves.RemoveAll([](const auto& A){return !IsValid(A);});
    for(ARogueFrostSwordWave* Wave:Waves)if(Wave->GetClass()==Class&&Wave->IsAvailableInPool())return Wave;
    FActorSpawnParameters P;P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Wave=GetWorld()->SpawnActor<ARogueFrostSwordWave>(Class,FVector::ZeroVector,FRotator::ZeroRotator,P);
    if(Wave)Waves.Add(Wave);
    return Wave;
}
void URogueFrostWavePool::Release(ARogueFrostSwordWave* Wave)
{
    if(!Waves.Contains(Wave))return;
    int32 Idle=0;for(const auto& Item:Waves)if(IsValid(Item)&&Item->IsAvailableInPool())++Idle;
    // Limit retained idle actors only. This never shortens an active sword wave's lifetime.
    if(Idle>24){Waves.RemoveSingleSwap(Wave);Wave->Destroy();}
}
ARogueFrostSwordWave::ARogueFrostSwordWave()
{
    bReplicates=true;SetReplicateMovement(false);PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.bStartWithTickEnabled=false;
    NetUpdateFrequency=15;MinNetUpdateFrequency=2;SetActorEnableCollision(false);
    // Keep gameplay size on a child component. A scaled root would also be sent
    // in the initial network spawn transform and applied a second time by State.
    auto* Root=CreateDefaultSubobject<USceneComponent>(TEXT("Root"));SetRootComponent(Root);
    Mesh=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WaveMesh"));Mesh->SetupAttachment(Root);
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);Mesh->SetGenerateOverlapEvents(false);Mesh->SetCanEverAffectNavigation(false);Mesh->SetCastShadow(false);
    SetActorHiddenInGame(true);WallAccent.bEnabled=false;
}
void ARogueFrostSwordWave::PostInitializeComponents(){Super::PostInitializeComponents();AuthoredScale=Mesh->GetRelativeScale3D();}
void ARogueFrostSwordWave::BeginPlay(){Super::BeginPlay();if(State.bActive)OnRep_State();}
float ARogueFrostSwordWave::GetWaveAge()const
{
    const auto* GS=GetWorld()?GetWorld()->GetGameState():nullptr;
    return GetWorld()?FMath::Max(0.f,(GS?GS->GetServerWorldTimeSeconds():GetWorld()->GetTimeSeconds())-State.Born):0;
}
void ARogueFrostSwordWave::Launch(ARogueCharacter* Player,const FVector& Start,const FVector& Direction,float Speed,float InDamage,float Size,float Lifetime,float HalfWidth,float Thickness,float HalfHeight)
{
    if(!HasAuthority()||!Player)return;
    SetOwner(Player);SetInstigator(Player);HitCycles.Reset();Damage=FMath::Max(0.f,InDamage);
    const uint32 Serial=State.Serial+1;State=FRogueFrostWaveState();State.Serial=Serial;State.bActive=true;
    State.Origin=Start;State.Direction=Direction.GetSafeNormal2D();State.Born=GetWorld()->GetTimeSeconds();State.Speed=FMath::Max(1.f,Speed);
    State.Scale=FMath::Max(.1f,Size);State.Lifetime=FMath::Max(.01f,Lifetime);State.HalfWidth=FMath::Max(1.f,HalfWidth)*State.Scale;
    // The crescent grows in its horizontal plane; vertical tolerance stays fixed,
    // otherwise enough size cards make an airborne wave immediately hit the floor.
    State.HalfThickness=FMath::Max(1.f,Thickness)*State.Scale;State.HalfHeight=FMath::Max(1.f,HalfHeight);
    SetActorLocation(Start);OnRep_State();ForceNetUpdate();
}
void ARogueFrostSwordWave::OnRep_State()
{
    SetActorHiddenInGame(!State.bActive);SetActorTickEnabled(State.bActive);
    const auto* GS=GetWorld()->GetGameState();const float ServerNow=GS?GS->GetServerWorldTimeSeconds():GetWorld()->GetTimeSeconds();
    if(!State.bActive&&State.bHitWorld&&LastWallAccentSerial!=State.Serial&&ServerNow-State.StopTime<.35f)
    {
        LastWallAccentSerial=State.Serial;
        if(GetNetMode()!=NM_DedicatedServer)if(auto* Pool=GetWorld()->GetSubsystem<URogueCombatPoolSubsystem>())Pool->PlayCombatAccent(WallAccent,State.StopLocation,State.Direction,true);
    }
    if(State.bActive)
    {
        SetActorLocation(FVector(State.Origin)+FVector(State.Direction)*State.Speed*GetWaveAge());
        SetActorRotation(FVector(State.Direction).Rotation());Mesh->SetRelativeScale3D(AuthoredScale*State.Scale);
        Mesh->SetCustomPrimitiveDataVector4(0,FVector4(GetWaveAge(),State.Lifetime,State.Serial%31,1));
    }
}
void ARogueFrostSwordWave::Deactivate()
{
    if(!HasAuthority()||!State.bActive)return;
    State.bActive=false;HitCycles.Reset();OnRep_State();ForceNetUpdate();
    GetWorld()->GetSubsystem<URogueFrostWavePool>()->Release(this);
}
void ARogueFrostSwordWave::Tick(float Delta)
{
    Super::Tick(Delta);if(!State.bActive)return;
    const float Age=GetWaveAge();
    if(Age>=State.Lifetime){if(HasAuthority())Deactivate();else{SetActorHiddenInGame(true);SetActorTickEnabled(false);}return;}
    if(GetNetMode()!=NM_DedicatedServer)Mesh->SetCustomPrimitiveDataFloat(0,Age);
    const FVector Start=GetActorLocation();FVector End=FVector(State.Origin)+FVector(State.Direction)*State.Speed*Age;
    if(HasAuthority())
    {
        FCollisionQueryParams Q(SCENE_QUERY_STAT(FrostSwordWave),false,this);Q.AddIgnoredActor(GetOwner());
        const FQuat Rotation=FVector(State.Direction).Rotation().Quaternion();
        const auto Shape=FCollisionShape::MakeBox(FVector(State.HalfThickness,State.HalfWidth,State.HalfHeight));
        FHitResult Wall;
        const bool bWall=GetWorld()->SweepSingleByObjectType(Wall,Start,End,Rotation,FCollisionObjectQueryParams(ECC_WorldStatic),Shape,Q);
        if(bWall)End=Wall.Location;
        TArray<FHitResult> Hits;
        GetWorld()->SweepMultiByObjectType(Hits,Start,End,Rotation,FCollisionObjectQueryParams(ECC_Pawn),Shape,Q);
        for(const auto& Hit:Hits)
        {
            auto* Enemy=Cast<ARogueEnemy>(Hit.GetActor());if(!IsValid(Enemy)||Enemy->IsDead()||Enemy->IsAvailableInPool())continue;
            const FGuid Cycle=Enemy->GetSpawnCycleId();if(const auto* Old=HitCycles.Find(Enemy);Old&&*Old==Cycle)continue;
            HitCycles.Add(Enemy,Cycle);
            RogueAbilitySystem::ApplyDamageToTarget(GetOwner(),Enemy,Damage,RogueGameplayTags::Damage_Source_Projectile,nullptr,uint8(ERogueWeaponImpactStyle::FrostBlade));
        }
        if(bWall){State.bHitWorld=true;State.StopLocation=End;State.StopTime=GetWorld()->GetTimeSeconds();Deactivate();return;}
    }
    SetActorLocation(End);
}
void ARogueFrostSwordWave::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps)const
{Super::GetLifetimeReplicatedProps(OutLifetimeProps);DOREPLIFETIME(ARogueFrostSwordWave,State);}
