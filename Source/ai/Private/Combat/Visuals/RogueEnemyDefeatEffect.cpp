#include "Combat/Visuals/RogueEnemyDefeatEffect.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Combat/Visuals/RogueCombatCameraModifier.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

ARogueEnemyDefeatEffect::ARogueEnemyDefeatEffect()
{
    PrimaryActorTick.bCanEverTick=false;bReplicates=false;SetActorEnableCollision(false);
    Particles=CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("SpiritParticles"));SetRootComponent(Particles);
    Particles->SetMobility(EComponentMobility::Movable);Particles->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Particles->SetGenerateOverlapEvents(false);Particles->SetCanEverAffectNavigation(false);Particles->SetCastShadow(false);Particles->bReceivesDecals=false;
    Particles->NumCustomDataFloats=9;Particles->BoundsScale=4;
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Plane(TEXT("/Engine/BasicShapes/Plane.Plane"));Particles->SetStaticMesh(Plane.Object);
    SetActorHiddenInGame(true);
}
void ARogueEnemyDefeatEffect::AddParticle(float Birth,float Life,int32 Kind,float Seed,FLinearColor Color,float Size,FVector2D Quad)
{
    const int32 I=Particles->AddInstance(FTransform(FQuat::Identity,FVector::ZeroVector,FVector(Quad.X,Quad.Y,1)*Size));
    const float Data[]={Birth,Life,float(Kind),Seed,Color.R,Color.G,Color.B,0,Size};
    for(int32 J=0;J<9;++J)Particles->SetCustomDataValue(I,J,Data[J],false);
}
void ARogueEnemyDefeatEffect::ActivateDefeat(FVector Location,bool bBoss,float Size)
{
    UWorld* W=GetWorld();if(!W || W->GetNetMode()==NM_DedicatedServer || !DefeatMaterial || Location.ContainsNaN())return;
    W->GetTimerManager().ClearTimer(ReturnTimer);bAvailable=false;Particles->ClearInstances();Particles->SetMaterial(0,DefeatMaterial);
    SetActorLocation(Location);SetActorScale3D(FVector::OneVector);
    FRotator Facing(0,0,0);
    if(auto* PC=W->GetFirstPlayerController();PC && PC->PlayerCameraManager)Facing=(PC->PlayerCameraManager->GetCameraLocation()-Location).Rotation();
    SetActorRotation(Facing.Quaternion()*FRotator(90,0,0).Quaternion());
    const float Life=FMath::Clamp(bBoss?BossDuration:Duration,.2f,3.f),S=FMath::Clamp(Size,.25f,4.f)*(bBoss?FMath::Clamp(BossSize,1.f,4.f):1.f);
    const FLinearColor Color=bBoss?BossTint:Tint;const float Birth=W->GetTimeSeconds();
    AddParticle(Birth,FMath::Min(.20f,Life),0,.1f,Color,S,FVector2D(.75f));
    AddParticle(Birth,Life*.78f,1,.3f,Color,S,FVector2D(1.8f));
    if(bBoss)AddParticle(Birth+.08f,Life*.75f,1,.76f,Color,S,FVector2D(2.35f));
    AddParticle(Birth,Life,3,.6f,Color,S,FVector2D(1.7f));
    const int32 Count=FMath::Clamp(ShardCount+(bBoss?4:0),0,16);
    for(int32 I=0;I<Count;++I)
        AddParticle(Birth+.012f*(I%3),Life*(.7f+.28f*float(I%3)/2),2,(I+.4f)/FMath::Max(1,Count),Color,S,FVector2D(.22f,.33f));
    for(int32 I=0;I<3;++I)AddParticle(Birth,Life*.72f,4,(I+.7f)/3,Color,S,FVector2D(.22f));
    Particles->MarkRenderStateDirty();SetActorHiddenInGame(false);
    if(bBoss&&bBossCameraKick)URogueCombatCameraModifier::PlayLocal(W,Location,BossCameraKickStrength);
    W->GetTimerManager().SetTimer(ReturnTimer,this,&ARogueEnemyDefeatEffect::Deactivate,Life+.05f,false);
}
void ARogueEnemyDefeatEffect::Deactivate()
{
    if(UWorld* W=GetWorld())W->GetTimerManager().ClearTimer(ReturnTimer);
    bAvailable=true;SetActorHiddenInGame(true);Particles->ClearInstances();SetOwner(nullptr);SetActorEnableCollision(false);
}
