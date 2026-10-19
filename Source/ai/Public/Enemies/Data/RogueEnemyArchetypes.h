#pragma once

#include "CoreMinimal.h"
#include "Shared/RogueTypes.h"
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
	Spread3  UMETA(DisplayName = "散射"),
	RadialRing UMETA(DisplayName = "整圈弹幕")
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

/** 震荡柱一次齐射的参数；所有子弹水平飞行，碰到玩家/墙体或到时回收。 */
USTRUCT(BlueprintType)
struct AI_API FRogueEnemyBulletRingSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "环形弹幕", meta = (DisplayName = "每圈子弹数量", ClampMin = "8", ClampMax = "128"))
	int32 ProjectileCount = 48;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "环形弹幕", meta = (DisplayName = "子弹速度（厘米/秒）", ClampMin = "100", ClampMax = "3000"))
	float ProjectileSpeed = 720.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "环形弹幕", meta = (DisplayName = "最长飞行时间（秒）", ClampMin = "0.1", ClampMax = "10"))
	float ProjectileLifetime = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "环形弹幕", meta = (DisplayName = "碰撞与特效大小倍率", ToolTip = "影响碰撞球、灯光、拖尾偏移和命中特效；模型网格大小以蓝图组件的缩放为准。", ClampMin = "0.1", ClampMax = "1"))
	float ProjectileScale = 0.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "环形弹幕", meta = (DisplayName = "单发伤害倍率", ClampMin = "0"))
	float DamageMultiplier = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "环形弹幕", meta = (DisplayName = "发射高度（距柱底，厘米）", ClampMin = "20", ClampMax = "200"))
	float HeightAboveGround = 48.0f;

	/** 配置导入/蓝图赋值也需要边界约束，不能只依赖编辑器输入框。 */
	FRogueEnemyBulletRingSettings Sanitized() const
	{
		FRogueEnemyBulletRingSettings Result = *this;
		Result.ProjectileCount = FMath::Clamp(ProjectileCount, 8, 128);
		Result.ProjectileSpeed = FMath::Clamp(ProjectileSpeed, 100.0f, 3000.0f);
		Result.ProjectileLifetime = FMath::Clamp(ProjectileLifetime, 0.1f, 10.0f);
		Result.ProjectileScale = FMath::Clamp(ProjectileScale, 0.1f, 1.0f);
		Result.DamageMultiplier = FMath::Max(DamageMultiplier, 0.0f);
		Result.HeightAboveGround = FMath::Clamp(HeightAboveGround, 20.0f, 200.0f);
		return Result;
	}
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ranged|Ring", meta = (DisplayName = "整圈弹幕参数"))
	FRogueEnemyBulletRingSettings BulletRing;

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

	/** 可选攻击能力集；加载失败或未配置时按原型标志授予默认接触/远程/地面波能力。
	 *  默认 AI 只识别这三类能力槽；新增独立攻击决策仍需扩展 C++ 或蓝图调用路径。 */
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
