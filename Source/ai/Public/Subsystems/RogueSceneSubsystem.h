// 场景优化子系统 —— 管理关卡环境的性能优化
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Core/RogueGameModeConfig.h"
#include "RogueSceneSubsystem.generated.h"

UCLASS()
class AI_API URogueSceneSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** 执行关卡环境优化 */
	void OptimizeLevelEnvironment(const FRogueSceneOptimizationSettings& Settings);

private:
	void DisableActorRendering(AActor* Actor) const;
};