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
	LaserRefraction
};

UENUM()
enum class ERogueEnemyType : uint8
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

struct FRogueSpawnCadence
{
	int32 EffectiveMaxAliveEnemies = 0;
	int32 SpawnBatchSize = 1;
	float NextSpawnDelay = 0.0f;
};

struct FRogueEnemyProfile
{
	ERogueEnemyType EnemyType = ERogueEnemyType::Hunter;
	float Health = 0.0f;
	float Speed = 0.0f;
	float Damage = 0.0f;
	int32 ExperienceReward = 0;
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
