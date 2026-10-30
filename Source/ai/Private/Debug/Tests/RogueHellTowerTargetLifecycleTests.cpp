#if WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystemComponent.h"
#include "AbilitySystem/RogueAttributeSet.h"
#include "Combat/Pooling/RogueCombatPoolSubsystem.h"
#include "Combat/Targeting/RogueEnemyTrackerSubsystem.h"
#include "Combat/Weapons/RogueWeapon_HellTower.h"
#include "Enemies/RogueEnemy.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/RogueGameMode.h"
#include "Framework/RogueGameModeRules.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/WorldSettings.h"
#include "Misc/AutomationTest.h"
#include "Player/RogueCharacter.h"
#include "Player/RoguePlayerController.h"
#include "Tests/AutomationCommon.h"

namespace RogueHellTowerTargetLifecycleTests
{
    constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;

    struct FFixture
    {
        FTestConsoleVariable SelfTest{TEXT("Rogue.SelfTest.WeaponUpgradeChain")};
        FTestConsoleVariable Stress{TEXT("Rogue.SelfTest.EnemyStress")};
        FTestWorldWrapper TestWorld;
        UWorld* World = nullptr;
        ARogueCharacter* Player = nullptr;
        ARogueWeapon_HellTower* Weapon = nullptr;
        URogueCombatPoolSubsystem* Pools = nullptr;
        URogueEnemyTrackerSubsystem* Tracker = nullptr;
        FRogueWeaponTableRow Config;

