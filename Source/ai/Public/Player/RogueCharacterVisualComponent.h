#pragma once
#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "RogueCharacterVisualComponent.generated.h"
/** 玩家外观挂接点。漂浮和转向过渡由独立组件控制，蓝图继续配置子模型、材质和缩放。 */
UCLASS(ClassGroup=(Custom),meta=(BlueprintSpawnableComponent,DisplayName="玩家外观挂接点"))
class AI_API URogueCharacterVisualComponent : public USceneComponent
{
    GENERATED_BODY()
public:
    URogueCharacterVisualComponent();
    /** 兼容旧蓝图调用；新调用应直接访问 FloatingMotion。 */
    UFUNCTION(BlueprintCallable,Category="外观",meta=(DeprecatedFunction,DeprecationMessage="请改用独立漂浮组件 FloatingMotion 的 SetFloatingEnabled。"))
    void SetFloatingEnabled(bool bEnabled);
};
