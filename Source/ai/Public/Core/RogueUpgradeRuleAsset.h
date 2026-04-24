#pragma once

#include "CoreMinimal.h"
#include "Core/RogueTypes.h"
#include "Engine/DataAsset.h"
#include "RogueUpgradeRuleAsset.generated.h"

UENUM(BlueprintType)
enum class ERogueWeaponUpgradeSource : uint8
{
	Scythe    UMETA(DisplayName = "镰刀"),
	Rocket    UMETA(DisplayName = "火箭"),
	Laser     UMETA(DisplayName = "激光"),
	HellTower UMETA(DisplayName = "地狱塔"),
	Mortar    UMETA(DisplayName = "迫击炮")
};

USTRUCT(BlueprintType)
struct AI_API FRogueWeaponUpgradeRuleRow
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "升级", meta = (DisplayName = "武器来源"))
	ERogueWeaponUpgradeSource Source = ERogueWeaponUpgradeSource::Scythe;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "升级", meta = (DisplayName = "升级类型"))
	ERogueUpgradeType UpgradeType = ERogueUpgradeType::ScytheCount;
};

UCLASS(BlueprintType)
class AI_API URogueUpgradeRuleAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	URogueUpgradeRuleAsset();

	UFUNCTION(BlueprintCallable, CallInEditor, Category = "升级", meta = (DisplayName = "恢复默认升级规则"))
	void ResetToDefaultRules();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "升级", meta = (DisplayName = "基础升级池"))
	TArray<ERogueUpgradeType> BaseUpgradeTypes;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "升级", meta = (DisplayName = "默认武器数量升级池"))
	TArray<ERogueUpgradeType> DefaultWeaponUpgradeTypes;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "升级", meta = (DisplayName = "武器专属升级规则"))
	TArray<FRogueWeaponUpgradeRuleRow> WeaponUpgradeRules;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "升级", meta = (DisplayName = "冲刺最小冷却"))
	float MinimumDashCooldown = 0.5f;
};
