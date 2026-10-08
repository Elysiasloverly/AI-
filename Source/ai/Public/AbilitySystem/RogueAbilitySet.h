#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
// FGameplayAbilitySpecHandle 在这里只以「TArray 指针」的形式出现在函数签名里，
// 但 TArray 的声明仍需完整类型，所以直接包含（这也是项目里其他 GAS 头文件的做法）。
#include "GameplayAbilitySpec.h"
#include "RogueAbilitySet.generated.h"

class UGameplayAbility;
class UAbilitySystemComponent;

/**
 * 一条"要授予什么能力"的条目。
 */
USTRUCT(BlueprintType)
struct AI_API FRogueAbilitySetEntry
{
	GENERATED_BODY()

	/** 要授予的能力类。留空会被跳过（并在 GiveToAbilitySystem 里占一个空句柄，保持下标对应）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "能力")
	TSubclassOf<UGameplayAbility> AbilityClass;

	/** 能力等级，直接传给 FGameplayAbilitySpec::Level。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "能力")
	int32 Level = 1;

	/** GAS 输入 ID（配合 `AbilityLocalInputPressed`）。INDEX_NONE = 不绑输入。
	 *  敌人与武器这类"自动触发"的能力都用 INDEX_NONE；只有玩家主动技能才需要填。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "能力")
	int32 InputID = INDEX_NONE;
};

/**
 * "这个角色启动时拥有哪些能力"的数据资产。
 *
 * ## 为什么要有它
 *
 * 改造前，能力是**硬编码**在 C++ 里的：
 *   · 冲刺    → `URogueAbilitySystemComponent::GrantDefaultAbilities`
 *   · 6 把武器 → `ARogueCharacter::EnsureWeaponAbilities`
 *   · 敌人三种攻击 → `ARogueEnemy::GrantAttackAbilities`
 *
 * 后果是"加一种敌人攻击方式"要同时改：新增能力子类 → 加决定何时授予的分支 → 接线。
 * 有了 AbilitySet 之后，**授技这一步变成改资产**：在数据表里给这种敌人挂一个
 * `URogueAbilitySet`，里面列出它该有哪些能力即可，C++ 不用动。
 *
 * ## 授予时机（⚠️ 这是本项目踩过坑的地方）
 *
 * **必须在"这只怪的完整原型已经就位之后"授予**，也就是 `ARogueEnemy::InitializeEnemy()` 的末尾。
 * 早期版本把授予放在 `ActivatePooledEnemy()` 里 —— 那时 `CurrentArchetype` 还是
 * **上一只怪**的值（甚至是默认原型），于是会照着错误的能力集发技能。
 * 症状是"远程怪不远程、震荡柱不起波"，**而且没有任何报错**。
 * 详见 `ARogueEnemy::GrantAttackAbilities` 的注释与 Docs/GAS-Main-Architecture-Plan.md 的 Phase 3。
 *
 * ## 与 GameplayEffect 的关系
 *
 * 本类只管**能力**。属性基础值与常驻 GE 的授予仍在各自的位置
 *（玩家走 `InitializeGASAttributes` / `EnsureRegenEffects`）——
 * 那些有"只能灌一次"的语义，和"授技"不是同一件事，混在一起会让守卫逻辑变复杂。
 */
UCLASS(BlueprintType)
class AI_API URogueAbilitySet : public UDataAsset
{
	GENERATED_BODY()

public:
	/** 这份集合包含的能力。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "能力")
	TArray<FRogueAbilitySetEntry> Entries;

	/** 把 Entries 全部授予给 `ASC`。
	 *
	 *  @param SourceObject  写进 FGameplayAbilitySpec::SourceObject。
	 *                       注意它必须是 `UObject*` 而不是 `AActor*` ——
	 *                       ASC 自己就是个 UActorComponent（不是 Actor），
	 *                       用 AActor* 会导致调用方没法把 ASC 传进来。
	 *  @param OutHandles    可选。与 Entries **同序同长**：留空的条目会占一个无效句柄，
	 *                       这样调用方可以用固定下标安全地取句柄（例如敌人的三种攻击）。
	 *
	 *  调用方负责判定"该不该授予"（服务端 / 幂等），本函数不做任何权威判断。 */
	void GiveToAbilitySystem(UAbilitySystemComponent* ASC, UObject* SourceObject, TArray<FGameplayAbilitySpecHandle>* OutHandles = nullptr) const;
};
