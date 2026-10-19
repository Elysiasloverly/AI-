#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RogueShopTerminal.generated.h"

class ARogueCharacter;
class UBoxComponent;
class USceneComponent;
class UStaticMeshComponent;

UCLASS(Blueprintable, meta = (DisplayName = "商店终端"))
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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "商店组件", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "商店组件", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> BaseMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "商店组件", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> GlowMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "商店组件", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBoxComponent> InteractionBox;

    /** 可在蓝图/关卡里移动提示锚点，适配你设置的模型尺寸。 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "商店组件", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<USceneComponent> PromptAnchor;
};
