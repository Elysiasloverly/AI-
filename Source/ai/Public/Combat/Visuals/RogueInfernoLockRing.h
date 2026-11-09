#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RogueInfernoLockRing.generated.h"
class ARogueEnemy;
class UStaticMeshComponent;
class UMaterialInterface;

UCLASS(Blueprintable)
class AI_API ARogueInfernoLockRing : public AActor
{
	GENERATED_BODY()
  public:
	ARogueInfernoLockRing();
	UFUNCTION(BlueprintCallable, Category = "锁定圈") void ActivateRing(ARogueEnemy* Target, float LockedSeconds = 0);
	void UpdateRing(ARogueEnemy* Target, float LockedSeconds);
	UFUNCTION(BlueprintCallable, Category = "锁定圈") void DeactivateRing();
	bool IsAvailable() const
	{
		return bAvailable;
	}
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "锁定圈") TObjectPtr<UStaticMeshComponent> RingMesh;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "锁定圈", meta = (DisplayName = "锁定圈材质"))
	TObjectPtr<UMaterialInterface> RingMaterial;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "锁定圈",
			  meta = (DisplayName = "初始半径", ClampMin = "10", Units = "cm"))
	float OuterRadius = 90;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "锁定圈",
			  meta = (DisplayName = "收缩后半径", ClampMin = "5", Units = "cm"))
	float InnerRadius = 45;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "锁定圈",
			  meta = (DisplayName = "缩圈时长", ClampMin = ".1", Units = "s"))
	float ShrinkSeconds = 3;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "锁定圈",
			  meta = (DisplayName = "离脚底高度", Units = "cm"))
	float HeightOffset = 8;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "锁定圈", meta = (DisplayName = "随目标体型扩大"))
	bool bFitTargetSize = true;
    /** Keep the fully contracted ring outside the visible body instead of inside its feet. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="锁定圈", meta=(DisplayName="模型外沿留白", ClampMin="0", Units="cm"))
    float BodyClearance=16.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "锁定圈",
			  meta = (DisplayName = "透明度", ClampMin = "0", ClampMax = "1"))
	float Opacity = .8f;
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "锁定圈", meta = (DisplayName = "颜色"))
	FLinearColor Tint = FLinearColor(1, .77f, .36f);
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "锁定圈",
			  meta = (DisplayName = "未收到刷新时保留时间", ClampMin = ".1", Units = "s"))
	float RefreshGrace = .65f;

  private:
	float FittedEndRatio = .5f;
	float CachedBirth = -1.e20f;
    float CachedOpacity = -1.f;
	bool bAvailable = true;
};
