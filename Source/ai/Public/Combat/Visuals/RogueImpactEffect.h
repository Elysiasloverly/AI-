#pragma once

#include "Engine/World.h"
#include "Engine/EngineTypes.h"
#include "GameFramework/Actor.h"
#include "GameFramework/DamageType.h"
#include "Kismet/GameplayStatics.h"

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RogueImpactEffect.generated.h"

class UMaterialInterface;
class USceneComponent;
class UStaticMeshComponent;
class UStaticMesh;
class UParticleSystem;
class UParticleSystemComponent;
class APawn;

UENUM()
enum class ERogueImpactVisualStyle : uint8
{
	Spark,
	Laser,
	Explosion
};

USTRUCT(BlueprintType)
struct AI_API FRogueImpactStyleConfig
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	FVector MeshLocalScale = FVector(0.18f, 0.18f, 0.18f);

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	FRotator MeshLocalRotation = FRotator::ZeroRotator;

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	FVector ParticleScale = FVector(0.42f, 0.42f, 0.42f);

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	bool bSpawnPrimaryParticle = true;

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	bool bDeactivatePrimaryParticleImmediately = false;
};

UCLASS()
class AI_API ARogueImpactEffect : public AActor
{
	GENERATED_BODY()

public:
	ARogueImpactEffect();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
	void ActivatePooledEffect(AActor* InOwner, const FVector& InLocation, const FRotator& InRotation, ERogueImpactVisualStyle InStyle, const FVector& InScale, float InLifetime);
	void DeactivateToPool();
	bool IsAvailableInPool() const { return bPoolAvailable; }

	static ARogueImpactEffect* SpawnImpactEffect(UWorld* World, const FVector& Location, const FRotator& Rotation, ERogueImpactVisualStyle Style, const FVector& Scale, float Lifetime = 0.16f, AActor* Owner = nullptr);

private:
	void InitializeImpactVisuals(ERogueImpactVisualStyle InStyle, const FVector& InScale, const FRotator& InRotation);

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> EffectMesh;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UParticleSystemComponent> PrimaryParticleComponent;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UParticleSystemComponent> SmokeParticleComponent;

	UPROPERTY()
	TObjectPtr<UStaticMesh> SparkMeshAsset;

	UPROPERTY()
	TObjectPtr<UStaticMesh> LaserMeshAsset;

	UPROPERTY()
	TObjectPtr<UStaticMesh> ExplosionMeshAsset;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> SparkMaterial;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> LaserMaterial;

	UPROPERTY()
	TObjectPtr<UMaterialInterface> ExplosionMaterial;

	UPROPERTY()
	TObjectPtr<UParticleSystem> SparkParticle;

	UPROPERTY()
	TObjectPtr<UParticleSystem> LaserParticle;

	UPROPERTY()
	TObjectPtr<UParticleSystem> ExplosionParticle;

	UPROPERTY()
	TObjectPtr<UParticleSystem> ExplosionSmokeParticle;

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	FRogueImpactStyleConfig SparkStyle;

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	FRogueImpactStyleConfig LaserStyle;

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	FRogueImpactStyleConfig ExplosionStyle;

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	FVector ExplosionSmokeScale = FVector(0.32f, 0.32f, 0.32f);

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	float MinimumLifetime = 0.03f;

	FTimerHandle ReturnToPoolTimerHandle;
	bool bPoolAvailable = true;
	bool bCountedAsActive = false;
};
