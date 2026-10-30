#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RogueReturningTalisman.generated.h"

class ARogueCharacter;
class ARogueEnemy;
class USceneComponent;
class UStaticMeshComponent;
class USplineMeshComponent;
class UMaterialInterface;

UENUM(BlueprintType)
enum class ERogueTalismanPhase : uint8
{
    Orbiting UMETA(DisplayName="环绕待命"),
    Striking UMETA(DisplayName="飞出穿刺"),
    Returning UMETA(DisplayName="返回归位")
};

/** 一张长期跟随玩家的灵符。只有服务器选择目标、扫掠并结算伤害；各客户端显示复制状态。 */
UCLASS(Blueprintable)
class AI_API ARogueReturningTalisman : public AActor
{
    GENERATED_BODY()
public:
    ARogueReturningTalisman();
    virtual void Tick(float DeltaSeconds) override;
    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    void Configure(ARogueCharacter* Player, int32 Index, int32 Count, float Damage, float Range,
        float Cooldown, int32 Targets, int32 HitsPerTarget);
    bool TryLaunch();

    UFUNCTION(BlueprintPure, Category="灵符|状态") ERogueTalismanPhase GetFlightPhase() const { return Phase; }
    UFUNCTION(BlueprintPure, Category="灵符|状态") int32 GetSortieHitCount() const;
    UFUNCTION(BlueprintPure, Category="灵符|状态") int32 GetSortieTargetCount() const { return Victims.Num(); }

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="灵符|模型") TObjectPtr<USceneComponent> VisualRoot;
    /** 飞行时始终使用本组件在蓝图中保存的缩放、旋转和材质，不按伤害/速度重写。 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="灵符|模型") TObjectPtr<UStaticMeshComponent> Mesh;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="灵符|环绕", meta=(DisplayName="环绕半径", ClampMin="0", Units="cm")) float OrbitRadius=180;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="灵符|环绕", meta=(DisplayName="环绕高度偏移", Units="cm")) float OrbitHeight=50;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="灵符|环绕", meta=(DisplayName="环绕角速度", Units="deg/s")) float OrbitSpeed=75;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="灵符|环绕", meta=(DisplayName="待命尺寸倍率", ClampMin="0.05", ToolTip="只改变环绕时的大小；飞出去后恢复 Mesh 组件本身的缩放。")) float OrbitSizeMultiplier=1;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="灵符|飞行", meta=(DisplayName="飞行朝向修正", ToolTip="相对实际飞行方向的模型角度补偿，只影响外观。替换模型后可调俯仰、偏航、翻滚；保留 Mesh 组件的旋转和缩放。")) FRotator FlightRotationOffset=FRotator::ZeroRotator;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="灵符|飞行", meta=(DisplayName="出击速度", ClampMin="100", Units="cm/s")) float FlightSpeed=1500;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="灵符|飞行", meta=(DisplayName="返回速度", ClampMin="100", Units="cm/s")) float ReturnSpeed=1900;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="灵符|飞行", meta=(DisplayName="穿刺后的越过距离", ClampMin="40", Units="cm")) float OvershootDistance=150;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="灵符|飞行", meta=(DisplayName="伤害扫掠半径", ClampMin="1", Units="cm")) float HitRadius=28;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="灵符|飞行", meta=(DisplayName="单次出击超时返回", ClampMin="1", Units="s")) float MaxSortieSeconds=30;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="灵符|飞行", meta=(DisplayName="返回超时归位", ClampMin="0.25", Units="s", ToolTip="超过此时长仍未回到轨道则自动归位，避免极低速度或异常位置导致永久停火。")) float MaxReturnSeconds=8;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="灵符|拖尾", meta=(DisplayName="启用飞行拖尾")) bool bEnableTrail=true;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="灵符|拖尾", meta=(DisplayName="拖尾材质")) TObjectPtr<UMaterialInterface> TrailMaterial;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="灵符|拖尾", meta=(DisplayName="拖尾宽度", ClampMin="0.2", Units="cm")) float TrailWidth=9;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="灵符|拖尾", meta=(DisplayName="拖尾保留时间", ClampMin="0.03", ClampMax="1", Units="s")) float TrailLifetime=.45f;

private:
    struct FVictim
    {
        TWeakObjectPtr<ARogueEnemy> Enemy;
        FGuid Cycle;
        int32 Hits=0;
    };
    struct FTrailPoint { FVector Location; float Age=0; };
    UPROPERTY(Replicated) TObjectPtr<ARogueCharacter> Player;
    UPROPERTY(Replicated) ERogueTalismanPhase Phase=ERogueTalismanPhase::Orbiting;
    UPROPERTY(Replicated) int32 SlotIndex=0;
    UPROPERTY(Replicated) int32 SlotCount=1;
    UPROPERTY() TArray<TObjectPtr<USplineMeshComponent>> TrailSegments;
    TArray<FVictim> Victims;
    TSet<TWeakObjectPtr<ARogueEnemy>> HitThisPass;
    TWeakObjectPtr<ARogueEnemy> Target;
    FGuid TargetCycle;
    FVector LegEnd=FVector::ZeroVector;
    FVector PassDirection=FVector::ForwardVector;
    FVector SmoothedLocation=FVector::ZeroVector;
    FVector PreviousReturnHome=FVector::ZeroVector;
    float ReturnAge=0;
    TArray<FTrailPoint> TrailPoints;
    ERogueTalismanPhase LastVisualPhase=ERogueTalismanPhase::Orbiting;
    float Damage=0, SeekRange=1800, Cooldown=1.3f, NextLaunchTime=0, SortieAge=0;
    int32 TargetBudget=1, HitBudget=1, SortieTargetBudget=1, SortieHitBudget=1;
    bool bVisualInitialized=false;

    FVector OrbitLocation() const;
    bool IsUsable(const ARogueEnemy* Enemy, const FGuid& Cycle) const;
    ARogueEnemy* FindNextTarget() const;
    void StartPass(ARogueEnemy* Enemy);
    void SweepDamage(const FVector& Start, const FVector& End);
    void BeginReturn();
    void UpdatePresentation(float DeltaSeconds);
};
