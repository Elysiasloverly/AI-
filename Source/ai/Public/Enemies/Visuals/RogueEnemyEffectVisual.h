#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RogueEnemyEffectVisual.generated.h"
class UStaticMeshComponent;

/** 无碰撞、无伤害的本地表现。模型和材质均由子蓝图提供。 */
UCLASS(Blueprintable)
class AI_API ARogueEnemyEffectVisual : public AActor
{
    GENERATED_BODY()
public:
    ARogueEnemyEffectVisual();
    UFUNCTION(BlueprintCallable,Category="Enemy|Visual")
    void ConfigureCircle(FVector Center,float Radius);
    UFUNCTION(BlueprintCallable,Category="Enemy|Visual")
    void ConfigureBeam(FVector Start,FVector End);
    void HideVisual();
    UFUNCTION(BlueprintCallable,Category="Enemy|Visual")
    void ConfigureMarker(FVector GroundCenter,float Radius,float MarkerTop);
    UPROPERTY(EditDefaultsOnly,BlueprintReadWrite,Category="Enemy|Visual",meta=(DisplayName="主体组件标签"))
    FName MainMeshTag=TEXT("Rogue.EffectMain");
    UPROPERTY(EditDefaultsOnly,BlueprintReadWrite,Category="Enemy|Visual",meta=(DisplayName="头顶标记组件标签"))
    FName MarkerMeshTag=TEXT("Rogue.EffectMarker");
    UPROPERTY(EditDefaultsOnly,BlueprintReadWrite,Category="Enemy|Visual",meta=(DisplayName="头顶符印面向镜头"))
    bool bFaceMarkerToCamera=true;
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    UStaticMeshComponent* GetMain();
    UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> Main;
};
