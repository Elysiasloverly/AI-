#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RogueEnemySpawnRegion.generated.h"

class UBoxComponent;
class USplineComponent;

/** 关卡内的刷怪边界。框内还需有可站立地面和足够空间；框本身不参与碰撞。 */
UCLASS(Blueprintable, meta = (DisplayName = "怪物刷新区域"))
class AI_API ARogueEnemySpawnRegion : public AActor
{
    GENERATED_BODY()
public:
    ARogueEnemySpawnRegion();
    UFUNCTION(BlueprintPure, Category = "刷怪区域")
    UBoxComponent* GetSpawnBounds() const { return SpawnBounds; }
    /** 地面高度带只限制脚下表面；完整胶囊仍按外侧刷怪框验证。 */
    UFUNCTION(BlueprintCallable, Category = "刷怪区域|地面")
    void ConfigureGroundHeightRange(bool bEnabled, float MinLocalZ, float MaxLocalZ);
    UFUNCTION(BlueprintPure, Category = "刷怪区域|地面")
    bool IsGroundHeightAllowed(const FVector& GroundPoint) const;
    UFUNCTION(BlueprintPure, Category = "刷怪区域|轮廓")
    USplineComponent* GetSpawnOutline() const { return SpawnOutline; }
    UFUNCTION(BlueprintCallable, Category = "刷怪区域|轮廓")
    void SetSpawnOutlineEnabled(bool bEnabled) { bUseSpawnOutline = bEnabled; }
    /** 闭合直线多边形，可凹；按世界 XY 留出胶囊半径和边缘留白。 */
    UFUNCTION(BlueprintPure, Category = "刷怪区域|轮廓")
    bool IsInsideSpawnOutline(const FVector& Point, float CapsuleRadius = 0.0f) const;
    virtual void OnConstruction(const FTransform& Transform) override;
    bool ContainsCapsule(const FVector& Center, float Radius, float HalfHeight) const;
    bool ClipGroundTrace(const FVector& Candidate, float UpDistance, float DownDistance, FVector& OutStart, FVector& OutEnd) const;
    FVector GetRandomCandidate() const;
    void GetWorldCorners(TArray<FVector>& OutCorners) const;
private:
    bool CollectValidOutline(TArray<FVector2D>& OutVertices) const;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "刷怪区域|轮廓", meta = (AllowPrivateAccess = "true", DisplayName = "可编辑刷怪轮廓"))
    TObjectPtr<USplineComponent> SpawnOutline;
    UPROPERTY(EditAnywhere, Category = "刷怪区域|轮廓", meta = (DisplayName = "使用不规则刷怪轮廓", ToolTip = "启用后，XY 只在 SpawnOutline 的闭合直线多边形内生成。选择轮廓组件拖动顶点贴合空地；支持凹多边形，不允许边线自相交。外框及地面高度限制仍然生效。"))
    bool bUseSpawnOutline = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "刷怪区域", meta = (AllowPrivateAccess = "true", DisplayName = "刷怪范围框"))
    TObjectPtr<UBoxComponent> SpawnBounds;
    UPROPERTY(EditAnywhere, Category = "刷怪区域", meta = (DisplayName = "边缘留白", ClampMin = "0", Units = "cm", ToolTip = "敌人胶囊与框侧边至少保持此距离；普通怪和 Boss 共用。"))
    float EdgePadding = 100.0f;
    UPROPERTY(EditAnywhere, Category = "刷怪区域|地面", meta = (DisplayName = "限制地面高度", ToolTip = "启用后，仅在下限与上限之间向下寻找地面，避免把框内的树冠、屋顶当成出生地面。高度相对刷怪框的局部 Z；不限制怪物头顶高度。"))
    bool bRestrictGroundHeight = false;
    UPROPERTY(EditAnywhere, Category = "刷怪区域|地面", meta = (DisplayName = "地面高度下限", EditCondition = "bRestrictGroundHeight", Units = "cm"))
    float GroundMinLocalZ = -120.0f;
    UPROPERTY(EditAnywhere, Category = "刷怪区域|地面", meta = (DisplayName = "地面高度上限", EditCondition = "bRestrictGroundHeight", Units = "cm", ToolTip = "必须大于下限。只框选真实地面所在的高度，不要包含树冠。范围外的场景碰撞仍会用于检查出生空间。"))
    float GroundMaxLocalZ = 120.0f;
};
