#include "World/RogueSceneDetail.h"
#include "Components/InstancedStaticMeshComponent.h"
ARogueSceneDetail::ARogueSceneDetail()
{
    PrimaryActorTick.bCanEverTick=false;bReplicates=false;SetActorEnableCollision(false);
    Props=CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("ThemeProps"));SetRootComponent(Props);
    GroundAccents=CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("GroundAccents"));GroundAccents->SetupAttachment(Props);
    for(auto* C:{Props.Get(),GroundAccents.Get()})
    {C->SetMobility(EComponentMobility::Static);C->SetCollisionEnabled(ECollisionEnabled::NoCollision);C->SetGenerateOverlapEvents(false);C->SetCanEverAffectNavigation(false);C->SetCastShadow(false);C->bReceivesDecals=false;C->SetCullDistances(6500,9000);C->BoundsScale=1.08f;}
}
void ARogueSceneDetail::BeginPlay()
{
    Super::BeginPlay();if(GetNetMode()==NM_DedicatedServer){Props->ClearInstances();GroundAccents->ClearInstances();}
}
