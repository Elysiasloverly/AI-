#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RogueArena.generated.h"

class USceneComponent;
class UStaticMeshComponent;
class UParticleSystem;
class UPointLightComponent;

UCLASS()
class AI_API ARogueArena : public AActor
{
	GENERATED_BODY()

public:
	ARogueArena();

	UFUNCTION(BlueprintPure)
	float GetArenaHalfExtent() const { return ArenaHalfExtent; }

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> FloorMesh;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> NorthWall;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> SouthWall;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> EastWall;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> WestWall;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> NorthTrim;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> SouthTrim;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> EastTrim;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> WestTrim;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> CenterRing;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> NorthBeacon;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> SouthBeacon;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> EastBeacon;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> WestBeacon;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UPointLightComponent> CenterFillLight;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UPointLightComponent> NorthAccentLight;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UPointLightComponent> SouthAccentLight;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UPointLightComponent> EastAccentLight;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UPointLightComponent> WestAccentLight;

	UPROPERTY()
	TObjectPtr<UParticleSystem> AmbientCornerParticle;

	UPROPERTY(EditDefaultsOnly, Category = "Arena")
	float ArenaHalfExtent = 6000.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Arena")
	float WallHeight = 220.0f;
};
