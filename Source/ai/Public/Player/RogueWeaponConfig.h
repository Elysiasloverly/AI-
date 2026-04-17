#pragma once

#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"
#include "RogueWeaponConfig.generated.h"

class ARogueLaserBeam;
class ARogueOrbitingBlade;
class ARogueProjectile;
class ARogueRocketProjectile;

USTRUCT(BlueprintType)
struct AI_API FRogueProjectileWeaponConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Combat")
	TSubclassOf<ARogueProjectile> ProjectileClass;

	UPROPERTY(EditAnywhere, Category = "Combat")
	float Range = 1600.0f;

	UPROPERTY(EditAnywhere, Category = "Combat")
	float Speed = 1800.0f;

	UPROPERTY(EditAnywhere, Category = "Combat")
	float Interval = 0.8f;

	UPROPERTY(EditAnywhere, Category = "Combat")
	float Damage = 20.0f;

	UPROPERTY(EditAnywhere, Category = "Combat")
	int32 Count = 1;
};

USTRUCT(BlueprintType)
struct AI_API FRogueScytheWeaponConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Combat")
	TSubclassOf<ARogueOrbitingBlade> BladeClass;

	UPROPERTY(EditAnywhere, Category = "Combat")
	int32 Count = 0;

	UPROPERTY(EditAnywhere, Category = "Combat")
	float Damage = 14.0f;

	UPROPERTY(EditAnywhere, Category = "Combat")
	float OrbitRadius = 220.0f;

	UPROPERTY(EditAnywhere, Category = "Combat")
	float RotationSpeed = 130.0f;
};

USTRUCT(BlueprintType)
struct AI_API FRogueRocketWeaponConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Combat")
	TSubclassOf<ARogueRocketProjectile> ProjectileClass;

	UPROPERTY(EditAnywhere, Category = "Combat")
	int32 Count = 0;

	UPROPERTY(EditAnywhere, Category = "Combat")
	float Cooldown = 2.6f;

	UPROPERTY(EditAnywhere, Category = "Combat")
	float Damage = 46.0f;

	UPROPERTY(EditAnywhere, Category = "Combat")
	float ExplosionRadius = 280.0f;

	UPROPERTY(EditAnywhere, Category = "Combat")
	float Speed = 900.0f;
};

USTRUCT(BlueprintType)
struct AI_API FRogueLaserWeaponConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Combat")
	TSubclassOf<ARogueLaserBeam> BeamClass;

	UPROPERTY(EditAnywhere, Category = "Combat")
	int32 Count = 0;

	UPROPERTY(EditAnywhere, Category = "Combat")
	float Cooldown = 2.2f;

	UPROPERTY(EditAnywhere, Category = "Combat")
	float Damage = 34.0f;

	UPROPERTY(EditAnywhere, Category = "Combat")
	float Range = 1900.0f;

	UPROPERTY(EditAnywhere, Category = "Combat")
	int32 RefractionCount = 0;

	UPROPERTY(EditAnywhere, Category = "Combat")
	float RefractionRange = 950.0f;
};

USTRUCT(BlueprintType)
struct AI_API FRogueHellTowerWeaponConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Combat")
	int32 Count = 0;

	UPROPERTY(EditAnywhere, Category = "Combat")
	float Range = 1050.0f;

	UPROPERTY(EditAnywhere, Category = "Combat")
	float BaseDamage = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Combat")
	float DamageRampPerTick = 0.5f;

	UPROPERTY(EditAnywhere, Category = "Combat")
	float DamageTickInterval = 0.10f;

	UPROPERTY(EditAnywhere, Category = "Combat")
	float BeamRefreshInterval = 0.16f;

	UPROPERTY(EditAnywhere, Category = "Combat")
	float UpdateInterval = 0.05f;

	UPROPERTY(EditAnywhere, Category = "Combat")
	float HeavyCombatUpdateInterval = 0.08f;

	UPROPERTY(EditAnywhere, Category = "Combat")
	float HeavyCombatBeamRefreshPenalty = 0.05f;
};
