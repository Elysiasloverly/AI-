#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Combat/Visuals/RogueCombatAccentEffect.h"
#include "RoguePlayerMotionComponent.generated.h"
class UStaticMeshComponent;

/** Local visual poses only. Capsule, movement, authored scales and the floating/turning pivot stay independent. */
UCLASS(ClassGroup=(Custom),meta=(BlueprintSpawnableComponent,DisplayName="玩家施法与冲刺动作"))
class AI_API URoguePlayerMotionComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    URoguePlayerMotionComponent();
    void PlayDashPresentation(bool bStarting, const FVector& Position, const FVector& Direction);
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="表现|特效", meta=(DisplayName="冲刺起步霜雾")) FRogueCombatAccentSettings DashLaunch;
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="表现|特效", meta=(DisplayName="冲刺刹停冰屑")) FRogueCombatAccentSettings DashBrake;
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="表现|特效", meta=(DisplayName="落地冰屑")) FRogueCombatAccentSettings LandingFlakes;
    UFUNCTION(BlueprintCallable,Category="表现|动作") void PlayCastCue();
    UFUNCTION(BlueprintCallable,Category="表现|动作") void ResetPose();
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="表现|动作",meta=(DisplayName="启用外观动作")) bool bEnableMotion=true;
    /** 连续开火时合并身体动作；武器开火特效仍按每次实际攻击播放。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="表现|施法", meta=(DisplayName="启用施法动作"))
    bool bEnableCastMotion = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="表现|施法", meta=(DisplayName="施法动作幅度", ClampMin="0", ClampMax="2", EditCondition="bEnableCastMotion", ToolTip="1 为轻微施法前倾与抬身；0 关闭施法姿态，不影响移动、漂浮和冲刺。"))
    float CastMotionStrength = 1.f;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="表现|动作",meta=(DisplayName="移动前倾",ClampMin="0",ClampMax="12")) float MoveLean=4.f;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="表现|动作",meta=(DisplayName="冲刺前倾",ClampMin="0",ClampMax="20")) float DashLean=11.f;
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="表现|动作",meta=(DisplayName="配饰跟随幅度",ClampMin="0",ClampMax="10")) float AccessoryFollow=3.f;
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void TickComponent(float Delta,ELevelTick Type,FActorComponentTickFunction* Fn) override;
private:
    friend struct FRogueArtCompletionTestAccess;
    struct FPose { TWeakObjectPtr<UStaticMeshComponent> Mesh; FVector Location; FQuat Rotation; bool bSecondary=false; };
    TArray<FPose> Poses;
    FVector Velocity=FVector::ZeroVector;
    float LastHealth=-1,CastAge=10,HitAge=10,DashAmount=0,Phase=0;
    float DashReleaseAge = 1.f;
    float DownedAmount = 0.f;
    float VictoryAge = 0.f;
    bool bWasDashing = false;
    bool bRemoteDashing = false;
    float RemoteDashUntil = 0.f;
    bool bWasFalling = false;
    float LastFallVelocityZ = 0.f;
    bool bApplied=false;
};
