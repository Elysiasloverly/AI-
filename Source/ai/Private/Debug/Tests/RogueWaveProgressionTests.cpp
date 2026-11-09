#if WITH_DEV_AUTOMATION_TESTS
#include "RogueTestWorld.h"
#include "Framework/RogueGameMode.h"
#include "Framework/RogueGameState.h"
#include "Framework/RogueGameBalanceAsset.h"
#include "Framework/RogueGameModeRules.h"
#include "Enemies/Spawning/RogueSpawnSubsystem.h"
#include "Enemies/Data/RogueEnemyDataTable.h"
#include "Enemies/Data/RogueEnemyAssetPaths.h"
#include "Enemies/RogueEnemy.h"
#include "Enemies/Components/RogueBossSkillComponent.h"
#include "AbilitySystem/Abilities/RogueBossSkillAbility.h"
#include "Combat/Targeting/RogueEnemyTrackerSubsystem.h"
#include "Player/RogueCharacter.h"
#include "Player/RoguePlayerController.h"
#include "Player/RoguePlayerState.h"
#include "AbilitySystemComponent.h"
#include "Components/BoxComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/WorldSettings.h"
#include "World/RogueEnemySpawnRegion.h"
#include "World/RogueShopTerminal.h"
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
    static void SetShop(ARogueGameMode* Mode, ARogueShopTerminal* Shop) { Mode->LevelShopTerminal = Shop; }
    static FRogueRunState& Run(ARogueGameMode* Mode) { return Mode->RunState; }
    static void FinalDay(ARogueGameMode* Mode,int32 Wave)
    {
        Mode->bDayStageMap=true;Mode->StageSettings.WavesPerStage=Wave;Mode->StageSettings.CountdownSeconds=30;
    }
    static ERogueBattleStage Phase(ARogueGameMode* Mode) { return Mode->StageView.Phase; }
    static double Deadline(ARogueGameMode* Mode) { return Mode->StageView.CountdownDeadline; }
    static void Night(ARogueGameMode* Mode,URogueGameBalanceAsset* Balance)
    {
        Balance->BattleStage.NightMap = TSoftObjectPtr<UWorld>(Mode->GetWorld());
        Mode->InitializeBattleStage();
    }
    static void PrepareVictoryBoundary(ARogueGameMode* Mode,int32 Wave)
    {
        Mode->RunState.CurrentWave=Wave; Mode->RunState.BattleStageIndex=4; Mode->StageView.Phase=ERogueBattleStage::Day;
        Mode->bDayStageMap=false;
    }
    static void Composition(ARogueGameMode* Mode,bool Night,float Density)
    {
        Mode->StageView.Phase=Night?ERogueBattleStage::Night:ERogueBattleStage::Day;
        Mode->StageView.EnemyCountMultiplier=Density;Mode->StageView.EnemySpawnRateMultiplier=Density;
    }
    static void CompositionBlueprints(ARogueGameMode* Mode,URogueSpawnSubsystem* Spawn)
    {
        TMap<ERogueEnemyType,TSubclassOf<ARogueEnemy>> Classes;
        Classes.Add(ERogueEnemyType::Rifleman,LoadClass<ARogueEnemy>(nullptr,*RogueEnemyAssetPaths::EnemyClassPath(TEXT("Rifleman"))));
        Classes.Add(ERogueEnemyType::Orbiter,LoadClass<ARogueEnemy>(nullptr,*RogueEnemyAssetPaths::EnemyClassPath(TEXT("Orbiter"))));
        auto* Table=LoadObject<UDataTable>(nullptr,TEXT("/Game/Data/Enemies/DT_EnemyArchetypes.DT_EnemyArchetypes"));
        Spawn->Configure(ARogueEnemy::StaticClass(),Classes,Mode->SpawnSettings,Mode->BossSettings,Mode->LoadedGameBalanceAsset,Table);
    }
    static void FirstNightBoundary(ARogueGameMode* Mode)
    { PrepareVictoryBoundary(Mode,40); Mode->RunState.BattleStageIndex=1; Mode->StageView.Phase=ERogueBattleStage::Night; }

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
        bool Initialize(int32 Total=3,int32 BossInterval=0,UDataTable* Table=nullptr)
        {
            SelfTest.Set(TEXT("0"));Stress.Set(TEXT("0"));
            if (!RogueTestWorld::Begin(TestWorld, World)) { return false; }
            Mode = World->GetAuthGameMode<ARogueGameMode>();
            auto* PC = RogueTestWorld::SpawnFreshController(World);
            const FTransform Transform(FVector(0,0,300));
            Player=World->SpawnActorDeferred<ARogueCharacter>(ARogueCharacter::StaticClass(),Transform,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
            Player->AutoPossessPlayer=EAutoReceiveInput::Disabled;Player->FinishSpawning(Transform);PC->Possess(Player);
            Player->SetActorTickEnabled(false);Player->GetCharacterMovement()->SetComponentTickEnabled(false);
            Player->SetActorEnableCollision(false);Player->GetAbilitySystemComponent()->CancelAllAbilities();
            Floor = RogueTestWorld::StaticBox(World, FVector(0,0,-20), FVector(20000,20000,20),
                FRotator::ZeroRotator, ECollisionEnabled::QueryOnly);
            Region=World->SpawnActor<ARogueEnemySpawnRegion>();Region->GetSpawnBounds()->SetBoxExtent(FVector(5000,5000,6000));
            Balance=NewObject<URogueGameBalanceAsset>(World);
            Balance->Spawn.BaseEnemiesPerWave=Total;Balance->Spawn.EnemiesPerWaveIncrement=1;
            Balance->Spawn.BatchRules.Reset();Balance->Spawn.MinimumInterval=.01f;
            Balance->Spawn.EarlyWaves.FirstWaveIntervalMultiplier=Balance->Spawn.EarlyWaves.LastWaveIntervalMultiplier=1;
            Balance->EnemyWaveEntries={{1,ERogueEnemyType::Rifleman,1}};
            FRogueSpawnSettings Settings;Settings.SpawnInterval=.01f;
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
    F.FinishCurrentWave(); F.Step();
    TestEqual(TEXT("Clear starts the interval on the completed wave"), F.Mode->GetCurrentWave(), 1);
    F.Step(2.99f);
    TestEqual(TEXT("No new wave before three seconds"), F.Mode->GetCurrentWave(), 1);
    TestEqual(TEXT("No enemies spawn during the interval"), F.Tracker->GetActiveEnemyCount(), 0);
    F.Step(.01f);
    TestEqual(TEXT("Clear advances once at three seconds"), F.Mode->GetCurrentWave(), 2);
    TestEqual(TEXT("The new wave can start immediately without an intermission catch-up batch"), F.Tracker->GetActiveEnemyCount(), 1);
    TestEqual(TEXT("Next wave own budget"),F.Spawn->GetWaveEnemyBudget(),4);
    return true;

}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueWaveShopFlightTest,
    "Rogue.Gameplay.WaveClear.ShopDropsNearHostAndWaitsForLastShopper", RogueWaveProgressionTests::Flags)
