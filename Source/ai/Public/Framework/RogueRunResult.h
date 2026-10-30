#pragma once
#include "CoreMinimal.h"
#include "RogueRunResult.generated.h"

UENUM(BlueprintType)
enum class ERogueRunOutcome : uint8 { InProgress, Defeat, Victory };

/** 结束原因和结算数字在同一个复制属性中发送，避免用分离的胜利/结束布尔量选择界面。 */
USTRUCT(BlueprintType)
struct AI_API FRogueRunResult
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) ERogueRunOutcome Outcome = ERogueRunOutcome::InProgress;
    UPROPERTY(BlueprintReadOnly) int32 Wave = 0;
    UPROPERTY(BlueprintReadOnly) int32 EnemiesDefeated = 0;
    UPROPERTY(BlueprintReadOnly) float RunTimeSeconds = 0;
};
