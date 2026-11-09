#pragma once
#include "CoreMinimal.h"
#include "Engine/NetSerialization.h"
#include "RogueDefeatVisualEvent.generated.h"
class APlayerState;
class ARogueEnemyDefeatEffect;
/** Cosmetic snapshot. No live enemy reference, damage value, reward or progression data. */
USTRUCT()
struct FRogueDefeatVisualEvent
{
    GENERATED_BODY()
    UPROPERTY() FVector_NetQuantize10 Location;
    UPROPERTY() TSubclassOf<ARogueEnemyDefeatEffect> EffectClass;
    UPROPERTY() TObjectPtr<APlayerState> CreditedPlayer=nullptr;
    UPROPERTY() float Size=1;
    UPROPERTY() bool bBoss=false;
    UPROPERTY() uint16 Count=1;
};
