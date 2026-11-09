#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RogueArtAmbience.generated.h"

class UInstancedStaticMeshComponent;

/** Visual-only, deterministic instanced petals / leaves / glints. Animation lives in the material. */
UCLASS(Blueprintable, meta=(DisplayName="绘本环境微粒"))
class AI_API ARogueArtAmbience : public AActor
{
    GENERATED_BODY()
public:
    ARogueArtAmbience();
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="氛围")
    TObjectPtr<UInstancedStaticMeshComponent> Motes;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="氛围", meta=(DisplayName="随机种子"))
    int32 Seed=314;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="氛围", meta=(DisplayName="微粒数量", ClampMin="0", ClampMax="192"))
    int32 Count=64;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="氛围", meta=(DisplayName="分布半径", ClampMin="1", Units="cm"))
    FVector Extent=FVector(6500,6500,350);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="氛围", meta=(DisplayName="微粒最小尺寸", ClampMin="1", Units="cm"))
    float MinSize=5;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="氛围", meta=(DisplayName="微粒最大尺寸", ClampMin="1", Units="cm"))
    float MaxSize=13;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="氛围", meta=(DisplayName="最远显示距离", ClampMin="100", Units="cm"))
    int32 DrawDistance=6500;

    /** Rebuild after editing placement or distribution; never adds collision or navigation. */
    UFUNCTION(CallInEditor, BlueprintCallable, Category="氛围", meta=(DisplayName="重新分布微粒"))
    void Rebuild();
};
