#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "RogueAbilitySystemComponent.generated.h"

/** 玩家能力的输入 ID，对应 AbilityLocalInputPressed/Released。
 * 当前冲刺使用 Dash；武器由持续能力自动开火，敌人攻击由服务端 AI 激活。 */
namespace RogueAbilityInputID
{
	enum Type : int32
	{
		None = INDEX_NONE,
		Dash = 1,
	};
}

class URogueAbilitySet;

/** 项目 ASC：统一配置 GE 复制模式、授予玩家默认技能并清理池化敌人的 GAS 状态。
 * 玩家选 Mixed：拥有者接收完整 GE，其他相关代理接收最小信息；Full 也是可选模式。
 * 敌人选 Minimal，不向客户端发送完整活动 GE。属性是否复制由 AttributeSet 独立配置。 */
UCLASS(ClassGroup = (Rogue), meta = (BlueprintSpawnableComponent))
class AI_API URogueAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	URogueAbilitySystemComponent();

	/** 玩家使用 Mixed：拥有者/自主代理接收完整 GE，其他相关代理接收最小标签与 Cue 信息。
	 * 必须建立正确的 Owner/控制器链；该配置本身不实现能力或移动预测。 */
	void ConfigureAsPlayer();

	/** 敌人使用 Minimal：服务端执行 AI 与伤害，客户端接收最小 GE 标签/Cue 信息。
	 * 血条属性通过 AttributeSet 自己的 Replicated 配置同步，不由此模式自动授予复制。 */
	void ConfigureAsEnemy();

	/** 敌人回收到对象池前调用：清空已授予技能与所有激活中的 GE。 */
	void ResetForPool();

	/** 在服务端首次调用时授予默认能力。
	 * AbilitySet 非空时按集合授予；为空时回退到冲刺。新主动技能还需要配置输入与激活入口。 */
	void GrantDefaultAbilities(const URogueAbilitySet* AbilitySet = nullptr);

private:
	/** 默认技能是否已授予（避免 PossessedBy / OnRep 多次触发导致重复 GiveAbility） */
	bool bDefaultAbilitiesGranted = false;
};
