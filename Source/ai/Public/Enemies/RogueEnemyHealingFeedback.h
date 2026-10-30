#pragma once
#include "CoreMinimal.h"
#include "RogueEnemyHealingFeedback.generated.h"

/** 与生命属性分别复制，避免同一帧受伤和回血相抵后丢失绿色数字。 */
USTRUCT()
struct AI_API FRogueEnemyHealingFeedback
{
    GENERATED_BODY()
    UPROPERTY() uint32 Generation=0;
    UPROPERTY() uint32 Sequence=0;
    UPROPERTY() float TotalHealing=0;
    UPROPERTY() float LatestHealing=0;
    UPROPERTY() float ServerTime=0;
    UPROPERTY() bool bActive=false;
};
