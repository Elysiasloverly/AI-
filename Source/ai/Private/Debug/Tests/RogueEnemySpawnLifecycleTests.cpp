#if WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystemComponent.h"
#include "Enemies/Data/RogueEnemyAssetPaths.h"
#include "AbilitySystem/RogueAbilitySystemLibrary.h"
#include "AbilitySystem/RogueAttributeSet.h"
#include "AbilitySystem/RogueGameplayTags.h"
#include "Combat/Projectiles/RogueProjectile.h"
#include "Components/CapsuleComponent.h"
#include "Enemies/RogueEnemy.h"
#include "Engine/World.h"
#include "Framework/RogueGameMode.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/WorldSettings.h"
#include "Misc/AutomationTest.h"
#include "Player/RogueCharacter.h"
#include "Player/RoguePlayerController.h"
#include "Player/RoguePlayerState.h"
#include "Progression/Meta/RogueUpgradeTreeDefinition.h"
#include "Tests/AutomationCommon.h"

namespace RogueEnemySpawnLifecycleTests
{
    constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;

    struct FFixture
    {
        FTestConsoleVariable SelfTest{TEXT("Rogue.SelfTest.WeaponUpgradeChain")};
        FTestConsoleVariable Stress{TEXT("Rogue.SelfTest.EnemyStress")};
        FTestWorldWrapper TestWorld;
        UWorld* World = nullptr;
        ARogueGameMode* Mode = nullptr;
        ARogueCharacter* Player = nullptr;
        ARoguePlayerState* State = nullptr;

