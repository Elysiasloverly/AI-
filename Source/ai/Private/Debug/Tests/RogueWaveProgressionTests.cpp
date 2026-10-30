#if WITH_DEV_AUTOMATION_TESTS
#include "Framework/RogueGameMode.h"
#include "Framework/RogueGameState.h"
#include "Framework/RogueGameBalanceAsset.h"
#include "Framework/RogueGameModeRules.h"
#include "Enemies/Spawning/RogueSpawnSubsystem.h"
#include "Enemies/Data/RogueEnemyDataTable.h"
#include "Enemies/RogueEnemy.h"
#include "Combat/Targeting/RogueEnemyTrackerSubsystem.h"
#include "Player/RogueCharacter.h"
#include "Player/RoguePlayerController.h"
#include "Player/RoguePlayerState.h"
#include "AbilitySystemComponent.h"
#include "Components/BoxComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/WorldSettings.h"
#include "World/RogueEnemySpawnRegion.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include <limits>
#include "HAL/IConsoleManager.h"

struct FRogueWaveProgressionTestAccess
{
    static void Setup(ARogueGameMode* Mode, URogueGameBalanceAsset* Balance, ARogueEnemySpawnRegion* Region,
        const FRogueSpawnSettings& SpawnSettings, const FRogueBossSettings& BossSettings)
    {
        Mode->LoadedGameBalanceAsset = Balance;
        Mode->SpawnSettings = SpawnSettings; Mode->BossSettings = BossSettings;
        Mode->LevelSpawnRegion = Region; Mode->RunState = FRogueRunState();
        Mode->StageView = FRogueBattleStageView(); Mode->bDayStageMap = false;
    }
    static void Step(ARogueGameMode* Mode,float Delta) { Mode->TickRunSystems(Delta); Mode->PushRunStateToGameState(); }
    static FRogueRunState& Run(ARogueGameMode* Mode) { return Mode->RunState; }
    static void FinalDay(ARogueGameMode* Mode,int32 Wave)
    {
        Mode->bDayStageMap=true;Mode->StageSettings.DayWaveCount=Wave;Mode->StageSettings.CountdownSeconds=30;
    }
    static ERogueBattleStage Phase(ARogueGameMode* Mode) { return Mode->StageView.Phase; }
    static double Deadline(ARogueGameMode* Mode) { return Mode->StageView.CountdownDeadline; }
    static void Night(ARogueGameMode* Mode,URogueGameBalanceAsset* Balance,float Duration)
    {
        Balance->BattleStage.NightMap = TSoftObjectPtr<UWorld>(Mode->GetWorld());
        Balance->BattleStage.NightWaveDuration = Duration;
        Mode->InitializeBattleStage();
    }
    static void PrepareVictoryBoundary(ARogueGameMode* Mode,int32 Wave)
    {
        Mode->RunState.CurrentWave=Wave; Mode->RunState.BattleStageIndex=3; Mode->StageView.Phase=ERogueBattleStage::Night;
        Mode->bDayStageMap=false;
    }
    static void FirstNightBoundary(ARogueGameMode* Mode)
    { PrepareVictoryBoundary(Mode,50); Mode->RunState.BattleStageIndex=1; }

};

