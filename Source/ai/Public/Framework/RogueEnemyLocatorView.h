#pragma once
#include "CoreMinimal.h"
#include "Engine/NetSerialization.h"
#include "RogueEnemyLocatorView.generated.h"

/** 服务端存活统计，只有最后五只敌人才附带位置；不受客户端 Actor 相关性裁剪影响。 */
USTRUCT(BlueprintType)
struct FRogueEnemyLocatorView
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) int32 Remaining = 0;
    UPROPERTY() TArray<FVector_NetQuantize> Locations;
};
