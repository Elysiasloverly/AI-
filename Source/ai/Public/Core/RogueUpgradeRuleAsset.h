#pragma once

#include "CoreMinimal.h"
#include "Core/RogueTypes.h"
#include "Engine/DataAsset.h"
#include "RogueUpgradeRuleAsset.generated.h"

UENUM(BlueprintType)
enum class ERogueWeaponUpgradeSource : uint8
{
	Scythe,
	Rocket,
	Laser,
	HellTower
};

USTRUCT(BlueprintType)
struct AI_API FRogueWeaponUpgradeRuleRow
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Upgrade")
	ERogueWeaponUpgradeSource Source = ERogueWeaponUpgradeSource::Scythe;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Upgrade")
	ERogueUpgradeType UpgradeType = ERogueUpgradeType::ScytheCount;
};

UCLASS(BlueprintType)
class AI_API URogueUpgradeRuleAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Upgrade")
	TArray<ERogueUpgradeType> BaseUpgradeTypes;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Upgrade")
	TArray<ERogueUpgradeType> DefaultWeaponUpgradeTypes;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Upgrade")
	TArray<FRogueWeaponUpgradeRuleRow> WeaponUpgradeRules;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Upgrade")
	float MinimumDashCooldown = 0.5f;
};
