#pragma once

#include "CoreMinimal.h"
#include "Shared/RogueTypes.h"
#include "Engine/DataAsset.h"
#include "RogueUpgradeRuleAsset.generated.h"

UENUM(BlueprintType)
enum class ERogueWeaponUpgradeSource : uint8
{
	Scythe    UMETA(DisplayName = "银刃"),
	Rocket    UMETA(DisplayName = "冰焰·陨星"),
	Laser     UMETA(DisplayName = "星符·流光"),
	HellTower UMETA(DisplayName = "焰符·灼光"),
	Mortar    UMETA(DisplayName = "冰符·霜玉"),
	Projectile UMETA(DisplayName = "冰符·寒弹"),
	Talisman UMETA(DisplayName="灵符·归心"),
    FrostBlade UMETA(DisplayName="冰刀·霜月")
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

    /** 保底从已通过永久许可和本局前置检查的卡池中选择，不赠送未解锁卡。0关闭。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="升级", meta=(DisplayName="进攻选项保底截止等级",ClampMin="0",ClampMax="100"))
    int32 OpeningOffenseLevels = 8;
};
