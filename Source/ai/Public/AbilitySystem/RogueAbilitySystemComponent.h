#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "RogueAbilitySystemComponent.generated.h"

/** 玩家能力的输入 ID（对应 `UAbilitySystemComponent::AbilityLocalInputPressed` 的 InputID）。
 *
 *  为什么要有它：迁移到 Enhanced Input 之后，冲刺走的是 GAS 的**原生输入路径** ——
 *  能力在授予时带上 InputID，输入侧只喊一句 `ASC->AbilityLocalInputPressed(InputID)`，
 *  由 GAS 自己去技能表里找匹配的能力并激活。相比"输入直接调函数、函数再按标签找技能"，
 *  少一层间接，而且"哪个键对应哪个技能"变成了一份可查询的数据。
 *
 *  目前只有冲刺需要输入：6 把武器是**自动开火**（持续型能力自己跑心跳），不需要玩家按键；
 *  敌人攻击是 `ServerOnly`，由 AI 决策激活。 */
namespace RogueAbilityInputID
{
	enum Type : int32
	{
		None = INDEX_NONE,
		Dash = 1,
	};
}

class URogueAbilitySet;

/**
 * 项目 ASC（AbilitySystemComponent 子类）。
 *
 * 为什么要子类化而不是直接用 UAbilitySystemComponent：
 *   1) `ReplicationMode` **不是 UPROPERTY**（引擎 AbilitySystemComponent.cpp:75 里默认 Full），
 *      所以没法在编辑器默认值里配，只能在构造里按"玩家 / 敌人"分别设。
 *      玩家必须 Mixed、敌人用 Minimal —— 见下面两个函数的注释。
 *   2) 敌人 ASC 是**对象池复用**的，需要一个统一的"回收前清干净"入口，
 *      否则下一只怪会继承上一只的 Buff / 冷却 / 已授予技能。
 */
UCLASS(ClassGroup = (Rogue), meta = (BlueprintSpawnableComponent))
class AI_API URogueAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	URogueAbilitySystemComponent();

	/** 玩家侧：Mixed。
	 *  引擎枚举注释里写得很清楚：Minimal "does not work for Owned AbilitySystemComponents"，
	 *  所以被玩家拥有的 ASC 必须用 Mixed（自己拿到完整 GE 信息以便预测，其他人只收最小信息）。 */
	void ConfigureAsPlayer();

	/** 敌人侧：Minimal。
	 *  服务端权威：AI 决策、TryActivateAbility、GE 结算、冷却全部在服务器；
	 *  客户端只同步属性（血条）与 GameplayCue（特效/音效），不接收 GE 明细。 */
	void ConfigureAsEnemy();

	/** 敌人回收到对象池前调用：清空已授予技能与所有激活中的 GE。 */
	void ResetForPool();

	/** 授予玩家的默认技能（服务端调用，且只会生效一次）。
	 *
	 *  必须是服务端授予：能力与冷却 GE 都是权威数据，客户端靠复制拿到。
	 *
	 *  @param AbilitySet  **数据驱动**的能力集。传了就按它授予 ——
	 *                     于是"加一个玩家主动技能"只需要：写能力子类 → 改一个资产，C++ 不用动。
	 *                     留空则回退到下面那份硬编码（冲刺这一个能力），
	 *                     保证资产没配好时玩家依然能玩、不会变成"一个技能都没有"。 */
	void GrantDefaultAbilities(const URogueAbilitySet* AbilitySet = nullptr);

private:
	/** 默认技能是否已授予（避免 PossessedBy / OnRep 多次触发导致重复 GiveAbility） */
	bool bDefaultAbilitiesGranted = false;
};
