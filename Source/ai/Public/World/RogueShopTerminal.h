#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RogueShopTerminal.generated.h"

class ARogueCharacter;
class UBoxComponent;
class USceneComponent;
class UStaticMeshComponent;

UCLASS()
class AI_API ARogueShopTerminal : public AActor
{
	GENERATED_BODY()

public:
	ARogueShopTerminal();

	UFUNCTION(BlueprintPure)
	FVector GetPromptWorldLocation() const;

protected:
	virtual void BeginPlay() override;

private:
	UFUNCTION()
	void HandleOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void HandleOverlapEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> BaseMesh;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> GlowMesh;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UBoxComponent> InteractionBox;
};
