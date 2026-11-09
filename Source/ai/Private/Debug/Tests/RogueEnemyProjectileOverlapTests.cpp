#if WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystemComponent.h"
#include "Enemies/Data/RogueEnemyAssetPaths.h"
#include "AbilitySystem/RogueAttributeSet.h"
#include "Combat/Pooling/RogueCombatPoolSubsystem.h"
#include "Combat/Projectiles/RogueEnemyProjectile.h"
#include "Combat/Projectiles/RogueEnemyBulletSubsystem.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Combat/RogueCombatCollision.h"
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
        auto* HitSphere=Cast<USphereComponent>(Bullet->GetRootComponent());
        TestEqual(TEXT("Saved BP uses dedicated projectile channel"),HitSphere->GetCollisionObjectType(),RogueCombatCollision::EnemyProjectileChannel);
        TestEqual(TEXT("Friend ignores enemy projectile before overlap dispatch"),F.Friend->GetCapsuleComponent()->GetCollisionResponseToChannel(RogueCombatCollision::EnemyProjectileChannel),ECR_Ignore);
        F.Step(.24f); // Still geometrically inside the friendly capsule.
        TArray<AActor*> FriendOverlaps;HitSphere->GetOverlappingActors(FriendOverlaps,ARogueEnemy::StaticClass());
        TestEqual(TEXT("No friendly overlap pairs are maintained"),FriendOverlaps.Num(),0);
        F.Step(.26f);
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


// Paired engine/batch checks exercise the same physics scene rather than a math-only replacement.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueBatchBulletOrderedHitTest,
    "Rogue.Gameplay.EnemyBulletBatch.FastSweepKeepsPlayerAndWallOrdering",RogueEnemyProjectileOverlapTests::Flags)
