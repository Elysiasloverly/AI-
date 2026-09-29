#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Static.h"
#include "Combat/RogueImpactEffect.h"
#include "RogueGameplayCue_Impact.generated.h"

/**
 * 把项目的**原生** cue notify 注册进"当前生效的" GameplayCueManager 的运行时 cue set。
 *
 * 为什么需要这个自由函数（而不是像 Lyra 那样自定义一个 GameplayCueManager 子类）：
 *   实测结论（别再踩一遍）：DefaultGame.ini 里的 `GlobalGameplayCueManagerClass` **不保证生效** ——
 *   `UAbilitySystemGlobals::GetGameplayCueManager()` 在很早期的启动阶段就 LoadClass 这个配置项，
 *   那一刻本项目模块还没加载好，引擎会**静默**回退到默认的 UGameplayCueManager（不报错、不告警）。
 *   后果是"所有 GameplayCue 特效全部消失"，而且没有任何日志可查。
 *   所以注册不能挂在"自定义管理器被实例化"这个前提上，必须有一个"无论谁当管理器都能用"的入口，
 *   由游戏代码在确定会跑到的时机调用（现在有两处：ARoguePlayerState::BeginPlay、
 *   ARogueGameState::PlayImpactEffect 的权威端兜底）。
 *
 * 为什么需要手动注册（5.5 的另一个坑）：
 *   UGameplayCueManager 只从**资产注册表**里扫 GameplayCueNotifyPaths 下的蓝图资产来建立
 *   "Tag → notify 类"的映射，5.5 里**没有** AddGameplayCueNotifyClass 这类直接注册类的 API。
 *   也就是说：只写一个 C++ 的 UGameplayCueNotify_Static 子类，引擎根本不知道它存在，
 *   执行 cue 时会静默地什么都不发生（最难查的那种失败）。
 *   做法是用 UGameplayCueSet::AddCues 把 "Tag → /Script/ai.RogueGameplayCue_Impact" 塞进去
 *   （路径可以是原生类路径，AddCues 之后按 FSoftObjectPath 解析，原生类一样能解析到）。
 *
 * 幂等：已经注册过就直接返回（查一次 cue set 的 Tag 映射表，代价可忽略）。
 * 也因此可以廉价地重复调用 —— InitializeRuntimeObjectLibrary() 会先 CueSet->Empty() 再扫资产，
 * 任何"后加 notify 路径"的动作都可能把原生注册冲掉，多调几次就能自愈。
 */
namespace RogueGameplayCues
{
	AI_API void EnsureNativeImpactCuesRegistered();
}

/**
 * 打击/开火特效的 GameplayCue 通知（Static 版：不生成 Actor，只执行一次）。
 *
 * 为什么是 Static 而不是 Actor 版：
 *   本项目的特效本来就是**对象池里的短命 actor**（ARogueImpactEffect），
 *   cue 的职责只是"在正确的位置、用正确的风格把它捞出来摆好"，
 *   不需要 cue 自己再挂一个常驻 actor 去做生命周期管理。
 *
 * 参数映射（GameplayCue 的参数结构只有这么多自由度，这里显式写清用法）：
 *   · Location          —— 特效位置（复制）
 *   · Normal            —— 朝向（复制）
 *   · RawMagnitude      —— 统一缩放（"这次冲击有多大"）；<=0 视为 1
 *   · NormalizedMagnitude —— Z 轴压扁比例（1 = 不压）；地面波用 0.34 画成贴地一圈
 *   · EffectCauser      —— 生成的池对象 Owner，便于调试"这是谁打的"
 *   · MatchedTagName    —— 由 UGameplayCueSet 在路由时填好（服务端/客户端都有），
 *                          所以**不需要**给每个风格单独写一个 C++ 类
 *
 * 注意生命周期参数（旧 API 里的 Lifetime）不在参数里：
 *   它只决定池对象多久回收，与粒子时长无关（ARogueImpactEffect 内部有下限保护），
 *   所以按风格给一个够长的常数即可，见 GetLifetimeForStyle。
 */
UCLASS()
class AI_API URogueGameplayCue_Impact : public UGameplayCueNotify_Static
{
	GENERATED_BODY()

public:
	virtual bool OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const override;

private:
	/** 由 MatchedTagName 反查视觉风格；认不出来时退回 Spark。 */
	static ERogueImpactVisualStyle ResolveStyle(const FGameplayCueParameters& Parameters);

	/** 按风格给池对象一个足够长的存活时间（只影响回收时机，不影响粒子时长）。 */
	static float GetLifetimeForStyle(ERogueImpactVisualStyle Style);
};
