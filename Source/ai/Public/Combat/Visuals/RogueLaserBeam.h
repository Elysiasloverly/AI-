#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RogueLaserBeam.generated.h"

class USceneComponent;
class UStaticMeshComponent;
class UMaterialInterface;

USTRUCT(BlueprintType)
struct AI_API FRogueLaserBeamVisualStyle
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	float BeamThickness = 0.10f;

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	float GlowThickness = 0.18f;

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	float GlowLengthScaleMultiplier = 1.02f;

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	float StartRingSize = 0.22f;

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	float StartRingDepth = 0.10f;

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	float EndRingSize = 0.30f;

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	float EndRingDepth = 0.12f;

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	FVector ImpactEffectScale = FVector(0.22f, 0.22f, 0.22f);

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	float ImpactEffectLifetime = 0.12f;

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	bool bShowStartAndEndRings = true;
};

UCLASS()
class AI_API ARogueLaserBeam : public AActor
{
	GENERATED_BODY()

public:
	ARogueLaserBeam();

	void InitializeBeam(const FVector& StartLocation, const FVector& EndLocation, bool bUseInfernoStyle = false, bool bSpawnImpactEffect = true, float BeamLifetime = 0.10f);
	void ActivatePooledBeam(AActor* InOwner, const FVector& StartLocation, const FVector& EndLocation, bool bUseInfernoStyle = false, bool bSpawnImpactEffect = true, float BeamLifetime = 0.10f);
	void DeactivateToPool();
	bool IsAvailableInPool() const { return bPoolAvailable; }

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> BeamMesh;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> GlowMesh;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> StartRingMesh;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> EndRingMesh;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> PlayerBeamMaterial;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> PlayerGlowMaterial;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> PlayerRingMaterial;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> InfernoBeamMaterial;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> InfernoGlowMaterial;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> InfernoRingMaterial;

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	float MinimumBeamLengthScale = 0.2f;

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	float BeamLengthDivisor = 100.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	float MinimumBeamLifetime = 0.03f;

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	FRogueLaserBeamVisualStyle PlayerVisualStyle;

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	FRogueLaserBeamVisualStyle InfernoVisualStyle;

	FTimerHandle ReturnToPoolTimerHandle;
	bool bPoolAvailable = true;
};