bool FRogueWaveShopFlightTest::RunTest(const FString&)
{
    using namespace RogueWaveProgressionTests;
    FFixture F;
    if (!F.Initialize(1)) { return false; }
    const auto TickPresentation = [&F](int32 Frames)
    {
        for (int32 Frame = 0; Frame < Frames; ++Frame) { F.TestWorld.TickTestWorld(.02f); }
    };
    UClass* ShopClass = LoadClass<ARogueShopTerminal>(nullptr,
        TEXT("/Game/BP/World/BP_RogueShopTerminal.BP_RogueShopTerminal_C"));
    if (!TestNotNull(TEXT("The authored shop Blueprint loads"), ShopClass)) { return false; }
    auto* Shop = F.World->SpawnActor<ARogueShopTerminal>(ShopClass, FVector(-3000, -3000, 20), FRotator::ZeroRotator);
    if (!TestNotNull(TEXT("The authored shop Blueprint can be placed"), Shop)) { return false; }
    FRogueWaveProgressionTestAccess::SetShop(F.Mode, Shop);
    const FVector HostLocation(1000, 0, 98);
    F.Player->SetActorLocation(HostLocation);
    auto* HostState = F.Player->GetPlayerState<ARoguePlayerState>();
    auto* GuestController = RogueTestWorld::SpawnFreshController(F.World);
    const FVector GuestLocation(-2300, 0, 98);
    FActorSpawnParameters GuestParams;
    GuestParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Guest = F.World->SpawnActor<ARogueCharacter>(ARogueCharacter::StaticClass(), GuestLocation,
        FRotator::ZeroRotator, GuestParams);
    GuestController->Possess(Guest);
    Guest->SetActorTickEnabled(false);
    Guest->GetCharacterMovement()->SetComponentTickEnabled(false);
    Guest->GetAbilitySystemComponent()->CancelAllAbilities();
    auto* GuestState = GuestController->GetPlayerState<ARoguePlayerState>();
    const FVector ObstructedSpot(1255, 255, 120);
    RogueTestWorld::StaticBox(F.World, ObstructedSpot, FVector(72, 72, 120));

    TestTrue(TEXT("The shop starts hidden"), Shop->IsWaitingInSky() && Shop->IsHidden());
    HostState->Server_RequestOpenShop();
    TestFalse(TEXT("A hidden shop cannot be opened remotely"), HostState->IsShopOpen());
    F.FinishCurrentWave(); F.Step();
    const FVector AirStart = Shop->GetActorLocation();
    TestTrue(TEXT("Clearing the wave starts a visible drop from above"), !Shop->IsHidden() && AirStart.Z > HostLocation.Z + 500);
    TestFalse(TEXT("Interaction stays off in flight"), Shop->IsReady());
    TickPresentation(25);
    TestTrue(TEXT("The drop moves laterally and down"), FVector::Dist2D(AirStart, Shop->GetActorLocation()) > 40 && Shop->GetActorLocation().Z < AirStart.Z);
    TickPresentation(45);
    if (!TestTrue(TEXT("The Blueprint reaches its interactive phase"), Shop->IsReady())) { return false; }
    const FVector Landed = Shop->GetActorLocation();
    TestTrue(TEXT("The drop begins twenty metres above the landing pose"), FMath::IsNearlyEqual(AirStart.Z - Landed.Z, 2000.0, 1.0));
    TestTrue(TEXT("The shop lands near the host, not the far guest or authored placement"),
        FVector::Dist2D(Landed, HostLocation) <= 710.0 && FVector::Dist2D(Landed, GuestLocation) > 2000.0);
    TestTrue(TEXT("An obstructed first landing spot is avoided"), FVector::Dist2D(Landed, ObstructedSpot) > 150.0);
    GuestState->Server_RequestOpenShop();
    TestFalse(TEXT("An out-of-range guest cannot open the landed shop"), GuestState->IsShopOpen());

    F.Player->SetActorEnableCollision(true);
    F.Player->SetActorLocation(FVector(Landed.X, Landed.Y, HostLocation.Z));
    Guest->SetActorLocation(FVector(Landed.X + 15, Landed.Y, HostLocation.Z));
    TickPresentation(2);
    HostState->Server_RequestOpenShop();
    GuestState->Server_RequestOpenShop();
    TestTrue(TEXT("Both overlapping players can open their own shop"), HostState->IsShopOpen() && GuestState->IsShopOpen());
    F.Step(3.f);
    TestEqual(TEXT("Shopping does not postpone the next wave"), F.Mode->GetCurrentWave(), 2);
    HostState->Server_RequestCloseShop();
    TestTrue(TEXT("The terminal stays for the guest"), Shop->IsReady() && GuestState->IsShopOpen());
    GuestState->Server_RequestCloseShop();
    TestFalse(TEXT("The terminal stops accepting interactions at the start of its jump"), Shop->IsReady());
    const double StandingScale = Shop->GetActorScale3D().Z;
    TickPresentation(15);
    AddInfo(FString::Printf(TEXT("Standing scale %.3f, charging scale %.3f, landed Z %.2f, charging Z %.2f"),
        StandingScale, Shop->GetActorScale3D().Z, Landed.Z, Shop->GetActorLocation().Z));
    TestTrue(TEXT("The shop squashes to charge before launching"), Shop->GetActorScale3D().Z < StandingScale - .04);
    TestTrue(TEXT("The charge stays at the landing site before takeoff"),
        FVector::DistSquared2D(Shop->GetActorLocation(), Landed) < FMath::Square(2.0));
    TickPresentation(16);
    TestTrue(TEXT("The shop launches upward after the charge"), Shop->GetActorLocation().Z > Landed.Z + 100);
    TickPresentation(36);
    TestTrue(TEXT("The terminal reaches at least 23 metres before it vanishes"),
        !Shop->IsHidden() && Shop->GetActorLocation().Z - Landed.Z > 2300.0);
    TickPresentation(20);
    TestTrue(TEXT("The completed jump hides the terminal"), Shop->IsWaitingInSky() && Shop->IsHidden());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueNoGlobalAliveLimitTest,"Rogue.Gameplay.WaveClear.NoGlobalCapPreservesPacingAndBudget",RogueWaveProgressionTests::Flags)
