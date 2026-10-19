#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Static.h"
#include "Combat/Visuals/RogueImpactEffect.h"
#include "RogueGameplayCue_Impact.generated.h"

/** 将原生 Cue Tag/类路径映射加入当前 GameplayCueManager 的运行时集合。
 * 默认资源扫描面向资产；本项目用 AddCues 显式加入原生类，避免依赖蓝图 Cue 资产。
 * 已包含所需 Tag 时返回。若运行时 Cue 库重建清空映射，须在后续调用中重新注册。
 * 使用当前管理器，不要求自定义管理器；不能据此认定引擎配置加载存在固定时序故障。 */
namespace RogueGameplayCues
{
	AI_API void EnsureNativeImpactCuesRegistered();
}

/** 一次性打击 Cue（通知在 CDO 执行，实际特效由池化 ARogueImpactEffect 显示）。
 * 本项目参数约定：Location 为位置，Normal 为朝向；RawMagnitude=Scale.X，
 * NormalizedMagnitude=Scale.Z/Scale.X，Y 缩放未编码，因而仅重建 X/Y 相等的缩放。
 * 两个 magnitude 均非正时按 1 处理；这不是引擎对 magnitude 的通用语义。
 * EffectCauser 用作特效 Owner；MatchedTagName 由每端路由填写，不依赖网络序列化。
 * Lifetime 按风格选取，并受特效池内部最小时长约束；回收也会停止/隐藏视觉。 */
UCLASS()
class AI_API URogueGameplayCue_Impact : public UGameplayCueNotify_Static
{
	GENERATED_BODY()

public:
	virtual bool OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const override;

private:
	/** 由 MatchedTagName 反查视觉风格；认不出来时退回 Spark。 */
	static ERogueImpactVisualStyle ResolveStyle(const FGameplayCueParameters& Parameters);

	/** 按风格给池对象请求存活时间；内部最小时长保护可能延长实际回收时刻。 */
	static float GetLifetimeForStyle(ERogueImpactVisualStyle Style);
};
