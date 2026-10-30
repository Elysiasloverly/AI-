#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RogueEnemyPresentationComponent.generated.h"
class ARogueEnemyEffectVisual;
class UMeshComponent;
UCLASS(ClassGroup=(Enemy),meta=(BlueprintSpawnableComponent,DisplayName="敌人辨识表现"))
class AI_API URogueEnemyPresentationComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    URogueEnemyPresentationComponent();
    void RefreshRuntime();
    void ResetForPool();
    virtual void TickComponent(float Delta,ELevelTick TickType,FActorComponentTickFunction* Fn) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    UPROPERTY(EditDefaultsOnly,BlueprintReadWrite,Category="Boss",meta=(DisplayName="Boss模型附加倍率",ClampMin="1",ClampMax="3"))
    float BossModelScale=1.5f;
    UPROPERTY(EditDefaultsOnly,BlueprintReadWrite,Category="Boss",meta=(DisplayName="Boss光环与头顶标记蓝图"))
    TSubclassOf<ARogueEnemyEffectVisual> BossVisualClass;
    UPROPERTY(EditDefaultsOnly,BlueprintReadWrite,Category="飞行",meta=(DisplayName="飞行敌人地面标记蓝图"))
    TSubclassOf<ARogueEnemyEffectVisual> FlyingVisualClass;
private:
    struct FScale { TWeakObjectPtr<UMeshComponent> Mesh;FVector Value; };
    TArray<FScale> Scales;
    UPROPERTY(Transient) TObjectPtr<ARogueEnemyEffectVisual> BossVisual;
    UPROPERTY(Transient) TObjectPtr<ARogueEnemyEffectVisual> FlyingVisual;
    uint32 Generation=MAX_uint32;
    bool bScaled=false;
    ARogueEnemyEffectVisual* EnsureVisual(TObjectPtr<ARogueEnemyEffectVisual>& Actor,TSubclassOf<ARogueEnemyEffectVisual> Class);
};
