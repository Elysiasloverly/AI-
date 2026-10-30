#include "Enemies/Components/RogueEnemyPresentationComponent.h"
#include "Enemies/RogueEnemy.h"
#include "Enemies/Visuals/RogueEnemyEffectVisual.h"
#include "Components/MeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
URogueEnemyPresentationComponent::URogueEnemyPresentationComponent()
{ PrimaryComponentTick.bCanEverTick=true;PrimaryComponentTick.bStartWithTickEnabled=false;PrimaryComponentTick.TickInterval=.05f; }
ARogueEnemyEffectVisual* URogueEnemyPresentationComponent::EnsureVisual(TObjectPtr<ARogueEnemyEffectVisual>& V,TSubclassOf<ARogueEnemyEffectVisual> C)
{
    if (!V&&C&&GetNetMode()!=NM_DedicatedServer)
    { FActorSpawnParameters P;P.Owner=GetOwner();P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;V=GetWorld()->SpawnActor<ARogueEnemyEffectVisual>(C,FTransform(),P); }
    return V;
}
void URogueEnemyPresentationComponent::ResetForPool()
{
    for (const auto& Item:Scales) if (auto* M=Item.Mesh.Get()) { M->SetRelativeScale3D(Item.Value); }
    Scales.Reset();bScaled=false;Generation=MAX_uint32;
    if (BossVisual) BossVisual->SetActorHiddenInGame(true);
    if (FlyingVisual) FlyingVisual->SetActorHiddenInGame(true);
    SetComponentTickEnabled(false);
}
void URogueEnemyPresentationComponent::RefreshRuntime()
{ SetComponentTickEnabled(true);TickComponent(0,LEVELTICK_All,nullptr); }
void URogueEnemyPresentationComponent::TickComponent(float D,ELevelTick T,FActorComponentTickFunction* F)
{
    Super::TickComponent(D,T,F);
    auto* E=Cast<ARogueEnemy>(GetOwner());
    if (!E||E->IsAvailableInPool()||E->IsDead()) { ResetForPool();return; }
    if (Generation!=E->GetPresentationGeneration())
    { ResetForPool();Generation=E->GetPresentationGeneration();SetComponentTickEnabled(true); }
    if (E->IsBoss()&&!bScaled)
    {
        for (auto* M:TInlineComponentArray<UMeshComponent*>(E))
        {
            // 内置柱体已经由原型放大；其余用户模型保留原比例后追加Boss倍率。
            if (M->GetFName()==TEXT("ShockPillarMesh")||M->ComponentHasTag(TEXT("Rogue.DamageArea"))) continue;
            bool HasMeshParent=false;
            for (auto* P=M->GetAttachParent();P;P=P->GetAttachParent())
                if (Cast<UMeshComponent>(P)) { HasMeshParent=true;break; }
            if (HasMeshParent) continue;
            Scales.Add({M,M->GetRelativeScale3D()});M->SetRelativeScale3D(M->GetRelativeScale3D()*FMath::Clamp(BossModelScale,1.f,3.f));
        }
        bScaled=true;
    }
    const FVector Pos=E->GetActorLocation();
    const float Half=E->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    if (E->IsBoss())
    {
        float Top=Pos.Z+Half;
        for (auto* M:TInlineComponentArray<UMeshComponent*>(E))
            if (M->IsVisible()&&!M->bHiddenInGame) Top=FMath::Max(Top,float(M->Bounds.Origin.Z+M->Bounds.BoxExtent.Z));
        if (auto* V=EnsureVisual(BossVisual,BossVisualClass))
            V->ConfigureMarker(FVector(Pos.X,Pos.Y,Pos.Z-Half+5),FMath::Max(120.f,E->GetCapsuleComponent()->GetScaledCapsuleRadius()*2),Top+65);
    }
    else if (BossVisual) BossVisual->SetActorHiddenInGame(true);
    if (RogueEnemyArchetypes::IsFlyingType(E->GetEnemyType()))
    {
        if (auto* V=EnsureVisual(FlyingVisual,FlyingVisualClass))
            V->ConfigureCircle(FVector(Pos.X,Pos.Y,E->GetSpawnGroundZ()-Half+5),65);
    }
    else if (FlyingVisual) FlyingVisual->SetActorHiddenInGame(true);
}
void URogueEnemyPresentationComponent::EndPlay(const EEndPlayReason::Type R)
{ if (BossVisual) BossVisual->Destroy();if (FlyingVisual) FlyingVisual->Destroy();Super::EndPlay(R); }
