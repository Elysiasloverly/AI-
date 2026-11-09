#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Enemies/Data/RogueEnemyAssetPaths.h"
#include "Tests/AutomationCommon.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Components/SphereComponent.h"
#include "AbilitySystemComponent.h"
#include "Framework/RogueGameMode.h"
#include "Framework/RogueGameModeRules.h"
#include "Framework/RogueGameBalanceAsset.h"
#include "Enemies/RogueEnemy.h"
#include "Enemies/Data/RogueEnemyDataTable.h"
#include "Player/RogueCharacter.h"
#include "Player/RoguePlayerController.h"
#include "Player/RoguePlayerState.h"
#include "Combat/Projectiles/RogueEnemyProjectile.h"
#include "Combat/Projectiles/RogueProjectile.h"
#include "Combat/Targeting/RogueEnemyTrackerSubsystem.h"
#include "Combat/Weapons/RogueWeapon_Projectile.h"
#include "Engine/DataTable.h"

namespace RogueFlyingTests
{
    constexpr auto Flags=EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter;
    const ERogueEnemyType Types[]={ERogueEnemyType::SkyMoth,ERogueEnemyType::WindReaver,ERogueEnemyType::StarLantern};
    const TCHAR* Names[]={TEXT("SkyMoth"),TEXT("WindReaver"),TEXT("StarLantern")};
    struct FFixture
    {
        FTestWorldWrapper Test;
        UWorld* World=nullptr;
        ARogueCharacter* Player=nullptr;
        bool Init()
        {
            if (!Test.CreateTestWorld(EWorldType::Game)) { return false; }
            World=Test.GetTestWorld(); World->GetWorldSettings()->DefaultGameMode=ARogueGameMode::StaticClass();
            if (!Test.BeginPlayInTestWorld()) { return false; }
            World->GetAuthGameMode()->SetActorTickEnabled(false);
            auto* PC=World->SpawnActor<ARoguePlayerController>();
            PC->GetPlayerState<ARoguePlayerState>()->SetProgressionSnapshotForRun(FRogueProgressionSnapshot());
            FActorSpawnParameters P; P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            Player=World->SpawnActor<ARogueCharacter>(ARogueCharacter::StaticClass(),FVector(800,0,96),FRotator::ZeroRotator,P);
            PC->Possess(Player); Player->SetActorTickEnabled(false); Player->GetCharacterMovement()->SetComponentTickEnabled(false);
            Player->GetAbilitySystemComponent()->CancelAllAbilities(); return Player->IsAlive();
        }
        ARogueEnemy* Enemy(int32 Variant,FVector Position=FVector(0,0,72))
        {
            FString Path=RogueEnemyAssetPaths::EnemyClassPath(Names[Variant]);
            UClass* Class=LoadClass<ARogueEnemy>(nullptr,*Path);
            if (!Class) { return nullptr; }
            FActorSpawnParameters P; P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            auto* Enemy=World->SpawnActor<ARogueEnemy>(Class,Position,FRotator::ZeroRotator,P);
            Enemy->ActivatePooledEnemy(nullptr,Position,FRotator::ZeroRotator);
            FRogueEnemyProfile Profile; Profile.EnemyType=Types[Variant]; Profile.Health=200; Profile.Speed=0; Profile.Damage=10;
            Enemy->InitializeEnemy(Profile);
            // 本夹具显式调用 ExecuteRangedAttack 验证一轮弹幕；暂停自动发起，
            // 避免随机初始冷却在手动 Tick 推进齐射时再启动下一轮。
            Enemy->SetRangedAttackCooldown(60.0f);
            Enemy->Tick(0.01f); return Enemy;
        }
        void Step(float Seconds)
        { for (int32 I=0;I<FMath::CeilToInt(Seconds/0.01f);++I) { Test.TickTestWorld(0.01f); } }
        TArray<ARogueEnemyProjectile*> Bullets() const
        {
            TArray<ARogueEnemyProjectile*> R;
            for (TActorIterator<ARogueEnemyProjectile> It(World);It;++It) if (!It->IsAvailableInPool()) { R.Add(*It); }
            return R;
        }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFlyingFormalConfigTest,"Rogue.Gameplay.FlyingEnemies.AssetsWavesAndDistinctPatterns",RogueFlyingTests::Flags)
bool FFlyingFormalConfigTest::RunTest(const FString&)
{
    using namespace RogueFlyingTests;
    auto* Table=LoadObject<UDataTable>(nullptr,TEXT("/Game/Data/Enemies/DT_EnemyArchetypes.DT_EnemyArchetypes"));
    const auto* Balance=URogueGameBalanceAsset::GetDefaultGameBalance();
    if (!TestNotNull(TEXT("Enemy table"),Table)||!TestNotNull(TEXT("Balance"),Balance)) { return false; }
    auto* ModeClass=LoadClass<ARogueGameMode>(nullptr,TEXT("/Game/BP/Framework/BP_RogueGameMode.BP_RogueGameMode_C"));
    if (!TestNotNull(TEXT("Actual GameMode BP"),ModeClass)) { return false; }
    auto* Mode=ModeClass->GetDefaultObject<ARogueGameMode>();
    const ERogueEnemyShotPattern Patterns[]={ERogueEnemyShotPattern::CrossWeave,ERogueEnemyShotPattern::ReturningFan,ERogueEnemyShotPattern::StarTrap};
    const int32 Waves[]={5,9,14};
    for (int32 I=0;I<3;++I)
    {
        const auto A=RogueEnemyArchetypes::BuildEnemyArchetypeFromDataTable(Table,Types[I],false);
        TestEqual(TEXT("Distinct attack pattern"),A.Ranged.ShotPattern,Patterns[I]);
        TestTrue(TEXT("Projectile blueprint configured"),A.ProjectileClass!=nullptr);
        TestTrue(TEXT("Mapped in actual GameMode"),Mode->HasEnemyClassForType(Types[I]));
        TestTrue(TEXT("Gradual wave entry"),Balance->EnemyWaveEntries.ContainsByPredicate([&](const auto& E){return E.EnemyType==Types[I]&&E.MinWave==Waves[I];}));
        TestTrue(TEXT("Per-type spawn quotas configured"),RogueEnemyArchetypes::GetMaxAlive(Table,Types[I])>0);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFlyingHeightTest,"Rogue.Gameplay.FlyingEnemies.HoverPoolAndColdBulletHit",RogueFlyingTests::Flags)
bool FFlyingHeightTest::RunTest(const FString&)
{
    using namespace RogueFlyingTests; FFixture F; if (!F.Init()) { return false; }
    const auto* Table = LoadObject<UDataTable>(nullptr, TEXT("/Game/Data/Weapons/DT_WeaponConfig.DT_WeaponConfig"));
    if (!TestNotNull(TEXT("Actual weapon table loads"), Table)) { return false; }
    const auto* Row = Table->FindRow<FRogueWeaponTableRow>(TEXT("Projectile"), TEXT("FlyingEnemyTest"));
    if (!TestNotNull(TEXT("The cold projectile row exists"), Row) ||
        !TestNotNull(TEXT("The real projectile Blueprint is configured"), Row->ProjectileClass.Get())) { return false; }
    FRogueWeaponTableRow Config = *Row;
    Config.Count = 1;
    Config.bEnableTrajectoryDeviation = false;
    Config.bEnableSpeedRandomization = false;
    auto* Weapon = F.World->SpawnActor<ARogueWeapon_Projectile>();
    Weapon->InitializeWeapon(F.Player, Config);
    auto* Tracker = F.World->GetSubsystem<URogueEnemyTrackerSubsystem>();

    for (int32 I=0;I<3;++I)
    {
        auto* E=F.Enemy(I,FVector(1400,0,72)); if (!TestNotNull(TEXT("Actual flying BP"),E)) { return false; }
        F.Step(0.6f);
        TestEqual(TEXT("Flying movement"),E->GetCharacterMovement()->MovementMode.GetValue(),MOVE_Flying);
        TestEqual(TEXT("No gravity"),E->GetCharacterMovement()->GravityScale,0.f);
        const auto Flight=E->GetCurrentArchetype().Flying.Sanitized();
        TestTrue(TEXT("Height stays in configured hover band"),FMath::Abs(E->GetActorLocation().Z-72-Flight.HoverHeight)<=Flight.BobAmplitude+1);
        E->DeactivateToPool(); E->ActivatePooledEnemy(nullptr,FVector(1400,0,372),FRotator::ZeroRotator);
        FRogueEnemyProfile P; P.EnemyType=Types[I]; P.Health=200; E->InitializeEnemy(P);
        TestEqual(TEXT("Reactivation resets altitude instead of accumulating"),E->GetActorLocation().Z,372.0+Flight.HoverHeight);
        E->SetActorTickEnabled(false);
        Tracker->RegisterEnemy(E);
        TArray<ARogueEnemy*> Targets;
        Weapon->CollectFireTargets(Targets);
        if (!TestTrue(TEXT("The cold weapon actually locks the elevated target"), Targets.Contains(E))) { return false; }
        Weapon->FireAtTargets(Targets);
        F.Step(1.f);
        TestTrue(TEXT("The actual cold weapon damages a flying enemy far above the muzzle"), E->GetCurrentHealth() < P.Health);
        E->DeactivateToPool();
    }

    const FVector PlayerLocation = F.Player->GetActorLocation();
    auto* Overhead = F.Enemy(0, PlayerLocation + FVector(0, 0, 10));
    if (!TestNotNull(TEXT("An overhead flyer exists"), Overhead)) { return false; }
    Overhead->SetActorTickEnabled(false);
    Tracker->RegisterEnemy(Overhead);
    Weapon->FireAtTargets({Overhead});
    F.Step(.15f);
    TestTrue(TEXT("A shot at an enemy directly overhead still hits"), Overhead->GetCurrentHealth() < Overhead->GetMaxHealth());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMothAttackTest,"Rogue.Gameplay.FlyingEnemies.WeaveSequenceAndPoolCancellation",RogueFlyingTests::Flags)
bool FMothAttackTest::RunTest(const FString&)
{
    using namespace RogueFlyingTests; FFixture F; if (!F.Init()) { return false; }
    auto* E=F.Enemy(0); if (!E) { return false; }
    E->SetActorTickEnabled(false);
    TestTrue(TEXT("GAS attack execution starts weave"),E->ExecuteRangedAttack());
    TestEqual(TEXT("First twin volley"),F.Bullets().Num(),2);
    // 真实敌人Tick推进分段齐射，不调用数学替身。
    for (int32 I=0;I<5;++I) { E->Tick(0.2f); }
    TestEqual(TEXT("Six twin volleys"),F.Bullets().Num(),12);
    for (auto* B:F.Bullets()) { B->DeactivateToPool(); }
    E->ExecuteRangedAttack();
    TestEqual(TEXT("A new explicit attack emits its first twin volley before cancellation"),F.Bullets().Num(),2);
    E->DeactivateToPool(); E->Tick(1.f);
    TestEqual(TEXT("Pool cancellation stops pending volleys"),F.Bullets().Num(),2);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWindAttackTest,"Rogue.Gameplay.FlyingEnemies.WindReturnsOnceAndPoolResetsMotion",RogueFlyingTests::Flags)
bool FWindAttackTest::RunTest(const FString&)
{
    using namespace RogueFlyingTests; FFixture F; if (!F.Init()) { return false; }
    auto* E=F.Enemy(1); if (!E) { return false; } E->SetActorTickEnabled(false);
    F.Player->SetActorEnableCollision(false); E->ExecuteRangedAttack(); auto Bullets=F.Bullets();
    if (!TestEqual(TEXT("Three returning wind blades"),Bullets.Num(),3)) { return false; }
    auto* B=Bullets[0]; auto* Movement=B->FindComponentByClass<UProjectileMovementComponent>();
    const FVector Initial=Movement->Velocity; F.Step(1.f);
    TestTrue(TEXT("Wind turns back"),FVector::DotProduct(Initial,Movement->Velocity)<0);
    F.Step(0.5f); TestTrue(TEXT("Does not turn outward a second time"),FVector::DotProduct(Initial,Movement->Velocity)<0);
    B->DeactivateToPool(); B->ActivatePooledProjectile(E,E,FVector(0,0,500),FRotator::ZeroRotator,FVector::ForwardVector,500,1,false,3);
    F.Step(1.f); TestTrue(TEXT("Pool reuse as ordinary bullet has no stale return phase"),Movement->Velocity.X>0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FStarAttackTest,"Rogue.Gameplay.FlyingEnemies.StarWarningGapAndCollapse",RogueFlyingTests::Flags)
bool FStarAttackTest::RunTest(const FString&)
{
    using namespace RogueFlyingTests; FFixture F; if (!F.Init()) { return false; }
    auto* E=F.Enemy(2); if (!E) { return false; } E->SetActorTickEnabled(false);
    E->ExecuteRangedAttack(); auto Bullets=F.Bullets();
    if (!TestEqual(TEXT("Nine star seeds"),Bullets.Num(),9)) { return false; }
    const FVector Center=F.Player->GetActorLocation();
    for (auto* B:Bullets)
    {
        const FVector Radial=B->GetActorLocation()-Center;
        TestTrue(TEXT("Escape gap remains empty"),FMath::Abs(FMath::FindDeltaAngleDegrees(0.f,Radial.Rotation().Yaw))>=49.f);
        TestFalse(TEXT("Warning cannot hit"),B->GetActorEnableCollision());
    }
    const auto Settings=E->GetCurrentArchetype().Flying.Sanitized();
    F.Step(Settings.WarningSeconds*0.5f);
    for (auto* B:Bullets) { TestFalse(TEXT("Warning retained before release"),B->GetActorEnableCollision()); }
    F.Step(Settings.WarningSeconds*0.5f+0.1f);
    for (auto* B:Bullets)
    {
        TestTrue(TEXT("Released stars enable collision"),B->GetActorEnableCollision());
        TestTrue(TEXT("Stars move toward snapshot center"),FVector::DotProduct(Center-B->GetActorLocation(),B->FindComponentByClass<UProjectileMovementComponent>()->Velocity)>0);
    }
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFlyingBalanceCurveTest,"Rogue.Gameplay.FlyingEnemies.CurveProfilesAndPatternDamageBudgets",RogueFlyingTests::Flags)
bool FFlyingBalanceCurveTest::RunTest(const FString&)
{
    using namespace RogueFlyingTests;
    auto* Table=LoadObject<UDataTable>(nullptr,TEXT("/Game/Data/Enemies/DT_EnemyArchetypes.DT_EnemyArchetypes"));
    const auto* Balance=URogueGameBalanceAsset::GetDefaultGameBalance();
    if (!TestNotNull(TEXT("Actual table"),Table) || !TestNotNull(TEXT("Actual balance"),Balance)) { return false; }
    for (const auto Type:Types)
    {
        const auto* Row=Balance->FindEnemyBalanceRow(Type);
        if (!TestNotNull(TEXT("Type balance row"),Row)) { return false; }
        for (int32 Wave:{8,14,22,36,40,60,100})
        {
            for (bool Boss:{false,true})
            {
                const auto Profile=Boss?RogueGameModeRules::BuildBossProfile(Wave,Type,Balance)
                    :RogueGameModeRules::BuildEnemyProfile(Wave,Type,Balance);
                float CurveHealth=0,CurveDamage=0;
                TestTrue(TEXT("Health curve available"),Balance->TryEvaluateGrowthCurve(Boss?TEXT("BossHealth"):TEXT("EnemyHealth"),Wave,CurveHealth));
                TestTrue(TEXT("Damage curve available"),Balance->TryEvaluateGrowthCurve(Boss?TEXT("BossDamage"):TEXT("EnemyDamage"),Wave,CurveDamage));
                TestEqual(TEXT("Type health applied once"),Profile.Health,CurveHealth*(Boss?Row->BossHealthMultiplier:Row->HealthMultiplier));
                TestEqual(TEXT("Type damage applied once"),Profile.Damage,CurveDamage*(Boss?Row->BossDamageMultiplier:Row->DamageMultiplier));
                const auto A=RogueEnemyArchetypes::BuildEnemyArchetypeFromDataTable(Table,Type,Boss);
                const auto S=A.Flying.Sanitized();
                const int32 Chances=Type==ERogueEnemyType::SkyMoth?S.VolleyCount*2
                    :S.ShotCount; // 风刃往返两次，但每枚命中即回池，不能重复计入成功伤害。
                const float PatternDamage=Profile.Damage*A.Ranged.ProjectileDamageMultiplier*Chances;
                if (!Boss && Wave==36) { TestTrue(TEXT("Normal all-hit pattern stays below 45 initial HP at night entry"),PatternDamage<=45.f); }
                if (Boss && Wave==40) { TestTrue(TEXT("First night Boss all-hit pattern stays below 55 initial HP"),PatternDamage<=55.f); }
                TestTrue(TEXT("Finite positive attack cycle"),FMath::IsFinite(A.Ranged.NextShotCooldown)&&A.Ranged.NextShotCooldown>0);
                if (Type==ERogueEnemyType::StarLantern) { TestTrue(TEXT("Star warning remains readable"),S.WarningSeconds>=1.1f); }
            }
        }
    }
    return true;
}

#endif
