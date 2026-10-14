#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Actor.h"
#include "RogueGameplayCue_Downed.generated.h"

class UStaticMeshComponent;

/**
 * 倒地表现：在倒地玩家脚下套一圈缓慢旋转 + 呼吸的全息环。
 *
 * ## 为什么这个用 Actor 型 Cue，而打击特效用 Static
 *
 * | | `URogueGameplayCue_Impact` | `ARogueGameplayCue_Downed`（本类） |
 * |---|---|---|
 * | 基类 | `UGameplayCueNotify_Static` | `AGameplayCueNotify_Actor`（注意 **A** 前缀：它是个 Actor） |
 * | 有没有实例 | 没有（CDO 上执行） | **有**（每个目标一个 Actor） |
 * | 生命周期 | 触发即结束 | 从 `OnActive` 到 `OnRemove` |
 * | 适合 | 命中火花、爆炸 | **持续状态**：环要一直转，直到人复活 |
 *
 * ## 触发方式：挂在 GE 上，而不是靠代码记得去关
 *
 * `URogueDownedEffect` 的 `GameplayCues` 里登记了本类对应的 Tag，所以调用链是：
 *
 * ```
 * 血量归零 → 施加 URogueDownedEffect → GE 生效
 *              ↓（GAS 自动）
 *            OnActive   → 环出现，开始旋转
 *              ↓（10 秒后 GE 到期 / 或团灭时被移除）
 *            OnRemove   → 环消失
 * ```
 *
 * **没有任何一处代码需要"记得在复活时把特效关掉"** —— 这正是表现层挂 GE 而不是挂
 * 自己那套 bool + Timer 的价值。这个项目已经在"倒下的角色不再 Tick，于是清理永远不执行"
 * 上踩过两次，所以这条边界值得守住。
 *
 * 联机：玩家 ASC 是 `Mixed` 模式，而 Mixed **天生就复制 GameplayCue**
 * （见 RogueGameplayTags.h 里关于 Minimal/Mixed 的说明），
 * 所以队友能看到谁倒地了 —— 不需要任何额外的 RPC 或复制字段。
 *
 * ## 视觉资源
 *
 * 网格与材质**全部复用项目里已有的资产**（`ConstructorHelpers` 按路径加载，
 * 与 `ARogueCharacterVisualComponent` 同一套做法），因此本类**不需要新建任何 .uasset**：
 * 环用 StarterContent 的 Torus，材质用角色外壳同款的全息材质。
 * 加载失败时会退化成"没有网格"（环不可见）而**不会崩**——这点与项目里其他视觉组件的处理一致。
 */
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

	/** GE 移除（复活 / 团灭 / 被打断）：隐藏。实际销毁由 `bAutoDestroyOnRemove` 负责。 */
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
