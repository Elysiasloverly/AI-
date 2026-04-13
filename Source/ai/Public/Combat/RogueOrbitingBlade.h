#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RogueOrbitingBlade.generated.h"

class USphereComponent;
class UStaticMeshComponent;

USTRUCT(BlueprintType)
struct AI_API FRogueOrbitingBladeVisualConfig
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	float CollisionRadius = 46.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	FRotator MeshRotation = FRotator(0.0f, 45.0f, 90.0f);

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	FVector MeshScale = FVector(0.75f, 0.14f, 1.1f);

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	FVector GlowScale = FVector(0.90f, 0.22f, 1.25f);

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	FVector SimplifiedGlowScale = FVector(0.86f, 0.20f, 1.18f);

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	float PulseSpeed = 8.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	float PulsePhaseOffsetPerBlade = 0.6f;

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	FVector GlowPulseBaseScale = FVector(0.86f, 0.20f, 1.18f);

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	FVector GlowPulseAmplitude = FVector(0.10f, 0.05f, 0.12f);

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	FVector ImpactEffectScale = FVector(0.21f, 0.10f, 0.21f);

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	float ImpactEffectLifetime = 0.14f;

	UPROPERTY(EditDefaultsOnly, Category = "Combat")
	float HitCooldown = 0.35f;
};

UCLASS()
class AI_API ARogueOrbitingBlade : public AActor
{
	GENERATED_BODY()

public:
	ARogueOrbitingBlade();

	virtual void Tick(float DeltaSeconds) override;

	void ActivatePooledBlade(AActor* InOwnerActor);
	void DeactivateToPool();
	bool IsAvailableInPool() const { return bPoolAvailable; }
	void ConfigureBlade(AActor* InOwnerActor, int32 InBladeIndex, int32 InBladeCount, float InOrbitRadius, float InRotationSpeed, float InDamage, float InSharedOrbitAngle);

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	void UpdateCooldowns(float DeltaSeconds);
	void UpdateTransform();

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USphereComponent> Collision;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> GlowMesh;

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	FRogueOrbitingBladeVisualConfig VisualConfig;

	TMap<TObjectPtr<AActor>, float> HitCooldowns;

	TWeakObjectPtr<AActor> OwnerActor;
	int32 BladeIndex = 0;
	int32 BladeCount = 1;
	float OrbitRadius = 220.0f;
	float RotationSpeed = 120.0f;
	float Damage = 12.0f;
	float SharedOrbitAngle = 0.0f;
	bool bVisualsSimplified = false;
	bool bPoolAvailable = true;
};
