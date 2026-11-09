#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RogueDefeatFeedbackSubsystem.generated.h"
class ARogueEnemyDefeatEffect;
/** Local visual budget only; actor transforms and counts are snapshots, independent of enemy pool reuse. */
UCLASS()
class AI_API URogueDefeatFeedbackSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    ARogueEnemyDefeatEffect* Play(TSubclassOf<ARogueEnemyDefeatEffect> Class,FVector Location,bool bBoss,float Size);
    UFUNCTION(BlueprintPure,Category="退治特效") int32 GetPoolSize()const{return Pool.Num();}
    UFUNCTION(BlueprintPure,Category="退治特效") int32 GetActiveCount()const;
    UFUNCTION(BlueprintPure,Category="退治特效|诊断") int32 GetPlayedCount()const{return PlayedCount;}
private:
    int32 PlayedCount=0;
    UPROPERTY() TArray<TObjectPtr<ARogueEnemyDefeatEffect>> Pool;
};
