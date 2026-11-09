#include "Combat/Visuals/RogueGroundEffectFade.h"

#include "Components/DecalComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"

ARogueGroundEffectFade::ARogueGroundEffectFade()
{
    bReplicates=false;
    PrimaryActorTick.bCanEverTick=true;
    SetActorEnableCollision(false);
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("FadeRoot")));
}

ARogueGroundEffectFade* ARogueGroundEffectFade::CaptureActor(AActor* Source, float Seconds)
{
    if (!Source || Source->IsHidden()) return nullptr;
    TInlineComponentArray<USceneComponent*> Components(Source);
    return CaptureComponents(Source->GetWorld(), Components, Seconds);
}

ARogueGroundEffectFade* ARogueGroundEffectFade::CaptureComponents(UWorld* World,
    TConstArrayView<USceneComponent*> Sources, float Seconds)
{
    if (!World || World->bIsTearingDown || World->GetNetMode()==NM_DedicatedServer) return nullptr;
    bool HasVisibleComponent=false;
    for (auto* Source:Sources)
    {
        if (Source && Source->IsVisible() && !Source->bHiddenInGame && Source->IsRegistered()
            && (!Source->GetOwner() || !Source->GetOwner()->IsHidden())
            && (Cast<UStaticMeshComponent>(Source) || Cast<UDecalComponent>(Source)))
        {
            HasVisibleComponent=true;
            break;
        }
    }
    if (!HasVisibleComponent) return nullptr;
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Fade=World->SpawnActor<ARogueGroundEffectFade>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
    if (!Fade) return nullptr;
    Fade->Duration=FMath::Clamp(FMath::IsFinite(Seconds)?Seconds:.6f,.01f,2.f);
    for (auto* Source:Sources) Fade->CopyComponent(Source);
    if (Fade->Materials.IsEmpty())
    {
        Fade->Destroy();
        return nullptr;
    }
    Fade->SetLifeSpan(Fade->Duration+.05f);
    return Fade;
}

UMaterialInstanceDynamic* ARogueGroundEffectFade::CopyMaterial(UMaterialInterface* Source)
{
    float InitialFade=1.f;
    if (!Source || !Source->GetScalarParameterValue(FMaterialParameterInfo(TEXT("GroundFade")),InitialFade)
        || InitialFade<=.002f) return nullptr;
    float Opacity=1.f;
    if (Source->GetScalarParameterValue(FMaterialParameterInfo(TEXT("Opacity")),Opacity) && Opacity<=.002f) return nullptr;
    UMaterialInterface* Parent=Source;
    while (const auto* Dynamic=Cast<UMaterialInstanceDynamic>(Parent)) Parent=Dynamic->Parent;
    auto* Material=UMaterialInstanceDynamic::Create(Parent,this);
    // 固定回收瞬间的参数，源材质被对象池复用后也不会让旧残影跳色或闪亮。
    Material->CopyMaterialUniformParameters(Source);
    Materials.Add(Material);
    OriginalFade.Add(InitialFade);
    return Material;
}

void ARogueGroundEffectFade::CopyComponent(USceneComponent* Source)
{
    if (!Source || !Source->IsVisible() || Source->bHiddenInGame || !Source->IsRegistered()
        || (Source->GetOwner() && Source->GetOwner()->IsHidden())) return;
    if (auto* Mesh=Cast<UStaticMeshComponent>(Source))
    {
        if (!Mesh->GetStaticMesh()) return;
        UStaticMeshComponent* Copy=nullptr;
        for (int32 Index=0;Index<Mesh->GetNumMaterials();++Index)
        {
            auto* Material=CopyMaterial(Mesh->GetMaterial(Index));
            if (!Material) continue;
            if (!Copy)
            {
                Copy=NewObject<UStaticMeshComponent>(this);
                Copy->SetupAttachment(RootComponent);
                Copy->SetStaticMesh(Mesh->GetStaticMesh());
                Copy->SetCollisionEnabled(ECollisionEnabled::NoCollision);
                Copy->SetGenerateOverlapEvents(false);
                Copy->SetCanEverAffectNavigation(false);
                Copy->SetCastShadow(false);
                Copy->bReceivesDecals=false;
                Copy->SetTranslucentSortPriority(Mesh->TranslucencySortPriority);
                Copy->SetWorldTransform(Mesh->GetComponentTransform());
                const auto& Data=Mesh->GetCustomPrimitiveData().Data;
                for (int32 DataIndex=0;DataIndex<Data.Num();++DataIndex)
                    Copy->SetCustomPrimitiveDataFloat(DataIndex,Data[DataIndex]);
                Copy->RegisterComponent();
            }
            Copy->SetMaterial(Index,Material);
        }
    }
    else if (auto* Decal=Cast<UDecalComponent>(Source))
    {
        if (auto* Material=CopyMaterial(Decal->GetDecalMaterial()))
        {
            auto* Copy=NewObject<UDecalComponent>(this);
            Copy->SetupAttachment(RootComponent);
            Copy->DecalSize=Decal->DecalSize;
            Copy->SortOrder=Decal->SortOrder;
            Copy->SetDecalMaterial(Material);
            Copy->SetWorldTransform(Decal->GetComponentTransform());
            Copy->RegisterComponent();
        }
    }
}

void ARogueGroundEffectFade::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    Age+=FMath::Max(0.f,DeltaSeconds);
    const float Fade=1.f-FMath::SmoothStep(0.f,Duration,Age);
    for (int32 Index=0;Index<Materials.Num();++Index)
        Materials[Index]->SetScalarParameterValue(TEXT("GroundFade"),OriginalFade[Index]*Fade);
    if (Age>=Duration) Destroy();
}
