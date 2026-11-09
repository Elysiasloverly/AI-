#pragma once
#include "CoreMinimal.h"
#include "Shared/RogueTypes.h"
#include "RogueUpgradePickView.generated.h"

/** Owner-only mirror of successful run picks. Array order is first acquisition order. */
USTRUCT(BlueprintType)
struct AI_API FRogueUpgradePickView
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) ERogueUpgradeType Type=ERogueUpgradeType::AttackPower;
    UPROPERTY(BlueprintReadOnly) int32 Count=0;
};