namespace RogueWaveProgressionTests
{
    constexpr EAutomationTestFlags Flags=EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter;
    struct FFixture
    {
        FTestConsoleVariable SelfTest{TEXT("Rogue.SelfTest.WeaponUpgradeChain")};
        FTestConsoleVariable Stress{TEXT("Rogue.SelfTest.EnemyStress")};
        FTestWorldWrapper TestWorld;
        UWorld* World=nullptr; ARogueGameMode* Mode=nullptr; ARogueCharacter* Player=nullptr;
        URogueGameBalanceAsset* Balance=nullptr; URogueSpawnSubsystem* Spawn=nullptr;
        URogueEnemyTrackerSubsystem* Tracker=nullptr; ARogueEnemySpawnRegion* Region=nullptr;
        UBoxComponent* Floor=nullptr;
        bool Initialize(int32 Total=3,int32 Alive=1,int32 BossInterval=0,UDataTable* Table=nullptr)
        {
            SelfTest.Set(TEXT("0"));Stress.Set(TEXT("0"));
            if(!TestWorld.CreateTestWorld(EWorldType::Game))return false;
            World=TestWorld.GetTestWorld();World->GetWorldSettings()->DefaultGameMode=ARogueGameMode::StaticClass();
            if(!TestWorld.BeginPlayInTestWorld())return false;
            Mode=World->GetAuthGameMode<ARogueGameMode>();Mode->SetActorTickEnabled(false);
            ARoguePlayerController* PC=World->SpawnActor<ARoguePlayerController>();
            PC->GetPlayerState<ARoguePlayerState>()->SetProgressionSnapshotForRun({});
            const FTransform Transform(FVector(0,0,300));
            Player=World->SpawnActorDeferred<ARogueCharacter>(ARogueCharacter::StaticClass(),Transform,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
            Player->AutoPossessPlayer=EAutoReceiveInput::Disabled;Player->FinishSpawning(Transform);PC->Possess(Player);
            Player->SetActorTickEnabled(false);Player->GetCharacterMovement()->SetComponentTickEnabled(false);
            Player->SetActorEnableCollision(false);Player->GetAbilitySystemComponent()->CancelAllAbilities();
            AActor* Ground=World->SpawnActor<AActor>();Floor=NewObject<UBoxComponent>(Ground);
            Ground->SetRootComponent(Floor);Floor->SetBoxExtent(FVector(20000,20000,20));
            Floor->SetCollisionEnabled(ECollisionEnabled::QueryOnly);Floor->SetCollisionObjectType(ECC_WorldStatic);
            Floor->SetCollisionResponseToAllChannels(ECR_Block);Floor->RegisterComponent();Ground->SetActorLocation(FVector(0,0,-20));
            Region=World->SpawnActor<ARogueEnemySpawnRegion>();Region->GetSpawnBounds()->SetBoxExtent(FVector(5000,5000,6000));
            Balance=NewObject<URogueGameBalanceAsset>(World);
            Balance->Spawn.BaseEnemiesPerWave=Total;Balance->Spawn.EnemiesPerWaveIncrement=1;
            Balance->Spawn.MaxAlivePerWave=0;Balance->Spawn.BossAliveReduction=0;Balance->Spawn.BossAliveMinimumLimit=1;
            Balance->Spawn.BatchRules.Reset();Balance->Spawn.MinimumInterval=.01f;
            Balance->Spawn.EarlyWaves.FirstWaveIntervalMultiplier=Balance->Spawn.EarlyWaves.LastWaveIntervalMultiplier=1;
            Balance->EnemyWaveEntries={{1,ERogueEnemyType::Rifleman,1}};
            FRogueSpawnSettings Settings;Settings.SpawnInterval=.01f;Settings.MaxAliveEnemies=Alive;
            Settings.SpawnRadius=1500;Settings.EnemySpawnMinimumDistance=300;
            FRogueBossSettings Boss;Boss.BossWaveInterval=BossInterval;Boss.BossSpawnDistance=1500;Boss.BossSpawnMinimumDistance=300;
            Spawn=World->GetSubsystem<URogueSpawnSubsystem>();Tracker=World->GetSubsystem<URogueEnemyTrackerSubsystem>();
            Spawn->Configure(ARogueEnemy::StaticClass(),{},Settings,Boss,Balance,Table);
            FRogueWaveProgressionTestAccess::Setup(Mode,Balance,Region,Settings,Boss);
            return Player->IsAlive();
        }
        void Step(float Delta=.1f) { FRogueWaveProgressionTestAccess::Step(Mode,Delta); }
        void FinishCurrentWave()
        {
            for(int I=0;I<10000&&!Spawn->HasFinishedWaveSpawning(FRogueWaveProgressionTestAccess::Run(Mode));++I)
                if(!Spawn->SpawnEnemy(FRogueWaveProgressionTestAccess::Run(Mode),Player,Region)) break;
            while(Find())Clear();while(Find(true))Clear(true);
            FRogueWaveProgressionTestAccess::Run(Mode).bBossAlive=false;
        }
        ARogueEnemy* Find(bool bBoss=false)
        {
            for(const TWeakObjectPtr<ARogueEnemy>& Weak:Tracker->GetActiveEnemies())
                if(Weak.IsValid()&&Weak->IsBoss()==bBoss)return Weak.Get();
            return nullptr;
        }
        void Clear(bool bBoss=false) { if(ARogueEnemy* Enemy=Find(bBoss))Enemy->DeactivateToPool(); }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueWaveBudgetRulesTest,"Rogue.Gameplay.WaveClear.BudgetConfigurationAndOverflow",RogueWaveProgressionTests::Flags)
bool FRogueWaveBudgetRulesTest::RunTest(const FString& Parameters)
{
    URogueGameBalanceAsset* Balance=NewObject<URogueGameBalanceAsset>();
    TestEqual(TEXT("First wave budget"),RogueGameModeRules::BuildWaveEnemyCount(1,Balance),24);
    TestEqual(TEXT("Later wave budget"),RogueGameModeRules::BuildWaveEnemyCount(10,Balance),42);
    Balance->Spawn.EnemiesPerWaveLimit=30;
    TestEqual(TEXT("Cumulative wave cap remains effective"),RogueGameModeRules::BuildWaveEnemyCount(10,Balance),30);
    Balance->Spawn.EnemiesPerWaveLimit=0;Balance->Spawn.BaseEnemiesPerWave=MAX_int32;Balance->Spawn.EnemiesPerWaveIncrement=MAX_int32;
    TestEqual(TEXT("Large values saturate instead of wrapping"),RogueGameModeRules::BuildWaveEnemyCount(MAX_int32,Balance),MAX_int32);
    Balance->Spawn.BaseEnemiesPerWave=-1;Balance->Spawn.EnemiesPerWaveIncrement=-2;
    TestEqual(TEXT("Invalid values still allow a finite playable wave"),RogueGameModeRules::BuildWaveEnemyCount(MIN_int32,Balance),1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueWaveClearProgressionTest,"Rogue.Gameplay.WaveClear.TimeIndependentBudgetAndLastEnemy",RogueWaveProgressionTests::Flags)
bool FRogueWaveClearProgressionTest::RunTest(const FString& Parameters)
{
    RogueWaveProgressionTests::FFixture F;if(!F.Initialize())return false;
    F.Step(.001f);TestEqual(TEXT("First slot"),F.Spawn->GetSpawnedCountThisWave(),1);
    F.Step(10000);TestEqual(TEXT("Time cannot advance uncleared wave"),F.Mode->GetCurrentWave(),1);
    TestEqual(TEXT("Catch-up fills only this wave budget"),F.Spawn->GetSpawnedCountThisWave(),3);
    F.Clear();F.Step();TestEqual(TEXT("Remaining survivors hold wave"),F.Mode->GetCurrentWave(),1);
    F.FinishCurrentWave();F.Step();TestEqual(TEXT("Clear advances exactly once"),F.Mode->GetCurrentWave(),2);
    TestEqual(TEXT("Next wave own budget"),F.Spawn->GetWaveEnemyBudget(),4);
    return true;

}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueNoGlobalAliveLimitTest,"Rogue.Gameplay.WaveClear.NoGlobalCapPreservesPacingAndBudget",RogueWaveProgressionTests::Flags)
bool FRogueNoGlobalAliveLimitTest::RunTest(const FString& Parameters)
{
    RogueWaveProgressionTests::FFixture F; if(!F.Initialize(30,1))return false;
    F.Balance->Spawn.BaseMaxAliveEnemies=1; F.Balance->Spawn.MaxAliveLimit=1;
    F.Balance->Spawn.BatchRules={{1,999999,4}};
    F.Step(.001f);
    TestEqual(TEXT("Large old global slack no longer blocks the wave's batch size"),F.Spawn->GetSpawnedCountThisWave(),4);
    F.Step(.001f);
    TestEqual(TEXT("Spawn interval still prevents per-frame generation"),F.Spawn->GetSpawnedCountThisWave(),4);
    for(int32 I=0;I<10;++I)F.Step(.1f);
    TestEqual(TEXT("More than the old global ceiling can remain alive"),F.Tracker->GetActiveEnemyCount(),30);
    TestEqual(TEXT("A final partial batch respects the cumulative wave budget"),F.Spawn->GetSpawnedCountThisWave(),30);
    TestFalse(TEXT("Direct spawning also respects cumulative budget"),F.Spawn->SpawnEnemy(FRogueWaveProgressionTestAccess::Run(F.Mode),F.Player,F.Region));
    TestEqual(TEXT("Living enemies still prevent a daytime wave advance"),F.Mode->GetCurrentWave(),1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueWaveFailureAndBossTest,"Rogue.Gameplay.WaveClear.SpawnFailureAndBossMustFinish",RogueWaveProgressionTests::Flags)
bool FRogueWaveFailureAndBossTest::RunTest(const FString& Parameters)
{
    RogueWaveProgressionTests::FFixture F;if(!F.Initialize(1,10,1))return false;
    F.Floor->SetCollisionEnabled(ECollisionEnabled::NoCollision);F.Step(1000);
    TestEqual(TEXT("Failed ground search consumes no enemies"),F.Spawn->GetSpawnedCountThisWave(),0);
    TestEqual(TEXT("No enemies created is not a completed wave"),F.Mode->GetCurrentWave(),1);
    TestFalse(TEXT("Boss failure is not a completed generation"),F.Spawn->HasFinishedWaveSpawning(FRogueWaveProgressionTestAccess::Run(F.Mode)));
    F.Floor->SetCollisionEnabled(ECollisionEnabled::QueryOnly);F.Step(1);
    if(!TestNotNull(TEXT("Boss retry succeeds"),F.Find(true))||!TestNotNull(TEXT("Ordinary budget succeeds"),F.Find()))return false;
    F.Clear();F.Step();TestEqual(TEXT("Living Boss holds the wave"),F.Mode->GetCurrentWave(),1);
    F.Clear(true);F.Step();TestEqual(TEXT("Boss flag also guards against a missed kill notification"),F.Mode->GetCurrentWave(),1);
    FRogueWaveProgressionTestAccess::Run(F.Mode).bBossAlive=false;F.Step();
    TestEqual(TEXT("Only after Boss defeat can the next wave begin"),F.Mode->GetCurrentWave(),2);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueWaveTypeQuotaTest,"Rogue.Gameplay.WaveClear.TypeQuotaExhaustionAndAliveSlots",RogueWaveProgressionTests::Flags)
bool FRogueWaveTypeQuotaTest::RunTest(const FString& Parameters)
{
    UDataTable* Table=NewObject<UDataTable>();Table->RowStruct=FRogueEnemyArchetypeRow::StaticStruct();
    FRogueEnemyArchetypeRow Row;Row.EnemyType=ERogueEnemyType::Rifleman;Row.MaxAlive=1;Row.MaxSpawnPerWave=2;
    Table->AddRow(TEXT("Rifleman"),Row);
    RogueWaveProgressionTests::FFixture F;if(!F.Initialize(10,10,0,Table))return false;
    F.Step();F.Step(1000);
    TestFalse(TEXT("Alive slots temporarily full does not exhaust total quota"),F.Spawn->HasFinishedWaveSpawning(FRogueWaveProgressionTestAccess::Run(F.Mode)));
    TestEqual(TEXT("Type alive cap honored"),F.Spawn->GetSpawnedCountThisWave(),1);
    F.Clear();F.Step();TestEqual(TEXT("A kill frees the type's alive slot"),F.Spawn->GetSpawnedCountThisWave(),2);
    TestTrue(TEXT("All types' per-wave quotas can finish before the global target"),F.Spawn->HasFinishedWaveSpawning(FRogueWaveProgressionTestAccess::Run(F.Mode)));
    F.Clear();F.Step();TestEqual(TEXT("Limited-only pools do not deadlock or inject fallback enemies"),F.Mode->GetCurrentWave(),2);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueFinalWaveBudgetTest,"Rogue.Gameplay.WaveClear.FinalDayWaitsForBudgetAndClear",RogueWaveProgressionTests::Flags)
bool FRogueFinalWaveBudgetTest::RunTest(const FString& Parameters)
{
    RogueWaveProgressionTests::FFixture F;if(!F.Initialize(2,1))return false;
    FRogueWaveProgressionTestAccess::FinalDay(F.Mode,1);
    F.Step(10000);TestEqual(TEXT("No time-based day transition"),FRogueWaveProgressionTestAccess::Phase(F.Mode),ERogueBattleStage::Day);
    F.Clear();F.Step();F.Step();
    TestEqual(TEXT("Final day stops only after all its ordinary enemies spawned"),FRogueWaveProgressionTestAccess::Phase(F.Mode),ERogueBattleStage::Clearing);
    TestTrue(TEXT("Intermission suspends all spawn entry points"),F.Spawn->IsSpawningSuspended());
    const float CombatTime=F.Mode->GetRunTimeSeconds();F.Step(2);
    TestTrue(TEXT("Clearing combat still counts as survival time"),F.Mode->GetRunTimeSeconds()>CombatTime);
    F.Clear();F.Step();TestEqual(TEXT("Countdown starts after the final enemy"),FRogueWaveProgressionTestAccess::Phase(F.Mode),ERogueBattleStage::Countdown);
    TestEqual(TEXT("Final day never starts next wave in the same map"),F.Mode->GetCurrentWave(),1);
    TestTrue(TEXT("Rest remains thirty server seconds"),FMath::IsNearlyEqual(FRogueWaveProgressionTestAccess::Deadline(F.Mode)-F.World->GetTimeSeconds(),30.,.001));
    const float RestTime=F.Mode->GetRunTimeSeconds();F.Step(1);
    TestEqual(TEXT("Rest does not inflate survival time"),F.Mode->GetRunTimeSeconds(),RestTime);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueNightTimedWaveTest,"Rogue.Gameplay.WaveClear.NightWaitsForBudgetSurvivorsAndBoss",RogueWaveProgressionTests::Flags)
bool FRogueNightTimedWaveTest::RunTest(const FString& Parameters)
{
    RogueWaveProgressionTests::FFixture F;if(!F.Initialize(3,1))return false;
    FRogueWaveProgressionTestAccess::Night(F.Mode,F.Balance,2.f);
    TestEqual(TEXT("Actual night recognized"),FRogueWaveProgressionTestAccess::Phase(F.Mode),ERogueBattleStage::Night);
    F.Step(.1f);F.Step(1000);TestEqual(TEXT("Long time cannot advance night"),F.Mode->GetCurrentWave(),36);
    F.Spawn->SpawnBoss(FRogueWaveProgressionTestAccess::Run(F.Mode),F.Player,F.Region);
    TestNotNull(TEXT("Boss created"),F.Find(true));
    F.Step(1000);TestEqual(TEXT("Night Boss and survivors hold wave"),F.Mode->GetCurrentWave(),36);
    F.FinishCurrentWave();F.Step();TestEqual(TEXT("Cleared night starts exactly next wave"),F.Mode->GetCurrentWave(),37);
    TestEqual(TEXT("Replicated snapshot follows clear"),F.World->GetGameState<ARogueGameState>()->GetCurrentWave(),37);
    return true;

}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueNightWaveClockTest,"Rogue.Gameplay.WaveClear.NightIgnoresLegacyDurationAndHitches",RogueWaveProgressionTests::Flags)
bool FRogueNightWaveClockTest::RunTest(const FString& Parameters)
{
    RogueWaveProgressionTests::FFixture F;if(!F.Initialize())return false;
    FRogueWaveProgressionTestAccess::Night(F.Mode,F.Balance,.01f);F.Step(999999);
    TestEqual(TEXT("Legacy duration cannot skip waves"),F.Mode->GetCurrentWave(),36);
    TestFalse(TEXT("Hitch cannot grant victory"),F.Mode->IsVictory());
    F.FinishCurrentWave();F.Step(.01f);TestEqual(TEXT("Clear still progresses normally"),F.Mode->GetCurrentWave(),37);
    return true;

}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueWaveVictoryBoundaryTest,"Rogue.Gameplay.Victory.NightBoundaryAndTerminalCleanup",RogueWaveProgressionTests::Flags)
bool FRogueWaveVictoryBoundaryTest::RunTest(const FString& Parameters)
{
    RogueWaveProgressionTests::FFixture F;if(!F.Initialize())return false;
    FRogueWaveProgressionTestAccess::PrepareVictoryBoundary(F.Mode,99);
    F.Step(.5f); if(!TestNotNull(TEXT("Combat exists at 99"),F.Find()))return false;
    auto& Run=FRogueWaveProgressionTestAccess::Run(F.Mode);Run.EnemiesDefeated=17;
    F.Step(999.f);TestEqual(TEXT("99 waits for actual clear"),F.Mode->GetCurrentWave(),99);
    F.FinishCurrentWave();F.Step();
    TestEqual(TEXT("Exactly 100"),F.Mode->GetCurrentWave(),100);
    TestTrue(TEXT("Victory ends the game"),F.Mode->IsVictory()&&F.Mode->IsGameOver());
    TestTrue(TEXT("Generation suspended"),F.Spawn->IsSpawningSuspended());
    TestEqual(TEXT("Enemies retired without kill rewards"),F.Tracker->GetActiveEnemyCount(),0);
    TestEqual(TEXT("Cleanup gives no fabricated kill credit"),Run.EnemiesDefeated,17);
    TestTrue(TEXT("Living player remains alive rather than eliminated"),F.Player->IsAlive()&&!F.Player->IsEliminated());
    const auto* State=F.World->GetGameState<ARogueGameState>();
    TestTrue(TEXT("Client-facing result selects victory"),State->IsVictory()&&State->IsGameOver());
    TestEqual(TEXT("Result contains final wave"),State->GetRunResult().Wave,100);
    TestEqual(TEXT("Result contains final score"),State->GetRunResult().EnemiesDefeated,17);
    const float EndTime=Run.RunTimeSeconds;F.Step(1000);F.Mode->EvaluateTeamWipe();
    TestEqual(TEXT("Timer cannot continue after settlement"),Run.RunTimeSeconds,EndTime);
    TestEqual(TEXT("No wave 101"),Run.CurrentWave,100);
    TestTrue(TEXT("Team wipe cannot replace victory"),F.Mode->IsVictory());
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueWaveVictoryHitchTest,"Rogue.Gameplay.Victory.HitchStopsAt100",RogueWaveProgressionTests::Flags)
bool FRogueWaveVictoryHitchTest::RunTest(const FString& Parameters)
{
    RogueWaveProgressionTests::FFixture F;if(!F.Initialize())return false;
    FRogueWaveProgressionTestAccess::PrepareVictoryBoundary(F.Mode,98);F.Step(999999.f);
    TestEqual(TEXT("Hitch does not skip 98"),F.Mode->GetCurrentWave(),98);
    TestFalse(TEXT("Cannot win before clearing"),F.Mode->IsVictory());
    F.FinishCurrentWave();F.Step();TestEqual(TEXT("First clear reaches 99"),F.Mode->GetCurrentWave(),99);
    F.FinishCurrentWave();F.Step();TestTrue(TEXT("Second clear reaches victory"),F.Mode->IsVictory());
    TestEqual(TEXT("Victory at 100"),F.Mode->GetCurrentWave(),100);return true;

}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueWaveVictoryInitialTest,"Rogue.Gameplay.Victory.EntryAlreadyAt100",RogueWaveProgressionTests::Flags)
bool FRogueWaveVictoryInitialTest::RunTest(const FString& Parameters)
{
    RogueWaveProgressionTests::FFixture F;if(!F.Initialize())return false;
    FRogueWaveProgressionTestAccess::Run(F.Mode).CurrentWave=100;
    FRogueWaveProgressionTestAccess::FinalDay(F.Mode,100);F.Step();
    TestTrue(TEXT("100 wins before day-stage clearing/travel"),F.Mode->IsVictory());
    TestEqual(TEXT("No 100th wave enemies"),F.Spawn->GetSpawnedCountThisWave(),0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueFirstNightClearTest,"Rogue.Gameplay.BattleStage.FirstNightWaitsForClear",RogueWaveProgressionTests::Flags)
bool FRogueFirstNightClearTest::RunTest(const FString& Parameters)
{
    RogueWaveProgressionTests::FFixture F;if(!F.Initialize())return false;
    FRogueWaveProgressionTestAccess::FirstNightBoundary(F.Mode);
    F.Step(1.5f);TestEqual(TEXT("Final night wave remains during spawning"),FRogueWaveProgressionTestAccess::Phase(F.Mode),ERogueBattleStage::Night);
    if(!TestNotNull(TEXT("Final night wave has surviving enemies"),F.Find()))return false;
    while(!F.Spawn->HasFinishedWaveSpawning(FRogueWaveProgressionTestAccess::Run(F.Mode)))
        if(!F.Spawn->SpawnEnemy(FRogueWaveProgressionTestAccess::Run(F.Mode),F.Player,F.Region))break;
    F.Step(.5f);
    TestEqual(TEXT("Stops at 50 before clearing"),F.Mode->GetCurrentWave(),50);
    TestEqual(TEXT("Waits for survivors rather than travelling"),FRogueWaveProgressionTestAccess::Phase(F.Mode),ERogueBattleStage::Clearing);
    TestTrue(TEXT("Every generation entry is suspended"),F.Spawn->IsSpawningSuspended());
    const auto* State=F.World->GetGameState<ARogueGameState>();
    TestFalse(TEXT("Destination is day"),State->GetBattleStage().bNextStageIsNight);
    F.Step(999.f);TestEqual(TEXT("No timed advance during clearing"),F.Mode->GetCurrentWave(),50);
    while(F.Find())F.Clear();F.Step();
    TestEqual(TEXT("Countdown only after clearing"),FRogueWaveProgressionTestAccess::Phase(F.Mode),ERogueBattleStage::Countdown);
    TestTrue(TEXT("Real thirty second server deadline"),FMath::IsNearlyEqual(FRogueWaveProgressionTestAccess::Deadline(F.Mode)-F.World->GetTimeSeconds(),30.,.001));
    return true;
}

// Explicit development-only visual/network verification; never invoked by normal gameplay.
static FAutoConsoleCommandWithWorld RogueVictoryPreview(TEXT("Rogue.Test.Victory"),
    TEXT("In a test run, advance wave 99 across the actual victory boundary."),
    FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
    {
        if(auto* Mode=World?World->GetAuthGameMode<ARogueGameMode>():nullptr)
        {
            if(Mode->IsGameOver())return;
            FRogueWaveProgressionTestAccess::PrepareVictoryBoundary(Mode,100);
            FRogueWaveProgressionTestAccess::Step(Mode,2.f);
        }
    }));
#endif
