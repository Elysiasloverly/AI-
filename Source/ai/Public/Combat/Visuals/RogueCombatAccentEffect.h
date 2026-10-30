#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RogueCombatAccentEffect.generated.h"

class ARogueCombatAccentEffect;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UStaticMeshComponent;

/** 仅描述本地外观，不参与攻击、伤害、音效或存档。 */
USTRUCT(BlueprintType)
struct AI_API FRogueCombatAccentSettings
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="特效",meta=(DisplayName="启用特效")) bool bEnabled=true;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="特效",meta=(DisplayName="特效蓝图",EditCondition="bEnabled")) TSubclassOf<ARogueCombatAccentEffect> EffectClass;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="特效",meta=(DisplayName="颜色",EditCondition="bEnabled")) FLinearColor Tint=FLinearColor(.45f,.8f,1.f,1.f);
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="特效",meta=(DisplayName="大小倍率",ClampMin="0.1",ClampMax="4",EditCondition="bEnabled")) float Scale=1.f;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="特效",meta=(DisplayName="持续时间",ClampMin="0.05",ClampMax="0.6",Units="s",EditCondition="bEnabled")) float Duration=.2f;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="特效",meta=(DisplayName="最短触发间隔",ClampMin="0.02",Units="s",EditCondition="bEnabled",ToolTip="只限制特效频率，不改变开火或伤害频率。")) float MinimumInterval=.08f;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="特效",meta=(DisplayName="碎光数量",ClampMin="0",ClampMax="6",EditCondition="bEnabled")) int32 ShardCount=5;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="特效",meta=(DisplayName="显示扩散光环",EditCondition="bEnabled")) bool bShowRing=true;
};

/** 短命的局部施法闪光/命中碎光；由当前 World 的战斗对象池复用，不复制、不碰撞。 */
UCLASS(Blueprintable)
class AI_API ARogueCombatAccentEffect : public AActor
{
    GENERATED_BODY()
public:
    ARogueCombatAccentEffect();
    virtual void Tick(float DeltaSeconds) override;
    void Activate(const FRogueCombatAccentSettings& Settings,const FVector& Location,const FVector& Direction,bool bImpact);
    void Deactivate();
    bool IsAvailableInPool() const { return bAvailable; }
    UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="特效",meta=(DisplayName="闪光材质")) TObjectPtr<UMaterialInterface> AccentMaterial;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="特效") TObjectPtr<UStaticMeshComponent> Core;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="特效") TObjectPtr<UStaticMeshComponent> Ring;
private:
    void UpdateVisuals();
    void EnsureMaterials();
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Shards;
    UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> Materials;
    FRogueCombatAccentSettings Current;
    FVector FlightDirection=FVector::ForwardVector;
    FVector2D ShardDirections[6];
    float Age=0;
    bool bAvailable=true,bHit=false;
};
