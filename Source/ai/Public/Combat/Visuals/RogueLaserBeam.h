#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RogueLaserBeam.generated.h"

class USceneComponent;
class UStaticMeshComponent;
class UMaterialInterface;

USTRUCT(BlueprintType)
struct AI_API FRogueLaserBeamVisualStyle
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, Category = "Visual", meta = (DisplayName = "主光束粗细"))
	float BeamThickness = 0.10f;

	UPROPERTY(EditDefaultsOnly, Category = "Visual", meta = (DisplayName = "光晕粗细"))
	float GlowThickness = 0.18f;

	UPROPERTY(EditDefaultsOnly, Category = "Visual", meta = (DisplayName = "光晕长度倍率"))
	float GlowLengthScaleMultiplier = 1.02f;

	UPROPERTY(EditDefaultsOnly, Category = "Visual", meta = (DisplayName = "起点圆环大小"))
	float StartRingSize = 0.22f;

	UPROPERTY(EditDefaultsOnly, Category = "Visual", meta = (DisplayName = "起点圆环厚度"))
	float StartRingDepth = 0.10f;

	UPROPERTY(EditDefaultsOnly, Category = "Visual", meta = (DisplayName = "终点圆环大小"))
	float EndRingSize = 0.30f;

	UPROPERTY(EditDefaultsOnly, Category = "Visual", meta = (DisplayName = "终点圆环厚度"))
	float EndRingDepth = 0.12f;

	UPROPERTY(EditDefaultsOnly, Category = "Visual", meta = (DisplayName = "命中特效缩放"))
	FVector ImpactEffectScale = FVector(0.22f, 0.22f, 0.22f);

	UPROPERTY(EditDefaultsOnly, Category = "Visual", meta = (DisplayName = "命中特效持续时间"))
	float ImpactEffectLifetime = 0.12f;

	UPROPERTY(EditDefaultsOnly, Category = "Visual", meta = (DisplayName = "显示起点与终点圆环"))
	bool bShowStartAndEndRings = true;
};

UCLASS()
class AI_API ARogueLaserBeam : public AActor
{
	GENERATED_BODY()

public:
	ARogueLaserBeam();

	void InitializeBeam(const FVector& StartLocation, const FVector& EndLocation, bool bUseInfernoStyle = false, bool bSpawnImpactEffect = true, float BeamLifetime = 0.10f);
	void ActivatePooledBeam(AActor* InOwner, const FVector& StartLocation, const FVector& EndLocation, bool bUseInfernoStyle = false, bool bSpawnImpactEffect = true, float BeamLifetime = 0.10f);
	void DeactivateToPool();
	bool IsAvailableInPool() const { return bPoolAvailable; }

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> BeamMesh;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> GlowMesh;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> StartRingMesh;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UStaticMeshComponent> EndRingMesh;

	UPROPERTY(EditDefaultsOnly, Category = "外观|材质", meta = (DisplayName = "使用组件材质", ToolTip = "启用时保留 BeamMesh、GlowMesh 和两端圆环组件的材质槽；关闭时使用下方激光/地狱塔材质配置覆盖槽位。"))
	bool bUseBlueprintComponentMaterials = false;

	UPROPERTY(EditDefaultsOnly, Category = "外观|激光材质", meta = (DisplayName = "主光束材质", ToolTip = "在类默认值设置此材质；留空使用网格自身默认材质。", EditCondition = "!bUseBlueprintComponentMaterials", EditConditionHides))
	TObjectPtr<UMaterialInterface> PlayerBeamMaterial;

	UPROPERTY(EditDefaultsOnly, Category = "外观|激光材质", meta = (DisplayName = "光晕材质", ToolTip = "在类默认值设置此材质；留空使用网格自身默认材质。", EditCondition = "!bUseBlueprintComponentMaterials", EditConditionHides))
	TObjectPtr<UMaterialInterface> PlayerGlowMaterial;

	UPROPERTY(EditDefaultsOnly, Category = "外观|激光材质", meta = (DisplayName = "端点圆环材质", ToolTip = "在类默认值设置此材质；留空使用网格自身默认材质。", EditCondition = "!bUseBlueprintComponentMaterials", EditConditionHides))
	TObjectPtr<UMaterialInterface> PlayerRingMaterial;

	UPROPERTY(EditDefaultsOnly, Category = "外观|地狱塔材质", meta = (DisplayName = "主光束材质", ToolTip = "在类默认值设置此材质；留空使用网格自身默认材质。", EditCondition = "!bUseBlueprintComponentMaterials", EditConditionHides))
	TObjectPtr<UMaterialInterface> InfernoBeamMaterial;

	UPROPERTY(EditDefaultsOnly, Category = "外观|地狱塔材质", meta = (DisplayName = "光晕材质", ToolTip = "在类默认值设置此材质；留空使用网格自身默认材质。", EditCondition = "!bUseBlueprintComponentMaterials", EditConditionHides))
	TObjectPtr<UMaterialInterface> InfernoGlowMaterial;

	UPROPERTY(EditDefaultsOnly, Category = "外观|地狱塔材质", meta = (DisplayName = "端点圆环材质", ToolTip = "在类默认值设置此材质；留空使用网格自身默认材质。", EditCondition = "!bUseBlueprintComponentMaterials", EditConditionHides))
	TObjectPtr<UMaterialInterface> InfernoRingMaterial;

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	float MinimumBeamLengthScale = 0.2f;

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	float BeamLengthDivisor = 100.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Visual")
	float MinimumBeamLifetime = 0.03f;

	UPROPERTY(EditDefaultsOnly, Category = "外观|激光样式", meta = (DisplayName = "激光样式"))
	FRogueLaserBeamVisualStyle PlayerVisualStyle;

	UPROPERTY(EditDefaultsOnly, Category = "外观|地狱塔样式", meta = (DisplayName = "地狱塔样式"))
	FRogueLaserBeamVisualStyle InfernoVisualStyle;

	FTimerHandle ReturnToPoolTimerHandle;
	bool bPoolAvailable = true;
};
