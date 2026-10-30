#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RogueEnemySupportComponent.generated.h"
class ARogueEnemy;
class ARogueEnemyEffectVisual;
USTRUCT(BlueprintType)
struct AI_API FRogueHealerMovementSettings
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere,BlueprintReadWrite,meta=(DisplayName="友军搜索半径",ClampMin="500",ClampMax="4000")) float SearchRadius=1800;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,meta=(DisplayName="与玩家期望距离",ClampMin="400",ClampMax="2000")) float PreferredPlayerDistance=900;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,meta=(DisplayName="玩家紧急退避距离",ClampMin="200",ClampMax="1500")) float RetreatDistance=500;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,meta=(DisplayName="友军身后站位（治疗半径比例）",ClampMin="0.35",ClampMax="0.85")) float HoldRangeRatio=.65f;
    /** 无伤员时跟随普通友军，保持在背离玩家的一侧；不改变实际治疗范围。 */
    UPROPERTY(EditAnywhere,BlueprintReadWrite,meta=(DisplayName="空闲跟随间距",ClampMin="100",ClampMax="800")) float FollowDistance=300;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,meta=(DisplayName="友军搜索间隔",ClampMin="0.1",ClampMax="2")) float SearchInterval=.4f;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,meta=(DisplayName="绕障探测距离",ClampMin="50",ClampMax="300")) float ProbeDistance=140;
    FRogueHealerMovementSettings Sanitized() const
    {
        auto S=*this;
        auto Safe=[](float V,float D,float A,float B){return FMath::Clamp(FMath::IsFinite(V)?V:D,A,B);};
        S.SearchRadius=Safe(SearchRadius,1800,500,4000);S.RetreatDistance=Safe(RetreatDistance,500,200,1500);
        S.PreferredPlayerDistance=FMath::Max(S.RetreatDistance+100,Safe(PreferredPlayerDistance,900,400,2000));
        S.HoldRangeRatio=Safe(HoldRangeRatio,.65f,.35f,.85f);S.SearchInterval=Safe(SearchInterval,.4f,.1f,2);
        S.FollowDistance=Safe(FollowDistance,300,100,800);
        S.ProbeDistance=Safe(ProbeDistance,140,50,300);return S;
    }
};
USTRUCT()
struct FRogueEnemyHealingLink
{
    GENERATED_BODY()
    UPROPERTY() TObjectPtr<ARogueEnemy> Target;
    UPROPERTY() uint32 SourceGeneration=0;
    UPROPERTY() uint32 TargetGeneration=0;
};
UCLASS(ClassGroup=(Enemy),meta=(BlueprintSpawnableComponent,DisplayName="敌人治疗"))
class AI_API URogueEnemySupportComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    URogueEnemySupportComponent();
    void InitializeRuntime();
    FVector CalculateSupportMovement(float DeltaSeconds);
    UFUNCTION(BlueprintPure,Category="Enemy|Healing") ARogueEnemy* GetMovementTarget() const { return MovementTarget.Get(); }
    UPROPERTY(EditDefaultsOnly,BlueprintReadWrite,Category="治疗移动",meta=(DisplayName="治疗者移动设置")) FRogueHealerMovementSettings Movement;
    void ResetForPool();
    virtual void TickComponent(float Delta,ELevelTick Type,FActorComponentTickFunction* F) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    UFUNCTION(BlueprintPure,Category="Enemy|Healing") ARogueEnemy* GetHealingTarget() const { return Link.Target; }
    UFUNCTION(BlueprintCallable,Category="Enemy|Healing") float HealTarget(ARogueEnemy* Target,float Amount);
    UPROPERTY(EditDefaultsOnly,BlueprintReadWrite,Category="治疗表现",meta=(DisplayName="绿色治疗范围蓝图"))
    TSubclassOf<ARogueEnemyEffectVisual> HealingCircleClass;
    UPROPERTY(EditDefaultsOnly,BlueprintReadWrite,Category="治疗表现",meta=(DisplayName="绿色治疗连线蓝图"))
    TSubclassOf<ARogueEnemyEffectVisual> HealingLinkClass;
private:
    UPROPERTY(Replicated) FRogueEnemyHealingLink Link;
    UPROPERTY(Transient) TObjectPtr<ARogueEnemyEffectVisual> Circle;
    UPROPERTY(Transient) TObjectPtr<ARogueEnemyEffectVisual> Beam;
    float Timer=0;
    FGuid TargetCycle;
    bool CanHeal(const ARogueEnemy* Target) const;
    bool IsSupportAlly(const ARogueEnemy* Target) const;
    bool IsWoundedAlly(const ARogueEnemy* Target) const;
    bool HasHealingSight(const ARogueEnemy* Target) const;
    TWeakObjectPtr<ARogueEnemy> MovementTarget;
    FGuid MovementCycle;
    float SearchRemaining=0,SteeringRemaining=0;
    FVector CachedMoveDirection=FVector::ZeroVector;
    bool bWasRetreating=false;
    void UpdateVisuals();
    void SetTarget(ARogueEnemy* Target);
    ARogueEnemyEffectVisual* EnsureVisual(TObjectPtr<ARogueEnemyEffectVisual>& V,TSubclassOf<ARogueEnemyEffectVisual> C);
};
