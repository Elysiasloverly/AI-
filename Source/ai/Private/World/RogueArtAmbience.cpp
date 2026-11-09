#include "World/RogueArtAmbience.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"

ARogueArtAmbience::ARogueArtAmbience()
{
    PrimaryActorTick.bCanEverTick=false;
    bReplicates=false;
    SetActorEnableCollision(false);
    Motes=CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Motes"));
    SetRootComponent(Motes);
    Motes->SetMobility(EComponentMobility::Static);
    Motes->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Motes->SetGenerateOverlapEvents(false);
    Motes->SetCanEverAffectNavigation(false);
    Motes->SetCastShadow(false);
    Motes->bReceivesDecals=false;
    Motes->BoundsScale=1.12f;
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Plane(TEXT("/Engine/BasicShapes/Plane.Plane"));
    Motes->SetStaticMesh(Plane.Object);
}

void ARogueArtAmbience::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    Rebuild();
}

void ARogueArtAmbience::Rebuild()
{
    if(!Motes)return;
    Motes->ClearInstances();
    Motes->SetCullDistances(FMath::Max(0,DrawDistance-1500),FMath::Max(100,DrawDistance));
    FRandomStream Random(Seed);
    const float Small=FMath::Max(1.f,MinSize),Large=FMath::Max(Small,MaxSize);
    for(int32 I=0;I<FMath::Clamp(Count,0,192);++I)
    {
        const FVector Location(Random.FRandRange(-FMath::Abs(Extent.X),FMath::Abs(Extent.X)),
            Random.FRandRange(-FMath::Abs(Extent.Y),FMath::Abs(Extent.Y)),
            Random.FRandRange(-FMath::Abs(Extent.Z),FMath::Abs(Extent.Z)));
        const float Size=Random.FRandRange(Small,Large)/100.f;
        // Broad face angled toward the overhead gameplay view, rather than edge-on vertical cards.
        const FRotator Rotation(Random.FRandRange(-35.f,35.f),Random.FRandRange(0.f,360.f),0);
        Motes->AddInstance(FTransform(Rotation,Location,FVector(Size,Size,Size)));
    }
}

void ARogueArtAmbience::BeginPlay()
{
    Super::BeginPlay();
    if(GetNetMode()==NM_DedicatedServer)Motes->ClearInstances();
}
