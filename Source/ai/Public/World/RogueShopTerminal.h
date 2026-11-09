#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RogueShopTerminal.generated.h"

class ARogueCharacter;
class UBoxComponent;
class UParticleSystem;
class USceneComponent;
class UStaticMeshComponent;

UENUM()
enum class ERogueShopFlightPhase : uint8
{
	Hidden,
	Falling,
	Settling,
	Ready,
	Charging,
	Rising
};

/** 一次阶段变更的落点、方向与服务端时间，客户端据此播放同一段动画。 */
USTRUCT()
struct FRogueShopFlightState
{
	GENERATED_BODY()
	UPROPERTY() ERogueShopFlightPhase Phase = ERogueShopFlightPhase::Hidden;
	UPROPERTY() FVector_NetQuantize10 Ground = FVector::ZeroVector;
	UPROPERTY() FVector_NetQuantizeNormal Direction = FVector::ForwardVector;
	UPROPERTY() double ServerStartTime = 0.0;
	UPROPERTY() double LastLandingTime = -1.0;
};

UCLASS(Blueprintable, meta = (DisplayName = "商店终端"))
class AI_API ARogueShopTerminal : public AActor
{
	GENERATED_BODY()

public:
	ARogueShopTerminal();
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	bool TryDropNear(const ARogueCharacter& Player);
	void BeginDeparture();
	bool IsWaitingInSky() const { return Flight.Phase == ERogueShopFlightPhase::Hidden; }
	bool IsReady() const { return Flight.Phase == ERogueShopFlightPhase::Ready; }
	bool CanInteract(const ARogueCharacter& Player) const;

	UFUNCTION(BlueprintPure)
	FVector GetPromptWorldLocation() const;

protected:
	virtual void BeginPlay() override;

private:
	void SetPhase(ERogueShopFlightPhase Phase);
	void UpdateFlightPose(double Age);
	void PlayLandingDust();
	double GetServerTime() const;
	UFUNCTION() void OnRep_Flight();
	UPROPERTY(ReplicatedUsing = OnRep_Flight) FRogueShopFlightState Flight;

	FRotator AuthoredRotation = FRotator::ZeroRotator;
	FVector AuthoredScale = FVector::OneVector;
	double RootHeightFromGround = 10.0;
	double LastDustStartTime = -1.0;
	bool bFlightPresentationInitialized = false;

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

	/** 可在蓝图或关卡里移动提示锚点，适配模型尺寸。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "商店组件", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> PromptAnchor;

	UPROPERTY(EditDefaultsOnly, Category = "商店表现", meta = (DisplayName = "落地尘土"))
	TObjectPtr<UParticleSystem> LandingDust;
};
