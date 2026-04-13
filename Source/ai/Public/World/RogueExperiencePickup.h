#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RogueExperiencePickup.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UPointLightComponent;
class ARogueCharacter;

UCLASS()
class AI_API ARogueExperiencePickup : public AActor
{
	GENERATED_BODY()

public:
	ARogueExperiencePickup();

	virtual void Tick(float DeltaSeconds) override;
	void AddExperienceValue(int32 InExperienceValue);
	void ActivatePooledPickup(AActor* InOwner, const FVector& SpawnLocation, int32 InExperienceValue);
	void DeactivateToPool();
	bool IsAvailableInPool() const { return bPoolAvailable; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	UFUNCTION()
	void HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	void InitializePickup(int32 InExperienceValue);

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USphereComponent> Collision;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> GlowMesh;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UPointLightComponent> GlowLight;

	UPROPERTY(EditDefaultsOnly, Category = "Pickup")
	float AttractionSpeed = 900.0f;

	float PulseTime = 0.0f;
	float ActiveTimeSeconds = 0.0f;
	float PickupLifetime = 20.0f;
	float AutoMagnetDelay = 6.0f;
	int32 ExperienceValue = 1;
	bool bVisualsSimplified = false;
	bool bPoolAvailable = true;
	TWeakObjectPtr<ARogueCharacter> CachedPlayerCharacter;
};
