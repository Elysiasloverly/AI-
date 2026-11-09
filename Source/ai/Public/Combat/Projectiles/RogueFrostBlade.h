#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Combat/Weapons/RogueWeaponCombatSettings.h"
#include "Combat/Visuals/RogueCombatAccentEffect.h"
#include "RogueFrostBlade.generated.h"
class ARogueCharacter;
class ARogueEnemy;
class ARogueFrostSwordWave;
class ARogueWeaponBase;
class UStaticMeshComponent;
class USceneComponent;

UENUM(BlueprintType)
enum class ERogueFrostBladePhase:uint8{Hover UMETA(DisplayName="垂直待命"),Approach UMETA(DisplayName="贴近目标"),Slash UMETA(DisplayName="水平横斩"),Return UMETA(DisplayName="收刀归位")};
USTRUCT()
struct FRogueFrostBladeState
{
    GENERATED_BODY()
    UPROPERTY() ERogueFrostBladePhase Phase=ERogueFrostBladePhase::Hover;
    UPROPERTY() FVector_NetQuantize Start=FVector::ZeroVector;
    UPROPERTY() FVector_NetQuantize Anchor=FVector::ZeroVector;
    UPROPERTY() FRotator Rotation=FRotator::ZeroRotator;
    UPROPERTY() float AimYaw=0;
    UPROPERTY() float Born=0;
    UPROPERTY() uint32 Serial=0;
};
/** One replicated blade per owner. Server drives phases/hits, clients reconstruct the same motion. */
UCLASS(Blueprintable)
class AI_API ARogueFrostBlade:public AActor
{
    GENERATED_BODY()
public:
    ARogueFrostBlade();
    virtual void Tick(float Delta)override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out)const override;
    void Configure(ARogueCharacter* Owner,const FRogueFrostBladeCombatSettings& Settings,float Size);
    bool StartAttack(ARogueEnemy* Enemy,ARogueWeaponBase* Weapon,float InDamage,int32 Waves,float WaveDamage,float WaveSize,float WaveSpeed,TSubclassOf<ARogueFrostSwordWave> Class);
    UFUNCTION(BlueprintPure,Category="冰刀") ERogueFrostBladePhase GetPhase()const{return State.Phase;}
    UFUNCTION(BlueprintPure,Category="冰刀") float GetBladeScale()const{return BladeScale;}
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="外观") TObjectPtr<USceneComponent> VisualRoot;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="外观") TObjectPtr<UStaticMeshComponent> Mesh;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="外观") TObjectPtr<UStaticMeshComponent> SlashTrail;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="外观") TObjectPtr<UStaticMeshComponent> Afterimage;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="外观") TObjectPtr<UStaticMeshComponent> IceMotes;
    UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="外观|待命",meta=(DisplayName="待命追随速度",ClampMin="0")) float HoverFollowSpeed=12;
    UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="外观|待命",meta=(DisplayName="待命轻摇角度",ClampMin="0",ClampMax="5")) float HoverSwayDegrees=1.5f;
    UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="外观|待命",meta=(DisplayName="巨化后刀尖离脚底高度",ClampMin="0",Units="cm")) float TipClearance=22;
    UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="外观|刀光",meta=(DisplayName="拖尾扇面角度",ClampMin="10",ClampMax="140")) float TrailAngleDegrees=84;
    UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="外观|刀光",meta=(DisplayName="收刀余光时长",ClampMin="0.01",ClampMax="0.4",Units="s")) float AfterimageDuration=.16f;
    UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="外观|待命",meta=(DisplayName="身后悬浮偏移")) FVector HoverOffset=FVector(-105,30,135);
    UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="外观|待命",meta=(DisplayName="轻微漂浮幅度",Units="cm")) float HoverAmplitude=7;
    UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="外观|待命",meta=(DisplayName="漂浮频率",Units="Hz")) float HoverFrequency=1.2f;
    UPROPERTY(EditDefaultsOnly,BlueprintReadOnly,Category="外观|开火",meta=(DisplayName="出刀特效")) FRogueCombatAccentSettings FireAccent;
private:
    UPROPERTY(Replicated) FRogueFrostBladeCombatSettings Combat;
    UPROPERTY(Replicated) float BladeScale=1;
    UPROPERTY(ReplicatedUsing=OnRep_State) FRogueFrostBladeState State;
    UFUNCTION() void OnRep_State();
    TWeakObjectPtr<ARogueEnemy> Target;
    FGuid TargetCycle;
    TWeakObjectPtr<ARogueWeaponBase> SourceWeapon;
    TMap<TWeakObjectPtr<ARogueEnemy>,FGuid> HitCycles;
    TSubclassOf<ARogueFrostSwordWave> WaveClass;
    float Damage=0,WaveDamage=0,WaveScale=1,WaveSpeed=1200,LastSweep=0;
    int32 WaveCount=0;
    double Now()const;
    FVector Home()const;
    FVector ClampToOwner(const FVector& Location)const;
    void SetPhase(ERogueFrostBladePhase Phase);
    void DamageSweep(float Previous,float Current);
    void EmitWaves();
    void UpdateArt(float Delta,float SwingProgress);
    float AfterimageAge=1;
    bool bPoseInitialized=false;
    ERogueFrostBladePhase LastVisualPhase=ERogueFrostBladePhase::Hover;
};
