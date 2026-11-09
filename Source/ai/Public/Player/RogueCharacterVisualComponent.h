#pragma once
#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "RogueCharacterVisualComponent.generated.h"
class UMaterialInterface;
class UStaticMeshComponent;

/** 玩家外观挂接点。漂浮和转向过渡由独立组件控制，蓝图继续配置子模型、材质和缩放。 */
UCLASS(ClassGroup=(Custom),meta=(BlueprintSpawnableComponent,DisplayName="玩家外观挂接点"))
class AI_API URogueCharacterVisualComponent : public USceneComponent
{
    GENERATED_BODY()
public:
    URogueCharacterVisualComponent();
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="外观|辨识", meta=(DisplayName="玩家与队友定位材质"))
    TObjectPtr<UMaterialInterface> FocusMarkerMaterial;

    virtual void TickComponent(float DeltaSeconds, ELevelTick TickType, FActorComponentTickFunction* TickFunction) override;
    /** 兼容旧蓝图调用；新调用应直接访问 FloatingMotion。 */
    UFUNCTION(BlueprintCallable,Category="外观",meta=(DeprecatedFunction,DeprecationMessage="请改用独立漂浮组件 FloatingMotion 的 SetFloatingEnabled。"))
    void SetFloatingEnabled(bool bEnabled);

protected:
    virtual void BeginPlay() override;

private:
    UPROPERTY() TObjectPtr<UStaticMeshComponent> FocusMarker;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> FocusCrest;
    int32 MarkerStyle = INDEX_NONE;
};