bool FRogueNoGlobalAliveLimitTest::RunTest(const FString& Parameters)
{
    RogueWaveProgressionTests::FFixture F; if(!F.Initialize(30))return false;
    F.Balance->Spawn.BatchRules={{1,4}};
    F.Step(.001f);
    TestEqual(TEXT("Batch size is controlled by the current wave's rule"),F.Spawn->GetSpawnedCountThisWave(),4);
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
    RogueWaveProgressionTests::FFixture F;if(!F.Initialize(1,1))return false;
    F.Floor->SetCollisionEnabled(ECollisionEnabled::NoCollision);F.Step(1000);
    TestEqual(TEXT("Failed ground search consumes no enemies"),F.Spawn->GetSpawnedCountThisWave(),0);
    TestEqual(TEXT("No enemies created is not a completed wave"),F.Mode->GetCurrentWave(),1);
    TestFalse(TEXT("Boss failure is not a completed generation"),F.Spawn->HasFinishedWaveSpawning(FRogueWaveProgressionTestAccess::Run(F.Mode)));
    F.Floor->SetCollisionEnabled(ECollisionEnabled::QueryOnly);F.Step(1);
    if(!TestNotNull(TEXT("Boss retry succeeds"),F.Find(true))||!TestNotNull(TEXT("Ordinary budget succeeds"),F.Find()))return false;
    F.Clear();F.Step();TestEqual(TEXT("Living Boss holds the wave"),F.Mode->GetCurrentWave(),1);
    F.Clear(true);F.Step();TestEqual(TEXT("Boss flag also guards against a missed kill notification"),F.Mode->GetCurrentWave(),1);
    FRogueWaveProgressionTestAccess::Run(F.Mode).bBossAlive=false;F.Step();F.Step(3.f);
    TestEqual(TEXT("Only after Boss defeat can the next wave begin"),F.Mode->GetCurrentWave(),2);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueWaveTypeQuotaTest,"Rogue.Gameplay.WaveClear.TypeQuotaExhaustionAndAliveSlots",RogueWaveProgressionTests::Flags)
