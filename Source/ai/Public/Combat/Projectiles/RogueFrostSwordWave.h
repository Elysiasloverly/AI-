#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Subsystems/WorldSubsystem.h"
#include "Combat/Visuals/RogueCombatAccentEffect.h"
#include "RogueFrostSwordWave.generated.h"
class ARogueCharacter;
class ARogueEnemy;
class UStaticMeshComponent;
class ARogueFrostSwordWave;

USTRUCT()
struct FRogueFrostWaveState
{
    GENERATED_BODY()
    UPROPERTY() bool bActive=false;
    UPROPERTY() FVector_NetQuantize Origin=FVector::ZeroVector;
    UPROPERTY() FVector_NetQuantizeNormal Direction=FVector::ForwardVector;
    UPROPERTY() float Born=0;
    UPROPERTY() float Speed=1200;
    UPROPERTY() float Scale=1;
    UPROPERTY() float Lifetime=30;
    UPROPERTY() float HalfWidth=240;
    UPROPERTY() float HalfThickness=55;
    UPROPERTY() float HalfHeight=30;
    UPROPERTY() uint32 Serial=0;
    UPROPERTY() bool bHitWorld=false;
    UPROPERTY() FVector_NetQuantize StopLocation=FVector::ZeroVector;
    UPROPERTY() float StopTime=0;
};

/** Shared reusable waves. Active waves are never removed by an idle-cache limit. */
UCLASS()
class AI_API URogueFrostWavePool:public UWorldSubsystem
{
    GENERATED_BODY()
public:
    ARogueFrostSwordWave* Acquire(TSubclassOf<ARogueFrostSwordWave> Class);
    void Release(ARogueFrostSwordWave* Wave);
private:
    UPROPERTY() TArray<TObjectPtr<ARogueFrostSwordWave>> Waves;
};

/** Replicates launch state once, simulates rendering locally; only authority sweeps/damages. */
UCLASS(Blueprintable)
class AI_API ARogueFrostSwordWave:public AActor
{
    GENERATED_BODY()
public:
    ARogueFrostSwordWave();
    virtual void PostInitializeComponents()override;
    virtual void BeginPlay()override;
    virtual void Tick(float Delta)override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out)const override;
    void Launch(ARogueCharacter* Player,const FVector& Start,const FVector& Direction,float Speed,float Damage,float Size,float Lifetime,float HalfWidth,float Thickness,float HalfHeight);
    void Deactivate();
    UFUNCTION(BlueprintPure,Category="霜月剑气") bool IsAvailableInPool()const{return !State.bActive;}
    UFUNCTION(BlueprintPure,Category="霜月剑气") float GetWaveAge()const;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="外观") TObjectPtr<UStaticMeshComponent> Mesh;
    UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="外观",meta=(DisplayName="碰墙冰屑特效")) FRogueCombatAccentSettings WallAccent;
private:
    UPROPERTY(ReplicatedUsing=OnRep_State) FRogueFrostWaveState State;
    UFUNCTION() void OnRep_State();
    TMap<TWeakObjectPtr<ARogueEnemy>,FGuid> HitCycles;
    FVector AuthoredScale=FVector::OneVector;
    float Damage=0;
    uint32 LastWallAccentSerial=0;
};
