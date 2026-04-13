#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RogueRocketProjectile.generated.h"

class UProjectileMovementComponent;
class USphereComponent;
class UStaticMeshComponent;
class UPointLightComponent;
class APawn;

USTRUCT(BlueprintType)
struct AI_API FRogueRocketVisualConfig
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	FVector MeshScale = FVector(0.18f, 0.18f, 0.34f);

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	FVector BodyScale = FVector(0.14f, 0.14f, 0.34f);

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	FVector ExhaustCoreScale = FVector(0.10f, 0.10f, 0.22f);

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	FVector ExhaustGlowScale = FVector(0.18f, 0.18f, 0.30f);

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	FVector AuraScale = FVector(0.22f, 0.22f, 0.42f);

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	FVector TrailScale = FVector(0.11f, 0.11f, 0.42f);

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	FVector SimplifiedExhaustCoreScale = FVector(0.08f, 0.08f, 0.18f);

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	FVector SimplifiedExhaustGlowScale = FVector(0.15f, 0.15f, 0.26f);

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	FVector SimplifiedAuraScale = FVector(0.18f, 0.18f, 0.36f);

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	FVector SimplifiedTrailScale = FVector(0.08f, 0.08f, 0.30f);

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	float LightIntensity = 860.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	float SimplifiedLightIntensity = 720.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	float LightPulseAmplitude = 620.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	float PulseSpeed = 11.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	FVector ExhaustCorePulseAmplitude = FVector(0.04f, 0.04f, 0.10f);

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	FVector ExhaustGlowPulseAmplitude = FVector(0.07f, 0.07f, 0.16f);

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	FVector AuraPulseAmplitude = FVector(0.06f, 0.06f, 0.10f);

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	FVector TrailPulseAmplitude = FVector(0.05f, 0.05f, 0.18f);

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	float AuraRotationSpeed = -160.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	float ImpactEffectLifetime = 0.16f;
};

UCLASS()
class AI_API ARogueRocketProjectile : public AActor
{
	GENERATED_BODY()

public:
	ARogueRocketProjectile();

	virtual void Tick(float DeltaSeconds) override;
	void ActivatePooledRocket(AActor* InOwner, APawn* InInstigator, const FVector& SpawnLocation, const FRotator& SpawnRotation, const FVector& InDirection, float InSpeed, float InDamage, float InExplosionRadius);
	void DeactivateToPool();
	bool IsAvailableInPool() const { return bPoolAvailable; }

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void HandleProjectileStop(const FHitResult& ImpactResult);

	void InitializeRocket(const FVector& InDirection, float InSpeed, float InDamage, float InExplosionRadius);
	void Explode();

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USphereComponent> Collision;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> BodyMesh;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> ExhaustCoreMesh;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> ExhaustGlowMesh;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> FinLeftMesh;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> FinRightMesh;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> AuraMesh;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> TrailMesh;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UPointLightComponent> RocketLight;

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	FRogueRocketVisualConfig VisualConfig;

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	float CollisionRadius = 18.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Pooling")
	float PooledLifetime = 5.0f;

	float Damage = 40.0f;
	float ExplosionRadius = 280.0f;
	float PulseTime = 0.0f;
	bool bExploded = false;
	float RemainingLifetime = 0.0f;
	bool bVisualsSimplified = false;
	bool bPoolAvailable = true;
};
