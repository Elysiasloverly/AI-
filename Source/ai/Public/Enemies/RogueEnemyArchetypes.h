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
	ZigZagChase  UMETA(DisplayName = "Z字形追击")
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
	Spitter
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

USTRUCT(BlueprintType)
struct AI_API FRogueEnemyArchetype
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Archetype")
	FRogueEnemyMovementArchetype Movement;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Archetype")
	FRogueEnemyRangedArchetype Ranged;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Archetype")
	FRogueEnemyVisualArchetype Visual;
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
}
