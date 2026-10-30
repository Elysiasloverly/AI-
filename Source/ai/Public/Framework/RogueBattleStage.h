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
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(DisplayName="第一次切换：白天结束波次", ClampMin="1", ClampMax="97", ToolTip="本波生成结束并清场后进入休整，不再生成下一波白天敌人。"))
    int32 DayWaveCount = 35;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(DisplayName="第二次切换：黑夜结束波次", ClampMin="2", ClampMax="98", ToolTip="默认清完第50波后休整，进入白天第51波。三次切换波数必须递增且小于100。"))
    int32 FirstNightEndWave = 50;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(DisplayName="第三次切换：白天结束波次", ClampMin="3", ClampMax="99", ToolTip="默认清完第85波后休整，进入黑夜第86波，抵达100波通关。"))
    int32 SecondDayEndWave = 85;

    void NormalizeTransitionWaves()
    {
        DayWaveCount=FMath::Clamp(DayWaveCount,1,97);
        FirstNightEndWave=FMath::Clamp(FirstNightEndWave,DayWaveCount+1,98);
        SecondDayEndWave=FMath::Clamp(SecondDayEndWave,FirstNightEndWave+1,99);
    }
    int32 GetTransitionEndWave(int32 StageIndex) const
    {
        return StageIndex==0?DayWaveCount:StageIndex==1?FirstNightEndWave:StageIndex==2?SecondDayEndWave:MAX_int32;
    }
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(DisplayName="昼夜切图倒计时", ClampMin="1", ToolTip="每次昼夜切图清场后的休整时长，倒计时结束由服务器统一切换黑夜地图。", Units="s"))
    float CountdownSeconds = 30;
    // 仅供旧资产反序列化，昼夜均按清场推进。
    UPROPERTY() float NightWaveDuration = 28.f;
    // 保留旧序列化字段；黑夜不再改变攻击速度。
    UPROPERTY() float NightAttackSpeedMultiplier = 1;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(DisplayName="黑夜普通怪数量倍率",ClampMin="1",ClampMax="10",ToolTip="每波普通怪名额和各类型数量限制乘以此倍率；Boss不额外复制。"))
    float NightEnemyCountMultiplier = 2;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(DisplayName="黑夜生成速度倍率",ClampMin="1",ClampMax="10",ToolTip="基础批次间隔（含最短间隔）除以此倍率。"))
    float NightSpawnRateMultiplier = 2;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(DisplayName="白天地图",ToolTip="新局开始使用的白天战斗地图。"))
    TSoftObjectPtr<UWorld> DayMap = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/Map/Battle_Day.Battle_Day")));
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(DisplayName="黑夜地图",ToolTip="休整倒计时结束后全员进入的战斗地图。"))
    TSoftObjectPtr<UWorld> NightMap = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/Map/Battle_Night.Battle_Night")));
};

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
