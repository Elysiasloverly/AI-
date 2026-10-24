#pragma once
#include "CoreMinimal.h"
#include "RogueStagePlayerSnapshot.generated.h"

/** 仅供服务器同局换图使用，不接收客户端数值。新 ASC 使用已结算属性作基值，不搬运旧世界的 GE。 */
USTRUCT()
struct AI_API FRogueStagePlayerSnapshot
{
    GENERATED_BODY()
    UPROPERTY() TMap<FName, float> Attributes;
    int32 Level = 1;
    int32 Experience = 0;
    int32 NextLevelExperience = 6;
    int32 Money = 0;
    float ExperienceRemainder = 0;
    bool bDowned = false;
    float ReviveRemaining = 0;
};
