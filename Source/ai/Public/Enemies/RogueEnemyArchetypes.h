#pragma once

#include "CoreMinimal.h"
#include "Core/RogueTypes.h"
#include "RogueEnemyArchetypes.generated.h"

UENUM(BlueprintType)
enum class ERogueEnemyMovementModel : uint8
{
	Direct       UMETA(DisplayName = "直线追击"),
	BurstCharge  UMETA(DisplayName = "爆发冲刺"),
	Orbit        UMETA(DisplayName = "环绕运动"),
	RangedKite   UMETA(DisplayName = "远程风筝"),
	ZigZagChase  UMETA(DisplayName = "Z字形追击"),
	Stationary   UMETA(DisplayName = "固定不动")
};

UENUM(BlueprintType)
enum class ERogueEnemyShotPattern : uint8
{
	None     UMETA(DisplayName = "无"),
	Single   UMETA(DisplayName = "单发"),
	Spread3  UMETA(DisplayName = "散射")
};

UENUM(BlueprintType)
enum class ERogueEnemyVisualKey : uint8
{
	Hunter,
	Charger,
	Tank,
	Orbiter,
	Shooter,
	Skirmisher,
	Artillery,
	Spitter,
	ShockPillar
};

USTRUCT(BlueprintType)
struct AI_API FRogueEnemyMovementArchetype
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	ERogueEnemyMovementModel Model = ERogueEnemyMovementModel::Direct;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float BaseMoveMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Burst")
	float BurstCycleTime = 2.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Burst")
	float BurstDuration = 0.9f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Burst")
	float BurstActiveMultiplier = 2.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Burst")
	float BurstIdleMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Range")
	float PreferredRange = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Range")
	float FarDistance = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Range")
	float CloseDistance = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Weight")
	float FarTowardWeight = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Weight")
	float FarTangentWeight = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Weight")
	float CloseTowardWeight = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Weight")
	float CloseTangentWeight = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Weight")
	float MidTowardWeight = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Weight")
	float MidTangentWeight = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Range")
	bool bUseRangeBiasForMidToward = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|ZigZag")
	float ZigZagFrequency = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|ZigZag")
	float ZigZagTowardWeight = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|ZigZag")
	float ZigZagTangentWeight = 0.0f;
};

USTRUCT(BlueprintType)
struct AI_API FRogueEnemyRangedArchetype
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged")
	bool bUsesRangedAttack = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged")
	ERogueEnemyShotPattern ShotPattern = ERogueEnemyShotPattern::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged|Cooldown")
	float InitialCooldownMin = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged|Cooldown")
	float InitialCooldownMax = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged|Range")
	float PreferredRange = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged|Range")
	float MinimumRange = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged|Range")
	float MaximumRange = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged|Range")
	float AttackRangeMinFactor = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged|Cooldown")
	float NextShotCooldown = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged|Projectile")
	float ProjectileSpeed = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged|Projectile")
	float ProjectileDamageMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged|Visual")
	bool bLargeVisual = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged|Visual")
	bool bSpawnMuzzleEffect = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged|Visual")
	FVector MuzzleEffectScale = FVector(0.2f, 0.2f, 0.2f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged|Visual")
	float MuzzleEffectLifetime = 0.12f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged|Spread")
	int32 SpreadShotCount = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged|Spread")
	float SpreadAngleStep = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged|Spread")
	float SpreadSpawnOffset = 12.0f;
};

