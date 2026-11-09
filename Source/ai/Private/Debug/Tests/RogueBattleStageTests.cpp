#if WITH_DEV_AUTOMATION_TESTS
#include "Framework/RogueGameMode.h"
#include "Framework/RogueGameModeRules.h"
#include "Framework/RogueGameBalanceAsset.h"
#include "Framework/RogueGameState.h"
#include "Framework/RogueBattleStage.h"
#include "Player/RogueCharacter.h"
#include "Player/RoguePlayerController.h"
#include "Player/RoguePlayerState.h"
#include "AbilitySystem/RogueAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Combat/Targeting/RogueEnemyTrackerSubsystem.h"
#include "Enemies/RogueEnemy.h"
#include "Enemies/Spawning/RogueSpawnSubsystem.h"
#include "Enemies/Data/RogueEnemyArchetypes.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/IConsoleManager.h"
#include "Containers/Ticker.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"

struct FRogueBattleStageTestAccess
{
    static void DayBoundary(ARogueGameMode* Mode, int32 Wave = 20)
    {
        Mode->bDayStageMap = true;
        Mode->StageSettings = FRogueBattleStageSettings();
        Mode->StageSettings.WavesPerStage = Wave;
        Mode->RunState.BattleStageIndex = 0;
        Mode->StageView = FRogueBattleStageView();
        Mode->RunState.RunTimeSeconds = 123.f;
        Mode->RunState.CurrentWave = Wave;
        URogueSpawnSubsystem* Spawn = Mode->GetWorld()->GetSubsystem<URogueSpawnSubsystem>();
        Spawn->SetSpawningSuspended(false);
        Spawn->ResetWaveSpawnCounts(Wave);
        Spawn->TotalSpawnedThisWave = Spawn->WaveEnemyBudget;
        Mode->RunState.LastBossWaveSpawned = Wave;
    }
    static bool Step(ARogueGameMode* Mode, float Delta) { return Mode->TickBattleStage(Delta); }
    static FRogueBattleStageView View(ARogueGameMode* Mode) { return Mode->StageView; }
    static void SecondDayBoundary(ARogueGameMode* Mode)
    { DayBoundary(Mode,60);Mode->StageSettings.WavesPerStage=20;Mode->RunState.BattleStageIndex=2; }
    static void CycleBoundary(ARogueGameMode* Mode,int32 Index)
    {
        DayBoundary(Mode,(Index+1)*20);
        if(const auto* Balance=URogueGameBalanceAsset::GetDefaultGameBalance())Mode->StageSettings=Balance->BattleStage;
        Mode->StageSettings.WavesPerStage=20;
        Mode->RunState.BattleStageIndex=Index;
        Mode->StageSettings.CountdownSeconds=1;
        Mode->BeginBattleStageClearing();Mode->TickBattleStage(0);Mode->PushRunStateToGameState();
    }
    static void SetPeriodicStage(ARogueGameMode* Mode,int32 Index)
    {
        Mode->StageSettings.WavesPerStage=20;Mode->RunState.BattleStageIndex=Index;
        Mode->StageView.Phase=Index%2==0?ERogueBattleStage::Day:ERogueBattleStage::Night;
    }
    static void Win(ARogueGameMode* Mode) { Mode->RunState.CurrentWave=100;Mode->CompleteRunIfVictoryReached(); }
    static void Density(ARogueGameMode* M,float V) { M->StageView.EnemyCountMultiplier=V;M->StageView.EnemySpawnRateMultiplier=V; }
    static void ResetBudget(URogueSpawnSubsystem* S,int32 W) { S->ResetWaveSpawnCounts(W); }
    static int32 Budget(URogueSpawnSubsystem* S) { return S->WaveEnemyBudget; }
    static void Speed(ARogueGameMode* Mode, float Value) { Mode->StageView.EnemyAttackSpeedMultiplier = Value; }
    static void Progress(ARogueCharacter* Player, int32 Value)
    {
        Player->Money = Value;
        Player->PlayerLevel = Value / 10;
        Player->CurrentExperience = 3;
        Player->ExperienceRemainder = .4f;
        Player->ExperienceToNextLevel = 50;
    }
};

