#if WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystemComponent.h"
#include "Enemies/Data/RogueEnemyAssetPaths.h"
#include "AbilitySystem/RogueAttributeSet.h"
#include "Combat/Pooling/RogueCombatPoolSubsystem.h"
#include "Combat/Projectiles/RogueEnemyProjectile.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Enemies/RogueEnemy.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/RogueGameMode.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameFramework/WorldSettings.h"
#include "Misc/AutomationTest.h"
#include "Player/RogueCharacter.h"
#include "Player/RoguePlayerController.h"
#include "Tests/AutomationCommon.h"

namespace RogueEnemyProjectileOverlapTests
{
    constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;
    struct FFixture
    {
        FTestConsoleVariable SelfTest{TEXT("Rogue.SelfTest.WeaponUpgradeChain")};
        FTestConsoleVariable Stress{TEXT("Rogue.SelfTest.EnemyStress")};
        FTestWorldWrapper TestWorld;
        UWorld* World = nullptr;
        ARogueCharacter* Player = nullptr;
        ARogueEnemy* Shooter = nullptr;
        ARogueEnemy* Friend = nullptr;

        ARogueEnemy* Enemy(const TCHAR* Variant, FVector Location)
        {
            const FString Path = RogueEnemyAssetPaths::EnemyClassPath(Variant);
            UClass* Class = LoadClass<ARogueEnemy>(nullptr, *Path);
            if (!Class) { return nullptr; }
            FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            auto* Result = World->SpawnActor<ARogueEnemy>(Class, Location, FRotator::ZeroRotator, Params);
            if (!Result) { return nullptr; }
            const ERogueEnemyType Type = Result->GetEnemyType();
            Result->ActivatePooledEnemy(nullptr, Location, FRotator::ZeroRotator);
            FRogueEnemyProfile Profile; Profile.EnemyType = Type; Profile.Health = 1000; Profile.Speed = 0; Profile.Damage = 10;
            Result->InitializeEnemy(Profile);
            Result->SetRangedAttackCooldown(1000);
            Result->SetActorTickEnabled(false);
            Result->GetCharacterMovement()->SetComponentTickEnabled(false);
            return Result;
        }