bool FRogueBatchBulletOrderedHitTest::RunTest(const FString&)
{
    using namespace RogueEnemyProjectileOverlapTests;
    FTestConsoleVariable Mode(TEXT("Rogue.Projectiles.BatchEnemyMovement"));
    for(int Path=0;Path<2;++Path)for(int WallFirst=0;WallFirst<2;++WallFirst)
    {
        Mode.Set(Path?TEXT("1"):TEXT("0"));FFixture F;if(!F.Initialize(*this))return false;
        auto* A=F.World->SpawnActor<AActor>();auto* Wall=NewObject<UBoxComponent>(A);A->SetRootComponent(Wall);
        Wall->SetBoxExtent(FVector(5,100,150));Wall->SetCollisionObjectType(ECC_WorldStatic);
        Wall->SetCollisionResponseToAllChannels(ECR_Block);Wall->SetCollisionEnabled(ECollisionEnabled::QueryOnly);Wall->RegisterComponent();
        A->SetActorLocation(FVector(WallFirst?500:1400,0,150));
        auto* B=F.Projectile(ARogueEnemyProjectile::StaticClass(),FVector(100,0,150));
        B->ActivatePooledProjectile(F.Shooter,F.Shooter,FVector(100,0,150),FRotator::ZeroRotator,FVector::ForwardVector,12000,7,false);
        TestEqual(TEXT("Requested path actually selected"),B->IsUsingBatchedMovement(),Path==1);
        F.TestWorld.TickTestWorld(.15f); // This segment crosses both surfaces within a single frame.
        TestTrue(TEXT("The nearest valid obstacle consumes the bullet"),B->IsAvailableInPool());
        TestEqual(TEXT("Player is hit exactly once only when ahead of wall"),F.Player->GetRogueAttributeSet()->GetHealth(),WallFirst?1000.f:993.f);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueBatchBulletPoolGenerationTest,
    "Rogue.Gameplay.EnemyBulletBatch.SameFramePoolReuseAndDestroyedEntries",RogueEnemyProjectileOverlapTests::Flags)
bool FRogueBatchBulletPoolGenerationTest::RunTest(const FString&)
{
    using namespace RogueEnemyProjectileOverlapTests;FTestConsoleVariable Mode(TEXT("Rogue.Projectiles.BatchEnemyMovement"));Mode.Set(TEXT("1"));
    FFixture F;if(!F.Initialize(*this))return false;F.Player->SetActorEnableCollision(false);
    auto* B=F.Projectile(ARogueEnemyProjectile::StaticClass(),FVector(100,500,150));
    B->ActivatePooledProjectile(F.Shooter,F.Shooter,FVector(100,500,150),FRotator::ZeroRotator,FVector::ForwardVector,500,7,false);
    B->DeactivateToPool();B->DeactivateToPool();
    B->ActivatePooledProjectile(F.Shooter,F.Shooter,FVector(100,500,150),FRotator::ZeroRotator,FVector::ForwardVector,500,7,false);
    auto* Batch=F.World->GetSubsystem<URogueEnemyBulletSubsystem>();
    Batch->Tick(.2f);
    TestTrue(TEXT("Old generation does not move new occupant a second time"),B->GetActorLocation().Equals(FVector(200,500,150),.01f));
    TestEqual(TEXT("Stale generation entries removed"),Batch->GetTrackedCount(),1);
    B->Destroy();Batch->Tick(.1f);
    TestEqual(TEXT("Destroyed actor cannot remain in scheduler"),Batch->GetTrackedCount(),0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueBatchBulletCollisionPolicyTest,
    "Rogue.Gameplay.EnemyBulletBatch.NoCollisionAndOverlapOptOutMatchComponentPath",RogueEnemyProjectileOverlapTests::Flags)
bool FRogueBatchBulletCollisionPolicyTest::RunTest(const FString&)
{
    using namespace RogueEnemyProjectileOverlapTests;FTestConsoleVariable Mode(TEXT("Rogue.Projectiles.BatchEnemyMovement"));
    for(int Path=0;Path<2;++Path)for(int DisableActor=0;DisableActor<2;++DisableActor)
    {
        Mode.Set(Path?TEXT("1"):TEXT("0"));FFixture F;if(!F.Initialize(*this))return false;
        if(DisableActor)F.Player->SetActorEnableCollision(false);else F.Player->GetCapsuleComponent()->SetGenerateOverlapEvents(false);
        auto* B=F.Projectile(ARogueEnemyProjectile::StaticClass(),FVector(100,0,150));
        B->ActivatePooledProjectile(F.Shooter,F.Shooter,FVector(100,0,150),FRotator::ZeroRotator,FVector::ForwardVector,3000,7,false);
        F.Step(.5f);
        TestFalse(TEXT("A player that opted out of collision never consumes bullet"),B->IsAvailableInPool());
        TestEqual(TEXT("A player without active overlap receives no damage"),F.Player->GetRogueAttributeSet()->GetHealth(),1000.f);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueBatchBulletRadiusTest,
    "Rogue.Gameplay.EnemyBulletBatch.ScaledSphereStillHitsOffAxisPlayer",RogueEnemyProjectileOverlapTests::Flags)
bool FRogueBatchBulletRadiusTest::RunTest(const FString&)
{
    using namespace RogueEnemyProjectileOverlapTests;FTestConsoleVariable Mode(TEXT("Rogue.Projectiles.BatchEnemyMovement"));
    for(int Path=0;Path<2;++Path)
    {
        Mode.Set(Path?TEXT("1"):TEXT("0"));FFixture F;if(!F.Initialize(*this))return false;
        auto* B=F.Projectile(ARogueEnemyProjectile::StaticClass(),FVector(100,80,150));
        B->ActivatePooledProjectile(F.Shooter,F.Shooter,FVector(100,80,150),FRotator::ZeroRotator,FVector::ForwardVector,3000,7,false,30,3);
        F.Step(.5f);
        TestTrue(TEXT("Actual scaled collision radius is used by both paths"),B->IsAvailableInPool());
        TestEqual(TEXT("Off-axis capsule hit applies configured damage"),F.Player->GetRogueAttributeSet()->GetHealth(),993.f);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueBatchBulletFallbackTest,
    "Rogue.Gameplay.EnemyBulletBatch.SpecialMotionFallsBackAndReuseCanBatch",RogueEnemyProjectileOverlapTests::Flags)
bool FRogueBatchBulletFallbackTest::RunTest(const FString&)
{
    using namespace RogueEnemyProjectileOverlapTests;FTestConsoleVariable Mode(TEXT("Rogue.Projectiles.BatchEnemyMovement"));Mode.Set(TEXT("1"));
    FFixture F;if(!F.Initialize(*this))return false;
    auto* B=F.Projectile(ARogueEnemyProjectile::StaticClass(),FVector(100,500,150));
    auto* Movement=B->FindComponentByClass<UProjectileMovementComponent>();
    const FVector P(100,500,150);
    B->bAllowBatchedMovement=false;
    B->ActivatePooledProjectile(F.Shooter,F.Shooter,P,FRotator::ZeroRotator,FVector::ForwardVector,500,7,false);
    TestFalse(TEXT("Blueprint may explicitly keep its component simulation"),B->IsUsingBatchedMovement());B->DeactivateToPool();B->bAllowBatchedMovement=true;
    Movement->bSimulationEnabled=false;
    B->ActivatePooledProjectile(F.Shooter,F.Shooter,P,FRotator::ZeroRotator,FVector::ForwardVector,500,7,false);
    TestFalse(TEXT("Disabled simulation is never replaced by moving batch"),B->IsUsingBatchedMovement());B->DeactivateToPool();Movement->bSimulationEnabled=true;
    Movement->ProjectileGravityScale=1;
    B->ActivatePooledProjectile(F.Shooter,F.Shooter,P,FRotator::ZeroRotator,FVector::ForwardVector,500,7,false);
    TestFalse(TEXT("Gravity retains original simulation"),B->IsUsingBatchedMovement());TestTrue(TEXT("Fallback component ticks"),Movement->IsComponentTickEnabled());B->DeactivateToPool();Movement->ProjectileGravityScale=0;
    Movement->bShouldBounce=true;
    B->ActivatePooledProjectile(F.Shooter,F.Shooter,P,FRotator::ZeroRotator,FVector::ForwardVector,500,7,false);
    TestFalse(TEXT("Bounce retains original simulation"),B->IsUsingBatchedMovement());B->DeactivateToPool();Movement->bShouldBounce=false;
    FRogueEnemyProjectileMotion Motion;Motion.WarningSeconds=.8f;
    B->ActivatePooledProjectile(F.Shooter,F.Shooter,P,FRotator::ZeroRotator,FVector::ForwardVector,500,7,false,30,1,false,Motion);
    TestFalse(TEXT("Warning retains original phase timing"),B->IsUsingBatchedMovement());B->DeactivateToPool();
    B->ActivatePooledProjectile(F.Shooter,F.Shooter,P,FRotator::ZeroRotator,FVector::ForwardVector,500,7,false);
    TestTrue(TEXT("Reused ordinary bullet can enter batch"),B->IsUsingBatchedMovement());
    TestFalse(TEXT("Batched bullet has no movement component tick"),Movement->IsComponentTickEnabled());
    TestFalse(TEXT("No individual Actor tick is scheduled"),B->IsActorTickEnabled());
    return true;
}

#endif
