#pragma once

#include "CoreMinimal.h"
#include "RogueTypes.generated.h"

UENUM(BlueprintType)
enum class ERogueUpgradeType : uint8
{
	MaxHealth,
	MoveSpeed,
	DashCooldown,
	AttackPower,
	AttackSpeed,
	PickupRadius,
	Recovery,
	AttackRange,
	ProjectileSpeed,
	ProjectileCount,
	Armor,
	ArmorCapacity,
	ExperienceGain,
	ScytheCount,
	RocketCount,
	LaserCount,
	HellTowerCount,
	LaserRefraction,
	MortarCount,
	MortarBlastRadius
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
	Spitter     UMETA(DisplayName = "喷射者")
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EnemyProfile")
	ERogueEnemyType EnemyType = ERogueEnemyType::Hunter;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EnemyProfile")
	float Health = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EnemyProfile")
	float Speed = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EnemyProfile")
	float Damage = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EnemyProfile")
	int32 ExperienceReward = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EnemyProfile")
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
