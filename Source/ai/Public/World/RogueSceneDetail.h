#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RogueSceneDetail.generated.h"
class UInstancedStaticMeshComponent;

/** Authored map decoration; material animation, bounded instancing, no collision/navigation/replication. */
UCLASS(Blueprintable,meta=(DisplayName="主题景物与局部动态"))
class AI_API ARogueSceneDetail : public AActor
{
    GENERATED_BODY()
public:
    ARogueSceneDetail();
    virtual void BeginPlay() override;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="主题景物") TObjectPtr<UInstancedStaticMeshComponent> Props;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="主题景物") TObjectPtr<UInstancedStaticMeshComponent> GroundAccents;
};