        bool Initialize(FAutomationTestBase& Test)
        {
            SelfTest.Set(TEXT("0"));
            Stress.Set(TEXT("0"));
            if (!TestWorld.CreateTestWorld(EWorldType::Game)) { return false; }
            World = TestWorld.GetTestWorld();
            World->GetWorldSettings()->DefaultGameMode = ARogueGameMode::StaticClass();
            if (!TestWorld.BeginPlayInTestWorld()) { return false; }
            Mode = World->GetAuthGameMode<ARogueGameMode>();
            if (!Test.TestNotNull(TEXT("Authoritative mode exists"), Mode)) { return false; }
            Mode->SetActorTickEnabled(false); // Keep unrelated spawning and weapon updates out of the regression.
            auto* PC = World->SpawnActor<ARoguePlayerController>();
            if (!Test.TestNotNull(TEXT("Controller exists"), PC)
                || !Test.TestNull(TEXT("Fixture has no local progression save owner"), PC->GetLocalPlayer())) { return false; }
            State = PC->GetPlayerState<ARoguePlayerState>();
            if (!Test.TestNotNull(TEXT("Player state exists"), State)) { return false; }
            FRogueProgressionSnapshot Snapshot;
            for (const auto& Node : URogueUpgradeTreeDefinition::GetDefaultDefinition()->GetNodes())
            {
                if (Node.Kind != ERogueMetaNodeKind::BaseStat) { Snapshot.NodeRanks.Add(Node.NodeId, 1); }
            }
            State->SetProgressionSnapshotForRun(Snapshot);
            const FTransform Transform(FVector(0, 0, 4000));
            Player = World->SpawnActorDeferred<ARogueCharacter>(ARogueCharacter::StaticClass(), Transform,
                nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
            if (!Test.TestNotNull(TEXT("Player pawn exists"), Player)) { return false; }
            Player->AutoPossessPlayer = EAutoReceiveInput::Disabled;
            Player->FinishSpawning(Transform);
            PC->Possess(Player);
            Player->SetActorTickEnabled(false);
            Player->GetCharacterMovement()->SetComponentTickEnabled(false);
            Player->SetActorEnableCollision(false);
            Player->GetAbilitySystemComponent()->CancelAllAbilities();

            Player->AddExperience(Player->GetExperienceToNextLevel());
            return Test.TestTrue(TEXT("A real XP level-up leaves the player choosing an upgrade"),
                State->IsAwaitingUpgradeChoice());
        }

        void Step(float Seconds = .04f)
        {
            while (Seconds > KINDA_SMALL_NUMBER)
            {
                const float Delta = FMath::Min(Seconds, .01f);
                TestWorld.TickTestWorld(Delta);
                Seconds -= Delta;
            }
        }

        ARogueEnemy* SpawnInactive(FAutomationTestBase& Test, UClass* Class)
        {
            FActorSpawnParameters Params;
            Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            auto* Enemy = World->SpawnActor<ARogueEnemy>(Class, FVector(10000, 0, 4000), FRotator::ZeroRotator, Params);
            if (!Test.TestNotNull(TEXT("Enemy spawns"), Enemy)) { return nullptr; }
            Enemy->GetCharacterMovement()->SetComponentTickEnabled(false);
            if (!Test.TestNotNull(TEXT("Enemy GAS attribute set exists"), Enemy->GetRogueAttributeSet())) { return nullptr; }
            Test.TestTrue(TEXT("BeginPlay leaves the fresh enemy available in its pool"), Enemy->IsAvailableInPool());
            return Enemy;
        }

        FRogueEnemyProfile Profile(const ARogueEnemy* Enemy, float Health, bool bBoss) const
        {
            FRogueEnemyProfile Result;
            Result.EnemyType = Enemy->GetEnemyType();
            Result.Health = Health;
            Result.Speed = 0;
            Result.Damage = 0;
            Result.ExperienceReward = 0;
            Result.bIsBoss = bBoss;
            return Result;
        }

        bool KillNormallyIntoPool(FAutomationTestBase& Test, ARogueEnemy* Enemy, const FVector& Location)
        {
            Enemy->ActivatePooledEnemy(nullptr, Location, FRotator::ZeroRotator);
            Enemy->InitializeEnemy(Profile(Enemy, 40, false));
            Enemy->SetActorTickEnabled(false);
            if (!Test.TestTrue(TEXT("Previous life receives a real lethal hit"),
                RogueAbilitySystem::ApplyDamageToTarget(Player, Enemy, 41, RogueGameplayTags::Damage_Source_Projectile))) { return false; }
            Step();
            return Test.TestTrue(TEXT("The previous life finishes its deferred death and returns normally"),
                Enemy->IsDead() && Enemy->IsAvailableInPool());
        }

        void Trace(FAutomationTestBase& Test, const FString& Label, const TCHAR* Stage, ARogueEnemy* Enemy) const
        {
            auto* ASC = Enemy->GetAbilitySystemComponent();
            Test.AddInfo(FString::Printf(TEXT("%s %s: mirror=%.2f GAS=%.2f/%.2f deadTag=%d dead=%d pooled=%d spawn=%s"),
                *Label, Stage, Enemy->GetCurrentHealth(),
                ASC->GetNumericAttribute(URogueAttributeSet::GetHealthAttribute()),
                ASC->GetNumericAttribute(URogueAttributeSet::GetMaxHealthAttribute()),
                ASC->HasMatchingGameplayTag(RogueGameplayTags::State_Dead),
                Enemy->IsDead(), Enemy->IsAvailableInPool(), *Enemy->GetSpawnCycleId().ToString()));
        }

        bool CheckLivingBoss(FAutomationTestBase& Test, const FString& Label, ARogueEnemy* Enemy, float ExpectedHealth)
        {
            auto* ASC = Enemy->GetAbilitySystemComponent();
            bool bResult = Test.TestFalse(*FString::Printf(TEXT("%s has no death tag"), *Label),
                ASC->HasMatchingGameplayTag(RogueGameplayTags::State_Dead));
            bResult &= Test.TestFalse(*FString::Printf(TEXT("%s is not dead"), *Label), Enemy->IsDead());
            bResult &= Test.TestFalse(*FString::Printf(TEXT("%s remains active"), *Label), Enemy->IsAvailableInPool());
            bResult &= Test.TestEqual(*FString::Printf(TEXT("%s retains its expected GAS health"), *Label),
                ASC->GetNumericAttribute(URogueAttributeSet::GetHealthAttribute()), ExpectedHealth);
            bResult &= Test.TestEqual(*FString::Printf(TEXT("%s mirror matches GAS"), *Label),
                Enemy->GetCurrentHealth(), ExpectedHealth);
            return bResult;
        }

        void CheckOneNormalHit(FAutomationTestBase& Test, const FString& Label, ARogueEnemy* Enemy, float InitialHealth)
        {
            if (Enemy->IsDead() || Enemy->IsAvailableInPool()) { return; } // Avoid hiding the first failure in damage cascades.
            Test.TestTrue(*FString::Printf(TEXT("%s accepts a later normal one-point hit"), *Label),
                RogueAbilitySystem::ApplyDamageToTarget(Player, Enemy, 1, RogueGameplayTags::Damage_Source_Projectile));
            Step();
            CheckLivingBoss(Test, Label + TEXT(" after normal hit"), Enemy, InitialHealth - 1);
        }

        void CheckSpawnOnExistingBullet(FAutomationTestBase& Test, UClass* Class, bool bReuse, const FVector& Location)
        {
            const FString Label = FString::Printf(TEXT("%s %s"), *Class->GetName(), bReuse ? TEXT("normal reuse") : TEXT("first spawn"));
            auto* Enemy = SpawnInactive(Test, Class);
            if (!Enemy) { return; }
            if (bReuse && !KillNormallyIntoPool(Test, Enemy, Location + FVector(0, 2000, 0))) { return; }
            Test.TestEqual(*FString::Printf(TEXT("%s pooled GAS health starts at zero"), *Label),
                Enemy->GetAbilitySystemComponent()->GetNumericAttribute(URogueAttributeSet::GetHealthAttribute()), 0.f);
            Test.TestTrue(*FString::Printf(TEXT("%s uses its actual overlapping capsule"), *Label),
                Enemy->GetCapsuleComponent()->GetGenerateOverlapEvents());

            // This is a live player projectile already occupying the chosen spawn point. No private handlers,
            // manual overlap broadcasts, artificial death tags, or early pool return are used in this test.
            FActorSpawnParameters Params;
            Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            auto* Bullet = World->SpawnActor<ARogueProjectile>(ARogueProjectile::StaticClass(), Location, FRotator::ZeroRotator, Params);
            if (!Test.TestNotNull(TEXT("Native player bullet exists"), Bullet)) { return; }
            Bullet->ActivatePooledProjectile(Player, Player, Location, FRotator::ZeroRotator, FVector::ForwardVector, 0, 1);
            Step(.02f); // Commit the live projectile to the physics query scene before the enemy appears.
            Test.TestFalse(TEXT("The bullet is active before enemy activation"), Bullet->IsAvailableInPool());
            Test.TestTrue(TEXT("Upgrade choice is still open when the boss is spawned"), State->IsAwaitingUpgradeChoice());

            const FRogueEnemyProfile BossProfile = Profile(Enemy, 5000, true);
            // The production spawn subsystem calls these two public lifecycle functions in precisely this order.
            Enemy->ActivatePooledEnemy(nullptr, Location, FRotator::ZeroRotator);
            Trace(Test, Label, TEXT("after Activate, before Initialize"), Enemy);
            Enemy->InitializeEnemy(BossProfile);
            Enemy->SetActorTickEnabled(false);
            Trace(Test, Label, TEXT("immediately after Initialize"), Enemy);
            CheckLivingBoss(Test, Label + TEXT(" after Initialize"), Enemy, BossProfile.Health);
            Bullet->DeactivateToPool(); // All later damage is explicit and independently accounted for.
            Step();
            Trace(Test, Label, TEXT("after timer ticks"), Enemy);
            CheckLivingBoss(Test, Label + TEXT(" after timer ticks"), Enemy, BossProfile.Health);
            CheckOneNormalHit(Test, Label, Enemy, BossProfile.Health);
            Enemy->DeactivateToPool();
        }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueEnemyNativeSpawnOverlapLifecycleTest,
    "Rogue.Gameplay.EnemySpawnLifecycle.NativeBirthOverlapWhileChoosingUpgrade",
    RogueEnemySpawnLifecycleTests::Flags)
bool FRogueEnemyNativeSpawnOverlapLifecycleTest::RunTest(const FString& Parameters)
{
    using namespace RogueEnemySpawnLifecycleTests;
    FFixture F;
    if (!F.Initialize(*this)) { return false; }
    F.CheckSpawnOnExistingBullet(*this, ARogueEnemy::StaticClass(), false, FVector(10000, 0, 4000));
    F.CheckSpawnOnExistingBullet(*this, ARogueEnemy::StaticClass(), true, FVector(10000, 500, 4000));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueEnemyBlueprintSpawnOverlapLifecycleTest,
    "Rogue.Gameplay.EnemySpawnLifecycle.BlueprintBirthOverlapWhileChoosingUpgrade",
    RogueEnemySpawnLifecycleTests::Flags)
bool FRogueEnemyBlueprintSpawnOverlapLifecycleTest::RunTest(const FString& Parameters)
{
    using namespace RogueEnemySpawnLifecycleTests;
    FFixture F;
    if (!F.Initialize(*this)) { return false; }
    const TCHAR* Variants[] = {TEXT("Rifleman"), TEXT("Assault"), TEXT("Juggernaut"), TEXT("Orbiter"),
        TEXT("Skirmisher"), TEXT("Sniper"), TEXT("Shotgunner"), TEXT("Repeater"), TEXT("Ringcaster"),
        TEXT("Marksman"), TEXT("Cannoneer"), TEXT("Scatterer"), TEXT("ShockPillar")};
    int32 Case = 0;
    for (const auto* Variant : Variants)
    {
        const FString Path = RogueEnemyAssetPaths::EnemyClassPath(Variant);
        UClass* Class = LoadClass<ARogueEnemy>(nullptr, *Path);
        if (!TestNotNull(*Path, Class)) { continue; }
        for (bool bReuse : {false, true})
        {
            F.CheckSpawnOnExistingBullet(*this, Class, bReuse, FVector(10000, 500 * Case++, 4000));
        }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueEnemyStaleDeferredDeathLifecycleTest,
    "Rogue.Gameplay.EnemySpawnLifecycle.DefenseOnlyForcedEarlyReturnInvalidatesOldDeath",
    RogueEnemySpawnLifecycleTests::Flags)
bool FRogueEnemyStaleDeferredDeathLifecycleTest::RunTest(const FString& Parameters)
{
    using namespace RogueEnemySpawnLifecycleTests;
    FFixture F;
    if (!F.Initialize(*this)) { return false; }
    auto* Enemy = F.SpawnInactive(*this, ARogueEnemy::StaticClass());
    if (!Enemy) { return false; }
    const FVector Location(10000, 0, 4000);
    Enemy->ActivatePooledEnemy(nullptr, Location, FRotator::ZeroRotator);
    Enemy->InitializeEnemy(F.Profile(Enemy, 40, false));
    Enemy->SetActorTickEnabled(false);
    const FGuid OldSpawn = Enemy->GetSpawnCycleId();
    TestTrue(TEXT("A real lethal hit queues the previous life's death"),
        RogueAbilitySystem::ApplyDamageToTarget(F.Player, Enemy, 41, RogueGameplayTags::Damage_Source_Projectile));
    TestTrue(TEXT("Previous life's death tag exists before timer execution"),
        Enemy->GetAbilitySystemComponent()->HasMatchingGameplayTag(RogueGameplayTags::State_Dead));

    // Defense in depth: unlike the birth-overlap regressions above, this deliberately returns the actor
    // before its pending ProcessDeath runs. No ordinary native pool path was found to require that ordering.
    Enemy->DeactivateToPool();
    Enemy->ActivatePooledEnemy(nullptr, Location, FRotator::ZeroRotator);
    const FRogueEnemyProfile NewProfile = F.Profile(Enemy, 5000, true);
    Enemy->InitializeEnemy(NewProfile);
    Enemy->SetActorTickEnabled(false);
    TestTrue(TEXT("Reactivation replaces the actor's spawn identity"), Enemy->GetSpawnCycleId() != OldSpawn);
    F.CheckLivingBoss(*this, TEXT("New spawn before old timer"), Enemy, NewProfile.Health);
    F.Step();
    F.Trace(*this, TEXT("Forced early return"), TEXT("after old timer deadline"), Enemy);
    F.CheckLivingBoss(*this, TEXT("New spawn after old timer"), Enemy, NewProfile.Health);
    F.CheckOneNormalHit(*this, TEXT("New spawn after invalidating old timer"), Enemy, NewProfile.Health);
    return true;
}

#endif
