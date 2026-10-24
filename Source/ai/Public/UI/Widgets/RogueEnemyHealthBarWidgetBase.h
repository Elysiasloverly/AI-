#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/Widgets/RogueBarAnimation.h"
#include "UI/Widgets/RogueHUDViewData.h"
#include "RogueEnemyHealthBarWidgetBase.generated.h"

class ARogueHUD;
class UProgressBar;

/** 头顶血条仅做表现插值。池中 widget 转交对象、敌人重新出池时清理全部残影。 */
UCLASS(Abstract, Blueprintable)
class AI_API URogueEnemyHealthBarWidgetBase : public UUserWidget
{
	GENERATED_BODY()
public:
	/** 每帧接收真实比例和投影位置；过渡由 NativeTick 持续驱动。 */
	void UpdateBar(const FRogueEnemyHealthBarView& InView);
	void SetOwningRogueHUD(ARogueHUD* InOwningHUD);
	UFUNCTION(BlueprintPure, Category = "Rogue|EnemyBar")
	FVector2D GetBarSize() const { return BarSize; }
	const FRogueEnemyHealthBarView& GetCachedBar() const { return CachedView; }
protected:
	virtual void NativeTick(const FGeometry& Geometry, float DeltaSeconds) override;
	virtual void NativeDestruct() override;
	/** 数据事件仍提供真实比例；C++ 在事件后应用平滑进度。 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Rogue|EnemyBar")
	void OnBarUpdated(const FRogueEnemyHealthBarView& View);
	/** 设计器中的填充颜色作为正常颜色保留。 */
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UProgressBar> FillBar;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UProgressBar> DamageBar;
	UPROPERTY(EditDefaultsOnly, Category = "Rogue|EnemyBar", meta = (DisplayName = "血条屏幕尺寸"))
	FVector2D BarSize = FVector2D(96.0f, 10.0f);
	UPROPERTY(EditDefaultsOnly, Category = "Rogue|EnemyBar", meta = (DisplayName = "Boss 头顶血条宽度倍率", ClampMin = "1", ClampMax = "2"))
	float BossWidthMultiplier = 1.25f;
	UPROPERTY(EditDefaultsOnly, Category = "Rogue|EnemyBar|变化效果", meta = (DisplayName = "启用平滑变化"))
	bool bSmoothBars = true;
	UPROPERTY(EditDefaultsOnly, Category = "Rogue|EnemyBar|变化效果", meta = (DisplayName = "主血条变化速度", ClampMin = "0"))
	float InterpolationSpeed = 18.0f;
	UPROPERTY(EditDefaultsOnly, Category = "Rogue|EnemyBar|变化效果", meta = (DisplayName = "伤害残影停留秒数", ClampMin = "0", ClampMax = "1"))
	float DamageTrailDelay = 0.14f;
	UPROPERTY(EditDefaultsOnly, Category = "Rogue|EnemyBar|变化效果", meta = (DisplayName = "伤害残影追赶速度", ClampMin = "0"))
	float DamageTrailSpeed = 7.0f;
	UPROPERTY(EditDefaultsOnly, Category = "Rogue|EnemyBar|变化效果", meta = (DisplayName = "受击提亮时长", ClampMin = "0", ClampMax = "1"))
	float HitHighlightDuration = 0.14f;
	UPROPERTY(EditDefaultsOnly, Category = "Rogue|EnemyBar|变化效果", meta = (DisplayName = "受击提亮强度", ClampMin = "0", ClampMax = "1"))
	float HitHighlightStrength = 0.48f;
	UPROPERTY(EditDefaultsOnly, Category = "Rogue|EnemyBar|变化效果", meta = (DisplayName = "受击提亮颜色"))
	FLinearColor HitHighlightColor = FLinearColor(1.0f, 0.78f, 0.66f, 1.0f);
private:
	friend struct FRogueEnemyBarTestAccess;
	UPROPERTY(Transient) TWeakObjectPtr<ARogueHUD> OwningHUD;
	UPROPERTY(Transient) FRogueEnemyHealthBarView CachedView;
	FRogueBarAnimation Animation;
	FLinearColor DesignerFillColor = FLinearColor::White;
	float HitHighlightRemaining = 0.0f;
	bool bCapturedDesignerColor = false;
	void CaptureDesignerColor();
	void ResetPresentation();
	void ApplyBars();
};
