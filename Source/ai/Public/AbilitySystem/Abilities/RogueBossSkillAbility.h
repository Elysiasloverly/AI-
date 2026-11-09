#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "Enemies/Data/RogueBossSkillTypes.h"
#include "RogueBossSkillAbility.generated.h"

class URogueBossSkillComponent;
class ARogueEnemy;

/** 创建蓝图子类并实现 OnSkillPulse。原生类仅提供时间线，不包含具体攻击或默认伤害。 */
UCLASS(Blueprintable)
class AI_API URogueBossSkillAbility : public UGameplayAbility
{
    GENERATED_BODY()
public:
    URogueBossSkillAbility();
    virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle,const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayTagContainer* SourceTags=nullptr,const FGameplayTagContainer* TargetTags=nullptr,FGameplayTagContainer* RelevantTags=nullptr) const override;
    virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,const FGameplayEventData* EventData) override;
    virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,const FGameplayAbilityActorInfo* ActorInfo,
        const FGameplayAbilityActivationInfo ActivationInfo,bool bReplicateEndAbility,bool bWasCancelled) override;
    UFUNCTION(BlueprintPure,Category="Boss技能") ARogueEnemy* GetSkillEnemy() const;
    UFUNCTION(BlueprintPure,Category="Boss技能") FRogueBossSkillState GetExecutionState() const;
    /** 生命周期强制停止，供死亡/回池调用；不受普通的“不可打断”设置影响。 */
    void StopExecution();
    /** 仅登记自己创建的预警或持续区域；进入收招/取消时销毁，不登记应独立飞行的弹体。 */
    UFUNCTION(BlueprintCallable,Category="Boss技能") void TrackActorUntilSkillEnds(AActor* Actor);

protected:
    const FRogueBossSkillRow& GetCastSettings() const { return Settings; }
    float GetCastProgress() const;
    // 光束和金刚只创建一次实体，随后维持到同一个施法截止时间。
    virtual bool IsSustainedEffect() const { return false; }
    /** 服务端回调；持久预警的客户端表现绑定组件 OnPresentationChanged。 */
    UFUNCTION(BlueprintNativeEvent,Category="Boss技能") void OnChargeStarted(const FRogueBossSkillState& Context);
    virtual void OnChargeStarted_Implementation(const FRogueBossSkillState& Context);
    /** 服务端每次只调用一次。PulseIndex 从 1 开始；在此执行本次权威攻击。 */
    UFUNCTION(BlueprintNativeEvent,Category="Boss技能") void OnSkillPulse(const FRogueBossSkillState& Context);
    virtual void OnSkillPulse_Implementation(const FRogueBossSkillState& Context);
    UFUNCTION(BlueprintNativeEvent,Category="Boss技能") void OnRecoveryStarted(const FRogueBossSkillState& Context);
    virtual void OnRecoveryStarted_Implementation(const FRogueBossSkillState& Context);
    /** 完成与取消都会到此；回收自己创建的持续区域、预警和资源，不补发任何攻击。 */
    UFUNCTION(BlueprintNativeEvent,Category="Boss技能") void OnSkillStopped(bool bCancelled);
    virtual void OnSkillStopped_Implementation(bool bCancelled);

private:
    TWeakObjectPtr<URogueBossSkillComponent> Coordinator;
    FRogueBossSkillRow Settings;
    FGuid SpawnId;
    int32 CastId=0,Pulses=0;
    bool bEnding=false;
    double CastStartedAt=0, CastEndsAt=0;
    FTimerHandle SequenceTimer;
    TArray<TWeakObjectPtr<AActor>> SkillActors;
    bool CheckExecution();
    void FirePulse();
    void EnterRecovery();
    void FinishSequence();
    void ClearSkillActors();
};