namespace RogueBattleStageTests
{
    constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;
    struct FFixture
    {
        FTestConsoleVariable SelfTest{TEXT("Rogue.SelfTest.WeaponUpgradeChain")};
        FTestWorldWrapper TestWorld;
        UWorld* World = nullptr;
        ARogueGameMode* Mode = nullptr;
        bool Initialize()
        {
            SelfTest.Set(TEXT("0"));
            if (!TestWorld.CreateTestWorld(EWorldType::Game)) { return false; }
            World = TestWorld.GetTestWorld();
            World->GetWorldSettings()->DefaultGameMode = ARogueGameMode::StaticClass();
            if (!TestWorld.BeginPlayInTestWorld()) { return false; }
            Mode = World->GetAuthGameMode<ARogueGameMode>();
            Mode->SetActorTickEnabled(false);
            return true;
        }
        ARogueCharacter* Player(ARoguePlayerController* PC = nullptr, bool bSetProfile = true)
        {
            if (!PC) { PC = World->SpawnActor<ARoguePlayerController>(); }
            if (bSetProfile) { PC->GetPlayerState<ARoguePlayerState>()->SetProgressionSnapshotForRun({}); }
            ARogueCharacter* P = World->SpawnActor<ARogueCharacter>();
            PC->Possess(P);
            P->SetActorTickEnabled(false);
            P->GetCharacterMovement()->SetComponentTickEnabled(false);
            P->GetAbilitySystemComponent()->CancelAllAbilities();
            return P;
        }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueDayClearingCountdownTest, "Rogue.Gameplay.BattleStage.Wave20ClearThenThirtySeconds", RogueBattleStageTests::Flags)
bool FRogueDayClearingCountdownTest::RunTest(const FString& Parameters)
{
    using namespace RogueBattleStageTests;
    FFixture F; if (!F.Initialize()) { return false; }
    ARogueEnemy* Enemy = F.World->SpawnActor<ARogueEnemy>();
    Enemy->ActivatePooledEnemy(nullptr, FVector(1000,0,100), FRotator::ZeroRotator);
    Enemy->InitializeEnemy({ERogueEnemyType::Rifleman, 100, 200, 1, 1, false});
    Enemy->SetActorTickEnabled(false);
    F.World->GetSubsystem<URogueEnemyTrackerSubsystem>()->RegisterEnemy(Enemy);
    FRogueBattleStageTestAccess::DayBoundary(F.Mode);
    TestTrue(TEXT("Boundary takes over spawning"), FRogueBattleStageTestAccess::Step(F.Mode, .02f));
    TestEqual(TEXT("Wave 21 cannot start in day"), F.Mode->GetCurrentWave(), 20);
    TestEqual(TEXT("Wait for the living enemy"), FRogueBattleStageTestAccess::View(F.Mode).Phase, ERogueBattleStage::Clearing);
    URogueSpawnSubsystem* Spawn = F.World->GetSubsystem<URogueSpawnSubsystem>();
    TestTrue(TEXT("All spawn entry points suspended"), Spawn->IsSpawningSuspended());
    FRogueRunState Dummy;
    TestFalse(TEXT("Direct spawn cannot bypass intermission"), Spawn->SpawnEnemy(Dummy, nullptr, nullptr));
    Spawn->SpawnBoss(Dummy, nullptr, nullptr);
    TestFalse(TEXT("Direct boss spawn cannot bypass intermission"), Dummy.bBossAlive);
    Enemy->DeactivateToPool();
    FRogueBattleStageTestAccess::Step(F.Mode, .02f);
    const FRogueBattleStageView View = FRogueBattleStageTestAccess::View(F.Mode);
    TestEqual(TEXT("Countdown starts only after clearing"), View.Phase, ERogueBattleStage::Countdown);
    TestTrue(TEXT("Countdown is exactly thirty server seconds"), FMath::IsNearlyEqual(View.CountdownDeadline - F.World->GetTimeSeconds(), 30., .001));
    const float Time = F.Mode->GetRunTimeSeconds();
    for (int32 Index = 0; Index < 20; ++Index) { F.TestWorld.TickTestWorld(.05f); FRogueBattleStageTestAccess::Step(F.Mode,.05f); }
    TestEqual(TEXT("Wave time freezes during the rest"), F.Mode->GetRunTimeSeconds(), Time);
    TestEqual(TEXT("No premature travel after one second"), FRogueBattleStageTestAccess::View(F.Mode).Phase, ERogueBattleStage::Countdown);
    TestTrue(TEXT("World keeps running for UI and revivals"), F.World->GetTimeSeconds() > View.CountdownDeadline - 30.);
    FRogueBattleStageTestAccess::DayBoundary(F.Mode, 7);
    FRogueBattleStageTestAccess::Step(F.Mode, .02f);
    TestEqual(TEXT("Configured day-ending wave replaces the default 20"), F.Mode->GetCurrentWave(), 7);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueNightAttackSpeedTest, "Rogue.Gameplay.BattleStage.AttackSpeedForAllTypesAndPoolReuse", RogueBattleStageTests::Flags)
bool FRogueNightAttackSpeedTest::RunTest(const FString& Parameters)
{
    using namespace RogueBattleStageTests;
    FFixture F; if (!F.Initialize()) { return false; }
    ARogueEnemy* Enemy = F.World->SpawnActor<ARogueEnemy>();
    Enemy->SetActorTickEnabled(false);
    for (int32 Type = 0; Type <= static_cast<int32>(ERogueEnemyType::StarLantern); ++Type)
    {
        for (bool Boss : {false, true})
        {
            FRogueEnemyProfile Profile; Profile.EnemyType = static_cast<ERogueEnemyType>(Type);
            Profile.bIsBoss = Boss; Profile.Health = 100; Profile.Speed = 200; Profile.Damage = 1;
            FRogueBattleStageTestAccess::Speed(F.Mode, 1);
            Enemy->InitializeEnemy(Profile);
            const float Contact = Enemy->GetContactInterval();
            const FRogueEnemyRangedArchetype Ranged = Enemy->GetRangedArchetype();
            FRogueBattleStageTestAccess::Speed(F.Mode, 2);
            for (int32 Reuse = 0; Reuse < 3; ++Reuse)
            {
                Enemy->InitializeEnemy(Profile);
                Enemy->ResetRangedAttackCooldown();
                TestEqual(TEXT("Contact attack interval halved"), Enemy->GetContactInterval(), Contact / 2);
                TestEqual(TEXT("Actual ranged timer halved without compounding"), Enemy->GetRangedAttackCooldownRemaining(), Ranged.NextShotCooldown / 2);
                TestEqual(TEXT("First shot minimum halved"), Enemy->GetRangedArchetype().InitialCooldownMin, Ranged.InitialCooldownMin / 2);
                TestEqual(TEXT("First shot maximum halved"), Enemy->GetRangedArchetype().InitialCooldownMax, Ranged.InitialCooldownMax / 2);
                TestEqual(TEXT("Projectile speed preserved"), Enemy->GetRangedArchetype().ProjectileSpeed, Ranged.ProjectileSpeed);
            }
        }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueStagePlayerGrowthTest, "Rogue.Gameplay.BattleStage.TwoDistinctPlayerSnapshotsAndFreshRestart", RogueBattleStageTests::Flags)
bool FRogueStagePlayerGrowthTest::RunTest(const FString& Parameters)
{
    using namespace RogueBattleStageTests;
    FFixture F; if (!F.Initialize()) { return false; }
    for (int32 Value : {70, 130})
    {
        ARogueCharacter* OldPawn = F.Player();
        ARoguePlayerState* Old = OldPawn->GetPlayerState<ARoguePlayerState>();
        FRogueBattleStageTestAccess::Progress(OldPawn, Value);
        UAbilitySystemComponent* ASC = OldPawn->GetAbilitySystemComponent();
        ASC->SetNumericAttributeBase(URogueAttributeSet::GetMaxHealthAttribute(), 250);
        ASC->SetNumericAttributeBase(URogueAttributeSet::GetHealthAttribute(), 81);
        ASC->SetNumericAttributeBase(URogueAttributeSet::GetRocketCountBonusAttribute(), Value / 10.f);
        ASC->SetNumericAttributeBase(URogueAttributeSet::GetProjectileBurstBonusAttribute(), 2);
        Old->QueueUpgradeSelections(2);
        Old->PrepareStageTravel();
        ARoguePlayerController* NewPC = F.World->SpawnActor<ARoguePlayerController>();
        ARoguePlayerState* New = NewPC->GetPlayerState<ARoguePlayerState>();
        Old->CopyProperties(New);
        ARogueCharacter* NewPawn = F.Player(NewPC, false);
        TestTrue(TEXT("Frozen progression ready without a second handshake"), New->IsProgressionReady());
        TestEqual(TEXT("Own money preserved"), NewPawn->GetMoney(), Value);
        TestEqual(TEXT("Own level preserved"), NewPawn->GetPlayerLevel(), Value / 10);
        TestEqual(TEXT("Experience preserved"), NewPawn->GetCurrentExperience(), 3);
        TestEqual(TEXT("Unspent choices preserved"), New->GetQueuedUpgradeSelections(), 2);
        TestEqual(TEXT("Injured health preserved without accidental refill"), New->GetAttributeSet()->GetHealth(), 81.f);
        TestEqual(TEXT("Maximum health preserved"), New->GetAttributeSet()->GetMaxHealth(), 250.f);
        TestEqual(TEXT("Own weapon count preserved"), New->GetAttributeSet()->GetRocketCountBonus(), Value / 10.f);
        TestEqual(TEXT("Exclusive card preserved"), New->GetAttributeSet()->GetProjectileBurstBonus(), 2.f);
        NewPawn->NotifyProgressionReady();
        TestEqual(TEXT("Second initialization is idempotent"), New->GetAttributeSet()->GetRocketCountBonus(), Value / 10.f);
        ARoguePlayerController* RestartPC = F.World->SpawnActor<ARoguePlayerController>();
        New->CopyProperties(RestartPC->GetPlayerState<ARoguePlayerState>());
        ARogueCharacter* Restart = F.Player(RestartPC);
        TestEqual(TEXT("Restart does not carry prior run money"), Restart->GetMoney(), 0);
        TestEqual(TEXT("Restart does not carry prior run weapons"), Restart->GetRogueAttributeSet()->GetRocketCountBonus(), 0.f);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueStageRunTransferTest, "Rogue.Gameplay.BattleStage.RunContinuationConsumedOnce", RogueBattleStageTests::Flags)
bool FRogueStageRunTransferTest::RunTest(const FString& Parameters)
{
    UGameInstance* GI = NewObject<UGameInstance>();
    URogueRunTravelSubsystem* Transfer = NewObject<URogueRunTravelSubsystem>(GI);
    FRogueRunState In; In.CurrentWave = 36; In.EnemiesDefeated = 456; In.RunTimeSeconds = 875;
    FRogueRunState Out;
    Transfer->Prepare(TEXT("/Game/Map/Battle_Night"), In);
    TestTrue(TEXT("Destination receives continuation"), Transfer->Consume(TEXT("/Game/Map/Battle_Night"), Out));
    TestEqual(TEXT("Wave continues at 36"), Out.CurrentWave, 36);
    TestEqual(TEXT("Kills preserved"), Out.EnemiesDefeated, 456);
    TestFalse(TEXT("Cannot consume twice"), Transfer->Consume(TEXT("/Game/Map/Battle_Night"), Out));
    Transfer->Prepare(TEXT("/Game/Map/Battle_Night"), In);
    TestFalse(TEXT("A new day run cannot inherit continuation"), Transfer->Consume(TEXT("/Game/Map/Battle_Day"), Out));
    TestFalse(TEXT("Wrong destination invalidates stale transfer"), Transfer->Consume(TEXT("/Game/Map/Battle_Night"), Out));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueSecondDayBoundaryTest,"Rogue.Gameplay.BattleStage.SecondDayAndConfiguration",RogueBattleStageTests::Flags)
bool FRogueSecondDayBoundaryTest::RunTest(const FString& Parameters)
{
    RogueBattleStageTests::FFixture F;if(!F.Initialize())return false;
    FRogueBattleStageTestAccess::SecondDayBoundary(F.Mode);
    FRogueBattleStageTestAccess::Step(F.Mode,.1f);
    TestEqual(TEXT("Second daytime stops at60"),F.Mode->GetCurrentWave(),60);
    TestEqual(TEXT("Empty field begins countdown"),FRogueBattleStageTestAccess::View(F.Mode).Phase,ERogueBattleStage::Countdown);
    TestTrue(TEXT("Third transition goes to night"),FRogueBattleStageTestAccess::View(F.Mode).bNextStageIsNight);
    FRogueBattleStageSettings Settings;
    for (int32 Index=0;Index<4;++Index)
    {
        TestEqual(TEXT("Default transitions recur every twenty waves"),Settings.GetTransitionEndWave(Index),(Index+1)*20);
    }
    TestEqual(TEXT("Victory replaces the fifth transition"),Settings.GetTransitionEndWave(4),MAX_int32);
    TestEqual(TEXT("Invalid negative index cannot trigger a transition"),Settings.GetTransitionEndWave(-1),MAX_int32);
    TestEqual(TEXT("Large indices do not wrap to an early transition"),Settings.GetTransitionEndWave(MAX_int32),MAX_int32);
    Settings.WavesPerStage=10;Settings.NormalizeTransitionWaves();
    TestEqual(TEXT("One configurable interval determines every boundary"),Settings.GetTransitionEndWave(7),80);
    Settings.WavesPerStage=0;Settings.NormalizeTransitionWaves();
    TestEqual(TEXT("Nonpositive interval is clamped"),Settings.WavesPerStage,1);
    Settings.WavesPerStage=1000;Settings.NormalizeTransitionWaves();
    TestEqual(TEXT("Oversized interval remains below victory"),Settings.WavesPerStage,99);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRoguePeriodicFourBoundaryTest,"Rogue.Gameplay.BattleStage.FourPeriodicClearAndCountdownBoundaries",RogueBattleStageTests::Flags)
bool FRoguePeriodicFourBoundaryTest::RunTest(const FString& Parameters)
{
    RogueBattleStageTests::FFixture F;if(!F.Initialize())return false;
    for (int32 Index=0;Index<4;++Index)
    {
        const int32 Wave=(Index+1)*20;
        FRogueBattleStageTestAccess::DayBoundary(F.Mode,Wave);
        FRogueBattleStageTestAccess::SetPeriodicStage(F.Mode,Index);
        FRogueBattleStageTestAccess::Step(F.Mode,.01f);
        const auto View=FRogueBattleStageTestAccess::View(F.Mode);
        TestEqual(TEXT("Boundary preserves completed wave"),F.Mode->GetCurrentWave(),Wave);
        TestEqual(TEXT("Every cleared boundary starts countdown"),View.Phase,ERogueBattleStage::Countdown);
        TestEqual(TEXT("Destination alternates night and day"),View.bNextStageIsNight,Index%2==0);
        TestTrue(TEXT("All four boundaries retain thirty seconds"),FMath::IsNearlyEqual(View.CountdownDeadline-F.World->GetTimeSeconds(),30.,.001));
    }
    return true;
}

static FAutoConsoleCommandWithWorld RogueTestStageCycle(TEXT("Rogue.Test.StageCycle"),
    TEXT("Verify all four real multiplayer map transitions and victory with QA slots."),
    FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
    {
        if(!World||!World->GetAuthGameMode<ARogueGameMode>())return;
        // Freeze the QA scene while a separate client starts; waiting must not kill the host.
        if(auto* Spawns=World->GetSubsystem<URogueSpawnSubsystem>())Spawns->SetSpawningSuspended(true);
        for(TActorIterator<ARogueEnemy> It(World);It;++It)It->DeactivateToPool();
        TWeakObjectPtr<UGameInstance> Instance=World->GetGameInstance();
        TSharedRef<int32> Phase=MakeShared<int32>(0);
        TSharedRef<double> Start=MakeShared<double>(FPlatformTime::Seconds());
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Instance,Phase,Start](float)
        {
            UWorld* W=Instance.IsValid()?Instance->GetWorld():nullptr;auto* M=W?W->GetAuthGameMode<ARogueGameMode>():nullptr;
            if(!M)return true;
            if(FPlatformTime::Seconds()-*Start>180){UE_LOG(LogTemp,Error,TEXT("[StageCycleTest] FAIL timeout phase%d"),*Phase);return false;}
            TArray<ARogueCharacter*> Players;
            for(FConstPlayerControllerIterator It=W->GetPlayerControllerIterator();It;++It)
            {
                auto* P=It->Get()?Cast<ARogueCharacter>(It->Get()->GetPawn()):nullptr;
                auto* S=P?P->GetPlayerState<ARoguePlayerState>():nullptr;
                if(S&&S->IsBaseAttributesInitialized())Players.Add(P);
            }
            if(Players.Num()!=2)return true;
            if(*Phase==0)
            {
                int32 Money=70;for(auto* P:Players){FRogueBattleStageTestAccess::Progress(P,Money);Money+=60;}
                for(TActorIterator<ARogueEnemy> It(W);It;++It)It->DeactivateToPool();
                FRogueBattleStageTestAccess::CycleBoundary(M,0);*Phase=1;return true;
            }
            const int32 ExpectedWave=*Phase*20+1;
            const float ExpectedSpeed=1.f;
            if(M->GetCurrentWave()!=ExpectedWave||M->GetEnemyAttackSpeedMultiplier()!=ExpectedSpeed)return true;
            int32 Sum=0;bool Valid=true;
            for(auto* P:Players){Sum+=P->GetMoney();Valid&=P->GetPlayerLevel()==P->GetMoney()/10;}
            Valid&=Sum==200;
            UE_LOG(LogTemp,Display,TEXT("[StageCycleTest] %s phase=%d Players=%d Wave=%d Speed=%.1f MoneySum=%d Map=%s"),Valid?TEXT("PASS"):TEXT("FAIL"),*Phase,Players.Num(),M->GetCurrentWave(),M->GetEnemyAttackSpeedMultiplier(),Sum,*W->GetOutermost()->GetName());
            if(!Valid)return false;
            if(*Phase==4){FRogueBattleStageTestAccess::Win(M);return false;}
            for(TActorIterator<ARogueEnemy> It(W);It;++It)It->DeactivateToPool();
            FRogueBattleStageTestAccess::CycleBoundary(M,*Phase);++*Phase;return true;
        }),.1f);
    }));

