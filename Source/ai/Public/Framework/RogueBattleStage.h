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
    UPROPERTY(BlueprintReadOnly) float EnemyAttackSpeedMultiplier = 1;
};

USTRUCT(BlueprintType)
struct AI_API FRogueBattleStageSettings
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(DisplayName="白天结束波次", ClampMin="1"))
    int32 DayWaveCount = 35;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(DisplayName="切换黑夜倒计时", ClampMin="1", Units="s"))
    float CountdownSeconds = 30;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(DisplayName="黑夜波次时长（秒）", ClampMin="1", Units="s", ToolTip="进入黑夜后从0单独计时，每经过此时长进入下一波，不等待上一波清场。未消耗的生成名额不结转，场上存活敌人保留，仍受数量上限限制。"))
    float NightWaveDuration = 28.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(DisplayName="黑夜敌人攻速倍率", ClampMin="1", ToolTip="普通敌人及 Boss 的近战、远程攻击间隔除以此倍率；不改变移动速度和弹速。"))
    float NightAttackSpeedMultiplier = 2;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(DisplayName="白天地图"))
    TSoftObjectPtr<UWorld> DayMap = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/Map/Battle_Day.Battle_Day")));
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(DisplayName="黑夜地图"))
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
