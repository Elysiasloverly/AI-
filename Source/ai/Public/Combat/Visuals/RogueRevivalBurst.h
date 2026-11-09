#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RogueRevivalBurst.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UStaticMeshComponent;

/** Short local effect at the confirmed respawn position. */
UCLASS(Blueprintable)
class AI_API ARogueRevivalBurst : public AActor
{
	GENERATED_BODY()

public:
	ARogueRevivalBurst();
	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="复活表现")
	TObjectPtr<UStaticMeshComponent> Seal;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="复活表现")
	TObjectPtr<UInstancedStaticMeshComponent> Crystals;

private:
	void ApplyPose(float Progress);
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> SealMaterial;
	float Age = 0.f;
};
