#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Enemies/Data/RogueEnemyArchetypes.h"
#include "RogueEnemySkyfallProjectile.generated.h"

class ARogueEnemy;
class ARogueEnemyProjectile;
class UDecalComponent;

/** 起点/落点/时间一次复制；触地状态连同实际撞击点一起复制，避免红圈先消失而弹体仍在空中。 */
USTRUCT()
struct FRogueEnemySkyfallState
{
    GENERATED_BODY()
    UPROPERTY() bool bActive=false;
    UPROPERTY() bool bImpacted=false;
    UPROPERTY() FVector_NetQuantize Launch=FVector::ZeroVector;
    UPROPERTY() FVector_NetQuantize Ground=FVector::ZeroVector;
    UPROPERTY() FVector_NetQuantize Impact=FVector::ZeroVector;
    UPROPERTY() float StartTime=0;
    UPROPERTY() float StartYaw=0;
    UPROPERTY() FRogueEnemySkyfallSettings Settings;
    UPROPERTY() TSubclassOf<ARogueEnemyProjectile> BurstClass;
};

/** 独立、短生命周期的天坠弹。服务端扫掠地面、结算落地伤害；客户端仅重建轨迹与弹幕表现。 */
UCLASS(Blueprintable)
class AI_API ARogueEnemySkyfallProjectile : public AActor
{
    GENERATED_BODY()
public:
    ARogueEnemySkyfallProjectile();
    virtual void Tick(float DeltaSeconds) override;
    virtual void SetActorHiddenInGame(bool bNewHidden) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    void InitializeAttack(ARogueEnemy* Source, const FVector& Launch, const FVector& Ground,
        const FRogueEnemySkyfallSettings& Settings, TSubclassOf<ARogueEnemyProjectile> BurstClass, float Damage);

    UFUNCTION(BlueprintPure, Category="天坠弹") bool HasImpacted() const { return State.bImpacted; }
    UFUNCTION(BlueprintPure, Category="天坠弹") bool IsWarningVisible() const;
    UFUNCTION(BlueprintPure, Category="天坠弹") FVector GetLockedGroundLocation() const { return State.Ground; }
    UFUNCTION(BlueprintPure, Category="天坠弹") float GetAttackAge() const;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    void RetireWarning();
    /** 蓝图可额外播放粒子或动画。伤害与子弹只由原生服务端路径生成。 */
    UFUNCTION(BlueprintImplementableEvent, Category="天坠弹") void OnSkyfallImpact(FVector Location);

private:
    UPROPERTY(ReplicatedUsing=OnRep_State) FRogueEnemySkyfallState State;
    UFUNCTION() void OnRep_State();
    void UpdateVisuals();
    void ImpactGround(const FVector& Ground, const FVector& Normal);
    void EmitBurst(bool bDamageEnabled);
    float ServerTime() const;
    bool bPlayedImpact=false;
    float AttackDamage=0;
    FGuid SourceCycle;
};
