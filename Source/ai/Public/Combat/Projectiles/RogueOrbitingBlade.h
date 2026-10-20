#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RogueOrbitingBlade.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;

USTRUCT(BlueprintType)
struct AI_API FRogueOrbitingBladeVisualConfig
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	float CollisionRadius = 46.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	FRotator MeshRotation = FRotator(0.0f, 45.0f, 90.0f);

	// 保留旧资产字段；基础尺寸在蓝图 Mesh 组件中设置，大小升级在该尺寸上乘算。
	UPROPERTY()
	FVector MeshScale = FVector(0.75f, 0.14f, 1.1f);

	// Overlay Material 会额外绘制网格；使用合适的半透明/叠加材质表现发光，
	// 并检查材质混合方式与绘制成本。
	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	TObjectPtr<UMaterialInterface> GlowMaterial;

	/** 要驱动的标量参数名；SetupGlowMaterial 解析可用参数，缺失时跳过脉冲并限次提示。 */
	UPROPERTY(EditDefaultsOnly, Category = "Visual", meta = (DisplayName = "脉冲参数名"))
	FName GlowPulseParameterName = TEXT("GlowPulse");

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	float PulseSpeed = 8.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	float PulsePhaseOffsetPerBlade = 0.6f;

	/** 脉冲参数的下限/上限（sin 的 0 与 1 两端各映射到哪个值） */
	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	float GlowPulseMin = 0.35f;

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	float GlowPulseMax = 1.0f;

	/** 高负载裁剪时脉冲参数固定成的值（不再每帧算 sin） */
	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	float GlowPulseHighLoadValue = 0.35f;

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	FVector ImpactEffectScale = FVector(0.21f, 0.10f, 0.21f);

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	float ImpactEffectLifetime = 0.14f;

	UPROPERTY(EditDefaultsOnly, Category = "Combat")
	float HitCooldown = 0.35f;
};

UCLASS()
class AI_API ARogueOrbitingBlade : public AActor
{
	GENERATED_BODY()

public:
	ARogueOrbitingBlade();

	virtual void Tick(float DeltaSeconds) override;

	void ActivatePooledBlade(AActor* InOwnerActor);
	void DeactivateToPool();
	bool IsAvailableInPool() const { return bPoolAvailable; }
	void ConfigureBlade(AActor* InOwnerActor, int32 InBladeIndex, int32 InBladeCount, float InOrbitRadius, float InRotationSpeed, float InDamage, float InSharedOrbitAngle, float InSizeMultiplier = 1.0f);

protected:
	virtual void BeginPlay() override;

private:
	/** 贴身持续伤害：遍历当前重叠的敌人，按 HitCooldown 节流结算。
	 *  取代原来的 OnComponentBeginOverlap 方案（那个只在"进入"那一帧触发一次）。 */
	void ApplyContactDamage();

	void UpdateCooldowns(float DeltaSeconds);
	void UpdateTransform();
	void ApplySizeMultiplier(float InSizeMultiplier);

	/** 建立（或重建）发光用的动态材质实例，并解析脉冲参数是否存在。
	 *  只做一次，之后池复用不再重建——避免每次激活都 new 一个 MID。 */
	void SetupGlowMaterial();

	/** 驱动发光的脉冲参数。bHighLoad 时直接写固定值，跳过 sin。 */
	void UpdateGlowPulse(bool bHighLoad);

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USphereComponent> Collision;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Mesh;

	/** 发光用的动态材质实例（挂在 Mesh 的 Overlay Material 槽上） */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> GlowMaterialInstance;

	/** 材质是否包含配置的标量参数；缺失时跳过逐帧写入。 */
	bool bHasGlowPulseParameter = false;

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	FRogueOrbitingBladeVisualConfig VisualConfig;

	TMap<TObjectPtr<AActor>, float> HitCooldowns;

	TWeakObjectPtr<AActor> OwnerActor;
	int32 BladeIndex = 0;
	int32 BladeCount = 1;
	// 这些只是"还没被 ConfigureBlade 覆盖"时的占位值，运行时以武器推送的配置为准。
	// 只有 OrbitRadius 与表默认值相同；实际转速与伤害以 ConfigureBlade 输入为准。
	float OrbitRadius = 120.0f;
	float RotationSpeed = 120.0f;
	float Damage = 12.0f;
	float SharedOrbitAngle = 0.0f;
	// BeginPlay 缓存蓝图组件的实际基础尺寸；升级乘此基准，池复用恢复此基准。
	FVector BaseMeshScale = FVector(0.75f, 0.14f, 1.1f);
	float BaseCollisionRadius = 46.0f;
	float SizeMultiplier = 1.0f;
	bool bVisualsSimplified = false;
	bool bPoolAvailable = true;
};
