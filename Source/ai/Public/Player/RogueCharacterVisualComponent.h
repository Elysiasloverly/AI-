#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "RogueCharacterVisualComponent.generated.h"

class UPointLightComponent;
class UStaticMeshComponent;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class AI_API URogueCharacterVisualComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	URogueCharacterVisualComponent();

	void TickVisuals(float DeltaSeconds, float HorizontalSpeed, bool bDashActive, float DashDuration, float DashTimeRemaining);

private:
	/*
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> BodyMesh;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> ChestPlateMesh;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> CoreMesh;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> HaloMesh;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> ShellMesh;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> ShoulderLeftMesh;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> ShoulderRightMesh;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> BackpackMesh;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> ThrusterLeftMesh;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> ThrusterRightMesh;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> HeadMesh;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> VisorMesh;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> WingLeftMesh;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> WingRightMesh;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> TailFinMesh;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UPointLightComponent> BodyLight;
	*/
	float VisualPulseTime = 0.0f;
	FVector BodyBaseScale = FVector(0.90f, 0.90f, 0.90f);
	FVector CoreBaseScale = FVector(0.30f, 0.30f, 0.30f);
	FVector HaloBaseScale = FVector(0.68f, 0.68f, 0.08f);
};
