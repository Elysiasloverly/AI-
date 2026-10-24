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
    static float NightDuration(ARogueGameMode* Mode) { return Mode->StageSettings.NightWaveDuration; }
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
    TestEqual(TEXT("Cumulative cap independent of alive cap"),RogueGameModeRules::BuildWaveEnemyCount(10,Balance),30);
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
    F.Step();TestEqual(TEXT("First enemy consumes one slot"),F.Spawn->GetSpawnedCountThisWave(),1);
    F.Step(10000);TestEqual(TEXT("Elapsed time cannot advance an uncleared wave"),F.Mode->GetCurrentWave(),1);
    TestEqual(TEXT("Alive cap blocks new spawns without consuming budget"),F.Spawn->GetSpawnedCountThisWave(),1);
    F.Clear();F.Step();TestEqual(TEXT("Early empty field continues the same wave"),F.Mode->GetCurrentWave(),1);
    TestEqual(TEXT("Second enemy consumes next slot"),F.Spawn->GetSpawnedCountThisWave(),2);
    F.Clear();F.Step();TestEqual(TEXT("Third enemy finishes this wave's generation"),F.Spawn->GetSpawnedCountThisWave(),3);
    F.Step(10000);TestEqual(TEXT("Last living enemy still holds wave"),F.Mode->GetCurrentWave(),1);
    TestFalse(TEXT("Direct spawn cannot bypass the cumulative budget"),F.Spawn->SpawnEnemy(FRogueWaveProgressionTestAccess::Run(F.Mode),F.Player,F.Region));
    F.Clear();F.Step();TestEqual(TEXT("Clearing the last enemy starts exactly next wave"),F.Mode->GetCurrentWave(),2);
    TestEqual(TEXT("New wave resets cumulative count"),F.Spawn->GetSpawnedCountThisWave(),1);
    TestEqual(TEXT("New wave grows its own budget"),F.Spawn->GetWaveEnemyBudget(),4);
    TestEqual(TEXT("Shared state exposes the authoritative wave"),F.World->GetGameState<ARogueGameState>()->GetCurrentWave(),2);
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
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueNightTimedWaveTest,"Rogue.Gameplay.WaveClear.NightTimerIgnoresRemainingEnemiesAndBoss",RogueWaveProgressionTests::Flags)
bool FRogueNightTimedWaveTest::RunTest(const FString& Parameters)
{
    RogueWaveProgressionTests::FFixture F;if(!F.Initialize(100,1))return false;
    FRogueWaveProgressionTestAccess::Night(F.Mode,F.Balance,2.f);
    if(!TestEqual(TEXT("Actual stage initialization recognizes the night world"),FRogueWaveProgressionTestAccess::Phase(F.Mode),ERogueBattleStage::Night))return false;
    TestEqual(TEXT("Night begins after the configured day waves"),F.Mode->GetCurrentWave(),36);
    F.Step(.25f);
    ARogueEnemy* Survivor=F.Find();if(!TestNotNull(TEXT("Previous-wave enemy is alive"),Survivor))return false;
    F.Step(1.5f);TestEqual(TEXT("No early time-based wave"),F.Mode->GetCurrentWave(),36);
    F.Step(.25f);
    TestEqual(TEXT("Time advances despite incomplete budget and living enemies"),F.Mode->GetCurrentWave(),37);
    TestTrue(TEXT("Previous enemy remains in the field"),F.Find()==Survivor);
    TestEqual(TEXT("Alive cap still prevents extra spawns"),F.Tracker->GetActiveEnemyCount(),1);
    TestEqual(TEXT("Timed wave gets fresh generation quotas"),F.Spawn->GetSpawnedCountThisWave(),0);
    TestEqual(TEXT("Clients receive the authoritative wave snapshot"),F.World->GetGameState<ARogueGameState>()->GetCurrentWave(),37);
    F.Clear();F.Step(.1f);
    TestEqual(TEXT("An early clear cannot accelerate a night wave"),F.Mode->GetCurrentWave(),37);
    F.Spawn->SpawnBoss(FRogueWaveProgressionTestAccess::Run(F.Mode),F.Player,F.Region);
    if(!TestNotNull(TEXT("A living Boss exists for the boundary"),F.Find(true)))return false;
    F.Step(2.f);
    TestEqual(TEXT("Boss does not hold timed waves"),F.Mode->GetCurrentWave(),38);
    TestTrue(TEXT("Boss is preserved across the timed boundary"),FRogueWaveProgressionTestAccess::Run(F.Mode).bBossAlive&&F.Find(true));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueNightWaveClockTest,"Rogue.Gameplay.WaveClear.NightClockOriginRemainderAndConfiguration",RogueWaveProgressionTests::Flags)
bool FRogueNightWaveClockTest::RunTest(const FString& Parameters)
{
    RogueWaveProgressionTests::FFixture F;if(!F.Initialize())return false;
    FRogueWaveProgressionTestAccess::Run(F.Mode).RunTimeSeconds=900000;
    FRogueWaveProgressionTestAccess::Night(F.Mode,F.Balance,2.f);
    TestEqual(TEXT("Variable day survival time is preserved"),F.Mode->GetRunTimeSeconds(),900000.f);
    TestEqual(TEXT("Old run time does not jump the first night wave"),F.Mode->GetCurrentWave(),36);
    F.Step(7.25f);TestEqual(TEXT("Hitch accounts for three completed periods"),F.Mode->GetCurrentWave(),39);
    F.Step(.5f);TestEqual(TEXT("Partial-period remainder is retained"),F.Mode->GetCurrentWave(),39);
    F.Step(.25f);TestEqual(TEXT("Remainder reaches the next precise boundary"),F.Mode->GetCurrentWave(),40);
    FRogueWaveProgressionTestAccess::Night(F.Mode,F.Balance,0.f);
    TestEqual(TEXT("Non-positive night duration clamps to one second"),FRogueWaveProgressionTestAccess::NightDuration(F.Mode),1.f);
    F.Step(.75f);TestEqual(TEXT("A new night resets its phase clock"),F.Mode->GetCurrentWave(),36);
    F.Step(.25f);TestEqual(TEXT("Clamped duration is effective"),F.Mode->GetCurrentWave(),37);
    FRogueWaveProgressionTestAccess::Night(F.Mode,F.Balance,std::numeric_limits<float>::quiet_NaN());
    TestEqual(TEXT("Invalid duration uses the normal 28-second fallback"),FRogueWaveProgressionTestAccess::NightDuration(F.Mode),28.f);
    F.Step(27);TestEqual(TEXT("Fallback waits its full duration"),F.Mode->GetCurrentWave(),36);
    F.Step(1);TestEqual(TEXT("Fallback advances at 28 seconds"),F.Mode->GetCurrentWave(),37);
    return true;
}
#endif
