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
    StageSettings.NormalizeTransitionWaves();
    StageSettings.CountdownSeconds = FMath::IsFinite(StageSettings.CountdownSeconds)
        ? FMath::Max(1.f, StageSettings.CountdownSeconds) : 30.f;
    StageSettings.NightEnemyCountMultiplier=FMath::IsFinite(StageSettings.NightEnemyCountMultiplier)?FMath::Clamp(StageSettings.NightEnemyCountMultiplier,1.f,10.f):2.f;
    StageSettings.NightSpawnRateMultiplier=FMath::IsFinite(StageSettings.NightSpawnRateMultiplier)?FMath::Clamp(StageSettings.NightSpawnRateMultiplier,1.f,10.f):2.f;
    const FString Map = CurrentMapPackage(this);
    bDayStageMap = StageSettings.ContainsMap(Map,false);
    const bool bNight = StageSettings.ContainsMap(Map,true);
    StageView.Phase = bNight ? ERogueBattleStage::Night : ERogueBattleStage::Day;
    StageView.EnemyAttackSpeedMultiplier = 1.f;
    StageView.EnemyCountMultiplier = bNight ? StageSettings.NightEnemyCountMultiplier : 1.f;
    StageView.EnemySpawnRateMultiplier = bNight ? StageSettings.NightSpawnRateMultiplier : 1.f;
    if (URogueRunTravelSubsystem* Travel = GetGameInstance() ? GetGameInstance()->GetSubsystem<URogueRunTravelSubsystem>() : nullptr)
    {
        bContinuingStageRun = (bNight || bDayStageMap) && UGameplayStatics::HasOption(OptionsString, TEXT("RogueStageContinue"))
            && Travel->Consume(Map, RunState);
        if (!bContinuingStageRun) { Travel->Reset(); }
    }
    // 直接打开黑夜地图也从白天结束后的波次开始；正常入口始终进入白天。
    if (bNight && !bContinuingStageRun)
    {
        RunState.CurrentWave = StageSettings.GetTransitionEndWave(0) + 1;
        RunState.BattleStageIndex = 1;
    }
    PushRunStateToGameState();
    UE_LOG(LogTemp, Log, TEXT("[RogueStage] Map=%s Wave=%d Continue=%d Count=%.2f SpawnRate=%.2f AttackSpeed=%.2f"),
        *Map, RunState.CurrentWave, bContinuingStageRun, StageView.EnemyCountMultiplier, StageView.EnemySpawnRateMultiplier, StageView.EnemyAttackSpeedMultiplier);
}

bool ARogueGameMode::TickBattleStage(float DeltaSeconds)
{
    if (!bDayStageMap && StageView.Phase == ERogueBattleStage::Day) { return false; }
    URogueSpawnSubsystem* Spawn = GetWorld()->GetSubsystem<URogueSpawnSubsystem>();
    if ((StageView.Phase == ERogueBattleStage::Day || StageView.Phase == ERogueBattleStage::Night) && RunState.CurrentWave >= StageSettings.GetTransitionEndWave(RunState.BattleStageIndex)
        && Spawn && Spawn->HasFinishedWaveSpawning(RunState))
    {
        BeginBattleStageClearing();
    }
    if (StageView.Phase == ERogueBattleStage::Day || StageView.Phase == ERogueBattleStage::Night) { return false; }
    if (StageView.Phase == ERogueBattleStage::Clearing)
    {
        const URogueEnemyTrackerSubsystem* Tracker = GetWorld()->GetSubsystem<URogueEnemyTrackerSubsystem>();
        if (Tracker && Tracker->GetActiveEnemyCount() == 0 && !RunState.bBossAlive)
        {
            StageView.Phase = ERogueBattleStage::Countdown;
            StageView.CountdownDeadline = GetWorld()->GetTimeSeconds() + StageSettings.CountdownSeconds;
            QueueWaveShopDrop();
            UE_LOG(LogTemp, Log, TEXT("[RogueStage] Wave cleared; stage countdown %.2f seconds."), StageSettings.CountdownSeconds);
        }
    }
    else if (StageView.Phase == ERogueBattleStage::Countdown
        && GetWorld()->GetTimeSeconds() >= StageView.CountdownDeadline
        && GetWorld()->GetTimeSeconds() >= NextTravelAttemptTime)
    {
        TravelToNextBattleStage();
    }
    PushRunStateToGameState();
    return true;
}

void ARogueGameMode::BeginBattleStageClearing()
{
    StageView.bNextStageIsNight=(RunState.BattleStageIndex%2)==0;
    StageView.Phase=ERogueBattleStage::Clearing;
    if (auto* Spawn=GetWorld()->GetSubsystem<URogueSpawnSubsystem>()) Spawn->SetSpawningSuspended(true);
    UE_LOG(LogTemp,Log,TEXT("[RogueStage] Wave %d spawning stopped; clearing before %s."),RunState.CurrentWave,
        StageView.bNextStageIsNight?TEXT("night"):TEXT("day"));
}

void ARogueGameMode::TravelToNextBattleStage()
{
    if (!HasAuthority() || RunState.bGameOver || StageView.Phase != ERogueBattleStage::Countdown) { return; }
    NextTravelAttemptTime = GetWorld()->GetTimeSeconds() + 5.;
    const FString Destination = StageSettings.ChooseMap(StageView.bNextStageIsNight,CurrentMapPackage(this));
    if (Destination.IsEmpty() || Destination == CurrentMapPackage(this) || !FPackageName::DoesPackageExist(Destination))
    {
        UE_LOG(LogTemp, Error, TEXT("[RogueStage] Next battle map is invalid: %s. Correct DA_GameBalance to retry."), *Destination);
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
    Continuation.CurrentWave = RunState.CurrentWave + 1;
    Continuation.BattleStageIndex = RunState.BattleStageIndex + 1;
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
