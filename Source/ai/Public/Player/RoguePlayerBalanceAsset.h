#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RoguePlayerBalanceAsset.generated.h"

USTRUCT(BlueprintType)
struct AI_API FRoguePlayerBaseStatConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Progression")
	int32 ExperienceToNextLevel = 6;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats")
	float MaxHealth = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats")
	float MaxArmor = 25.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats")
	float MoveSpeed = 650.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats")
	float PickupRadius = 260.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats")
	float HealthRegenPerSecond = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats")
	float DamageReductionPercent = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats")
	float ExperienceMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats")
	float ArmorRechargeDelay = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats")
	float ArmorRechargeRate = 45.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mobility")
	float DashCooldownDuration = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mobility")
	float DashMinCooldown = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mobility")
	float DashDuration = 0.16f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mobility")
	float DashSpeed = 4200.0f;
};

UCLASS(BlueprintType)
class AI_API URoguePlayerBalanceAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats")
	FRoguePlayerBaseStatConfig BaseStats;

	// 武器配置已迁移到 DataTable（DT_WeaponConfig），此处不再包含武器字段
};
