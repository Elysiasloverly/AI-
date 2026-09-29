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

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	FVector MeshScale = FVector(0.75f, 0.14f, 1.1f);

	// ---- 发光（Overlay Material）----
	// 用 UE5 的 Mesh Overlay Material 做辉光：同一份网格再画一遍，叠一层材质。
	// 引擎对它的定义就是"Translucent material to blend on top of this mesh"，
	// 所以这个材质必须是半透明/叠加（Additive）的，否则会直接盖住本体。
	// 原来这里是"再建一个略大的静态网格套在外面"（GlowMesh），代价是多一个组件、
	// 多一个 draw call，而且外层一旦不透明就会把本体完全挡住。
	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	TObjectPtr<UMaterialInterface> GlowMaterial;

	/** 脉冲要驱动的标量参数名。必须和 GlowMaterial 里的参数名一字不差；
	 *  找不到这个参数时不会报错也不会刷日志，只是"不脉冲"（启动时查一次并提示）。 */
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
	void ConfigureBlade(AActor* InOwnerActor, int32 InBladeIndex, int32 InBladeCount, float InOrbitRadius, float InRotationSpeed, float InDamage, float InSharedOrbitAngle);

protected:
	virtual void BeginPlay() override;

private:
	/** 贴身持续伤害：遍历当前重叠的敌人，按 HitCooldown 节流结算。
	 *  取代原来的 OnComponentBeginOverlap 方案（那个只在"进入"那一帧触发一次）。 */
	void ApplyContactDamage();

	void UpdateCooldowns(float DeltaSeconds);
	void UpdateTransform();

	/** 建立（或重建）发光用的动态材质实例，并解析脉冲参数是否存在。
	 *  只做一次，之后池复用不再重建——避免每次激活都 new 一个 MID。 */
	void SetupGlowMaterial();

	/** 驱动发光的脉冲参数。bHighLoad 时直接写固定值，跳过 sin。 */
	void UpdateGlowPulse(bool bHighLoad);

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USphereComponent> Collision;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> Mesh;

	/** 发光用的动态材质实例（挂在 Mesh 的 Overlay Material 槽上） */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> GlowMaterialInstance;

	/** 解析结果：GlowMaterial 里到底有没有 GlowPulseParameterName 这个标量参数。
	 *  没有的话就别每帧去 SetScalarParameterValue —— 那会白白刷引擎警告。 */
	bool bHasGlowPulseParameter = false;

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	FRogueOrbitingBladeVisualConfig VisualConfig;

	TMap<TObjectPtr<AActor>, float> HitCooldowns;

	TWeakObjectPtr<AActor> OwnerActor;
	int32 BladeIndex = 0;
	int32 BladeCount = 1;
	// 这些只是"还没被 ConfigureBlade 覆盖"时的占位值，运行时以武器推送的配置为准。
	// 默认值与 FRogueWeaponTableRow 对齐，避免出问题时两处不一致。
	float OrbitRadius = 120.0f;
	float RotationSpeed = 120.0f;
	float Damage = 12.0f;
	float SharedOrbitAngle = 0.0f;
	bool bVisualsSimplified = false;
	bool bPoolAvailable = true;
};
