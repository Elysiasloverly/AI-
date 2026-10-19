#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
// 显式包含句柄定义，避免依赖其他 GAS 头文件的间接包含。
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

/** 启动时授予的能力集合；本资产不负责初始化属性或施加常驻效果。
 * 敌人在 InitializeEnemy 完成原型配置后使用它，避免池化复用时读取上一个原型。
 * 玩家默认技能可使用该集合；武器能力仍由角色根据武器类型单独授予。 */
UCLASS(BlueprintType)
class AI_API URogueAbilitySet : public UDataAsset
{
	GENERATED_BODY()

public:
	/** 这份集合包含的能力。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "能力")
	TArray<FRogueAbilitySetEntry> Entries;

	/** 按 Entries 顺序调用 ASC->GiveAbility；ASC 为空时直接返回。
	 * @param SourceObject 写入 FGameplayAbilitySpec::SourceObject，可为任意 UObject。
	 * @param OutHandles ASC 有效时重置并填充，与 Entries 同序同长；空类占无效句柄。
	 * 调用方负责服务端调用与防重复；底层 GiveAbility 也会检查权威性。 */
	void GiveToAbilitySystem(UAbilitySystemComponent* ASC, UObject* SourceObject, TArray<FGameplayAbilitySpecHandle>* OutHandles = nullptr) const;
};
