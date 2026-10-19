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
	virtual bool IsNetRelevantFor(const AActor* RealViewer, const AActor* ViewTarget, const FVector& SrcLocation) const override;
	void AddExperienceValue(int32 InExperienceValue);
	void ActivatePooledPickup(AActor* InOwner, const FVector& SpawnLocation, int32 InExperienceValue);
	void DeactivateToPool();
	bool IsAvailableInPool() const { return bPoolAvailable; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 注册本类的活跃位复制；调用父类注册以保留 Actor 自身的复制字段。 */
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UFUNCTION()
	void HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	void InitializePickup(int32 InExperienceValue);
	bool TryCollect(ARogueCharacter* PlayerCharacter);

	/**
	 * 选一个"能吸到这颗球"的玩家作为磁吸目标。
	 * 联机下必须遍历所有玩家：旧实现固定用 GetPlayerCharacter(0)，
	 * 于是玩家 2 附近的经验球会朝玩家 1 飞过去（位置还复制给所有人，看起来像球乱跑）。
	 * 过期时间超过 AutoMagnetDelay 后无视拾取半径，吸向最近的活着的玩家。
	 */
	ARogueCharacter* FindMagnetTarget() const;

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

	// 服务端维护在场状态；客户端通过 OnRep 和 BeginPlay 应用本地可见性、Tick与碰撞。
	// bPoolAvailable 等本地状态不单独复制，初次到达时也必须根据活跃位重建。
	UPROPERTY(ReplicatedUsing = OnRep_ReplicatedActive)
	bool bReplicatedActive = false;

	UFUNCTION()
	void OnRep_ReplicatedActive();

	/** 把复制来的"在场"状态翻译成本机状态（只在客户端做）。 */
	void ApplyReplicatedActiveState();
};
