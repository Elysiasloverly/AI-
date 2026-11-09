#pragma once
#include "CoreMinimal.h"
#include "Engine/NetSerialization.h"
#include "GameFramework/Actor.h"
#include "Combat/Weapons/RogueWeaponCombatSettings.h"
#include "RogueIceFlameField.generated.h"

class ARogueEnemy;
class UStaticMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;

/** 只复制地面区域表现；伤害与减速始终由服务器处理。序号保证池复用时客户端重新入场。 */
USTRUCT()
struct FRogueIceFlameView
{
    GENERATED_BODY()
    UPROPERTY() FVector_NetQuantize Location=FVector::ZeroVector;
    UPROPERTY() FVector_NetQuantizeNormal Normal=FVector::UpVector;
    UPROPERTY() float Radius=0;
    UPROPERTY() float StartTime=0;
    UPROPERTY() float EndTime=0;
    UPROPERTY() uint32 Sequence=0;
    UPROPERTY() bool bActive=false;
};

/** 池化的冰焰残留。无碰撞，使用追踪器的空间查询；外观参数在子蓝图，战斗参数来自武器 DT。 */
UCLASS(Blueprintable)
class AI_API ARogueIceFlameField : public AActor
{
    GENERATED_BODY()
public:
    ARogueIceFlameField();
    virtual void Tick(float DeltaSeconds) override;
    virtual void SetActorHiddenInGame(bool bNewHidden) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
    void Activate(AActor* Source,const FVector& Location,const FVector& Normal,float Radius,float DamagePerSecond,const FRogueIceFlameCombatSettings& Settings);
    void Deactivate();
    bool IsAvailableInPool() const { return !View.bActive; }
    float GetRadius() const { return View.Radius; }
    UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="外观") TObjectPtr<UMaterialInterface> GroundMaterial;
    UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="外观") TObjectPtr<UMaterialInterface> FlameMaterial;
    UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="外观",meta=(DisplayName="蓝焰高度",ClampMin="1",Units="cm")) float FlameHeight=90;
    UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="外观",meta=(DisplayName="蓝焰簇数",ClampMin="1",ClampMax="8")) int32 FlameCount=7;
private:
    UFUNCTION() void OnRep_View();
    float Clock() const;
    void UpdateVisuals();
    void ReleaseSlows();
    UPROPERTY(ReplicatedUsing=OnRep_View) FRogueIceFlameView View;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Ground;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Flames;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> GroundMID;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> FlameMID;
    FRogueIceFlameCombatSettings Combat;
    float DPS=0,LastDamageTime=0;
    TMap<TWeakObjectPtr<ARogueEnemy>,uint32> SlowedEnemies;
};
