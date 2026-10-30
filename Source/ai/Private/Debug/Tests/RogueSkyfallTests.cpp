#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/RogueAttributeSet.h"
#include "Combat/Projectiles/RogueEnemySkyfallProjectile.h"
#include "Combat/Projectiles/RogueEnemyProjectile.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Enemies/RogueEnemy.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/RogueGameMode.h"
#include "Framework/RogueGameState.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/WorldSettings.h"
#include "Player/RogueCharacter.h"
#include "Player/RoguePlayerController.h"

namespace RogueSkyfallTests
{
    constexpr auto Flags=EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter;
    struct FFixture
    {
        FTestWorldWrapper Test;
        UWorld* World=nullptr;
        ARogueEnemy* Source=nullptr;
        ARogueCharacter* Player=nullptr;
        AActor* Floor=nullptr;
        bool Init()
        {
            if (!Test.CreateTestWorld(EWorldType::Game)) return false;
            World=Test.GetTestWorld(); World->GetWorldSettings()->DefaultGameMode=ARogueGameMode::StaticClass();
            if (!Test.BeginPlayInTestWorld()) return false;
            World->GetAuthGameMode()->SetActorTickEnabled(false);
            Floor=World->SpawnActor<AActor>();
            auto* Box=NewObject<UBoxComponent>(Floor); Floor->SetRootComponent(Box);
            Box->SetBoxExtent(FVector(5000,5000,25)); Box->SetCollisionObjectType(ECC_WorldStatic);
            Box->SetCollisionResponseToAllChannels(ECR_Block); Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
            Box->RegisterComponent(); Floor->SetActorLocation(FVector(0,0,-25));
            auto* PC=World->SpawnActor<ARoguePlayerController>();
            FActorSpawnParameters P; P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            Player=World->SpawnActor<ARogueCharacter>(ARogueCharacter::StaticClass(),FVector(800,0,96),FRotator::ZeroRotator,P);
            PC->Possess(Player); Player->SetActorTickEnabled(false);
            Player->GetCharacterMovement()->SetComponentTickEnabled(false); Player->GetAbilitySystemComponent()->CancelAllAbilities();
            auto* A=Player->GetRogueAttributeSet(); A->SetMaxHealth(1000); A->SetHealth(1000);
            A->SetArmor(0); A->SetMaxArmor(0); A->SetDamageReductionPercent(0); A->SetHealthRegenPerSecond(0);
            UClass* C=LoadClass<ARogueEnemy>(nullptr,TEXT("/Game/BP/Enemy/Marksman/BP_EnemyMarksman.BP_EnemyMarksman_C"));
            if (!C) return false;
            Source=World->SpawnActor<ARogueEnemy>(C,FVector(0,0,72),FRotator::ZeroRotator,P);
            Source->ActivatePooledEnemy(nullptr,Source->GetActorLocation(),FRotator::ZeroRotator);
            FRogueEnemyProfile Profile; Profile.EnemyType=ERogueEnemyType::Marksman; Profile.Health=1000; Profile.Speed=0; Profile.Damage=10;
            Source->InitializeEnemy(Profile); Source->SetRangedAttackCooldown(1000);
            Source->GetCharacterMovement()->SetComponentTickEnabled(false); Source->Tick(.01f); Source->SetActorTickEnabled(false);
            return true;
        }
        ARogueEnemySkyfallProjectile* Fire()
        {
            Source->SetRangedAttackCooldown(1000); Source->Tick(.001f);
            Source->ExecuteRangedAttack();
            for (TActorIterator<ARogueEnemySkyfallProjectile> It(World);It;++It) if (!It->HasImpacted()) return *It;
            return nullptr;
        }
        void Step(float Seconds)
        {
            while (Seconds>KINDA_SMALL_NUMBER) { const float Dt=FMath::Min(.01f,Seconds); Test.TickTestWorld(Dt); Seconds-=Dt; }
        }
        int32 BulletCount() const
        {
            int32 N=0;for (TActorIterator<ARogueEnemyProjectile> It(World);It;++It) if (!It->IsAvailableInPool()) ++N;
            return N;
        }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueSkyfallWarningTest,"Rogue.Gameplay.Skyfall.LockedWarningLastsUntilGroundImpact",RogueSkyfallTests::Flags)
bool FRogueSkyfallWarningTest::RunTest(const FString&)
{
    RogueSkyfallTests::FFixture F;if (!F.Init()) return false;
    const auto S=F.Source->GetCurrentArchetype().Ranged.Skyfall.Sanitized();
    TestEqual(TEXT("Actual Marksman row selects new attack"),F.Source->GetCurrentArchetype().Ranged.ShotPattern,ERogueEnemyShotPattern::Skyfall);
    auto* Shot=F.Fire(); if (!TestNotNull(TEXT("GAS attack creates actual skyfall BP"),Shot)) return false;
    TestTrue(TEXT("Ground warning appears immediately"),Shot->IsWarningVisible());
    const FVector Ground=Shot->GetLockedGroundLocation(),Launch=Shot->GetActorLocation();
    TestTrue(TEXT("Ground uses physical floor rather than actor height"),FMath::IsNearlyZero(Ground.Z,1.));
    F.Player->SetActorLocation(FVector(1800,400,96));
    F.Step(S.WarningSeconds*.5f);
    TestTrue(TEXT("Large projectile hovers during summon warning"),Shot->GetActorLocation().Equals(Launch,.1));
    TestTrue(TEXT("Player movement does not drag locked warning"),Shot->GetLockedGroundLocation().Equals(Ground,.1));
    F.Step(S.WarningSeconds*.5f+S.FallSeconds*.6f);
    TestTrue(TEXT("Red circle remains throughout descent"),Shot->IsWarningVisible());
    TestFalse(TEXT("No premature impact"),Shot->HasImpacted());
    TestEqual(TEXT("No ordinary bullets before real ground contact"),F.BulletCount(),0);
    F.Step(S.FallSeconds*.5f);
    TestTrue(TEXT("Actual swept ground contact resolves"),Shot->HasImpacted());
    TestFalse(TEXT("Warning hides at contact"),Shot->IsWarningVisible());
    TestEqual(TEXT("Exactly one full normal bullet ring"),F.BulletCount(),S.Burst.ProjectileCount);
    for (TActorIterator<ARogueEnemyProjectile> It(F.World);It;++It)
        if (!It->IsAvailableInPool()) TestEqual(TEXT("Normal burst uses configured collision scale"),It->GetVisualScale(),S.Burst.ProjectileScale);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueSkyfallDamageTest,"Rogue.Gameplay.Skyfall.ImpactDamageOnceAndNoFriendlyFire",RogueSkyfallTests::Flags)
bool FRogueSkyfallDamageTest::RunTest(const FString&)
{
    RogueSkyfallTests::FFixture F;if (!F.Init()) return false;
    auto* Shot=F.Fire();if (!Shot) return false;
    const auto S=F.Source->GetCurrentArchetype().Ranged.Skyfall.Sanitized();
    const float HP=F.Player->GetRogueAttributeSet()->GetHealth(),EnemyHP=F.Source->GetCurrentHealth();
    F.Step(S.WarningSeconds+.1f);
    TestEqual(TEXT("Warning/airborne large orb does not damage early"),F.Player->GetRogueAttributeSet()->GetHealth(),HP);
    F.Step(S.FallSeconds);
    TestTrue(TEXT("Floor impact"),Shot->HasImpacted());
    TestEqual(TEXT("Center player takes configured ground hit once"),F.Player->GetRogueAttributeSet()->GetHealth(),HP-10*S.ImpactDamageMultiplier);
    F.Step(.15f);
    TestEqual(TEXT("No repeat ground damage"),F.Player->GetRogueAttributeSet()->GetHealth(),HP-10*S.ImpactDamageMultiplier);
    TestEqual(TEXT("Enemy is never damaged by its spell"),F.Source->GetCurrentHealth(),EnemyHP);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueSkyfallCancelTest,"Rogue.Gameplay.Skyfall.SourcePoolReuseAndVictoryCancelWarning",RogueSkyfallTests::Flags)
bool FRogueSkyfallCancelTest::RunTest(const FString&)
{
    RogueSkyfallTests::FFixture F;if (!F.Init()) return false;
    auto* Shot=F.Fire();if (!Shot) return false;
    F.Source->DeactivateToPool(); F.Source->ActivatePooledEnemy(nullptr,FVector(0,0,72),FRotator::ZeroRotator);
    FRogueEnemyProfile P;P.EnemyType=ERogueEnemyType::Marksman;P.Health=1000;P.Speed=0;F.Source->InitializeEnemy(P);
    F.Source->SetActorTickEnabled(false); F.Step(.05f);
    TestFalse(TEXT("Old attack invalidated even when same actor was immediately reused"),IsValid(Shot)&&!Shot->IsActorBeingDestroyed());
    TestEqual(TEXT("Canceled attack produces no burst"),F.BulletCount(),0);
    Shot=F.Fire();if (!Shot) return false;
    F.World->GetGameState<ARogueGameState>()->SetRunStateSnapshot(100,0,0,true,false,true);F.Step(.05f);
    TestFalse(TEXT("Terminal run removes pending warning"),IsValid(Shot)&&!Shot->IsActorBeingDestroyed());
    TestEqual(TEXT("Victory cancellation creates no bullets"),F.BulletCount(),0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueSkyfallRealContactTest,"Rogue.Gameplay.Skyfall.NoTimerBasedExplosionWithoutGround",RogueSkyfallTests::Flags)
bool FRogueSkyfallRealContactTest::RunTest(const FString&)
{
    RogueSkyfallTests::FFixture F;if (!F.Init()) return false;
    auto* Shot=F.Fire();if (!Shot) return false;
    F.Floor->Destroy();
    const auto S=F.Source->GetCurrentArchetype().Ranged.Skyfall.Sanitized();
    F.Step(S.WarningSeconds+S.FallSeconds+.25f);
    TestFalse(TEXT("Elapsed animation alone cannot count as ground contact"),Shot->HasImpacted());
    TestTrue(TEXT("Warning remains until actual contact or attack cancellation"),Shot->IsWarningVisible());
    TestEqual(TEXT("No false ring when floor removed"),F.BulletCount(),0);
    return true;
}
#endif
