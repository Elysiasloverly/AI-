#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/DataTable.h"
#include "Core/RogueTypes.h"
#include "RogueUpgradeDefinitionAsset.generated.h"

/**
 * 升级定义行结构 —— 继承 FTableRowBase，可直接用于 DataTable 配置。
 * RowName 建议使用 ERogueUpgradeType 枚举值名称（如 "MaxHealth"、"MoveSpeed"）。
 */
USTRUCT(BlueprintType)
struct AI_API FRogueUpgradeDefinitionRow : public FTableRowBase
{
	GENERATED_BODY()

	/** 升级类型 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Upgrade", meta = (DisplayName = "升级类型"))
	ERogueUpgradeType Type = ERogueUpgradeType::MaxHealth;

	/** 显示名称 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Upgrade", meta = (DisplayName = "名称"))
	FString Title;

	/** 效果描述 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Upgrade", meta = (DisplayName = "效果描述"))
	FString Description;

	/** 数值幅度 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Upgrade", meta = (DisplayName = "数值"))
	float Magnitude = 0.0f;
};

/**
 * 升级定义资产 —— 通过引用 DataTable 来配置所有升级的名称与效果。
 * 在编辑器中创建一个以 FRogueUpgradeDefinitionRow 为行结构的 DataTable，
 * 然后将其赋值给本资产的 UpgradeDefinitionTable 属性即可。
 */
UCLASS(BlueprintType)
class AI_API URogueUpgradeDefinitionAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/**
	 * 根据升级类型从 DataTable 中查找对应的定义行。
	 * @return 找到返回 true，否则返回 false。
	 */
	bool FindDefinition(ERogueUpgradeType Type, FRogueUpgradeDefinitionRow& OutDefinition) const;

	/** 升级定义 DataTable（行结构为 FRogueUpgradeDefinitionRow） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Upgrade", meta = (DisplayName = "升级定义表"))
	TObjectPtr<UDataTable> UpgradeDefinitionTable;
};
