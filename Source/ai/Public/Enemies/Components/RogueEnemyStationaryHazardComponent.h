#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RogueEnemyStationaryHazardComponent.generated.h"
class ARogueCharacter;
class UStaticMeshComponent;
UCLASS(ClassGroup=(Enemy),meta=(BlueprintSpawnableComponent,DisplayName="固定敌人法阵伤害"))
class AI_API URogueEnemyStationaryHazardComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    URogueEnemyStationaryHazardComponent();
    void InitializeRuntime();
    void ResetForPool();
    virtual void TickComponent(float Delta,ELevelTick Type,FActorComponentTickFunction* F) override;
    UFUNCTION(BlueprintPure,Category="Enemy|Hazard") bool IsPlayerInside(ARogueCharacter* Player) const;
    UFUNCTION(BlueprintPure,Category="Enemy|Hazard") UStaticMeshComponent* GetAreaMesh() const;
    UPROPERTY(EditDefaultsOnly,BlueprintReadWrite,Category="法阵伤害",meta=(DisplayName="启用固定敌人法阵伤害"))
    bool bEnabled=true;
    UPROPERTY(EditDefaultsOnly,BlueprintReadWrite,Category="法阵伤害",meta=(DisplayName="法阵网格组件标签"))
    FName AreaMeshTag=TEXT("Rogue.DamageArea");
    UPROPERTY(EditDefaultsOnly,BlueprintReadWrite,Category="法阵伤害",meta=(DisplayName="每跳伤害倍率",ClampMin="0"))
    float DamageMultiplier=.2f;
    UPROPERTY(EditDefaultsOnly,BlueprintReadWrite,Category="法阵伤害",meta=(DisplayName="伤害间隔（秒）",ClampMin="0.2"))
    float Interval=.9f;
    UPROPERTY(EditDefaultsOnly,BlueprintReadWrite,Category="法阵伤害",meta=(DisplayName="法阵上方有效高度",ClampMin="50"))
    float HeightTolerance=160;
private:
    float Timer=0;
};
