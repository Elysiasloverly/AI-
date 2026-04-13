#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Core/RogueTypes.h"
#include "RogueUpgradeDefinitionAsset.generated.h"

USTRUCT(BlueprintType)
struct AI_API FRogueUpgradeDefinitionRow
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	ERogueUpgradeType Type = ERogueUpgradeType::MaxHealth;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FString Title;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FString Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	float Magnitude = 0.0f;
};

UCLASS(BlueprintType)
class AI_API URogueUpgradeDefinitionAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	bool FindDefinition(ERogueUpgradeType Type, FRogueUpgradeDefinitionRow& OutDefinition) const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<FRogueUpgradeDefinitionRow> Definitions;
};
