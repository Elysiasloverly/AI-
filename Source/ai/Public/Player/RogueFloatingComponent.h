#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "RogueFloatingComponent.generated.h"
class USceneComponent;
/** 独立的外观漂浮组件；默认玩家目标为 Visuals，不改变胶囊、相机、模型和材质。 */
UCLASS(ClassGroup=(Custom),meta=(BlueprintSpawnableComponent,DisplayName="漂浮组件"))
class AI_API URogueFloatingComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    URogueFloatingComponent();
    virtual void TickComponent(float DeltaTime,ELevelTick TickType,FActorComponentTickFunction* ThisTickFunction) override;
    virtual void Deactivate() override;
    UFUNCTION(BlueprintCallable,Category="漂浮") void SetFloatingEnabled(bool bEnabled);
    UFUNCTION(BlueprintCallable,Category="漂浮") void SetTargetComponent(USceneComponent* Target);
    UFUNCTION(BlueprintPure,Category="漂浮") USceneComponent* GetTargetComponent() const;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="漂浮",meta=(DisplayName="漂浮目标",UseComponentPicker,AllowedClasses="/Script/Engine.SceneComponent"))
    FComponentReference FloatingTarget;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="漂浮",meta=(DisplayName="启用漂浮")) bool bFloatingEnabled=true;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="漂浮",meta=(DisplayName="漂浮幅度",ClampMin="0",Units="cm")) float FloatAmplitude=4.0f;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="漂浮",meta=(DisplayName="漂浮周期",ClampMin="0.1",Units="s")) float FloatPeriod=3.2f;
protected:
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void OnUnregister() override;
private:
    void ResetOffset();
    double Elapsed=0;
    FVector AppliedOffset=FVector::ZeroVector;
    TWeakObjectPtr<USceneComponent> AppliedTarget;
};
