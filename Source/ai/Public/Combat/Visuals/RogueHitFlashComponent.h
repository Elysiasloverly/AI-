#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RogueHitFlashComponent.generated.h"

class UMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;

USTRUCT()
struct FRogueHitFlashMeshSnapshot
{
	GENERATED_BODY()
	UPROPERTY() TWeakObjectPtr<UMeshComponent> Mesh;
	UPROPERTY() TObjectPtr<UMaterialInterface> PreviousOverlay;
	float PreviousDrawDistance = 0.0f;
	bool bPreviousForceDisableNanite = false;
};

/** Local presentation only. Temporarily borrows the overlay slot without changing base materials. */
UCLASS(ClassGroup = (Rogue), meta = (BlueprintSpawnableComponent, DisplayName = "受击闪白"))
class AI_API URogueHitFlashComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	URogueHitFlashComponent();

	/** Consecutive hits inside MinFlashInterval are coalesced, allowing the model to return to its original look. */
	UFUNCTION(BlueprintCallable, Category = "受击闪白")
	bool PlayHitFlash();
	UFUNCTION(BlueprintCallable, Category = "受击闪白")
	void ClearHitFlash();
	UFUNCTION(BlueprintPure, Category = "受击闪白")
	bool IsFlashing() const { return !FlashedMeshes.IsEmpty(); }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "受击闪白", meta = (DisplayName = "启用闪白"))
	bool bEnableHitFlash = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "受击闪白", meta = (DisplayName = "叠加材质"))
	TObjectPtr<UMaterialInterface> FlashMaterial;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "受击闪白", meta = (DisplayName = "闪光颜色"))
	FLinearColor FlashColor = FLinearColor::White;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "受击闪白", meta = (DisplayName = "亮度", ClampMin = "0", ClampMax = "10"))
	float FlashIntensity = 1.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "受击闪白", meta = (DisplayName = "覆盖强度", ClampMin = "0", ClampMax = "1"))
	float FlashOpacity = 0.85f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "受击闪白", meta = (DisplayName = "保持时间", ClampMin = "0", ClampMax = "1"))
	float HoldSeconds = 0.02f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "受击闪白", meta = (DisplayName = "淡出时间", ClampMin = "0.001", ClampMax = "1"))
	float FadeOutSeconds = 0.06f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "受击闪白", meta = (DisplayName = "两次闪白最短间隔", ClampMin = "0", ClampMax = "2"))
	float MinFlashInterval = 0.12f;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	friend struct FRogueEnemyHitFlashTestAccess;
	void RestoreOverlays();
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> FlashMID;
	UPROPERTY(Transient) TArray<FRogueHitFlashMeshSnapshot> FlashedMeshes;
	FTimerHandle RestoreTimer;
	double NextAllowedFlashTime = -1.0;
};
