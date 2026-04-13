#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RogueProjectile.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UProjectileMovementComponent;
class UPointLightComponent;
class APawn;

USTRUCT(BlueprintType)
struct AI_API FRogueProjectileVisualConfig
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	float MeshScale = 0.14f;

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	float GlowScale = 0.34f;

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	float RingScale = 0.48f;

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	FVector TrailOffset = FVector(-18.0f, 0.0f, 0.0f);

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	FRotator TrailRotation = FRotator(90.0f, 0.0f, 0.0f);

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	FVector TrailScale = FVector(0.08f, 0.08f, 0.22f);

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	float LightIntensity = 700.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	FLinearColor LightColor = FLinearColor(0.20f, 0.55f, 1.0f);

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	float LightAttenuationRadius = 78.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	float LightSourceRadius = 6.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	float LightSoftSourceRadius = 10.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	float PulseSpeed = 12.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	float CorePulseBaseScale = 0.13f;

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	float CorePulseAmplitude = 0.03f;

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	float GlowPulseBaseScale = 0.30f;

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	float GlowPulseAmplitude = 0.08f;

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	float RingPulseBaseScale = 0.42f;

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	float RingPulseAmplitude = 0.10f;

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	float TrailPulseBaseRadius = 0.06f;

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	float TrailPulseRadiusAmplitude = 0.02f;

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	float TrailPulseBaseLength = 0.20f;

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	float TrailPulseLengthAmplitude = 0.12f;

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	float SimplifiedMeshScale = 0.13f;

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	float SimplifiedGlowScale = 0.30f;

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	float SimplifiedRingScale = 0.42f;

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	FVector SimplifiedTrailScale = FVector(0.06f, 0.06f, 0.20f);

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	FVector ImpactEffectScale = FVector(0.17f, 0.17f, 0.17f);

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	float ImpactEffectLifetime = 0.14f;

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	FVector StopImpactEffectScale = FVector(0.15f, 0.15f, 0.15f);

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	float StopImpactEffectLifetime = 0.12f;
};

UCLASS()
class AI_API ARogueProjectile : public AActor
{
	GENERATED_BODY()

public:
	ARogueProjectile();

	virtual void Tick(float DeltaSeconds) override;
	void ActivatePooledProjectile(AActor* InOwner, APawn* InInstigator, const FVector& SpawnLocation, const FRotator& SpawnRotation, const FVector& InDirection, float InSpeed, float InDamage);
	void DeactivateToPool();
	bool IsAvailableInPool() const { return bPoolAvailable; }

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void HandleProjectileStop(const FHitResult& ImpactResult);

	void InitializeProjectile(const FVector& InDirection, float InSpeed, float InDamage);

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USphereComponent> Collision;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> GlowMesh;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> RingMesh;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> TrailMesh;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UPointLightComponent> GlowLight;

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	FRogueProjectileVisualConfig VisualConfig;

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	float CollisionRadius = 16.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Pooling")
	float PooledLifetime = 3.0f;

	FVector TravelDirection = FVector::ForwardVector;
	float PulseTime = 0.0f;
	float Speed = 1600.0f;
	float Damage = 20.0f;
	float RemainingLifetime = 0.0f;
	bool bResolvedImpact = false;
	bool bVisualsSimplified = false;
	bool bPoolAvailable = true;
};