        bool Initialize(FAutomationTestBase& Test)
        {
            SelfTest.Set(TEXT("0"));
            Stress.Set(TEXT("0"));
            if (!TestWorld.CreateTestWorld(EWorldType::Game)) { return false; }
            World = TestWorld.GetTestWorld();
            World->GetWorldSettings()->DefaultGameMode = ARogueGameMode::StaticClass();
            if (!TestWorld.BeginPlayInTestWorld()) { return false; }
            World->GetAuthGameMode()->SetActorTickEnabled(false);
            auto* PC = World->SpawnActor<ARoguePlayerController>();
            const FTransform Transform(FVector(0, 0, 120));
            Player = World->SpawnActorDeferred<ARogueCharacter>(ARogueCharacter::StaticClass(), Transform,
                nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
            if (!Test.TestNotNull(TEXT("Player and controller exist"), Player)
                || !Test.TestNotNull(TEXT("Controller exists"), PC)
                || !Test.TestNull(TEXT("Fixture cannot write a real local player's save"), PC->GetLocalPlayer())) { return false; }
            Player->AutoPossessPlayer = EAutoReceiveInput::Disabled;
            Player->FinishSpawning(Transform);
            PC->Possess(Player);
            Player->SetActorTickEnabled(false);
            Player->GetCharacterMovement()->SetComponentTickEnabled(false);
            Player->SetActorEnableCollision(false);
            auto* ASC = Player->GetAbilitySystemComponent();
            auto* Attributes = Player->GetRogueAttributeSet();
            if (!Test.TestNotNull(TEXT("Player has real GAS"), ASC)
                || !Test.TestNotNull(TEXT("Player attributes exist"), Attributes)) { return false; }
            ASC->CancelAllAbilities();
            Attributes->SetWeaponBonusCaps(FRogueWeaponBonusCaps());
            ASC->SetNumericAttributeBase(URogueAttributeSet::GetAttackPowerBonusAttribute(), 0);
            ASC->SetNumericAttributeBase(URogueAttributeSet::GetAttackSpeedBonusAttribute(), 0);
            ASC->SetNumericAttributeBase(URogueAttributeSet::GetHellTowerDamageRampBonusAttribute(), 0);
            ASC->SetNumericAttributeBase(URogueAttributeSet::GetHellTowerCountBonusAttribute(), 0);
            Player->RefreshWeaponBonusSnapshotFromAttributes();
            Pools = World->GetSubsystem<URogueCombatPoolSubsystem>();
            Tracker = World->GetSubsystem<URogueEnemyTrackerSubsystem>();
            // The production pool rotates across all free actors. Occupy its prewarmed native slots
            // so that this controlled high-load case really reacquires the just-killed actor.
            for (TActorIterator<ARogueEnemy> It(World); It; ++It)
            {
                if (It->GetClass() != ARogueEnemy::StaticClass() || !It->IsAvailableInPool()) { continue; }
                It->ActivatePooledEnemy(nullptr, FVector(30000, 30000, 4000), FRotator::ZeroRotator);
                FRogueEnemyProfile Busy; Busy.Health = 35; Busy.Speed = 0; Busy.Damage = 0;
                It->InitializeEnemy(Busy);
                It->SetActorTickEnabled(false);
                It->SetActorEnableCollision(false);
                It->GetCharacterMovement()->SetComponentTickEnabled(false);
            }
            Weapon = World->SpawnActor<ARogueWeapon_HellTower>();
            if (!Test.TestNotNull(TEXT("Combat pool exists"), Pools)
                || !Test.TestNotNull(TEXT("Real enemy tracker exists"), Tracker)
                || !Test.TestNotNull(TEXT("Real HellTower exists"), Weapon)) { return false; }
            // Current shipped damage/ramp values, with one unlocked tower and a controlled scheduling quantum.
            Config.Count = 1;
            Config.BaseDamage = 1;
            Config.DamageRampPerTick = .1f;
            Config.DamageTickInterval = .1f;
            Config.Range = 2000;
            Config.UpdateInterval = .05f;
            Config.HeavyCombatUpdateInterval = .05f;
            Weapon->InitializeWeapon(Player, Config);
            return true;
        }

        ARogueEnemy* Acquire(const FRogueEnemyProfile& Profile, FVector Location = FVector(700, 0, 120))
        {
            auto* Enemy = Pools->AcquireEnemy(ARogueEnemy::StaticClass(), nullptr, Location, FRotator::ZeroRotator);
            if (!Enemy) { return nullptr; }
            Enemy->ActivatePooledEnemy(nullptr, Location, FRotator::ZeroRotator);
            Enemy->InitializeEnemy(Profile);
            Enemy->SetActorTickEnabled(false);
            Enemy->GetCharacterMovement()->SetComponentTickEnabled(false);
            Enemy->SetActorEnableCollision(false);
            Tracker->RegisterEnemy(Enemy);
            return Enemy;
        }

        void SetTowerBonus(float Count)
        {
            Player->GetAbilitySystemComponent()->SetNumericAttributeBase(URogueAttributeSet::GetHellTowerCountBonusAttribute(), Count);
            Player->RefreshWeaponBonusSnapshotFromAttributes();
        }

        float Hit(ARogueEnemy* Enemy, float Delta)
        {
            const float Before = Enemy->GetCurrentHealth();
            Weapon->WeaponTick(Delta);
            return Before - Enemy->GetCurrentHealth();
        }

        void ProcessDeath()
        {
            // Advance the actual next-tick death handler, never the weapon. Pooling/reacquisition
            // therefore occurs between the tower's two samples, exactly the vulnerable ordering.
            TestWorld.TickTestWorld(.001f);
        }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueHellTowerRepeatedPoolBossTest,
    "Rogue.Gameplay.HellTowerTargetLifecycle.RepeatedNormalDeathsThenUpgradedBoss", RogueHellTowerTargetLifecycleTests::Flags)
bool FRogueHellTowerRepeatedPoolBossTest::RunTest(const FString& Parameters)
{
    using namespace RogueHellTowerTargetLifecycleTests;
    FFixture F;
    if (!F.Initialize(*this)) { return false; }
    TestTrue(TEXT("Real attack upgrade GE applies"), F.Player->ApplyUpgradeEffectViaGAS(ERogueUpgradeType::AttackPower, 8));
    TestTrue(TEXT("Real ramp upgrade GE applies"), F.Player->ApplyUpgradeEffectViaGAS(ERogueUpgradeType::HellTowerDamageRamp, .1f));
    const float Base = F.Weapon->GetEffectiveBaseDamage();
    const auto Normal = RogueGameModeRules::BuildEnemyProfile(1, ERogueEnemyType::Rifleman);
    TestTrue(TEXT("Warm-up uses ordinary HP rather than an artificial immortal enemy"), Normal.Health > Base && Normal.Health < 100);
    ARogueEnemy* SameActor = nullptr;
    FGuid PreviousCycle;
    float LargestFirstHit = 0;
    for (int32 Life = 0; Life < 100; ++Life)
    {
        auto* Enemy = F.Acquire(Normal);
        if (!TestNotNull(TEXT("Normal enemy acquired"), Enemy)) { return false; }
        if (SameActor && !TestTrue(TEXT("Pool reuses the exact actor"), Enemy == SameActor)) { return false; }
        TestTrue(TEXT("Each acquisition has a fresh spawn identity"), Enemy->GetSpawnCycleId() != PreviousCycle);
        SameActor = Enemy;
        PreviousCycle = Enemy->GetSpawnCycleId();
        LargestFirstHit = FMath::Max(LargestFirstHit, F.Hit(Enemy, .1f));
        for (int32 Tick = 0; Enemy->GetCurrentHealth() > 0 && Tick < 100; ++Tick) { F.Hit(Enemy, .1f); }
        if (!TestTrue(TEXT("Real beam GAS damage kills each ordinary enemy"), Enemy->GetCurrentHealth() <= 0)) { return false; }
        F.ProcessDeath();
        if (!TestTrue(TEXT("Real death processing returns each enemy to its pool"), Enemy->IsAvailableInPool())) { return false; }
    }
    TestTrue(TEXT("Every normal life begins at base damage; no ramp accumulates across 100 pool cycles"),
        FMath::IsNearlyEqual(LargestFirstHit, Base, .001f));

    const auto Boss = RogueGameModeRules::BuildBossProfile(1, ERogueEnemyType::Rifleman);
    auto* Enemy = F.Acquire(Boss);
    if (!TestNotNull(TEXT("Boss acquired"), Enemy)) { return false; }
    TestTrue(TEXT("The same ordinary-enemy actor becomes the boss"), Enemy == SameActor && Enemy->IsBoss());
    TestTrue(TEXT("The boss has another spawn identity"), Enemy->GetSpawnCycleId() != PreviousCycle);
    TestTrue(TEXT("Same-frame second attack upgrade applies through GAS"), F.Player->ApplyUpgradeEffectViaGAS(ERogueUpgradeType::AttackPower, 8));
    const float UpgradedBase = F.Weapon->GetEffectiveBaseDamage();
    const float Ramp = F.Weapon->GetEffectiveDamageRampPerTick();
    TestTrue(TEXT("Boss starts from the newly upgraded base, not the ordinary enemy's old ramp"),
        FMath::IsNearlyEqual(F.Hit(Enemy, .1f), UpgradedBase, .001f));
    for (int32 Tick = 1; Tick < 10; ++Tick) { F.Hit(Enemy, .1f); }
    const float Expected = 10 * UpgradedBase + 45 * Ramp;
    TestTrue(TEXT("First second is exactly a fresh ten-tick arithmetic damage progression"),
        FMath::IsNearlyEqual(Enemy->GetCurrentHealth(), Boss.Health - Expected, .01f));
    TestTrue(TEXT("A normally configured first-wave boss survives the first second"), Enemy->GetCurrentHealth() > 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueHellTowerPoolTimerResetTest,
    "Rogue.Gameplay.HellTowerTargetLifecycle.ReusedActorResetsPartialDamageTimer", RogueHellTowerTargetLifecycleTests::Flags)
bool FRogueHellTowerPoolTimerResetTest::RunTest(const FString& Parameters)
{
    using namespace RogueHellTowerTargetLifecycleTests;
    FFixture F;
    if (!F.Initialize(*this)) { return false; }
    auto Profile = RogueGameModeRules::BuildEnemyProfile(1, ERogueEnemyType::Rifleman);
    auto* Enemy = F.Acquire(Profile);
    if (!TestNotNull(TEXT("Enemy exists"), Enemy)) { return false; }
    TestEqual(TEXT("First half-interval has no damage"), F.Hit(Enemy, .05f), 0.f);
    const auto OldId = Enemy->GetSpawnCycleId();
    Enemy->DeactivateToPool();
    auto* Reused = F.Acquire(RogueGameModeRules::BuildBossProfile(1, ERogueEnemyType::Rifleman));
    if (!TestTrue(TEXT("Real pool returns the same actor with a new cycle"), Reused == Enemy && Reused->GetSpawnCycleId() != OldId)) { return false; }
    TestEqual(TEXT("A new lifecycle must wait its own full interval, without the previous half-interval"), F.Hit(Reused, .05f), 0.f);
    TestTrue(TEXT("The next half-interval delivers only the fresh base damage"),
        FMath::IsNearlyEqual(F.Hit(Reused, .05f), F.Weapon->GetEffectiveBaseDamage(), .001f));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueHellTowerSlotLifecycleTest,
    "Rogue.Gameplay.HellTowerTargetLifecycle.CountResizeAndOwnerDeath", RogueHellTowerTargetLifecycleTests::Flags)
bool FRogueHellTowerSlotLifecycleTest::RunTest(const FString& Parameters)
{
    using namespace RogueHellTowerTargetLifecycleTests;
    FFixture F;
    if (!F.Initialize(*this)) { return false; }
    F.Config.Count = 0;
    F.Weapon->InitializeWeapon(F.Player, F.Config);
    F.SetTowerBonus(2);
    const auto Profile = RogueGameModeRules::BuildEnemyProfile(1, ERogueEnemyType::Rifleman);
    auto* First = F.Acquire(Profile, FVector(500, 0, 120));
    auto* Second = F.Acquire(Profile, FVector(900, 0, 120));
    if (!TestNotNull(TEXT("First target exists"), First) || !TestNotNull(TEXT("Second target exists"), Second)) { return false; }
    F.Weapon->WeaponTick(.1f);
    F.Weapon->WeaponTick(.1f);
    F.SetTowerBonus(1);
    F.Weapon->WeaponTick(.05f);
    F.SetTowerBonus(2);
    TestEqual(TEXT("Restored slot starts a new timer"), F.Hit(Second, .05f), 0.f);
    TestTrue(TEXT("Restored slot starts with base damage"), FMath::IsNearlyEqual(F.Hit(Second, .05f), 1.f, .001f));
    F.SetTowerBonus(0);
    F.Weapon->WeaponTick(.05f);
    F.SetTowerBonus(1);
    TestEqual(TEXT("Unlocking after count zero has no stale half-tick"), F.Hit(First, .05f), 0.f);
    TestTrue(TEXT("Unlocking after count zero restarts ramp"), FMath::IsNearlyEqual(F.Hit(First, .05f), 1.f, .001f));
    F.Weapon->WeaponTick(.05f); // Leave both a partial timer and a nonzero ramp behind.
    F.Weapon->OnOwnerDied();
    TestEqual(TEXT("Owner death clears the previous target timer"), F.Hit(First, .05f), 0.f);
    TestTrue(TEXT("Owner death clears the previous target ramp"), FMath::IsNearlyEqual(F.Hit(First, .05f), 1.f, .001f));
    return true;
}

#endif
