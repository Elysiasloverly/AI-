#pragma once

#include "CoreMinimal.h"
#include "RogueTypes.generated.h"

UENUM(BlueprintType)
enum class ERogueUpgradeType : uint8
{
	MaxHealth         UMETA(DisplayName = "最大生命"),
	MoveSpeed         UMETA(DisplayName = "移动速度"),
	DashCooldown      UMETA(DisplayName = "冲刺冷却"),
	AttackPower       UMETA(DisplayName = "攻击力"),
	AttackSpeed       UMETA(DisplayName = "攻击速度"),
	PickupRadius      UMETA(DisplayName = "拾取范围"),
	Recovery          UMETA(DisplayName = "生命恢复"),
	AttackRange       UMETA(DisplayName = "攻击范围"),
	ProjectileSpeed   UMETA(DisplayName = "弹体速度"),
	ProjectileCount   UMETA(DisplayName = "普通子弹数量"),
	Armor             UMETA(DisplayName = "护甲恢复"),
	ArmorCapacity     UMETA(DisplayName = "护甲上限"),
	ExperienceGain    UMETA(DisplayName = "经验获取"),
	ScytheCount       UMETA(DisplayName = "镰刀数量"),
	RocketCount       UMETA(DisplayName = "火箭数量"),
	LaserCount        UMETA(DisplayName = "激光数量"),
	HellTowerCount    UMETA(DisplayName = "地狱塔数量"),
	LaserRefraction   UMETA(DisplayName = "激光折射"),
	MortarCount       UMETA(DisplayName = "迫击炮数量"),
	MortarBlastRadius UMETA(DisplayName = "迫击炮爆炸范围")
};

UENUM(BlueprintType)
enum class ERogueEnemyType : uint8
{
	Hunter      UMETA(DisplayName = "猎手"),
	Charger     UMETA(DisplayName = "冲锋者"),
	Tank        UMETA(DisplayName = "坦克"),
	Orbiter     UMETA(DisplayName = "环绕者"),
	Shooter     UMETA(DisplayName = "射手"),
	Skirmisher  UMETA(DisplayName = "游击者"),
	Artillery   UMETA(DisplayName = "炮兵"),
	Spitter     UMETA(DisplayName = "喷射者"),
	ShockPillar UMETA(DisplayName = "震荡柱")
};

struct FRogueSpawnCadence
{
	int32 EffectiveMaxAliveEnemies = 0;
	int32 SpawnBatchSize = 1;
	float NextSpawnDelay = 0.0f;
};

USTRUCT(BlueprintType)
struct FRogueEnemyProfile
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "敌人档案", meta = (DisplayName = "敌人类型"))
	ERogueEnemyType EnemyType = ERogueEnemyType::Hunter;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "敌人档案", meta = (DisplayName = "生命值"))
	float Health = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "敌人档案", meta = (DisplayName = "移动速度"))
	float Speed = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "敌人档案", meta = (DisplayName = "伤害"))
	float Damage = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "敌人档案", meta = (DisplayName = "经验奖励"))
	int32 ExperienceReward = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "敌人档案", meta = (DisplayName = "是否Boss"))
	bool bIsBoss = false;
};

USTRUCT()
struct FRogueUpgradeOption
{
	GENERATED_BODY()

	UPROPERTY()
	ERogueUpgradeType Type = ERogueUpgradeType::MaxHealth;

	UPROPERTY()
	FString Title;

	UPROPERTY()
	FString Description;

	UPROPERTY()
	float Magnitude = 0.0f;
};