// 开发配置下的真实双进程回归入口；默认不执行，也不写永久进度。
static FAutoConsoleCommandWithWorld RogueTestStageTravel(TEXT("Rogue.Test.StageTransition"),
    TEXT("Run a two-process day/night transition test once both players are initialized."),
    FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
    {
        if (!World || !World->GetAuthGameMode<ARogueGameMode>()) { return; }
        TWeakObjectPtr<UGameInstance> Instance = World->GetGameInstance();
        TSharedRef<int32> Phase = MakeShared<int32>(0);
        TSharedRef<double> Start = MakeShared<double>(FPlatformTime::Seconds());
        FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Instance, Phase, Start](float)
        {
            UWorld* Current = Instance.IsValid() ? Instance->GetWorld() : nullptr;
            if (!Current) { return false; }
            ARogueGameMode* Mode = Current->GetAuthGameMode<ARogueGameMode>();
            if (!Mode) { return true; }
            if (FPlatformTime::Seconds() - *Start > 160)
            { UE_LOG(LogTemp, Error, TEXT("[StageTravelTest] FAIL timeout phase=%d"), *Phase); return false; }
            TArray<ARogueCharacter*> Players;
            for (FConstPlayerControllerIterator It = Current->GetPlayerControllerIterator(); It; ++It)
            {
                ARogueCharacter* P = It->Get() ? Cast<ARogueCharacter>(It->Get()->GetPawn()) : nullptr;
                ARoguePlayerState* S = P ? P->GetPlayerState<ARoguePlayerState>() : nullptr;
                if (S && S->IsBaseAttributesInitialized()) { Players.Add(P); }
            }
            if (*Phase == 0 && Players.Num() == 2)
            {
                int32 Value = 70;
                for (ARogueCharacter* P : Players)
                {
                    FRogueBattleStageTestAccess::Progress(P, Value);
                    P->GetAbilitySystemComponent()->SetNumericAttributeBase(URogueAttributeSet::GetRocketCountBonusAttribute(), Value / 10.f);
                    Value += 60;
                }
                FRogueBattleStageTestAccess::DayBoundary(Mode);
                *Phase = 1;
                UE_LOG(LogTemp, Log, TEXT("[StageTravelTest] START two initialized players, real 30 second countdown."));
            }
            else if (*Phase == 1 && Mode->GetCurrentWave() == 21 && Mode->GetEnemyAttackSpeedMultiplier() == 1 && Players.Num() == 2)
            {
                bool bValid = Mode->GetCurrentWave() == 21;
                int32 Sum = 0;
                for (ARogueCharacter* P : Players)
                {
                    const int32 Money = P->GetMoney(); Sum += Money;
                    bValid &= (Money == 70 || Money == 130) && P->GetPlayerLevel() == Money / 10
                        && P->GetRogueAttributeSet()->GetRocketCountBonus() == Money / 10.f;
                }
                bValid &= Sum == 200;
                UE_LOG(LogTemp, Display, TEXT("[StageTravelTest] %s Players=%d Wave=%d AttackSpeed=%.1f MoneySum=%d"),
                    bValid ? TEXT("PASS") : TEXT("FAIL"), Players.Num(), Mode->GetCurrentWave(), Mode->GetEnemyAttackSpeedMultiplier(), Sum);
                return false;
            }
            return true;
        }), .1f);
    }));

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNightDensityTest,"Rogue.Gameplay.BattleStage.NightDoublesBudgetAndRateWithoutCompounding",RogueBattleStageTests::Flags)
bool FNightDensityTest::RunTest(const FString&)
{
    using namespace RogueBattleStageTests;FFixture F;if(!F.Initialize())return false;
    auto* B=URogueGameBalanceAsset::GetDefaultGameBalance();
    for(int W:{1,10,36,50,51,85,86,99})
    {
        TestEqual(TEXT("Night budget exactly doubles same-wave baseline"),RogueGameModeRules::BuildWaveEnemyCount(W,B,2),2*RogueGameModeRules::BuildWaveEnemyCount(W,B));
        auto Day=RogueGameModeRules::BuildSpawnCadence(W,.8f,B,1);auto Night=RogueGameModeRules::BuildSpawnCadence(W,.8f,B,2);
        TestEqual(TEXT("Same batch at half interval"),Night.NextSpawnDelay,Day.NextSpawnDelay*.5f);
        TestEqual(TEXT("No accidental quadrupled throughput"),Night.SpawnBatchSize,Day.SpawnBatchSize);
    }
    auto* Spawn=F.World->GetSubsystem<URogueSpawnSubsystem>();
    FRogueBattleStageTestAccess::ResetBudget(Spawn,36);const int32 Day=FRogueBattleStageTestAccess::Budget(Spawn);
    FRogueBattleStageTestAccess::Density(F.Mode,2);FRogueBattleStageTestAccess::ResetBudget(Spawn,36);
    TestEqual(TEXT("Real spawner uses stage multiplier"),FRogueBattleStageTestAccess::Budget(Spawn),Day*2);
    FRogueBattleStageTestAccess::ResetBudget(Spawn,36);TestEqual(TEXT("Repeated reset does not stack"),FRogueBattleStageTestAccess::Budget(Spawn),Day*2);
    FRogueBattleStageTestAccess::Density(F.Mode,1);FRogueBattleStageTestAccess::ResetBudget(Spawn,36);
    TestEqual(TEXT("Return to day clears multiplier"),FRogueBattleStageTestAccess::Budget(Spawn),Day);
    return true;
}
#endif
