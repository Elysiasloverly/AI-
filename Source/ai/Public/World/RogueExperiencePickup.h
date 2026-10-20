#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RogueExperiencePickup.generated.h"

class USphereComponent;
class ARogueCharacter;

/** 经验球。
 *
 * C++ 只保留拾取碰撞、对象池、复制与磁吸位移；**不创建任何网格、粒子或灯光，也不写死资产路径**。
 * 外观由 BP 子类（`/Game/BP/World/BP_ExperiencePickup`）用 SCS 组件自行搭建：
 * 在 BP 里加网格 / 特效 / 灯光、换材质都即时生效，C++ 不会覆盖它们的缩放或材质。
 * BP 未配置时回退本原生类，此时只有碰撞体，画面上看不到球 —— 配置入口见
 * `Docs/Experience-Pickup-Blueprint.md`。
 */
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

	// ===========================================================
	//  供 BP 表现读取的状态（只读，不参与玩法判定）
	// ===========================================================

	/** 当前经验值。BP 可据此换材质、加发光强度或叠层数；合并后会变大。 */
	UFUNCTION(BlueprintPure, Category = "Rogue|Pickup")
	int32 GetExperienceValue() const { return ExperienceValue; }

	/** 当前是否处于高负载表现裁剪。为 true 时 BP 应关掉拖尾、粒子、多灯等贵表现；
	 *  判据与敌人表现裁剪共用 `URogueEnemyTrackerSubsystem::ShouldCullCombatEffects()`。 */
	UFUNCTION(BlueprintPure, Category = "Rogue|Pickup")
	bool ShouldSimplifyVisuals() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** 注册本类的活跃位复制；调用父类注册以保留 Actor 自身的复制字段。 */
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ===========================================================
	//  外观钩子 —— 全部由 BP 实现，C++ 只负责在正确的时机通知
	// ===========================================================

	/** 本颗球被激活（服务端与各客户端都会收到）。用于一次性外观初始化：
	 *  按 `InExperienceValue` 换颜色/尺寸/材质参数。经验值相同的球会反复复用本对象。 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Rogue|Pickup", meta = (DisplayName = "经验球激活"))
	void OnPickupActivated(int32 InExperienceValue);

	/** 与附近的球合并，经验值变大且寿命重置。 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Rogue|Pickup", meta = (DisplayName = "经验球合并"))
	void OnPickupMerged(int32 NewExperienceValue);

	/** 回到对象池（被拾取、超时或整局结束）。此时 Actor 已隐藏，但位置仍是最后一次的位置，
	 *  可以在 BP 里从这里生成一个"被吸走"的特效。 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Rogue|Pickup", meta = (DisplayName = "经验球回收"))
	void OnPickupDeactivated();

	/** 磁吸飞行速度（cm/s）。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Pickup", meta = (DisplayName = "磁吸速度", ClampMin = "0", Units = "cm/s"))
	float AttractionSpeed = 900.0f;

	/** 存活时间（秒）；超时自动回池。合并会重置该计时。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Pickup", meta = (DisplayName = "存活时间", ClampMin = "0.1", Units = "s"))
	float PickupLifetime = 20.0f;

	/** 无视拾取半径、直接吸向最近活玩家的等待时间（秒）。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Pickup", meta = (DisplayName = "自动磁吸延迟", ClampMin = "0", Units = "s"))
	float AutoMagnetDelay = 6.0f;

private:
	UFUNCTION()
	void HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	void InitializePickup(int32 InExperienceValue);
	bool TryCollect(ARogueCharacter* PlayerCharacter);

	/** 经验值越高球越大：`1 + 经验 × 0.015`，上限 2.4 倍。作用在 Actor 缩放上，
	 *  BP 的子组件按相对缩放跟随，不需要自己再乘一遍。 */
	void ApplyExperienceScale();

	/**
	 * 选一个"能吸到这颗球"的玩家作为磁吸目标。
	 * 联机下必须遍历所有玩家：旧实现固定用 GetPlayerCharacter(0)，
	 * 于是玩家 2 附近的经验球会朝玩家 1 飞过去（位置还复制给所有人，看起来像球乱跑）。
	 * 过期时间超过 AutoMagnetDelay 后无视拾取半径，吸向最近的活着的玩家。
	 */
	ARogueCharacter* FindMagnetTarget() const;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USphereComponent> Collision;

	float ActiveTimeSeconds = 0.0f;
	bool bPoolAvailable = true;

	/** 经验值复制到客户端。
	 *  FRepMovement 只带位置/朝向/速度，不带缩放；不复制经验值的话，客户端上的球永远是 1 倍大小，
	 *  与服务器的尺寸对不上，BP 也没法按经验值分档换外观。经验值只在激活与合并时变化，量很小。 */
	UPROPERTY(ReplicatedUsing = OnRep_ExperienceValue)
	int32 ExperienceValue = 1;

	UFUNCTION()
	void OnRep_ExperienceValue();

	// 服务端维护在场状态；客户端通过 OnRep 和 BeginPlay 应用本地可见性、Tick与碰撞。
	// bPoolAvailable 等本地状态不单独复制，初次到达时也必须根据活跃位重建。
	UPROPERTY(ReplicatedUsing = OnRep_ReplicatedActive)
	bool bReplicatedActive = false;

	UFUNCTION()
	void OnRep_ReplicatedActive();

	/** 把复制来的"在场"状态翻译成本机状态（只在客户端做），
	 *  并在入场时补发一次 `OnPickupActivated`，让客户端的 BP 外观与服务器一致。 */
	void ApplyReplicatedActiveState();
};
