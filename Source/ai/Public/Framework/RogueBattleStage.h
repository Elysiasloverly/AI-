#pragma once

#include "CoreMinimal.h"
#include "Engine/World.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Framework/RogueGameModeConfig.h"
#include "RogueBattleStage.generated.h"

UENUM(BlueprintType)
enum class ERogueBattleStage : uint8
{
    Day,
    Clearing,
    Countdown,
    Travelling,
    Night
};

/** 同一份复制状态携带阶段与服务端截止时间；客户端按同步后的服务器时间计算倒计时。 */
USTRUCT(BlueprintType)
struct AI_API FRogueBattleStageView
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) ERogueBattleStage Phase = ERogueBattleStage::Day;
    UPROPERTY(BlueprintReadOnly) double CountdownDeadline = 0;
    UPROPERTY(BlueprintReadOnly) bool bNextStageIsNight = true;
    UPROPERTY(BlueprintReadOnly) float EnemyAttackSpeedMultiplier = 1;
    UPROPERTY(BlueprintReadOnly) float EnemyCountMultiplier = 1;
    UPROPERTY(BlueprintReadOnly) float EnemySpawnRateMultiplier = 1;
};

USTRUCT(BlueprintType)
struct AI_API FRogueBattleStageSettings
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(DisplayName="昼夜切换间隔（波）", ClampMin="1", ClampMax="99", ToolTip="每完成这些波次，停刷并清场，休整倒计时后切换昼夜。默认20：第20、40、60、80波后切图，第100波优先通关。"))
    int32 WavesPerStage = 20;


    void NormalizeTransitionWaves()
    {
        WavesPerStage = FMath::Clamp(WavesPerStage, 1, 99);
    }
    int32 GetTransitionEndWave(int32 StageIndex) const
    {
        if (StageIndex < 0) { return MAX_int32; }
        const int64 EndWave = (int64(StageIndex) + 1) * FMath::Clamp(WavesPerStage, 1, 99);
        // 100 波是胜利入口，不再安排一轮切图倒计时。
        return EndWave < 100 ? int32(EndWave) : MAX_int32;
    }
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(DisplayName="昼夜切图倒计时", ClampMin="1", ToolTip="每次昼夜切图清场后的休整时长，倒计时结束由服务器统一切换到下一阶段的昼夜地图。", Units="s"))
    float CountdownSeconds = 30;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(DisplayName="黑夜普通怪数量倍率",ClampMin="1",ClampMax="10",ToolTip="每波普通怪名额和各类型数量限制乘以此倍率；Boss不额外复制。"))
    float NightEnemyCountMultiplier = 2;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(DisplayName="黑夜生成速度倍率",ClampMin="1",ClampMax="10",ToolTip="基础批次间隔（含最短间隔）除以此倍率。"))
    float NightSpawnRateMultiplier = 2;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(DisplayName="黑夜近战兵占比（%）",ClampMin="0",ClampMax="100",UIMin="0",UIMax="100",ToolTip="每波普通怪按此百分比划分近战名额，剩余名额按原权重生成其他类型；Boss另计。近战按敌人表“普通怪仅接触攻击”分类，不含治疗和飞行怪。修改后下一波生效；某组未开放或每波名额耗尽时由另一组补足，仍遵守各类限制。"))
    float NightMeleePercent = 70.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(DisplayName="黑夜近战兵移速倍率",ClampMin="0.1",ClampMax="10",ToolTip="只乘普通近战兵本波成长后的移速；其他类型和Boss不受影响。下一波生效，不会因对象池复用累乘。"))
    float NightMeleeMoveSpeedMultiplier = 2.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(DisplayName="黑夜近战兵伤害倍率",ClampMin="0.1",ClampMax="10",ToolTip="只乘普通近战兵本波成长后的接触伤害；其他类型和Boss不受影响。下一波生效。"))
    float NightMeleeDamageMultiplier = 2.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(DisplayName="白天备用地图",ToolTip="白天随机池为空或全部无效时使用。"))
    TSoftObjectPtr<UWorld> DayMap = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/Map/Day/Battle_Forest_Day.Battle_Forest_Day")));
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(DisplayName="黑夜备用地图",ToolTip="黑夜随机池为空或全部无效时使用。"))
    TSoftObjectPtr<UWorld> NightMap = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/Map/Night/Battle_Forest_Night.Battle_Forest_Night")));
    UPROPERTY(EditAnywhere,BlueprintReadOnly,meta=(DisplayName="白天随机地图池",ToolTip="新局及每个白天由服务器从有效地图中等概率选择。空池回退到白天地图。"))
    TArray<TSoftObjectPtr<UWorld>> DayMaps;
    UPROPERTY(EditAnywhere,BlueprintReadOnly,meta=(DisplayName="黑夜随机地图池",ToolTip="每个黑夜由服务器从有效地图中等概率选择，所有玩家进入同一张。空池回退到黑夜地图。"))
    TArray<TSoftObjectPtr<UWorld>> NightMaps;
    bool ContainsMap(const FString& Package,bool bNight) const;
    FString ChooseMap(bool bNight,const FString& Exclude=FString()) const;
};

namespace RogueBattleMaps
{
    /** Shared single-player / lobby entry. Reads the same balance asset as battle travel. */
    AI_API FString ChooseInitialDay(const FString& Fallback);
}

/** 只承接同一局的日夜切图；不写本地存档，也不承接主菜单开局或死亡重开。 */
UCLASS()
class AI_API URogueRunTravelSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    void Prepare(const FString& Destination, const FRogueRunState& State);
    bool Consume(const FString& Destination, FRogueRunState& OutState);
    void Reset();
private:
    FString PendingDestination;
    UPROPERTY() FRogueRunState PendingState;
};
