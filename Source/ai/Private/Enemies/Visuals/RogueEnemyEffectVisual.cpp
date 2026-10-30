#include "Enemies/Visuals/RogueEnemyEffectVisual.h"
#include "Components/SceneComponent.h"
#include "Components/DecalComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
ARogueEnemyEffectVisual::ARogueEnemyEffectVisual()
{
    PrimaryActorTick.bCanEverTick=false;bReplicates=false;
    RootComponent=CreateDefaultSubobject<USceneComponent>(TEXT("VisualRoot"));
    SetActorEnableCollision(false);
}
void ARogueEnemyEffectVisual::BeginPlay()
{
    Super::BeginPlay();SetActorEnableCollision(false);
    for (auto* Mesh:TInlineComponentArray<UStaticMeshComponent*>(this))
    { Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);Mesh->SetGenerateOverlapEvents(false);Mesh->SetSimulatePhysics(false); }
}
UStaticMeshComponent* ARogueEnemyEffectVisual::GetMain()
{
    if (!Main)
        for (auto* Mesh:TInlineComponentArray<UStaticMeshComponent*>(this))
            if (Mesh->ComponentHasTag(MainMeshTag)) { Main=Mesh;break; }
    return Main;
}
void ARogueEnemyEffectVisual::ConfigureCircle(FVector Center,float Radius)
{
    SetActorLocation(Center);SetActorRotation(FRotator::ZeroRotator);
    if (auto* M=GetMain();M&&M->GetStaticMesh())
    {
        const auto Extent=M->GetStaticMesh()->GetBoundingBox().GetExtent();
        const auto Scale=M->GetRelativeScale3D();
        M->SetRelativeScale3D(FVector(Radius/FMath::Max(Extent.X,1.),Radius/FMath::Max(Extent.Y,1.),Scale.Z));
    }
    // 投影覆盖地面高低差；半径仍由治疗/标记配置驱动，深度由蓝图决定。
    for (auto* Decal:TInlineComponentArray<UDecalComponent*>(this))
        if (Decal->ComponentHasTag(MainMeshTag))
        {
            const FVector Size(Decal->DecalSize.X,Radius,Radius);
            if (!Decal->DecalSize.Equals(Size,.01))
            { Decal->DecalSize=Size;Decal->MarkRenderStateDirty(); }
        }
    SetActorHiddenInGame(false);
}
void ARogueEnemyEffectVisual::ConfigureBeam(FVector Start,FVector End)
{
    const FVector D=End-Start;SetActorLocation((Start+End)*.5);
    SetActorRotation(FRotationMatrix::MakeFromZ(D.GetSafeNormal()).Rotator());
    if (auto* M=GetMain();M&&M->GetStaticMesh())
    {
        const double Length=M->GetStaticMesh()->GetBoundingBox().GetSize().Z;
        const auto S=M->GetRelativeScale3D();M->SetRelativeScale3D(FVector(S.X,S.Y,D.Size()/FMath::Max(Length,1.)));
    }
    SetActorHiddenInGame(false);
}
void ARogueEnemyEffectVisual::ConfigureMarker(FVector Center,float Radius,float Top)
{
    ConfigureCircle(Center,Radius);
    for (auto* Mesh:TInlineComponentArray<UStaticMeshComponent*>(this))
        if (Mesh->ComponentHasTag(MarkerMeshTag))
        { FVector P=Mesh->GetRelativeLocation();P.Z=Top-Center.Z;Mesh->SetRelativeLocation(P); }
}