bool FRogueWaveTypeQuotaTest::RunTest(const FString& Parameters)
{
    UDataTable* Table=NewObject<UDataTable>();Table->RowStruct=FRogueEnemyArchetypeRow::StaticStruct();
    FRogueEnemyArchetypeRow Row;Row.EnemyType=ERogueEnemyType::Rifleman;Row.MaxAlive=1;Row.MaxSpawnPerWave=2;
    Table->AddRow(TEXT("Rifleman"),Row);
    RogueWaveProgressionTests::FFixture F;if(!F.Initialize(10,0,Table))return false;
    F.Step();F.Step(1000);
    TestFalse(TEXT("Alive slots temporarily full does not exhaust total quota"),F.Spawn->HasFinishedWaveSpawning(FRogueWaveProgressionTestAccess::Run(F.Mode)));
    TestEqual(TEXT("Type alive cap honored"),F.Spawn->GetSpawnedCountThisWave(),1);
    F.Clear();F.Step();TestEqual(TEXT("A kill frees the type's alive slot"),F.Spawn->GetSpawnedCountThisWave(),2);
    TestTrue(TEXT("All types' per-wave quotas can finish before the global target"),F.Spawn->HasFinishedWaveSpawning(FRogueWaveProgressionTestAccess::Run(F.Mode)));
    F.Clear();F.Step();F.Step(3.f);TestEqual(TEXT("Limited-only pools do not deadlock or inject fallback enemies"),F.Mode->GetCurrentWave(),2);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueFinalWaveBudgetTest,"Rogue.Gameplay.WaveClear.FinalDayWaitsForBudgetAndClear",RogueWaveProgressionTests::Flags)
