#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RogueRangedSpawnPoint.generated.h"

class ARogueEnemySpawnRegion;

/** 关卡中的远程敌人出生位置。箭头和圆圈只在编辑器显示，不产生任何碰撞。 */
UCLASS(Blueprintable, meta=(DisplayName="远程怪固定刷新点"))
class AI_API ARogueRangedSpawnPoint : public AActor
{
    GENERATED_BODY()
public:
    ARogueRangedSpawnPoint();
    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="固定刷新点", meta=(DisplayName="所属刷怪区域"))
    TObjectPtr<ARogueEnemySpawnRegion> SpawnRegion;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="固定刷新点", meta=(DisplayName="启用刷新点"))
    bool bEnabled = true;
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
};
