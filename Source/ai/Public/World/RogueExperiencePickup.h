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

	/** 复制的属性表：只有一个"这颗球在不在场"的状态位（见下）。 */
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	UFUNCTION()
	void HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	void InitializePickup(int32 InExperienceValue);

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

	// ---- 联机：把"这颗球在不在场"这**一位**复制给客户端 ----
	//
	// 为什么必须复制：池化的取出/归还是**本机状态**（bPoolAvailable / SetActorTickEnabled /
	// SetActorEnableCollision），都不走复制。而客户端的 BeginPlay 里也会执行一次
	// DeactivateToPool()，于是客户端上的球永远停在"在池里"：
	//   · Tick 在开头就 return → 悬浮/脉冲动画完全不跑（客户端看到的是一颗钉死的球）；
	//   · 中途加入的客户端更糟：初始复制值（bHidden=false）先到、BeginPlay 又本地隐藏它，
	//     而服务端的 bHidden 之后不再变化 → 这颗球对他永久隐形。
	// 只复制这一位，具体表现由各端自己决定。
	UPROPERTY(ReplicatedUsing = OnRep_ReplicatedActive)
	bool bReplicatedActive = false;

	UFUNCTION()
	void OnRep_ReplicatedActive();

	/** 把复制来的"在场"状态翻译成本机状态（只在客户端做）。 */
	void ApplyReplicatedActiveState();
};
