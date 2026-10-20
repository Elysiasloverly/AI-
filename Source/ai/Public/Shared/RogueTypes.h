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
	MortarBlastRadius UMETA(DisplayName = "迫击炮爆炸范围"),
	ScytheSize        UMETA(DisplayName = "镰刀变大"),
	ScytheLifesteal   UMETA(DisplayName = "吸血收割")
};

UENUM(BlueprintType)
enum class ERogueEnemyType : uint8
{
	Rifleman    UMETA(DisplayName = "步枪手"),
	Assault     UMETA(DisplayName = "突击手"),
	Juggernaut  UMETA(DisplayName = "重装兵"),
	Orbiter     UMETA(DisplayName = "环绕射手"),
	Marksman    UMETA(DisplayName = "精准射手"),
	Skirmisher  UMETA(DisplayName = "游击射手"),
	Cannoneer   UMETA(DisplayName = "重炮手"),
	Scatterer   UMETA(DisplayName = "散射者"),
	ShockPillar UMETA(DisplayName = "震荡柱"),
	Sniper      UMETA(DisplayName = "狙击手"),
	Shotgunner  UMETA(DisplayName = "霰弹手"),
	Repeater    UMETA(DisplayName = "速射手"),
	Ringcaster  UMETA(DisplayName = "环击者")
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
	ERogueEnemyType EnemyType = ERogueEnemyType::Rifleman;

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

/** 武器使用的 13 项角色侧加成快照；服务端从 AttributeSet 抽取，变化后写入 Character。
 *  AttributeSet 本身也复制，此结构是统一武器输入接口，不证明 ASC 无法同步属性。
 *  快照无自定义 NetSerialize，不能假定各成员或它与属性集同帧原子到达。
 */
USTRUCT()
struct FRogueWeaponBonusValues
{
	GENERATED_BODY()

	UPROPERTY() float AttackPower = 0.0f;
	UPROPERTY() float AttackSpeed = 0.0f;
	UPROPERTY() float AttackRange = 0.0f;
	UPROPERTY() float ProjectileSpeed = 0.0f;
	UPROPERTY() float ProjectileCount = 0.0f;
	UPROPERTY() float ScytheCount = 0.0f;
	UPROPERTY() float RocketCount = 0.0f;
	UPROPERTY() float LaserCount = 0.0f;
	UPROPERTY() float HellTowerCount = 0.0f;
	UPROPERTY() float MortarCount = 0.0f;
	UPROPERTY() float LaserRefraction = 0.0f;
	UPROPERTY() float MortarBlastRadius = 0.0f;
	/** 额外尺寸比例：0.2 对应基础尺寸的 1.2 倍。 */
	UPROPERTY() float ScytheSize = 0.0f;

	/** 对 13 项做近似相等比较；用于跳过角色侧快照赋值，不等同于网络系统逐字段脏检测。 */
	bool Equals(const FRogueWeaponBonusValues& Other) const
	{
		return FMath::IsNearlyEqual(AttackPower, Other.AttackPower)
			&& FMath::IsNearlyEqual(AttackSpeed, Other.AttackSpeed)
			&& FMath::IsNearlyEqual(AttackRange, Other.AttackRange)
			&& FMath::IsNearlyEqual(ProjectileSpeed, Other.ProjectileSpeed)
			&& FMath::IsNearlyEqual(ProjectileCount, Other.ProjectileCount)
			&& FMath::IsNearlyEqual(ScytheCount, Other.ScytheCount)
			&& FMath::IsNearlyEqual(RocketCount, Other.RocketCount)
			&& FMath::IsNearlyEqual(LaserCount, Other.LaserCount)
			&& FMath::IsNearlyEqual(HellTowerCount, Other.HellTowerCount)
			&& FMath::IsNearlyEqual(MortarCount, Other.MortarCount)
			&& FMath::IsNearlyEqual(LaserRefraction, Other.LaserRefraction)
			&& FMath::IsNearlyEqual(MortarBlastRadius, Other.MortarBlastRadius)
			&& FMath::IsNearlyEqual(ScytheSize, Other.ScytheSize);
	}
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

	/** 首次获得的目标值；-1 = 使用代码默认（目前仅吸血收割使用）。 */
	UPROPERTY()
	float FirstMagnitude = -1.0f;

	/** 该升级允许的最大值；-1 = 使用代码默认。 */
	UPROPERTY()
	float MaxMagnitude = -1.0f;
};

/** 一次开火的表现载荷，不传伤害值。
 *  拥有者可在本机产生开火反馈，服务器独立判定伤害并向其他相关客户端发送表现。
 *  两端的时机/目标未由该结构强制统一，不能据此保证完整预测确认或弹道严格一致。
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

	/** 客户端表现弹体的速度输入；网络延迟与各端模拟仍可能造成位置差异。 */
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

/** 表现批次中的一条武器索引与开火载荷。
 *  按角色在刷新时合并发送，减少逐发 RPC 调用；一帧内仍可能有多个条目。
 *  实际消息率、带宽与延迟取决于帧率、人数、条目和网络条件，需通过当前构建测量。
 */
USTRUCT()
struct FRogueWeaponCosmeticBatchEntry
{
	GENERATED_BODY()

	/** 角色 Weapons 数组下标；要求各端配置与生成顺序一致。接收端检查范围，不按武器类核对。 */
	UPROPERTY()
	int32 WeaponIndex = INDEX_NONE;

	UPROPERTY()
	FRogueWeaponCosmeticFire Fire;
};
