#pragma once
#include "CoreMinimal.h"
#include "RogueBossHealthView.generated.h"

/** 从服务器复制给全部玩家的 Boss 显示数据；不依赖敌人 Actor 的网络相关性、屏幕投影或玩家距离。 */
USTRUCT(BlueprintType)
struct AI_API FRogueBossHealthView
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) bool bVisible = false;
    UPROPERTY(BlueprintReadOnly) FGuid SpawnId;
    UPROPERTY(BlueprintReadOnly) float CurrentHealth = 0;
    UPROPERTY(BlueprintReadOnly) float MaxHealth = 0;
    float GetPercent() const { return MaxHealth > 0 ? FMath::Clamp(CurrentHealth / MaxHealth, 0.f, 1.f) : 0.f; }
};
