#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RogueEnemyProjectile.generated.h"

class UProjectileMovementComponent;
class USphereComponent;
class UStaticMeshComponent;
class UPointLightComponent;
class APawn;

USTRUCT(BlueprintType)
struct AI_API FRogueEnemyProjectileVisualState
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	float BodyScale = 0.58f;

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	float CoreScale = 0.32f;

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	float GlowScale = 1.12f;

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	float SpineScale = 0.56f;

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	float RingScale = 1.28f;

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	FVector TrailScale = FVector(0.10f, 0.10f, 0.28f);

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	float CollisionRadius = 22.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	float LightIntensity = 4200.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	float LightAttenuationRadius = 260.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	float LightSourceRadius = 10.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	float LightSoftSourceRadius = 16.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	FVector ImpactEffectScale = FVector(0.24f, 0.24f, 0.24f);
};

UCLASS()
class AI_API ARogueEnemyProjectile : public AActor
{
	GENERATED_BODY()

public:
	ARogueEnemyProjectile();

	virtual void Tick(float DeltaSeconds) override;
	void ActivatePooledProjectile(AActor* InOwner, APawn* InInstigator, const FVector& SpawnLocation, const FRotator& SpawnRotation, const FVector& InDirection, float InSpeed, float InDamage, bool bInBossShot);
	void DeactivateToPool();
	bool IsAvailableInPool() const { return bPoolAvailable; }

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void HandleProjectileStop(const FHitResult& ImpactResult);

	void InitializeProjectile(const FVector& InDirection, float InSpeed, float InDamage, bool bInBossShot);
	void ResolveImpact(AActor* OtherActor, const FVector& ImpactLocation, const FVector& ImpactNormal);

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USphereComponent> Collision;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> BodyMesh;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> CoreMesh;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> GlowMesh;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> SpineMesh;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> RingMesh;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> TrailMesh;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UPointLightComponent> GlowLight;

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	FRogueEnemyProjectileVisualState NormalVisualState;

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	FRogueEnemyProjectileVisualState BossVisualState;

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	FRogueEnemyProjectileVisualState SimplifiedNormalVisualState;

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	FRogueEnemyProjectileVisualState SimplifiedBossVisualState;

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	float PulseSpeedNormal = 7.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	float PulseSpeedBoss = 9.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	float BodyPulseAmplitude = 0.06f;

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	float CorePulseAmplitude = 0.05f;

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	float GlowPulseAmplitude = 0.20f;

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	float SpinePulseAmplitude = 0.08f;

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	float RingPulseAmplitude = 0.22f;

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	FVector TrailPulseAmplitude = FVector(0.02f, 0.02f, 0.14f);

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	float LightPulseAmplitudeNormal = 1100.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	float LightPulseAmplitudeBoss = 1600.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Pooling")
	float PooledLifetime = 4.0f;

	float Damage = 10.0f;
	float PulseTime = 0.0f;
	float RemainingLifetime = 0.0f;
	bool bBossShot = false;
	bool bResolvedImpact = false;
	bool bVisualsSimplified = false;
	bool bPoolAvailable = true;
};
