// 场景优化子系统 —— 管理关卡环境的性能优化
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Framework/RogueGameModeConfig.h"
#include "RogueSceneSubsystem.generated.h"

UCLASS()
class AI_API URogueSceneSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	/** 按配置关闭部分光源/雾/天空/后处理并调整主光源；会改变画面，性能收益需实测。
	 *  操作当前 World，不保证每项组件设置自动跨端同步。 */
	void OptimizeLevelEnvironment(const FRogueSceneOptimizationSettings& Settings);

private:
	void DisableActorRendering(AActor* Actor) const;
};