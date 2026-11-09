#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RogueArenaBoundary.generated.h"
class UBoxComponent;
USTRUCT(BlueprintType)
struct AI_API FRogueBoundaryLoop
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere,BlueprintReadWrite) TArray<FVector> Points;
    /** Open runs can follow actual railings while leaving their gates usable. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(DisplayName="闭合边界")) bool bClosed = true;
};
/** Editable closed outlines for outer shores and interior water holes. Blocks Pawn only. */
UCLASS(Blueprintable,meta=(DisplayName="战斗区域空气墙"))
class AI_API ARogueArenaBoundary : public AActor
{
    GENERATED_BODY()
public:
    ARogueArenaBoundary();
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="边界") TArray<FRogueBoundaryLoop> Loops;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="边界",meta=(ClampMin="20")) float Thickness=60;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="边界",meta=(ClampMin="500")) float Height=3500;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="边界", meta=(DisplayName="向地下延伸", ClampMin="0", Units="cm"))
    float BelowGroundDepth = 200.f;
    static constexpr ECollisionChannel CollisionChannel = ECC_GameTraceChannel2;
    UFUNCTION(BlueprintCallable,Category="边界") void RebuildWalls();
    /** XY 射线到最大闭合外轮廓的距离。水面内孔和内部围栏不参与。 */
    bool TryGetOuterRayDistance(const FVector& Origin, const FVector& Direction, double& OutDistance) const;
    virtual void OnConstruction(const FTransform& Transform)override;
    virtual void PostInitializeComponents()override;
private:
    UPROPERTY(Transient) TArray<TObjectPtr<UBoxComponent>> Walls;
};
