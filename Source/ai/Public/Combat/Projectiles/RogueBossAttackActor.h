#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/NetSerialization.h"
#include "Enemies/Data/RogueBossAttackSettings.h"
#include "RogueBossAttackActor.generated.h"
class ARogueEnemy;
class UStaticMeshComponent;
class ARogueCharacter;
class ARogueCombatAccentEffect;
class ARogueArenaBoundary;

UENUM(BlueprintType)
enum class ERogueBossAttackShape:uint8 { RollingBall, Beam, Meteor, FlameWave, IceSpike, HealAura, GoldenWard };

USTRUCT()
struct FRogueBossAttackPayload
{
    GENERATED_BODY()
    UPROPERTY() ERogueBossAttackShape Shape=ERogueBossAttackShape::Meteor;
    UPROPERTY() FRogueBossAttackSettings Settings;
    UPROPERTY() FVector Origin=FVector::ZeroVector;
    UPROPERTY() FVector Direction=FVector::ForwardVector;
    UPROPERTY() TObjectPtr<ARogueCharacter> AimTarget;
    UPROPERTY() float StartServerTime=0;
    UPROPERTY() float Damage=0;
    UPROPERTY() float ActiveSeconds=5;
    UPROPERTY() float WarmupSeconds=1;
    UPROPERTY() float ReleasedBeamLength=0;
    UPROPERTY() uint32 Generation=0;
    UPROPERTY() bool bReleased=false;
};

/** 少量大招实体：服务器扫掠/结算，客户端按同一服务器时间播放；普通弹幕继续使用原集中对象池。 */
UCLASS(Blueprintable)
class AI_API ARogueBossAttackActor:public AActor
{
    GENERATED_BODY()
public:
    ARogueBossAttackActor();
    void InitializeAttack(ARogueEnemy* Source,ERogueBossAttackShape Shape,const FRogueBossAttackSettings& Settings,
        const FVector& Origin,const FVector& Direction,float Damage,float ActiveSeconds,bool bReleased,ARogueCharacter* AimTarget=nullptr,float WarmupSeconds=1.f);
    void Release(const FVector& Direction);
    FVector GetAttackDirection() const { return Payload.Direction; }
    static bool FindGround(UWorld* World,const FVector& Near,AActor* Ignore,FVector& Ground);
    virtual void Tick(float Delta) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="组件") TObjectPtr<UStaticMeshComponent> Body;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="组件") TObjectPtr<UStaticMeshComponent> Glow;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="组件") TObjectPtr<UStaticMeshComponent> Warning;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="组件") TObjectPtr<UStaticMeshComponent> Trail;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="组件") TObjectPtr<UStaticMeshComponent> Accent;
    UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="表现") TSubclassOf<ARogueCombatAccentEffect> WallImpactClass;
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    UPROPERTY(ReplicatedUsing=OnRep_Payload) FRogueBossAttackPayload Payload;
    UFUNCTION() void OnRep_Payload();
    bool bInitialized=false,bImpacted=false;
    bool bPresentationReady = false;
    bool bVisualsRetired = false;
    void RetireVisuals();
    float VisualGroundZ = 0.f;
    TWeakObjectPtr<ARogueArenaBoundary> ArenaBoundary;
    bool bLookedUpBoundary = false;
    float ResolveBeamLength(const FVector& Direction);
    float GetFlameWaveScale(const FVector& Position) const;
    float GetMovingAttackHeight(const FVector& Position) const;
    void CacheVisualGround();
    UFUNCTION(NetMulticast, Reliable) void Multicast_WallImpact(FVector_NetQuantize Location, FVector_NetQuantizeNormal Direction);
    float NextDamageTime=0;
    FGuid SourceCycle;
    TMap<TWeakObjectPtr<ARogueCharacter>,float> LastHits;
    FVector BaseBodyScale=FVector::OneVector,BaseGlowScale=FVector::OneVector;
    float ServerTime() const;
    bool SourceStillValid() const;
    void UpdateVisuals(float Age);
    void ApplyCircleDamage(const FVector& Center,float Radius,float Damage,float Interval,bool bOnce=false);
    void ApplySweptDamage(const FVector& From,const FVector& To,float HalfWidth,float Damage,bool bOnce);
    void AdvanceMovingAttack(float Delta,float Age);
};
