#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RogueGroundEffectFade.generated.h"

class UMaterialInstanceDynamic;
class UMaterialInterface;
class USceneComponent;

/** 保留结束瞬间的本地画面并渐隐，源 Actor 可立即停止伤害、销毁或回池。 */
UCLASS(NotBlueprintable, Transient)
class AI_API ARogueGroundEffectFade : public AActor
{
    GENERATED_BODY()
public:
    ARogueGroundEffectFade();
    static ARogueGroundEffectFade* CaptureActor(AActor* Source, float Seconds=.6f);
    static ARogueGroundEffectFade* CaptureComponents(UWorld* World,
        TConstArrayView<USceneComponent*> Sources, float Seconds=.6f);
    virtual void Tick(float DeltaSeconds) override;

private:
    void CopyComponent(USceneComponent* Source);
    UMaterialInstanceDynamic* CopyMaterial(UMaterialInterface* Source);
    UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> Materials;
    TArray<float> OriginalFade;
    float Age=0, Duration=.6f;
};
