#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "RogueTurnSmoothingComponent.generated.h"
class USceneComponent;
/** 仅平滑外观转向；角色移动、碰撞和网络同步仍使用原来的 CharacterMovement。 */
UCLASS(ClassGroup=(Custom),meta=(BlueprintSpawnableComponent,DisplayName="转向平滑组件"))
class AI_API URogueTurnSmoothingComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    URogueTurnSmoothingComponent();
    virtual void TickComponent(float DeltaTime,ELevelTick TickType,FActorComponentTickFunction* ThisTickFunction) override;
    virtual void Deactivate() override;
    UFUNCTION(BlueprintCallable,Category="转向") void SetTargetComponent(USceneComponent* Target);
    UFUNCTION(BlueprintPure,Category="转向") USceneComponent* GetTargetComponent() const;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="转向",meta=(DisplayName="外观目标",UseComponentPicker,AllowedClasses="/Script/Engine.SceneComponent")) FComponentReference VisualTarget;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="转向",meta=(DisplayName="启用转向平滑")) bool bSmoothTurning=true;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="转向",meta=(DisplayName="转向跟随速度",ClampMin="0.1",ToolTip="越大越跟手，越小过渡越柔和；不改变移动加速度和胶囊旋转。")) float FollowSpeed=10.0f;
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void OnUnregister() override;
private:
    void InitializeTarget();
    void ResetSmoothing();
    TWeakObjectPtr<USceneComponent> SmoothedTarget;
    FQuat AuthoredRelativeRotation=FQuat::Identity;
    FQuat SmoothedWorldRotation=FQuat::Identity;
};
