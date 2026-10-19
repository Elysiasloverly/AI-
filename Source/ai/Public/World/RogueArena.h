#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RogueArena.generated.h"

class USceneComponent;
class UStaticMeshComponent;
class UParticleSystem;
class UPointLightComponent;

/** 竞技场范围载体；本原生实现不创建地板/墙壁，场景外观由关卡或蓝图配置决定。 */
UCLASS()
class AI_API ARogueArena : public AActor
{
	GENERATED_BODY()

public:
	ARogueArena();

	UFUNCTION(BlueprintPure)
	float GetArenaHalfExtent() const { return ArenaHalfExtent; }

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(EditDefaultsOnly, Category = "Arena")
	float ArenaHalfExtent = 6000.0f;
	// 程序化几何、灯光与粒子字段已停用；本类当前只提供竞技场范围参数。
};