        bool Initialize(FAutomationTestBase& Test)
        {
            SelfTest.Set(TEXT("0")); Stress.Set(TEXT("0"));
            if (!TestWorld.CreateTestWorld(EWorldType::Game)) { return false; }
            World = TestWorld.GetTestWorld(); World->GetWorldSettings()->DefaultGameMode = ARogueGameMode::StaticClass();
            if (!TestWorld.BeginPlayInTestWorld()) { return false; }
            World->GetAuthGameMode()->SetActorTickEnabled(false);
            auto* PC = World->SpawnActor<ARoguePlayerController>();
            const FTransform Transform(FVector(1000, 0, 120));
            Player = World->SpawnActorDeferred<ARogueCharacter>(ARogueCharacter::StaticClass(), Transform,
                nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
            if (!Test.TestNotNull(TEXT("Controller exists"), PC) || !Test.TestNotNull(TEXT("Player exists"), Player)) { return false; }
            if (!Test.TestNull(TEXT("No real local save owner"), PC->GetLocalPlayer())) { return false; }
            Player->AutoPossessPlayer = EAutoReceiveInput::Disabled; Player->FinishSpawning(Transform); PC->Possess(Player);
            Player->SetActorTickEnabled(false); Player->GetCharacterMovement()->SetComponentTickEnabled(false);
            Player->GetAbilitySystemComponent()->CancelAllAbilities();
            auto* Attr = Player->GetRogueAttributeSet();
            if (!Test.TestNotNull(TEXT("Player GAS attributes exist"), Attr)) { return false; }
            Attr->SetMaxHealth(1000); Attr->SetHealth(1000); Attr->SetMaxArmor(0); Attr->SetArmor(0);
            Attr->SetDamageReductionPercent(0); Attr->SetHealthRegenPerSecond(0);
            Player->GetCapsuleComponent()->SetGenerateOverlapEvents(true);
            Player->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
            Shooter = Enemy(TEXT("Rifleman"), FVector(0, 0, 120));
            Friend = Enemy(TEXT("Juggernaut"), FVector(0, 2000, 120));
            return Test.TestNotNull(TEXT("Real rifleman Blueprint exists"), Shooter)
                && Test.TestNotNull(TEXT("Real friendly enemy Blueprint exists"), Friend);
        }

        void Step(float Seconds)
        {
            while (Seconds > KINDA_SMALL_NUMBER)
            {
                const float Delta = FMath::Min(Seconds, .01f); TestWorld.TickTestWorld(Delta); Seconds -= Delta;
            }
        }

        TArray<ARogueEnemyProjectile*> Active() const
        {
            TArray<ARogueEnemyProjectile*> Result;
            for (TActorIterator<ARogueEnemyProjectile> It(World); It; ++It)
            { if (!It->IsAvailableInPool()) { Result.Add(*It); } }
            return Result;
        }

        ARogueEnemyProjectile* Projectile(UClass* Class, FVector Location)
        {
            return World->GetSubsystem<URogueCombatPoolSubsystem>()->AcquireEnemyProjectile(Class, Shooter, Shooter,
                Location, FRotator::ZeroRotator);
        }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueEnemyOverlappingShooterTest,
    "Rogue.Gameplay.EnemyProjectileOverlap.RealShooterInsideFriendlyEnemyStillFires", RogueEnemyProjectileOverlapTests::Flags)
bool FRogueEnemyOverlappingShooterTest::RunTest(const FString& Parameters)
{
    using namespace RogueEnemyProjectileOverlapTests;
    FFixture F; if (!F.Initialize(*this)) { return false; }
    F.Shooter->Tick(.01f); // Establish the real AI target without firing during cooldown.
    F.Shooter->FireProjectileAtPlayer();
    TestEqual(TEXT("An isolated rifleman emits one bullet"), F.Active().Num(), 1);
    for (auto* Bullet : F.Active()) { Bullet->DeactivateToPool(); }
    F.Friend->SetActorLocation(F.Shooter->GetActorLocation(), false, nullptr, ETeleportType::TeleportPhysics);
    const float FriendHealth = F.Friend->GetCurrentHealth();
    F.Shooter->FireProjectileAtPlayer();
    TestEqual(TEXT("The same rifleman emits one bullet while its capsule overlaps a friendly enemy"), F.Active().Num(), 1);
    F.Step(.25f);
    const auto Bullets = F.Active();
    TestEqual(TEXT("The bullet escapes the overlapping capsules and remains active"), Bullets.Num(), 1);
    if (!Bullets.IsEmpty()) { TestTrue(TEXT("The bullet actually moves out of the crowd"), Bullets[0]->GetActorLocation().X > 100); }
    TestEqual(TEXT("A friendly enemy takes no damage"), F.Friend->GetCurrentHealth(), FriendHealth);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueEnemyAllBulletBlueprintOverlapTest,
    "Rogue.Gameplay.EnemyProjectileOverlap.AllBlueprintBulletsPassFriendAndHitPlayerOnce", RogueEnemyProjectileOverlapTests::Flags)
bool FRogueEnemyAllBulletBlueprintOverlapTest::RunTest(const FString& Parameters)
{
    using namespace RogueEnemyProjectileOverlapTests;
    FFixture F; if (!F.Initialize(*this)) { return false; }
    const TCHAR* Suffixes[] = {TEXT(""), TEXT("_Boss"), TEXT("_Cannoneer"), TEXT("_Marksman"), TEXT("_Repeater"),
        TEXT("_Ringcaster"), TEXT("_Scatterer"), TEXT("_ShockPillar"), TEXT("_Shotgunner"), TEXT("_Sniper"), TEXT("_SkyMoth"), TEXT("_WindReaver"), TEXT("_StarLantern"), TEXT("_Rifleman"), TEXT("_Assault"), TEXT("_Juggernaut"), TEXT("_Orbiter"), TEXT("_Skirmisher")};
    F.Friend->SetActorLocation(FVector(350, 0, 120), false, nullptr, ETeleportType::TeleportPhysics);
    for (const auto* Suffix : Suffixes)
    {
        const FString Path = RogueEnemyAssetPaths::ProjectileClassPath(FString(Suffix).RightChop(1));
        UClass* Class = LoadClass<ARogueEnemyProjectile>(nullptr, *Path);
        if (!TestNotNull(*Path, Class)) { continue; }
        auto* Bullet = F.Projectile(Class, FVector(100, 0, 150));
        if (!TestNotNull(TEXT("Blueprint bullet acquired"), Bullet)) { continue; }
        const float PlayerHP = F.Player->GetRogueAttributeSet()->GetHealth();
        const float FriendHP = F.Friend->GetCurrentHealth();
        // Ordinary visuals, including classes also used for rings, must obey the same team rule.
        Bullet->ActivatePooledProjectile(F.Shooter, F.Shooter, FVector(100, 0, 150), FRotator::ZeroRotator,
            FVector::ForwardVector, 900, 7, false);
        F.Step(.5f);
        TestFalse(*FString::Printf(TEXT("%s passes the friendly capsule"), Suffix), Bullet->IsAvailableInPool());
        TestTrue(TEXT("Movement was not stopped by a friend"), Bullet->GetActorLocation().X > 450);
        TestEqual(TEXT("Passing friend is not friendly fire"), F.Friend->GetCurrentHealth(), FriendHP);
        TestEqual(TEXT("The player has not been hit before arrival"), F.Player->GetRogueAttributeSet()->GetHealth(), PlayerHP);
        F.Step(.6f);
        TestTrue(TEXT("A real player hit consumes the bullet"), Bullet->IsAvailableInPool());
        TestEqual(TEXT("Player takes exactly one configured hit"), F.Player->GetRogueAttributeSet()->GetHealth(), PlayerHP - 7);
        F.Step(.1f);
        TestEqual(TEXT("No second damage after pooling"), F.Player->GetRogueAttributeSet()->GetHealth(), PlayerHP - 7);

        // Reuse at a player's existing overlap: activation must preserve the immediate hit/pool outcome.
        Bullet->ActivatePooledProjectile(F.Shooter, F.Shooter, F.Player->GetActorLocation(), FRotator::ZeroRotator,
            FVector::ForwardVector, 900, 3, true);
        TestTrue(TEXT("Immediate player overlap still consumes reused boss shots"), Bullet->IsAvailableInPool());
        TestEqual(TEXT("Reused boss shot applies damage exactly once"), F.Player->GetRogueAttributeSet()->GetHealth(), PlayerHP - 10);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueEnemyBulletWallTest,
    "Rogue.Gameplay.EnemyProjectileOverlap.WallStillStopsOrdinaryBullet", RogueEnemyProjectileOverlapTests::Flags)
bool FRogueEnemyBulletWallTest::RunTest(const FString& Parameters)
{
    using namespace RogueEnemyProjectileOverlapTests;
    FFixture F; if (!F.Initialize(*this)) { return false; }
    auto* WallActor = F.World->SpawnActor<AActor>();
    auto* Wall = NewObject<UBoxComponent>(WallActor); WallActor->SetRootComponent(Wall);
    Wall->SetBoxExtent(FVector(10, 100, 100)); Wall->SetCollisionObjectType(ECC_WorldStatic);
    Wall->SetCollisionResponseToAllChannels(ECR_Block); Wall->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Wall->RegisterComponent(); WallActor->SetActorLocation(FVector(300, 0, 150));
    auto* Bullet = F.Projectile(ARogueEnemyProjectile::StaticClass(), FVector(100, 0, 150));
    if (!TestNotNull(TEXT("Bullet exists"), Bullet)) { return false; }
    const float HP = F.Player->GetRogueAttributeSet()->GetHealth();
    Bullet->ActivatePooledProjectile(F.Shooter, F.Shooter, FVector(100, 0, 150), FRotator::ZeroRotator,
        FVector::ForwardVector, 900, 7, false);
    F.Step(.4f);
    TestTrue(TEXT("Scene wall consumes the bullet"), Bullet->IsAvailableInPool());
    TestEqual(TEXT("The wall protects the player behind it"), F.Player->GetRogueAttributeSet()->GetHealth(), HP);
    return true;
}

#endif
