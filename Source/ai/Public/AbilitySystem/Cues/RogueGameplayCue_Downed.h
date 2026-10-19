#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Actor.h"
#include "RogueGameplayCue_Downed.generated.h"

class UStaticMeshComponent;

/** 濒死持续表现：显示附着目标的全息环，逐帧自转/缩放脉动。
 * URogueDownedEffect 是 Infinite；十秒截止由角色 LifeState 和服务端定时器管理。
 * 复活/团灭时角色显式移除 GE，Cue 再收到 OnRemove 并结束显示/回收。
 * 新相关或晚加入客户端可能只收到 WhileActive；本类当前仅在 OnActive 中显示，
 * 尚未实现 WhileActive 显示兜底，不能保证所有到达顺序都能展示环。
 * 玩家 Mixed 允许传播持续 Cue，但仍依赖相关性、Cue 注册及资源加载成功。 */
UCLASS()
class AI_API ARogueGameplayCue_Downed : public AGameplayCueNotify_Actor
{
	GENERATED_BODY()

public:
	ARogueGameplayCue_Downed();

	// AGameplayCueNotify_Actor 的三个生命周期事件。
	// 注意它们是 BlueprintNativeEvent，所以覆写的是 *_Implementation，
	// 蓝图子类仍然可以再覆写一遍（这也是"C++ 提供骨架、蓝图做外观"的一贯做法）。

	/** GE 生效：把环摆到目标脚下并显示。 */
	virtual bool OnActive_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) override;

	/** GE 移除时隐藏；bAutoDestroyOnRemove 负责通知框架结束/回收实例。 */
	virtual bool OnRemove_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) override;

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	/** 环的网格组件（在构造函数里建，材质/网格在构造函数里按路径加载）。 */
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> RingMesh;

	/** 环相对目标原点的 Z 偏移（目标原点通常在胶囊体中心，所以这是个负值附近的值）。 */
	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	float RingHeightOffset = -78.0f;

	/** 环的基础缩放（Torus 原始尺寸较大，需要压到人物脚下合适的大小）。 */
	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	float RingBaseScale = 0.55f;

	/** 自转速度（度/秒）。 */
	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	float RotationSpeedDegreesPerSecond = 80.0f;

	/** 呼吸（缩放脉动）速度与幅度。 */
	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	float PulseSpeed = 2.6f;

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	float PulseAmplitude = 0.10f;

	/** 已经过的存活时间，用于驱动自转与呼吸。 */
	float ElapsedSeconds = 0.0f;
};
