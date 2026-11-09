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
    VisualSeed=FMath::Frac(FMath::Abs(Location.X*.013f+Location.Y*.037f+GetWorld()->GetTimeSeconds()*.31f));
    FlightDirection=Direction.GetSafeNormal();if(FlightDirection.IsNearlyZero())FlightDirection=FVector::ForwardVector;
    SetActorLocation(Location);SetActorScale3D(FVector::OneVector);EnsureMaterials();
    for(UMaterialInstanceDynamic* M:Materials)
    {M->SetVectorParameterValue(TEXT("Tint"),S.Tint);M->SetScalarParameterValue(TEXT("Opacity"),1.f);M->SetScalarParameterValue(TEXT("Seed"),VisualSeed);}
    // Per-burst orientation and seed are constant; animate the shader and shards
    // without updating two additional primitive transforms every frame.
    const bool Refined=MotionProfile!=ERogueAccentMotionProfile::Legacy;
    Core->SetRelativeRotation(FRotator(0,Refined?VisualSeed*360:0,0));
    Ring->SetRelativeRotation(FRotator(0,Refined?VisualSeed*180:0,0));
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
    const float P=FMath::Clamp(Age/Current.Duration,0.f,1.f);
    const bool Refined=MotionProfile!=ERogueAccentMotionProfile::Legacy;
    const bool Burst=MotionProfile==ERogueAccentMotionProfile::Burst;
    const bool Strike=MotionProfile==ERogueAccentMotionProfile::Strike;
    const float Attack=FMath::SmoothStep(0.f,Refined?.055f:.16f,P);
    const float Travel=1.f-FMath::Pow(1.f-P,3.f);
    const float Fade=Attack*FMath::Square(1.f-P);
    const float Size=Current.Scale;
    const float CoreSize=FMath::Clamp(CoreSizeMultiplier,.2f,3.f);
    const float RingScale=FMath::Clamp(RingSizeMultiplier,.2f,3.f);
    const float Reach=FMath::Clamp(ShardDistanceMultiplier,.2f,3.f);
    const float Flash=(bHit?46.f:38.f)*Size*CoreSize*(.55f+.7f*(1.f-FMath::Exp(-P*(Refined?24.f:12.f))))*FMath::Lerp(1.f,Burst?1.22f:.7f,P);
    Core->SetRelativeScale3D(FVector(Flash/100.f,Flash/100.f,1));Core->SetVisibility(true);
    const float RingTravel=Refined?FMath::SmoothStep(.04f,.82f,P):Travel;
    const float RingSize=(bHit?46.f:55.f)*Size*RingScale*(.32f+(Burst?2.2f:1.75f)*RingTravel);
    Ring->SetRelativeScale3D(FVector(RingSize/100.f,RingSize/100.f,1));Ring->SetVisibility(Current.bShowRing);
    if(Materials.Num()==3)
    {
        for(UMaterialInstanceDynamic* Material:Materials)
        {
            Material->SetScalarParameterValue(TEXT("Progress"),P);
        }
        const float CoreFade=Burst?Attack*(1-FMath::SmoothStep(.14f,.66f,P))*.90f:Fade;
        const float RingFade=Refined?FMath::SmoothStep(.08f,.24f,P)*FMath::Pow(1-P,1.35f)*.58f:Fade*.55f;
        const float ShardFade=Refined?FMath::SmoothStep(.05f,.18f,P)*FMath::Pow(1-P,1.65f)*.85f:Fade*.85f;
        Materials[0]->SetScalarParameterValue(TEXT("Opacity"),CoreFade);
        Materials[1]->SetScalarParameterValue(TEXT("Opacity"),RingFade);
        Materials[2]->SetScalarParameterValue(TEXT("Opacity"),ShardFade);
    }
    const FVector LocalAim=GetActorQuat().UnrotateVector(FlightDirection).GetSafeNormal2D();
    const float AimAngle=FMath::Atan2(LocalAim.Y,LocalAim.X);
    for(int I=0;I<Shards.Num();++I)
    {
        UStaticMeshComponent* Shard=Shards[I];Shard->SetVisibility(I<Current.ShardCount);if(I>=Current.ShardCount)continue;
        const float Seed=Refined?VisualSeed*.9f+I*.37f:0;
        const float A=FMath::Atan2(ShardDirections[I].Y,ShardDirections[I].X)+AimAngle+Seed*.28f;
        const FVector V(FMath::Cos(A),FMath::Sin(A),0);
        const float Launch=Refined?1-FMath::Pow(1-FMath::SmoothStep(.055f,1.f,P),2.5f):Travel;
        const float Distance=((bHit?6.f:3.f)+(bHit?65.f:42.f)*Launch)*(Refined?(.72f+.32f*FMath::Sin(I*2.1f+VisualSeed*5)):1);
        Shard->SetRelativeLocation((V*Distance*Reach+FVector(0,8.f*FMath::Sin(P*PI+I),0))*Size);
        Shard->SetRelativeRotation(FRotator(0,FMath::RadiansToDegrees(A)+P*(I%2?22.f:-18.f),0));
        const float Length=Refined?FMath::Lerp(28.f,3.f,Launch)*(Strike?1.15f:1.f):(18.f*(1.f-P)+3.f);
        Shard->SetRelativeScale3D(FVector(Length*Size/100.f,(Refined?3.f:4.f)*Size/100.f,1));
    }
}
