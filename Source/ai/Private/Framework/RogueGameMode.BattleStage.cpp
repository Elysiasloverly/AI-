#include "Framework/RogueGameMode.h"
#include "Framework/RogueGameBalanceAsset.h"
#include "Framework/RogueGameState.h"
#include "Enemies/Spawning/RogueSpawnSubsystem.h"
#include "Combat/Targeting/RogueEnemyTrackerSubsystem.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/PlayerController.h"
#include "Player/RoguePlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/PackageName.h"

namespace
{
    FString CurrentMapPackage(const UObject* Context)
    {
        const FString Folder = FPackageName::GetLongPackagePath(Context->GetWorld()->GetOutermost()->GetName());
        return Folder / UGameplayStatics::GetCurrentLevelName(Context, true);
    }
}

void ARogueGameMode::InitializeBattleStage()
{
    StageSettings = LoadedGameBalanceAsset ? LoadedGameBalanceAsset->BattleStage : FRogueBattleStageSettings();
    StageSettings.DayWaveCount = FMath::Max(1, StageSettings.DayWaveCount);
    StageSettings.CountdownSeconds = FMath::IsFinite(StageSettings.CountdownSeconds)
        ? FMath::Max(1.f, StageSettings.CountdownSeconds) : 30.f;
    StageSettings.NightWaveDuration = FMath::IsFinite(StageSettings.NightWaveDuration)
        ? FMath::Max(1.f, StageSettings.NightWaveDuration) : 28.f;
    NightWaveElapsedSeconds = 0;
    StageSettings.NightAttackSpeedMultiplier = FMath::IsFinite(StageSettings.NightAttackSpeedMultiplier)
        ? FMath::Max(1.f, StageSettings.NightAttackSpeedMultiplier) : 2.f;
    const FString Map = CurrentMapPackage(this);
    bDayStageMap = Map == StageSettings.DayMap.ToSoftObjectPath().GetLongPackageName();
    const bool bNight = Map == StageSettings.NightMap.ToSoftObjectPath().GetLongPackageName();
    StageView.Phase = bNight ? ERogueBattleStage::Night : ERogueBattleStage::Day;
    StageView.EnemyAttackSpeedMultiplier = bNight ? StageSettings.NightAttackSpeedMultiplier : 1.f;
    if (URogueRunTravelSubsystem* Travel = GetGameInstance() ? GetGameInstance()->GetSubsystem<URogueRunTravelSubsystem>() : nullptr)
    {
        bContinuingStageRun = bNight && UGameplayStatics::HasOption(OptionsString, TEXT("RogueStageContinue"))
            && Travel->Consume(Map, RunState);
        if (!bContinuingStageRun) { Travel->Reset(); }
    }
    // 直接打开黑夜地图也从白天结束后的波次开始；正常入口始终进入白天。
    if (bNight && !bContinuingStageRun)
    {
        RunState.CurrentWave = StageSettings.DayWaveCount + 1;
    }
    PushRunStateToGameState();
    UE_LOG(LogTemp, Log, TEXT("[RogueStage] Map=%s Wave=%d Continue=%d AttackSpeed=%.2f"),
        *Map, RunState.CurrentWave, bContinuingStageRun, StageView.EnemyAttackSpeedMultiplier);
}

bool ARogueGameMode::TickBattleStage(float DeltaSeconds)
{
    if (!bDayStageMap) { return false; }
    URogueSpawnSubsystem* Spawn = GetWorld()->GetSubsystem<URogueSpawnSubsystem>();
    if (StageView.Phase == ERogueBattleStage::Day && RunState.CurrentWave >= StageSettings.DayWaveCount
        && Spawn && Spawn->HasFinishedWaveSpawning(RunState))
    {
        // 最后一波名额用完才停刷；清完本波后休整，不能在白天开始第 36 波。
        StageView.Phase = ERogueBattleStage::Clearing;
        Spawn->SetSpawningSuspended(true);
        UE_LOG(LogTemp, Log, TEXT("[RogueStage] Day spawning stopped; clearing wave %d."), RunState.CurrentWave);
    }
    if (StageView.Phase == ERogueBattleStage::Day) { return false; }
    if (StageView.Phase == ERogueBattleStage::Clearing)
    {
        const URogueEnemyTrackerSubsystem* Tracker = GetWorld()->GetSubsystem<URogueEnemyTrackerSubsystem>();
        if (Tracker && Tracker->GetActiveEnemyCount() == 0 && !RunState.bBossAlive)
        {
            StageView.Phase = ERogueBattleStage::Countdown;
            StageView.CountdownDeadline = GetWorld()->GetTimeSeconds() + StageSettings.CountdownSeconds;
            UE_LOG(LogTemp, Log, TEXT("[RogueStage] Wave cleared; night countdown %.2f seconds."), StageSettings.CountdownSeconds);
        }
    }
    else if (StageView.Phase == ERogueBattleStage::Countdown
        && GetWorld()->GetTimeSeconds() >= StageView.CountdownDeadline
        && GetWorld()->GetTimeSeconds() >= NextTravelAttemptTime)
    {
        TravelToNight();
    }
    PushRunStateToGameState();
    return true;
}

void ARogueGameMode::TravelToNight()
{
    if (!HasAuthority() || RunState.bGameOver || StageView.Phase != ERogueBattleStage::Countdown) { return; }
    NextTravelAttemptTime = GetWorld()->GetTimeSeconds() + 5.;
    const FString Destination = StageSettings.NightMap.ToSoftObjectPath().GetLongPackageName();
    if (Destination.IsEmpty() || Destination == CurrentMapPackage(this) || !FPackageName::DoesPackageExist(Destination))
    {
        UE_LOG(LogTemp, Error, TEXT("[RogueStage] Night map is invalid: %s. Correct DA_GameBalance to retry."), *Destination);
        return;
    }
    URogueRunTravelSubsystem* Travel = GetGameInstance() ? GetGameInstance()->GetSubsystem<URogueRunTravelSubsystem>() : nullptr;
    if (!Travel) { return; }
    TArray<ARoguePlayerState*> Players;
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        ARoguePlayerState* State = It->Get() ? It->Get()->GetPlayerState<ARoguePlayerState>() : nullptr;
        if (State && !State->IsOnlyASpectator())
        {
            if (!State->IsBaseAttributesInitialized()) { return; }
            Players.Add(State);
        }
    }
    for (ARoguePlayerState* State : Players) { State->PrepareStageTravel(); }
    FRogueRunState Continuation = RunState;
    Continuation.CurrentWave = StageSettings.DayWaveCount + 1;
    Continuation.SpawnTimer = 0;
    Travel->Prepare(Destination, Continuation);
    StageView.Phase = ERogueBattleStage::Travelling;
    PushRunStateToGameState();
    FURL URL(nullptr, *Destination, TRAVEL_Absolute);
    URL.AddOption(TEXT("RogueStageContinue=1"));
    if (GetNetMode() == NM_ListenServer) { URL.AddOption(TEXT("listen")); }
    bUseSeamlessTravel = true;
    // 单机也走 seamless ServerTravel：PlayerState 的显式成长快照使用同一条恢复路径。
    if (!GetWorld()->ServerTravel(URL.ToString(), true))
    {
        Travel->Reset();
        for (ARoguePlayerState* State : Players) { State->CancelStageTravel(); }
        StageView.Phase = ERogueBattleStage::Countdown;
        StageView.CountdownDeadline = GetWorld()->GetTimeSeconds() + 5.;
        UE_LOG(LogTemp, Error, TEXT("[RogueStage] ServerTravel rejected; retry in 5 seconds: %s"), *Destination);
    }
}
