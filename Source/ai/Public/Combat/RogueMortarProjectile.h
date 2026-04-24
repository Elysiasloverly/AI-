#pragma once

#include "CoreMinimal.h"
#include "Combat/RogueRocketProjectile.h"
#include "RogueMortarProjectile.generated.h"

class UStaticMeshComponent;
class UMaterialInterface;

UCLASS()
class AI_API ARogueMortarProjectile : public ARogueRocketProjectile
{
	GENERATED_BODY()

public:
	ARogueMortarProjectile();

	virtual void Tick(float DeltaSeconds) override;
	virtual void DeactivateToPool() override;

	void ActivatePooledMortar(
		AActor* InOwner,
		APawn* InInstigator,
		const FVector& SpawnLocation,
		const FVector& InTargetLocation,
		float InDamage,
		float InExplosionRadius,
		float InLaunchSpeed);

protected:
	virtual void BeginPlay() override;

private:
	void UpdateLandingIndicator(float DeltaSeconds);

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> LandingIndicatorMesh;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> LandingIndicatorMaterial;

	UPROPERTY(EditDefaultsOnly, Category = "Mortar")
	float MortarArcParam = 0.74f;

	UPROPERTY(EditDefaultsOnly, Category = "Mortar")
	float MortarGravityScale = 1.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Mortar")
	float MinimumLaunchSpeed = 680.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Mortar")
	float MinimumFlightTime = 0.65f;

	UPROPERTY(EditDefaultsOnly, Category = "Mortar")
	float MaximumFlightTime = 2.8f;

	UPROPERTY(EditDefaultsOnly, Category = "Mortar")
	float LandingIndicatorHeight = 6.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Mortar")
	float LandingIndicatorScaleDivisor = 140.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Mortar")
	float LandingIndicatorPulseSpeed = 4.8f;

	UPROPERTY(EditDefaultsOnly, Category = "Mortar")
	float LandingIndicatorPulseAmplitude = 0.14f;

	FVector TargetLocation = FVector::ZeroVector;
	FVector IndicatorBaseScale = FVector(1.0f, 1.0f, 0.12f);
	float IndicatorPulseTime = 0.0f;
	bool bMortarProjectileActive = false;
};
