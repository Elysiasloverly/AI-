#include "Combat/Visuals/RogueCombatAccentEffect.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "HAL/IConsoleManager.h"
#include "UObject/ConstructorHelpers.h"

#if !UE_BUILD_SHIPPING
static TAutoConsoleVariable<int32> CVarCombatAccentDebug(TEXT("Rogue.VFX.Debug"),0,TEXT("Log local cast/hit accents for multiplayer verification."));
#endif

ARogueCombatAccentEffect::ARogueCombatAccentEffect()
{
    PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.bStartWithTickEnabled=false;
    bReplicates=false;SetActorEnableCollision(false);
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
    Core=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CoreFlash"));
    Ring=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ExpandingRing"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Plane(TEXT("/Engine/BasicShapes/Plane.Plane"));
    auto Setup=[&](UStaticMeshComponent* C)
    {
        C->SetupAttachment(RootComponent);C->SetMobility(EComponentMobility::Movable);
        C->SetStaticMesh(Plane.Object);C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        C->SetGenerateOverlapEvents(false);C->SetCastShadow(false);C->bReceivesDecals=false;
        C->SetCanEverAffectNavigation(false);
    };
    Setup(Core);Setup(Ring);
    for(int I=0;I<6;++I)
    {
        auto* C=CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Shard%d"),I));Setup(C);Shards.Add(C);
    }
    SetActorHiddenInGame(true);
}

void ARogueCombatAccentEffect::EnsureMaterials()
{
    if(!Materials.IsEmpty() || !AccentMaterial)return;
    for(int I=0;I<3;++I)
    {
        auto* MID=UMaterialInstanceDynamic::Create(AccentMaterial,this);
        MID->SetScalarParameterValue(TEXT("Shape"),float(I));Materials.Add(MID);
    }
    Core->SetMaterial(0,Materials[0]);Ring->SetMaterial(0,Materials[1]);
    for(UStaticMeshComponent* S:Shards)S->SetMaterial(0,Materials[2]);
}

void ARogueCombatAccentEffect::Activate(const FRogueCombatAccentSettings& S,const FVector& Location,const FVector& Direction,bool bImpact)
{
    if(GetNetMode()==NM_DedicatedServer)return;
    Current=S;Current.Scale=FMath::Clamp(S.Scale,.1f,4.f);Current.Duration=FMath::Clamp(S.Duration,.05f,.6f);
    Current.ShardCount=FMath::Clamp(S.ShardCount,0,6);Age=0;bHit=bImpact;bAvailable=false;
    FlightDirection=Direction.GetSafeNormal();if(FlightDirection.IsNearlyZero())FlightDirection=FVector::ForwardVector;
    SetActorLocation(Location);SetActorScale3D(FVector::OneVector);EnsureMaterials();
    for(UMaterialInstanceDynamic* M:Materials) { M->SetVectorParameterValue(TEXT("Tint"),S.Tint);M->SetScalarParameterValue(TEXT("Opacity"),1.f); }
    for(int I=0;I<6;++I)
    {
        const float Angle=2.f*PI*(float(I)/6.f+(bHit?.04f:0.f));
        ShardDirections[I]=FVector2D(FMath::Cos(Angle),FMath::Sin(Angle));
    }
    SetActorHiddenInGame(false);SetActorTickEnabled(true);UpdateVisuals();
#if !UE_BUILD_SHIPPING
    if(CVarCombatAccentDebug.GetValueOnGameThread())
        UE_LOG(LogTemp,Display,TEXT("[CombatAccent] Mode=%d Kind=%s LocalOnly=%d Tint=%s Material=%d Parent=%s Core=%s"),int32(GetNetMode()),bHit?TEXT("Hit"):TEXT("Cast"),!GetIsReplicated(),*S.Tint.ToString(),Materials.Num()==3,*GetNameSafe(AccentMaterial),*GetNameSafe(Core->GetMaterial(0)));
#endif
}
void ARogueCombatAccentEffect::Deactivate()
{
    bAvailable=true;Age=0;SetActorHiddenInGame(true);SetActorTickEnabled(false);SetOwner(nullptr);
    SetActorEnableCollision(false);
}
void ARogueCombatAccentEffect::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);if(bAvailable)return;
    Age+=FMath::Max(0.f,DeltaSeconds);if(Age>=Current.Duration){Deactivate();return;}UpdateVisuals();
}
void ARogueCombatAccentEffect::UpdateVisuals()
{
    // 面向本机摄像机，低俯视角也能辨认星芒和扩散圈；没有灯光或相机震动。
    FRotator Facing(0,0,0);
    if(auto* PC=GetWorld()->GetFirstPlayerController();PC && PC->PlayerCameraManager)
        Facing=(PC->PlayerCameraManager->GetCameraLocation()-GetActorLocation()).Rotation();
    const FQuat PlaneRotation=Facing.Quaternion()*FRotator(90,0,0).Quaternion();
    SetActorRotation(PlaneRotation);
    const float P=FMath::Clamp(Age/Current.Duration,0.f,1.f),Fade=FMath::Square(1.f-P);
    const float Size=Current.Scale;
    const float Flash=(bHit?46.f:38.f)*Size*(1.f+.45f*P);
    Core->SetRelativeScale3D(FVector(Flash/100.f,Flash/100.f,1));Core->SetVisibility(true);
    const float RingSize=(bHit?46.f:55.f)*Size*(.4f+1.9f*P);
    Ring->SetRelativeScale3D(FVector(RingSize/100.f,RingSize/100.f,1));Ring->SetVisibility(Current.bShowRing);
    if(Materials.Num()==3)
    {
        Materials[0]->SetScalarParameterValue(TEXT("Opacity"),Fade);
        Materials[1]->SetScalarParameterValue(TEXT("Opacity"),Fade*.55f);
        Materials[2]->SetScalarParameterValue(TEXT("Opacity"),Fade*.85f);
    }
    const FVector LocalAim=GetActorQuat().UnrotateVector(FlightDirection).GetSafeNormal2D();
    const float AimAngle=FMath::Atan2(LocalAim.Y,LocalAim.X);
    for(int I=0;I<Shards.Num();++I)
    {
        UStaticMeshComponent* Shard=Shards[I];Shard->SetVisibility(I<Current.ShardCount);if(I>=Current.ShardCount)continue;
        const float A=FMath::Atan2(ShardDirections[I].Y,ShardDirections[I].X)+AimAngle;
        const FVector V(FMath::Cos(A),FMath::Sin(A),0);
        Shard->SetRelativeLocation(V*((bHit?6.f:3.f)+(bHit?65.f:42.f)*P)*Size);
        Shard->SetRelativeRotation(FRotator(0,FMath::RadiansToDegrees(A),0));
        Shard->SetRelativeScale3D(FVector((18.f*(1.f-P)+3.f)*Size/100.f,4.f*Size/100.f,1));
    }
}
