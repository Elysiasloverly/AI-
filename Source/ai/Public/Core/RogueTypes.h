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

/**
 * 一次武器开火的**表现载荷**。
 *
 * 玩家武器在联机下的分工是"本人本地预测 + 服务端权威 + 广播给别人"：
 *   · 本人的机器自己算出这一发（开火零延迟，手感不变）；
 *   · 服务端权威地再算一遍，伤害只在这里结算；
 *   · 其他客户端不模拟别人的武器（旧行为是每台机器各算一份，弹道和锁敌会各显示各的），
 *     改由服务端把这一发广播过去，让队友看得见。
 *
 * 这个结构体就是广播的内容：只带"画出来需要什么"，**不带伤害** ——
 * 伤害永远只在服务端的权威那一发里算，客户端拿到的这一发伤害恒为 0。
 */
USTRUCT(BlueprintType)
struct FRogueWeaponCosmeticFire
{
	GENERATED_BODY()

	/** 弹体/光束起点（环绕刀片这类只用它当生成点） */
	UPROPERTY(BlueprintReadWrite)
	FVector_NetQuantize Start = FVector::ZeroVector;

	/** 光束终点；迫击炮用**起终点**解抛物线（见 ActivatePooledMortar）。弹体/刀片类不使用 */
	UPROPERTY(BlueprintReadWrite)
	FVector_NetQuantize End = FVector::ZeroVector;

	/** 弹体飞行方向（刀片/光束类不使用） */
	UPROPERTY(BlueprintReadWrite)
	FVector_NetQuantizeNormal Direction = FVector::ForwardVector;

	/** 弹体速度。客户端照着这个速度本地飞，视觉才和服务端那一发一致 */
	UPROPERTY(BlueprintReadWrite)
	float Speed = 0.0f;

	/** 武器自用的附加数值，各武器自己解释：
	 *  · 火箭 / 迫击炮 —— 爆炸半径（客户端据此算爆炸特效缩放与落点指示圈大小）
	 *  · 光束 —— 光束存活时间 */
	UPROPERTY(BlueprintReadWrite)
	float Param = 0.0f;

	/** 风格位：大弹体 / 炼狱光束 这类"看起来不一样"的开关 */
	UPROPERTY(BlueprintReadWrite)
	bool bAlternateStyle = false;
};

/**
 * 批量广播里的一条：哪个武器 + 这一发的内容。
 *
 * 为什么要批量化：原来是"每发弹体一条 RPC"，高射速档 × 4 人时估算可达
 * 约 2700 条/秒（≈400KB/s），而引擎默认客户端预算是 100KB/s —— 真联机下会顶到天花板。
 * 改成每帧把所有待发表现合成一条（一次 5 连发 = 1 条而不是 5 条），
 * 稳态降到约 240 条/秒（≈35KB/s）。代价只是表现最多晚一帧。
 */
USTRUCT()
struct FRogueWeaponCosmeticBatchEntry
{
	GENERATED_BODY()

	/** 武器在角色 Weapons 数组里的下标（各端由同一张 DT_WeaponConfig 按同样顺序建，故一致） */
	UPROPERTY()
	int32 WeaponIndex = INDEX_NONE;

	UPROPERTY()
	FRogueWeaponCosmeticFire Fire;
};
