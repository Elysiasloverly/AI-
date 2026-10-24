#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Framework/RogueBossHealthView.h"
#include "UI/Widgets/RogueBarAnimation.h"
#include "RogueBossHealthBarWidgetBase.generated.h"
class UProgressBar;
class UTextBlock;

UCLASS(Abstract, Blueprintable)
class AI_API URogueBossHealthBarWidgetBase : public UUserWidget
{
    GENERATED_BODY()
public:
    void UpdateBossHealth(const FRogueBossHealthView& View);
    UFUNCTION(BlueprintPure, Category="Rogue|Boss") const FRogueBossHealthView& GetBossView() const { return CachedView; }
protected:
    virtual void NativeTick(const FGeometry& Geometry, float DeltaSeconds) override;
    UFUNCTION(BlueprintImplementableEvent, Category="Rogue|Boss") void OnBossHealthUpdated(const FRogueBossHealthView& View);
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UProgressBar> FillBar;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UProgressBar> DamageBar;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> HealthText;
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Rogue|血条动画", meta=(ClampMin="0")) float InterpolationSpeed = 14;
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Rogue|血条动画", meta=(ClampMin="0")) float DamageTrailDelay = .25f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Rogue|血条动画", meta=(ClampMin="0")) float DamageTrailSpeed = 4;
private:
    friend struct FRogueUIPresentationTestAccess;
    FRogueBossHealthView CachedView;
    FRogueBarAnimation Animation;
    void ApplyBars();
};