bool FRogueFinalWaveBudgetTest::RunTest(const FString& Parameters)
{
    RogueWaveProgressionTests::FFixture F;if(!F.Initialize(2))return false;
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
    RogueWaveProgressionTests::FFixture F;if(!F.Initialize(3))return false;
    FRogueWaveProgressionTestAccess::Night(F.Mode,F.Balance);
    TestEqual(TEXT("Actual night recognized"),FRogueWaveProgressionTestAccess::Phase(F.Mode),ERogueBattleStage::Night);
    F.Step(.1f);F.Step(1000);TestEqual(TEXT("Long time cannot advance night"),F.Mode->GetCurrentWave(),21);
    F.Spawn->SpawnBoss(FRogueWaveProgressionTestAccess::Run(F.Mode),F.Player,F.Region);
    TestNotNull(TEXT("Boss created"),F.Find(true));
    F.Step(1000);TestEqual(TEXT("Night Boss and survivors hold wave"),F.Mode->GetCurrentWave(),21);
    F.FinishCurrentWave();F.Step();F.Step(3.f);TestEqual(TEXT("Cleared night starts exactly next wave"),F.Mode->GetCurrentWave(),22);
    TestEqual(TEXT("Replicated snapshot follows clear"),F.World->GetGameState<ARogueGameState>()->GetCurrentWave(),22);
    return true;

}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueNightWaveClockTest,"Rogue.Gameplay.WaveClear.NightIgnoresLegacyDurationAndHitches",RogueWaveProgressionTests::Flags)
bool FRogueNightWaveClockTest::RunTest(const FString& Parameters)
{
    RogueWaveProgressionTests::FFixture F;if(!F.Initialize())return false;
    FRogueWaveProgressionTestAccess::Night(F.Mode,F.Balance);F.Step(999999);
    TestEqual(TEXT("Legacy duration cannot skip waves"),F.Mode->GetCurrentWave(),21);
    TestFalse(TEXT("Hitch cannot grant victory"),F.Mode->IsVictory());
    F.FinishCurrentWave();F.Step(.01f);F.Step(3.f);TestEqual(TEXT("Clear still progresses normally"),F.Mode->GetCurrentWave(),22);
    return true;

}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueBossCatalogCompositionTest,"Rogue.Gameplay.BossSkills.NineSkills.CatalogUsesActualBlueprintAndWave",RogueWaveProgressionTests::Flags)
bool FRogueBossCatalogCompositionTest::RunTest(const FString&)
{
    RogueWaveProgressionTests::FFixture F; if (!F.Initialize()) return false;
    FRogueWaveProgressionTestAccess::CompositionBlueprints(F.Mode,F.Spawn);
    auto& Run=FRogueWaveProgressionTestAccess::Run(F.Mode);
    for (const FIntPoint Sample : { FIntPoint(10,3), FIntPoint(30,4), FIntPoint(100,9) })
    {
        Run.CurrentWave=Sample.X;
        FRogueWaveProgressionTestAccess::Composition(F.Mode,Sample.Y==4,1.f);
        F.Spawn->SpawnBoss(Run,F.Player,F.Region);
        auto* Boss=F.Find(true);
        if (!TestNotNull(TEXT("Actual blueprint boss spawns"),Boss)) return false;
        const auto& Chosen=Boss->BossSkills->GetSelectedSkills();
        TestEqual(TEXT("Day/night/final selected count"),Chosen.Num(),Sample.Y);
        TSet<FName> Unique;
        for (const auto& H:Chosen)
        {
            Unique.Add(H.RowName);
            const auto* R=H.GetRow<FRogueBossSkillRow>(TEXT("Catalog test"));
            TestTrue(TEXT("Every chosen row is enabled and has a skill, icon and guide"),R && R->bEnabled && R->AbilityClass && R->Icon && !R->Description.IsEmpty());
        }
        TestEqual(TEXT("Sampled without replacement"),Unique.Num(),Chosen.Num());
        auto* GS=F.World->GetGameState<ARogueGameState>();GS->RefreshBossHealthView();
        TestEqual(TEXT("HUD snapshot carries exactly the Boss selection"),GS->GetBossHealthView().Skills.Num(),Sample.Y);
        F.Clear(true); Run.bBossAlive=false;
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueWaveVictoryBoundaryTest,"Rogue.Gameplay.Victory.FinalDayBoundaryAndTerminalCleanup",RogueWaveProgressionTests::Flags)
bool FRogueWaveVictoryBoundaryTest::RunTest(const FString& Parameters)
{
    RogueWaveProgressionTests::FFixture F;if(!F.Initialize())return false;
    FRogueWaveProgressionTestAccess::PrepareVictoryBoundary(F.Mode,99);
    F.Step(.5f); if(!TestNotNull(TEXT("Combat exists at 99"),F.Find()))return false;
    auto& Run=FRogueWaveProgressionTestAccess::Run(F.Mode);Run.EnemiesDefeated=17;
    F.Step(999.f);TestEqual(TEXT("99 waits for actual clear"),F.Mode->GetCurrentWave(),99);
    F.FinishCurrentWave();F.Step();F.Step(3.f);
    TestEqual(TEXT("Exactly 100"),F.Mode->GetCurrentWave(),100);
    TestFalse(TEXT("Reaching 100 alone does not win"),F.Mode->IsVictory());
    if (!TestNotNull(TEXT("Final boss spawns even when regular interval is disabled"),F.Find(true))) return false;
    TestTrue(TEXT("Final boss tracked"),Run.bBossAlive);
    F.FinishCurrentWave(); F.Step();
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
    F.FinishCurrentWave();F.Step();F.Step(3.f);TestEqual(TEXT("First clear reaches 99"),F.Mode->GetCurrentWave(),99);
    F.FinishCurrentWave();F.Step();F.Step(3.f);TestFalse(TEXT("Second clear starts the final battle"),F.Mode->IsVictory());
    if (!TestNotNull(TEXT("Final battle has a boss"),F.Find(true))) return false;
    F.FinishCurrentWave();F.Step();TestTrue(TEXT("Final clear reaches victory"),F.Mode->IsVictory());
    TestEqual(TEXT("Victory at 100"),F.Mode->GetCurrentWave(),100);return true;

}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueWaveVictoryInitialTest,"Rogue.Gameplay.Victory.EntryAlreadyAt100",RogueWaveProgressionTests::Flags)
bool FRogueWaveVictoryInitialTest::RunTest(const FString& Parameters)
{
    RogueWaveProgressionTests::FFixture F;if(!F.Initialize())return false;
    FRogueWaveProgressionTestAccess::Run(F.Mode).CurrentWave=100;
    FRogueWaveProgressionTestAccess::FinalDay(F.Mode,100);F.Step();
    TestFalse(TEXT("Entering at 100 starts combat before any travel"),F.Mode->IsVictory());
    if (!TestNotNull(TEXT("Final boss is present"),F.Find(true))) return false;
    F.FinishCurrentWave();F.Step();
    TestTrue(TEXT("Clearing 100 wins before day-stage travel"),F.Mode->IsVictory());
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
    TestEqual(TEXT("Stops at 40 before clearing"),F.Mode->GetCurrentWave(),40);
    TestEqual(TEXT("Waits for survivors rather than travelling"),FRogueWaveProgressionTestAccess::Phase(F.Mode),ERogueBattleStage::Clearing);
    TestTrue(TEXT("Every generation entry is suspended"),F.Spawn->IsSpawningSuspended());
    const auto* State=F.World->GetGameState<ARogueGameState>();
    TestFalse(TEXT("Destination is day"),State->GetBattleStage().bNextStageIsNight);
    F.Step(999.f);TestEqual(TEXT("No timed advance during clearing"),F.Mode->GetCurrentWave(),40);
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

namespace RogueNightCompositionTests
{
    using namespace RogueWaveProgressionTests;
    UDataTable* Table(int32 Alive=0,int32 WaveCap=0)
    {
        auto* T=NewObject<UDataTable>();T->RowStruct=FRogueEnemyArchetypeRow::StaticStruct();
        FRogueEnemyArchetypeRow M;M.EnemyType=ERogueEnemyType::Rifleman;M.bContactOnlyNormal=true;M.MaxAlive=Alive;M.MaxSpawnPerWave=WaveCap;T->AddRow(TEXT("Rifleman"),M);
        FRogueEnemyArchetypeRow R;R.EnemyType=ERogueEnemyType::Orbiter;T->AddRow(TEXT("Orbiter"),R);return T;
    }
    void Configure(FFixture& F,float Percent=70,bool Night=true,float Density=1)
    {
        F.Balance->BattleStage.NightMeleePercent=Percent;
        F.Balance->BattleStage.NightMeleeMoveSpeedMultiplier=2;
        F.Balance->BattleStage.NightMeleeDamageMultiplier=2;
        // A strongly biased original pool still must not overwhelm the group ratio.
        F.Balance->EnemyWaveEntries={{1,ERogueEnemyType::Rifleman,9,true},{1,ERogueEnemyType::Orbiter,1,true}};
        FRogueWaveProgressionTestAccess::Composition(F.Mode,Night,Density);
    }
    void Fill(FFixture& F)
    {
        for(int I=0;I<1000;++I)
            if(!F.Spawn->SpawnEnemy(FRogueWaveProgressionTestAccess::Run(F.Mode),F.Player,F.Region))break;
    }
    int32 MeleeAlive(FFixture& F)
    {
        int32 N=0;for(auto E:F.Tracker->GetActiveEnemies())if(E.IsValid()&&!E->IsBoss()&&E->GetEnemyType()==ERogueEnemyType::Rifleman)++N;return N;
    }
    void ClearNormals(FFixture& F)
    {while(F.Find())F.Clear();}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNightCompositionActualTest,"Rogue.Gameplay.NightComposition.ExactSeventyPercentActualBlueprintStatsBossAndPoolReuse",RogueWaveProgressionTests::Flags)
bool FNightCompositionActualTest::RunTest(const FString&)
{
    using namespace RogueNightCompositionTests;FFixture F;if(!F.Initialize(10,0,Table()))return false;Configure(F);
    FRogueWaveProgressionTestAccess::CompositionBlueprints(F.Mode,F.Spawn);
    Fill(F);TestEqual(TEXT("The exact wave quota is filled"),F.Spawn->GetSpawnedCountThisWave(),10);TestEqual(TEXT("70% of ten is seven melee"),MeleeAlive(F),7);
    TSet<ARogueEnemy*> Previous;
    for(auto E:F.Tracker->GetActiveEnemies())if(E.IsValid())
    {
        Previous.Add(E.Get());const bool M=E->GetEnemyType()==ERogueEnemyType::Rifleman;const auto P=RogueGameModeRules::BuildEnemyProfile(1,E->GetEnemyType(),F.Balance);
        TestEqual(TEXT("Only melee move speed doubles"),E->GetMoveSpeed(),P.Speed*(M?2:1));
        TestEqual(TEXT("Only melee contact damage doubles"),E->GetContactDamage(),P.Damage*(M?2:1));
        TestEqual(TEXT("Health is unchanged"),E->GetMaxHealth(),P.Health);
        TestEqual(TEXT("Real melee BP has no ranged attack, other BP retains it"),E->GetRangedArchetype().bUsesRangedAttack,!M);
    }
    F.Spawn->SpawnBoss(FRogueWaveProgressionTestAccess::Run(F.Mode),F.Player,F.Region);auto* Boss=F.Find(true);
    if(!TestNotNull(TEXT("Boss still spawns separately"),Boss))return false;
    auto BP=RogueGameModeRules::BuildBossProfile(1,Boss->GetEnemyType(),F.Balance);
    TestEqual(TEXT("Boss speed is not doubled"),Boss->GetMoveSpeed(),BP.Speed);TestEqual(TEXT("Boss damage is not doubled"),Boss->GetContactDamage(),BP.Damage);TestTrue(TEXT("Boss retains ranged attacks"),Boss->GetRangedArchetype().bUsesRangedAttack);
    ClearNormals(F);FRogueWaveProgressionTestAccess::Run(F.Mode).CurrentWave=2;Fill(F);TestEqual(TEXT("Next wave rounds its own eleven slots"),MeleeAlive(F),8);
    bool Reused=false;
    for(auto E:F.Tracker->GetActiveEnemies())if(E.IsValid()&&!E->IsBoss())
    {
        Reused|=Previous.Contains(E.Get());const auto P=RogueGameModeRules::BuildEnemyProfile(2,E->GetEnemyType(),F.Balance);const float Scale=E->GetEnemyType()==ERogueEnemyType::Rifleman?2:1;
        TestEqual(TEXT("Pool reuse recomputes speed, never quadruples"),E->GetMoveSpeed(),P.Speed*Scale);TestEqual(TEXT("Pool reuse recomputes damage"),E->GetContactDamage(),P.Damage*Scale);
    }
    TestTrue(TEXT("Actual pooled instances were reused"),Reused);
    ClearNormals(F);FRogueWaveProgressionTestAccess::Composition(F.Mode,false,1);FRogueWaveProgressionTestAccess::Run(F.Mode).CurrentWave=3;
    F.Balance->EnemyWaveEntries={{1,ERogueEnemyType::Rifleman,1}};Fill(F);
    for(auto E:F.Tracker->GetActiveEnemies())if(E.IsValid()&&!E->IsBoss())
    {
        auto P=RogueGameModeRules::BuildEnemyProfile(3,E->GetEnemyType(),F.Balance);TestEqual(TEXT("Daytime reuse restores original speed"),E->GetMoveSpeed(),P.Speed);TestEqual(TEXT("Daytime reuse restores original damage"),E->GetContactDamage(),P.Damage);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNightCompositionEditableTest,"Rogue.Gameplay.NightComposition.EditablePercentExtremesAndNextWaveChanges",RogueWaveProgressionTests::Flags)
bool FNightCompositionEditableTest::RunTest(const FString&)
{
    using namespace RogueNightCompositionTests;
    for(float Percent:{0.f,30.f,100.f})
    {
        FFixture F;if(!F.Initialize(10,0,Table()))return false;Configure(F,Percent);Fill(F);
        TestEqual(TEXT("Configured ratio controls successful spawns"),MeleeAlive(F),FMath::RoundToInt(Percent/10));TestEqual(TEXT("Both extremes still complete the wave"),F.Spawn->GetSpawnedCountThisWave(),10);
        ClearNormals(F);F.Balance->BattleStage.NightMeleePercent=50;F.Balance->BattleStage.NightMeleeMoveSpeedMultiplier=1.5f;F.Balance->BattleStage.NightMeleeDamageMultiplier=3;
        FRogueWaveProgressionTestAccess::Run(F.Mode).CurrentWave=2;Fill(F);TestEqual(TEXT("Asset changes are sampled on next wave"),MeleeAlive(F),6);
        for(auto E:F.Tracker->GetActiveEnemies())if(E.IsValid()&&E->GetEnemyType()==ERogueEnemyType::Rifleman)
        {
            auto P=RogueGameModeRules::BuildEnemyProfile(2,E->GetEnemyType(),F.Balance);TestEqual(TEXT("Custom speed scale is used"),E->GetMoveSpeed(),P.Speed*1.5f);TestEqual(TEXT("Custom damage scale is used"),E->GetContactDamage(),P.Damage*3);
        }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNightCompositionAliveCapTest,"Rogue.Gameplay.NightComposition.AliveCapWaitsWithoutReplacingMeleeWithShooters",RogueWaveProgressionTests::Flags)
bool FNightCompositionAliveCapTest::RunTest(const FString&)
{
    using namespace RogueNightCompositionTests;FFixture F;if(!F.Initialize(10,0,Table(1)))return false;Configure(F);Fill(F);
    TestEqual(TEXT("One living melee plus the three other slots"),F.Spawn->GetSpawnedCountThisWave(),4);TestEqual(TEXT("Melee alive cap is respected"),MeleeAlive(F),1);
    TestFalse(TEXT("Temporary alive cap never completes the wave"),F.Spawn->HasFinishedWaveSpawning(FRogueWaveProgressionTestAccess::Run(F.Mode)));
    for(int I=0;I<6;++I)
    {
        for(auto E:F.Tracker->GetActiveEnemies())if(E.IsValid()&&E->GetEnemyType()==ERogueEnemyType::Rifleman){E->DeactivateToPool();break;}
        Fill(F);TestEqual(TEXT("Each death releases one melee slot"),MeleeAlive(F),1);
    }
    TestEqual(TEXT("Seven melee have now spawned cumulatively"),F.Spawn->GetSpawnedCountThisWave(),10);TestTrue(TEXT("All successful quotas finish spawning"),F.Spawn->HasFinishedWaveSpawning(FRogueWaveProgressionTestAccess::Run(F.Mode)));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNightCompositionQuotaTest,"Rogue.Gameplay.NightComposition.TypeWaveCapsFallbackAndMissingGroupStayPlayable",RogueWaveProgressionTests::Flags)
bool FNightCompositionQuotaTest::RunTest(const FString&)
{
    using namespace RogueNightCompositionTests;
    {FFixture F;if(!F.Initialize(10,0,Table(0,2)))return false;Configure(F,70,true,2);Fill(F);
     TestEqual(TEXT("Existing night total multiplier is preserved"),F.Spawn->GetSpawnedCountThisWave(),20);TestEqual(TEXT("Individual wave cap also keeps its existing x2 scaling"),MeleeAlive(F),4);
     TestTrue(TEXT("Impossible share redistributes without a stuck wave"),F.Spawn->HasFinishedWaveSpawning(FRogueWaveProgressionTestAccess::Run(F.Mode)));}
    {FFixture F;if(!F.Initialize(10,0,Table()))return false;Configure(F);F.Balance->EnemyWaveEntries={{1,ERogueEnemyType::Orbiter,1}};Fill(F);
     TestEqual(TEXT("A custom wave with no available melee still fills"),F.Spawn->GetSpawnedCountThisWave(),10);TestEqual(TEXT("No unavailable melee is injected"),MeleeAlive(F),0);}
    {FFixture F;if(!F.Initialize(10,0,Table()))return false;Configure(F);F.Balance->EnemyWaveEntries={{1,ERogueEnemyType::Rifleman,1}};Fill(F);
     TestEqual(TEXT("A custom wave with no other group still fills"),F.Spawn->GetSpawnedCountThisWave(),10);TestEqual(TEXT("Only eligible contact enemies appear"),MeleeAlive(F),10);}
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNightCompositionFailureTest,"Rogue.Gameplay.NightComposition.FailedGroundSearchConsumesNeitherGroupQuota",RogueWaveProgressionTests::Flags)
bool FNightCompositionFailureTest::RunTest(const FString&)
{
    using namespace RogueNightCompositionTests;FFixture F;if(!F.Initialize(10,0,Table()))return false;Configure(F);F.Floor->SetCollisionEnabled(ECollisionEnabled::NoCollision);Fill(F);
    TestEqual(TEXT("No quota consumed by an invalid location"),F.Spawn->GetSpawnedCountThisWave(),0);F.Floor->SetCollisionEnabled(ECollisionEnabled::QueryOnly);Fill(F);
    TestEqual(TEXT("Full total remains available after recovery"),F.Spawn->GetSpawnedCountThisWave(),10);TestEqual(TEXT("Recovered wave still has exact 70% contact enemies"),MeleeAlive(F),7);return true;
}

#endif
