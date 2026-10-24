#pragma once

#include "CoreMinimal.h"
#include "Shared/RogueTypes.h"
#include "RogueProgressionTypes.generated.h"

UENUM(BlueprintType)
enum class ERogueMetaNodeKind : uint8
{
	Weapon UMETA(DisplayName = "武器解锁"),
	UpgradeCard UMETA(DisplayName = "专属升级卡"),
	ArmorUnlock UMETA(DisplayName = "护甲解锁"),
	BaseStat UMETA(DisplayName = "基础属性")
};

UENUM(BlueprintType)
enum class ERogueMetaStat : uint8
{
	None UMETA(Hidden),
	AttackPower UMETA(DisplayName = "攻击力"),
	MaxHealth UMETA(DisplayName = "最大生命"),
	ArmorCapacity UMETA(DisplayName = "基础护甲"),
	MoveSpeed UMETA(DisplayName = "移动速度"),
	PickupRadius UMETA(DisplayName = "吸取范围"),
	HealthRegen UMETA(DisplayName = "生命恢复"),
	AttackSpeed UMETA(DisplayName = "攻击间隔缩短"),
	ExperienceGain UMETA(DisplayName = "经验倍率")
};

/** 只传递开局需要的解锁和等级；冰砖余额与已领取奖励不会发给服务器。 */
USTRUCT(BlueprintType)
struct AI_API FRogueProgressionSnapshot
{
	GENERATED_BODY()

	// RepSkip suppresses native map replication validation; NetSerialize above owns the wire format.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, NotReplicated, Category = "升级树")
	TMap<FName, int32> NodeRanks;

	/** 手动序列化有界的键值对，避免依赖网络不支持的原生 TMap 复制。 */
	bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess);
};

template<>
struct TStructOpsTypeTraits<FRogueProgressionSnapshot> : TStructOpsTypeTraitsBase2<FRogueProgressionSnapshot>
{
	enum { WithNetSerializer = true };
};

USTRUCT(BlueprintType)
struct AI_API FRogueUpgradeTreeNode
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "节点", meta = (DisplayName = "永久标识（发布后不要改名）"))
	FName NodeId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "节点", meta = (DisplayName = "名称"))
	FText Title;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "节点", meta = (DisplayName = "说明"))
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "节点", meta = (DisplayName = "每级冰砖花费", ClampMin = "1"))
	int32 Cost = 5;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "节点", meta = (DisplayName = "最大等级", ClampMin = "1", ClampMax = "5"))
	int32 MaxRank = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "节点", meta = (DisplayName = "前置节点（全部需要）"))
	TArray<FName> Prerequisites;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "节点", meta = (DisplayName = "节点类型"))
	ERogueMetaNodeKind Kind = ERogueMetaNodeKind::Weapon;

	/** 武器节点填对应的数量升级类型；升级卡节点填卡牌类型。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "效果", meta = (DisplayName = "武器或卡牌类型"))
	ERogueUpgradeType UpgradeType = ERogueUpgradeType::ProjectileCount;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "效果", meta = (DisplayName = "基础属性类型"))
	ERogueMetaStat Stat = ERogueMetaStat::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "效果", meta = (DisplayName = "每级属性增加值", ClampMin = "0"))
	float BonusPerRank = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "布局", meta = (DisplayName = "旧版位置（自动前置布局不再使用）"))
	FVector2D TreePosition = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "节点", meta = (DisplayName = "初始免费解锁"))
	bool bInitiallyUnlocked = false;
};