USTRUCT(BlueprintType)
struct AI_API FRogueEnemyGroundWaveArchetype
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "地面波", meta = (DisplayName = "启用地面波攻击"))
	bool bUsesGroundWave = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "地面波", meta = (DisplayName = "初始冷却最小值"))
	float InitialCooldownMin = 1.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "地面波", meta = (DisplayName = "初始冷却最大值"))
	float InitialCooldownMax = 3.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "地面波", meta = (DisplayName = "攻击冷却时间"))
	float AttackCooldown = 4.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "地面波", meta = (DisplayName = "最大半径"))
	float MaxRadius = 980.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "地面波", meta = (DisplayName = "最短扩散持续时间"))
	float ExpansionDuration = 1.05f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "地面波", meta = (DisplayName = "扩散速度（单位/秒）"))
	float ExpansionSpeed = 2600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "地面波", meta = (DisplayName = "命中厚度"))
	float HitThickness = 92.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "地面波", meta = (DisplayName = "伤害倍率"))
	float DamageMultiplier = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "地面波", meta = (DisplayName = "跳跃可躲避"))
	bool bJumpCanDodge = true;
};

USTRUCT(BlueprintType)
struct AI_API FRogueEnemyVisualArchetype
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual")
	ERogueEnemyVisualKey VisualKey = ERogueEnemyVisualKey::Hunter;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual")
	FVector MeshScale = FVector(0.52f, 0.52f, 0.96f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual")
	FVector MeshLocation = FVector(0.0f, 0.0f, -40.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual")
	FRotator MeshRotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual")
	float CapsuleRadius = 34.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual")
	float CapsuleHalfHeight = 72.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual")
	bool bUseBossMaterialOverride = false;
};

class URogueAbilitySet;

USTRUCT(BlueprintType)
struct AI_API FRogueEnemyArchetype
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Archetype")
	FRogueEnemyMovementArchetype Movement;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Archetype")
	FRogueEnemyRangedArchetype Ranged;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Archetype")
	FRogueEnemyGroundWaveArchetype GroundWave;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Archetype")
	FRogueEnemyVisualArchetype Visual;

	/** 这种敌人该有哪些攻击能力（数据驱动）。
	 *
	 *  留空 → `ARogueEnemy::GrantAttackAbilities` 回退到"按 Ranged/GroundWave 标志推导"的
	 *  硬编码逻辑，所以**老数据表不改也能跑**。
	 *
	 *  填了它之后，加一种敌人攻击方式就只需要：
	 *    ① 写能力子类 → ② 建/改一个 URogueAbilitySet 资产 → ③ 在数据表这一列挂上。
	 *  C++ 侧一行都不用动。
	 *
	 *  ⚠️ 授予时机必须在 `InitializeEnemy()` 末尾（原型就位之后），
	 *     详见 URogueAbilitySet 的类注释。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Archetype|Abilities")
	TSoftObjectPtr<URogueAbilitySet> AttackAbilitySet;
};

class UDataTable;

namespace RogueEnemyArchetypes
{
	/** 从硬编码构建原型（保留向后兼容，DataTable 未配置时的回退） */
	AI_API FRogueEnemyArchetype BuildEnemyArchetype(ERogueEnemyType Type, bool bIsBoss);

	/** 从 DataTable 构建原型（优先使用） */
	AI_API FRogueEnemyArchetype BuildEnemyArchetypeFromDataTable(const UDataTable* DataTable, ERogueEnemyType Type, bool bIsBoss);

	/** 在 DataTable 中查找指定敌人类型的行名（RowName） */
	AI_API FName FindRowNameForEnemyType(const UDataTable* DataTable, ERogueEnemyType Type);

	/** 读取该敌人类型的"每波刷新上限"（一整个波次内的累计刷出只数上限）。
	 *  返回 0 表示**不限制**，包括下面三种情况：DataTable 为空、表里没有这个类型的行、
	 *  行里该字段没填（默认就是 0）。 */
	AI_API int32 GetMaxSpawnPerWave(const UDataTable* DataTable, ERogueEnemyType Type);

	/** 读取该敌人类型的"同时存活上限"（场上同时存在的只数上限）。
	 *  返回 0 表示**不限制**（判定口径与 GetMaxSpawnPerWave 一致）。 */
	AI_API int32 GetMaxAlive(const UDataTable* DataTable, ERogueEnemyType Type);
}
