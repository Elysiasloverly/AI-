#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Enemies/Data/RogueEnemyAssetPaths.h"
#include "RogueEnemyHealingTestAccess.h"
#include "UObject/UnrealType.h"
#include "Tests/AutomationCommon.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Combat/Projectiles/RogueProjectile.h"
#include "Combat/Projectiles/RogueEnemyProjectile.h"
#include "Combat/Projectiles/RogueRocketProjectile.h"
#include "Combat/Projectiles/RogueMortarProjectile.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/StaticMesh.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/RogueAbilitySystemLibrary.h"
#include "AbilitySystem/RogueAttributeSet.h"
#include "AbilitySystem/RogueGameplayTags.h"
#include "Enemies/RogueEnemy.h"
#include "Progression/Upgrades/RogueUpgradeSystem.h"
#include "Progression/Upgrades/RogueUpgradeRuleAsset.h"
#include "Progression/Upgrades/RogueUpgradeDefinitionAsset.h"
#include "Enemies/Components/RogueEnemySupportComponent.h"
#include "Enemies/Components/RogueEnemyPresentationComponent.h"
#include "Enemies/Components/RogueEnemyStationaryHazardComponent.h"
#include "Enemies/Visuals/RogueEnemyEffectVisual.h"
#include "Enemies/Spawning/RogueSpawnSubsystem.h"
#include "Enemies/Data/RogueEnemyDataTable.h"
#include "Combat/Targeting/RogueEnemyTrackerSubsystem.h"
#include "Framework/RogueGameMode.h"
#include "Framework/RogueGameModeRules.h"
#include "Framework/RogueGameBalanceAsset.h"
#include "Player/RogueCharacter.h"
#include "Player/RoguePlayerQuery.h"
#include "Player/RoguePlayerController.h"
#include "Player/RoguePlayerState.h"
#include "World/RogueEnemySpawnRegion.h"
namespace RogueRoleTests
{
    constexpr auto Flags=EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter;
    UClass* Class(const TCHAR* Name)
    { return LoadClass<ARogueEnemy>(nullptr,*RogueEnemyAssetPaths::EnemyClassPath(Name)); }
    struct FFixture
    {
        FTestWorldWrapper Test;UWorld* World=nullptr;
        bool Init()
        {
            if (!Test.CreateTestWorld(EWorldType::Game)) return false;
            World=Test.GetTestWorld();World->GetWorldSettings()->DefaultGameMode=ARogueGameMode::StaticClass();
            if (!Test.BeginPlayInTestWorld()) return false;
            World->GetAuthGameMode()->SetActorTickEnabled(false);return true;
        }
        ARogueEnemy* Enemy(ERogueEnemyType Type,FVector P=FVector(0,0,72),float HP=1000,bool Boss=false,UClass* C=ARogueEnemy::StaticClass())
        {
            FActorSpawnParameters S;S.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            auto* E=World->SpawnActor<ARogueEnemy>(C,P,FRotator::ZeroRotator,S);
            E->ActivatePooledEnemy(nullptr,P,FRotator::ZeroRotator);E->InitializeEnemy({Type,HP,0,20,0,Boss});E->SetActorTickEnabled(false);
            World->GetSubsystem<URogueEnemyTrackerSubsystem>()->RegisterEnemy(E);return E;
        }
        ARogueCharacter* Player(FVector P)
        {
            auto* PC=World->SpawnActor<ARoguePlayerController>();PC->GetPlayerState<ARoguePlayerState>()->SetProgressionSnapshotForRun(FRogueProgressionSnapshot());
            FActorSpawnParameters S;S.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            auto* A=World->SpawnActorDeferred<ARogueCharacter>(ARogueCharacter::StaticClass(),FTransform(P),nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);A->AutoPossessPlayer=EAutoReceiveInput::Disabled;A->FinishSpawning(FTransform(P));PC->Possess(A);
            A->SetActorTickEnabled(false);A->GetCharacterMovement()->SetComponentTickEnabled(false);A->GetAbilitySystemComponent()->CancelAllAbilities();return A;
        }
        UStaticMeshComponent* Plane(ARogueEnemy* E,FVector Scale)
        {
            auto* M=NewObject<UStaticMeshComponent>(E);M->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Plane.Plane")));
            M->SetupAttachment(E->GetRootComponent());M->SetRelativeScale3D(Scale);M->SetRelativeLocation(FVector(0,0,-72));
            M->ComponentTags.Add(TEXT("Rogue.DamageArea"));M->SetCollisionEnabled(ECollisionEnabled::NoCollision);M->RegisterComponent();return M;
        }
    };
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRoleAssetTest,"Rogue.Gameplay.EnemyRoles.AssetsAndDistinctHealingBlueprints",RogueRoleTests::Flags)
bool FRoleAssetTest::RunTest(const FString&)
{
    using namespace RogueRoleTests;
    for (const TCHAR* Name:{TEXT("AuraHealer"),TEXT("LinkHealer")})
    {
        auto* C=Class(Name);if (!TestNotNull(TEXT("Healer Blueprint"),C)) return false;
        auto* E=C->GetDefaultObject<ARogueEnemy>();auto* S=E->FindComponentByClass<URogueEnemySupportComponent>();
        if (!TestNotNull(TEXT("Native healer component"),S)) return false;
        TestNotNull(TEXT("Independent circle BP assigned"),S->HealingCircleClass.Get());
        TestNotNull(TEXT("Independent line BP assigned"),S->HealingLinkClass.Get());
        TestTrue(TEXT("Healing visuals are not damage projectiles"),S->HealingCircleClass->IsChildOf(ARogueEnemyEffectVisual::StaticClass()));
    }
    auto* P=Class(TEXT("Rifleman"))->GetDefaultObject<ARogueEnemy>()->FindComponentByClass<URogueEnemyPresentationComponent>();
    TestNotNull(TEXT("Existing child inherits Boss marker"),P->BossVisualClass.Get());
    TestNotNull(TEXT("Existing child inherits flight marker"),P->FlyingVisualClass.Get());
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRoleHealingTest,"Rogue.Gameplay.EnemyRoles.AreaAndSingleHealRealGAS",RogueRoleTests::Flags)
bool FRoleHealingTest::RunTest(const FString&)
{
    using namespace RogueRoleTests;FFixture F;if (!F.Init()) return false;
    auto* Area=F.Enemy(ERogueEnemyType::AuraHealer);
    auto* A=F.Enemy(ERogueEnemyType::Rifleman,FVector(200,0,72));auto* B=F.Enemy(ERogueEnemyType::Rifleman,FVector(0,300,72));
    auto* Far=F.Enemy(ERogueEnemyType::Rifleman,FVector(900,0,72));
    for (auto* E:{A,B,Far}) RogueAbilitySystem::ApplyDamageToTarget(Area,E,200,RogueGameplayTags::Damage_Source_Projectile);
    auto* OtherHealer=F.Enemy(ERogueEnemyType::LinkHealer,FVector(200,200,72));OtherHealer->GetRogueAttributeSet()->SetHealth(500);
    Area->FindComponentByClass<URogueEnemySupportComponent>()->TickComponent(.01f,LEVELTICK_All,nullptr);
    TestEqual(TEXT("Area heals first ordinary target"),A->GetCurrentHealth(),825.f);
    TestEqual(TEXT("Area heals another target"),B->GetCurrentHealth(),825.f);
    TestEqual(TEXT("No healing outside radius"),Far->GetCurrentHealth(),800.f);
    TestEqual(TEXT("Healers cannot sustain one another"),OtherHealer->GetCurrentHealth(),500.f);
    A->GetRogueAttributeSet()->SetHealth(500);
    auto* Link=OtherHealer->FindComponentByClass<URogueEnemySupportComponent>();Link->TickComponent(.01f,LEVELTICK_All,nullptr);
    TestTrue(TEXT("Link selects wounded non-healer"),Link->GetHealingTarget()==A);
    TestEqual(TEXT("Single healing is stronger"),A->GetCurrentHealth(),560.f);
    TestEqual(TEXT("Link does not heal secondary target"),B->GetCurrentHealth(),825.f);
    A->DeactivateToPool();Link->TickComponent(.01f,LEVELTICK_All,nullptr);
    TestNull(TEXT("Target pooling immediately clears line"),Link->GetHealingTarget());
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRoleHealingSafetyTest,"Rogue.Gameplay.EnemyRoles.HealLimitsDeadAndReusedTargets",RogueRoleTests::Flags)
bool FRoleHealingSafetyTest::RunTest(const FString&)
{
    using namespace RogueRoleTests;FFixture F;if (!F.Init()) return false;
    auto* Caster=F.Enemy(ERogueEnemyType::LinkHealer);auto* S=Caster->FindComponentByClass<URogueEnemySupportComponent>();
    auto* Target=F.Enemy(ERogueEnemyType::Rifleman,FVector(200,0,72),100000,true);Target->GetRogueAttributeSet()->SetHealth(1000);
    S->TickComponent(.01f,LEVELTICK_All,nullptr);TestEqual(TEXT("Boss healing uses percentage with no caster cap"),Target->GetCurrentHealth(),1120.f);
    Target->GetRogueAttributeSet()->SetHealth(99999);TestEqual(TEXT("No overhealing"),S->HealTarget(Target,100),1.f);
    RogueAbilitySystem::ApplyDamageToTarget(Caster,Target,100001,RogueGameplayTags::Damage_Source_Projectile);TestEqual(TEXT("No rescue from queued death"),S->HealTarget(Target,100),0.f);
    Target->DeactivateToPool();TestEqual(TEXT("No pooled healing"),S->HealTarget(Target,100),0.f);
    Caster->DeactivateToPool();S->TickComponent(2,LEVELTICK_All,nullptr);TestNull(TEXT("Caster pool clears link"),S->GetHealingTarget());
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRoleBossScaleTest,"Rogue.Gameplay.EnemyRoles.BossScaleDoesNotAccumulateOnReuse",RogueRoleTests::Flags)
bool FRoleBossScaleTest::RunTest(const FString&)
{
    using namespace RogueRoleTests;FFixture F;if (!F.Init()) return false;
    auto* E=F.Enemy(ERogueEnemyType::Rifleman);auto* M=F.Plane(E,FVector(.4f));M->ComponentTags.Reset();
    for (int32 I=0;I<3;++I)
    {
        E->InitializeEnemy({ERogueEnemyType::Rifleman,1000,0,1,0,true});
        TestTrue(TEXT("Boss model enlarged"),M->GetRelativeScale3D().Equals(FVector(.6f),.001));
        E->DeactivateToPool();E->ActivatePooledEnemy(nullptr,FVector(0,0,72),FRotator::ZeroRotator);
        E->InitializeEnemy({ERogueEnemyType::Rifleman,1000,0,1,0,false});
        TestTrue(TEXT("Ordinary model restored exactly"),M->GetRelativeScale3D().Equals(FVector(.4f),.001));
    }
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRoleHazardTest,"Rogue.Gameplay.EnemyRoles.HazardFollowsBlueprintEllipseAndAllPlayers",RogueRoleTests::Flags)
bool FRoleHazardTest::RunTest(const FString&)
{
    using namespace RogueRoleTests;FFixture F;if (!F.Init()) return false;
    auto* A=F.Player(FVector(150,0,96));auto* B=F.Player(FVector(0,120,96));
    auto* E=F.Enemy(ERogueEnemyType::ShockPillar);auto* M=F.Plane(E,FVector(4,2,.1));
    auto* H=E->FindComponentByClass<URogueEnemyStationaryHazardComponent>();H->InitializeRuntime();
    TestFalse(TEXT("Hazard source is active after both players initialize"),E->IsAvailableInPool());
    AddInfo(FString::Printf(TEXT("Source HP=%.1f Damage=%.1f Stationary=%d PlayerAlive=%d/%d"),E->GetCurrentHealth(),E->GetContactDamage(),E->GetMovementModel()==ERogueEnemyMovementModel::Stationary,A->IsAlive(),B->IsAlive()));
    TestTrue(TEXT("Long axis inside"),H->IsPlayerInside(A));TestTrue(TEXT("Capsule edge overlaps circle"),H->IsPlayerInside(B));
    TArray<ARogueCharacter*,TInlineAllocator<4>> Seen;RoguePlayerQuery::CollectAlivePlayers(F.World,Seen);
    FGameplayTagContainer Tags;A->GetAbilitySystemComponent()->GetOwnedGameplayTags(Tags);
    AddInfo(FString::Printf(TEXT("Hazard diag: Seen=%d Enabled=%d Authority=%d SourceGAS=%.1f Armor=%.1f Reduction=%.3f Tags=%s"),Seen.Num(),H->bEnabled,E->HasAuthority(),E->GetRogueAttributeSet()->GetHealth(),A->GetRogueAttributeSet()->GetArmor(),A->GetRogueAttributeSet()->GetDamageReductionPercent(),*Tags.ToString()));
    H->TickComponent(.01f,LEVELTICK_All,nullptr);
    TestEqual(TEXT("First player damaged"),A->GetRogueAttributeSet()->GetHealth(),96.f);TestEqual(TEXT("Second player damaged"),B->GetRogueAttributeSet()->GetHealth(),96.f);
    B->SetActorLocation(FVector(0,180,96));TestFalse(TEXT("Short axis outside"),H->IsPlayerInside(B));
    M->SetRelativeRotation(FRotator(0,90,0));TestTrue(TEXT("Rotation changes real area"),H->IsPlayerInside(B));
    M->SetRelativeScale3D(FVector(1,1,.1));TestFalse(TEXT("Smaller authored mesh shrinks damage zone"),H->IsPlayerInside(A));
    E->DeactivateToPool();H->TickComponent(1,LEVELTICK_All,nullptr);TestEqual(TEXT("No pool damage"),A->GetRogueAttributeSet()->GetHealth(),96.f);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRoleIntroductionTest,"Rogue.Gameplay.EnemyRoles.IntroductionUsesRealSpawnerAndFlightHeight",RogueRoleTests::Flags)
bool FRoleIntroductionTest::RunTest(const FString&)
{
    using namespace RogueRoleTests;FFixture F;if (!F.Init()) return false;
    auto* Player=F.Player(FVector(0,0,96));
    auto* Floor=F.World->SpawnActor<AActor>();auto* Box=NewObject<UBoxComponent>(Floor);Floor->SetRootComponent(Box);
    Box->SetBoxExtent(FVector(6000,6000,50));Box->SetCollisionObjectType(ECC_WorldStatic);Box->SetCollisionResponseToAllChannels(ECR_Block);Box->RegisterComponent();Floor->SetActorLocation(FVector(0,0,-50));
    auto* Region=F.World->SpawnActor<ARogueEnemySpawnRegion>();Region->GetSpawnBounds()->SetBoxExtent(FVector(5000,5000,1000));
    auto* Balance=DuplicateObject<URogueGameBalanceAsset>(URogueGameBalanceAsset::GetDefaultGameBalance(),GetTransientPackage());
    auto* Table=LoadObject<UDataTable>(nullptr,TEXT("/Game/Data/Enemies/DT_EnemyArchetypes.DT_EnemyArchetypes"));
    TMap<ERogueEnemyType,TSubclassOf<ARogueEnemy>> Mapping;
    const ERogueEnemyType Types[]={ERogueEnemyType::SkyMoth,ERogueEnemyType::WindReaver,ERogueEnemyType::StarLantern};
    const TCHAR* Names[]={TEXT("SkyMoth"),TEXT("WindReaver"),TEXT("StarLantern")};const int32 Waves[]={5,9,14};
    for (int32 I=0;I<3;++I) Mapping.Add(Types[I],Class(Names[I]));
    auto* Spawn=F.World->GetSubsystem<URogueSpawnSubsystem>();
    Spawn->Configure(ARogueEnemy::StaticClass(),Mapping,FRogueSpawnSettings(),FRogueBossSettings(),Balance,Table);
    for (int32 I=0;I<3;++I)
    {
        FRogueRunState Run;Run.CurrentWave=Waves[I];
        TestTrue(TEXT("Intro spawn succeeds on real floor"),Spawn->SpawnEnemy(Run,Player,Region));
        auto* Tracker=F.World->GetSubsystem<URogueEnemyTrackerSubsystem>();const auto Active=Tracker->GetActiveEnemies();
        if (!TestEqual(TEXT("One newly introduced enemy"),Active.Num(),1)) return false;
        auto* E=Active[0].Get();TestEqual(TEXT("Guaranteed correct introduction type"),E->GetEnemyType(),Types[I]);
        TestEqual(TEXT("Hover offset survives spawning"),float(E->GetActorLocation().Z-E->GetSpawnGroundZ()),110.f);
        E->DeactivateToPool();
    }
    Balance->EnemyWaveEntries={{1,ERogueEnemyType::SkyMoth,1}};
    FRogueRunState BossRun;BossRun.CurrentWave=20;Spawn->SpawnBoss(BossRun,Player,Region);
    TestTrue(TEXT("Flying Boss spawned"),BossRun.bBossAlive);
    for (const auto& Ref:F.World->GetSubsystem<URogueEnemyTrackerSubsystem>()->GetActiveEnemies())
        if (auto* E=Ref.Get();E&&E->IsBoss())
            TestEqual(TEXT("Boss capsule resize keeps hover after ground correction"),float(E->GetActorLocation().Z-E->GetSpawnGroundZ()),110.f);
    Balance->EnemyWaveEntries={{1,ERogueEnemyType::Rifleman,1},{1,ERogueEnemyType::AuraHealer,100},{1,ERogueEnemyType::LinkHealer,100}};
    for (int32 I=0;I<50;++I) TestEqual(TEXT("Healers are not chosen as Boss"),RogueGameModeRules::PickWeightedEnemyTypeForWave(20,Balance),ERogueEnemyType::Rifleman);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHealerSeekTest,"Rogue.Gameplay.EnemyRoles.SeekWoundedBeyondHealRange",RogueRoleTests::Flags)
bool FHealerSeekTest::RunTest(const FString&)
{
    using namespace RogueRoleTests;FFixture F;if (!F.Init()) return false;
    F.Player(FVector(-2000,0,96));auto* E=F.Enemy(ERogueEnemyType::AuraHealer);
    auto* Near=F.Enemy(ERogueEnemyType::Rifleman,FVector(400,0,72));Near->GetRogueAttributeSet()->SetHealth(800);
    auto* Weak=F.Enemy(ERogueEnemyType::Rifleman,FVector(1100,0,72));Weak->GetRogueAttributeSet()->SetHealth(200);
    auto* S=E->FindComponentByClass<URogueEnemySupportComponent>();
    const FVector Direction=E->CalculateArchetypeMovement(.5f);
    TestTrue(TEXT("Seeks weak ally outside healing radius"),S->GetMovementTarget()==Weak);
    TestTrue(TEXT("Moves to ally, away from player"),Direction.X>.8f);
    TestEqual(TEXT("Movement does not remotely heal outside range"),S->HealTarget(Weak,100),0.f);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHealerRetreatTest,"Rogue.Gameplay.EnemyRoles.RetreatBeforeHealingAndAvoidAllPlayers",RogueRoleTests::Flags)
bool FHealerRetreatTest::RunTest(const FString&)
{
    using namespace RogueRoleTests;FFixture F;if (!F.Init()) return false;
    auto* First=F.Player(FVector(200,0,96));auto* Second=F.Player(FVector(-1500,0,96));
    auto* E=F.Enemy(ERogueEnemyType::LinkHealer);
    auto* Weak=F.Enemy(ERogueEnemyType::Rifleman,FVector(1000,0,72));Weak->GetRogueAttributeSet()->SetHealth(100);
    TestTrue(TEXT("Close player overrides ally approach"),E->CalculateArchetypeMovement(.5f).X<-.7f);
    First->SetActorLocation(FVector(-1500,0,96));Second->SetActorLocation(FVector(200,0,96));
    TestTrue(TEXT("Reacts to second player despite old cached aggro target"),E->CalculateArchetypeMovement(.5f).X<-.7f);
    First->SetActorLocation(E->GetActorLocation());
    TestFalse(TEXT("Overlapping player cannot trap all escape probes"),E->CalculateArchetypeMovement(.5f).IsNearlyZero());
    E->SetActorLocation(FVector(0,1700,72));Weak->GetRogueAttributeSet()->SetHealth(1000);
    TestTrue(TEXT("No wounded allies and safe distance means no player chase"),E->CalculateArchetypeMovement(.5f).IsNearlyZero());
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHealerPoolTargetTest,"Rogue.Gameplay.EnemyRoles.MovementTargetClearsOnHealDeathAndPoolReuse",RogueRoleTests::Flags)
bool FHealerPoolTargetTest::RunTest(const FString&)
{
    using namespace RogueRoleTests;FFixture F;if (!F.Init()) return false;
    F.Player(FVector(-2000,0,96));auto* E=F.Enemy(ERogueEnemyType::AuraHealer);
    auto* A=F.Enemy(ERogueEnemyType::Rifleman,FVector(800,0,72));A->GetRogueAttributeSet()->SetHealth(100);
    auto* B=F.Enemy(ERogueEnemyType::Rifleman,FVector(1000,300,72));B->GetRogueAttributeSet()->SetHealth(300);
    auto* S=E->FindComponentByClass<URogueEnemySupportComponent>();E->CalculateArchetypeMovement(.5f);
    TestTrue(TEXT("Starts with lowest fraction"),S->GetMovementTarget()==A);
    A->DeactivateToPool();A->ActivatePooledEnemy(nullptr,FVector(800,0,72),FRotator::ZeroRotator);A->InitializeEnemy({ERogueEnemyType::Rifleman,1000,0,0,0,false});
    E->CalculateArchetypeMovement(.5f);TestTrue(TEXT("Reused healthy actor is not old goal"),S->GetMovementTarget()==B);
    RogueAbilitySystem::ApplyDamageToTarget(E,B,2000,RogueGameplayTags::Damage_Source_Projectile);
    E->CalculateArchetypeMovement(.5f);TestTrue(TEXT("Queued death is replaced by healthy formation anchor"),S->GetMovementTarget()==A);
    E->DeactivateToPool();TestNull(TEXT("Caster pool clears goal"),S->GetMovementTarget());
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHealerObstacleTest,"Rogue.Gameplay.EnemyRoles.ActualMovementDetoursAroundWall",RogueRoleTests::Flags)
bool FHealerObstacleTest::RunTest(const FString&)
{
    using namespace RogueRoleTests;FFixture F;if (!F.Init()) return false;
    F.Player(FVector(-2000,0,96));auto* E=F.Enemy(ERogueEnemyType::AuraHealer,FVector(0,0,90));
    E->InitializeEnemy({ERogueEnemyType::AuraHealer,1000,200,0,0,false});
    auto* Weak=F.Enemy(ERogueEnemyType::Rifleman,FVector(1000,0,90));Weak->GetRogueAttributeSet()->SetHealth(200);
    auto* Wall=F.World->SpawnActor<AActor>();auto* Box=NewObject<UBoxComponent>(Wall);Wall->SetRootComponent(Box);
    Box->SetBoxExtent(FVector(20,60,30));Box->SetCollisionObjectType(ECC_WorldStatic);Box->SetCollisionResponseToAllChannels(ECR_Block);Box->RegisterComponent();Wall->SetActorLocation(FVector(150,0,30));
    const FVector Direction=E->CalculateArchetypeMovement(.5f);
    TestTrue(TEXT("Wall requires a lateral route"),!Direction.IsNearlyZero()&&FMath::Abs(Direction.Y)>.3f);
    E->Tick(.2f);TestTrue(TEXT("Walking movement receives lateral steering input"),!E->GetPendingMovementInputVector().IsNearlyZero());
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAllBulletLifetimeTest,"Rogue.Gameplay.EnemyRoles.PlayerEnemyRocketMortarExpireAt30Seconds",RogueRoleTests::Flags)
bool FAllBulletLifetimeTest::RunTest(const FString&)
{
    using namespace RogueRoleTests;FFixture F;if (!F.Init()) return false;
    auto* Player=F.Player(FVector(0,0,96));auto* Source=F.Enemy(ERogueEnemyType::Rifleman);
    auto* Cold=F.World->SpawnActor<ARogueProjectile>();auto* Enemy=F.World->SpawnActor<ARogueEnemyProjectile>();
    auto* Rocket=F.World->SpawnActor<ARogueRocketProjectile>();auto* Mortar=F.World->SpawnActor<ARogueMortarProjectile>();
    const FVector P(15000,0,600);
    Cold->ActivatePooledProjectile(Player,Player,P,FRotator::ZeroRotator,FVector::ForwardVector,100,0);
    Enemy->ActivatePooledProjectile(Source,Source,P+FVector(0,500,0),FRotator::ZeroRotator,FVector::ForwardVector,100,0,false);
    Rocket->ActivatePooledRocket(Player,Player,P+FVector(0,1000,0),FRotator::ZeroRotator,FVector::ForwardVector,100,0,50);
    Mortar->ActivatePooledMortar(Player,Player,P+FVector(0,1500,0),P+FVector(800,1500,0),0,50,700);
    for (auto* A:{static_cast<AActor*>(Cold),static_cast<AActor*>(Enemy),static_cast<AActor*>(Rocket),static_cast<AActor*>(Mortar)}) A->GetComponentByClass<UProjectileMovementComponent>()->SetComponentTickEnabled(false);
    Cold->Tick(29);Enemy->Tick(29);Rocket->Tick(29);Mortar->Tick(29);
    TestTrue(TEXT("All four projectile families survive 29 seconds"),!Cold->IsAvailableInPool()&&!Enemy->IsAvailableInPool()&&!Rocket->IsAvailableInPool()&&!Mortar->IsAvailableInPool());
    Cold->Tick(1.1f);Enemy->Tick(1.1f);Rocket->Tick(1.1f);Mortar->Tick(1.1f);
    TestTrue(TEXT("All four projectile families return after 30 seconds"),Cold->IsAvailableInPool()&&Enemy->IsAvailableInPool()&&Rocket->IsAvailableInPool()&&Mortar->IsAvailableInPool());
    return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHealerOcclusionRegression,"Rogue.Gameplay.HealingRegression.OccludedAllyDoesNotFreezeHealer",RogueRoleTests::Flags)
bool FHealerOcclusionRegression::RunTest(const FString&)
{
    using namespace RogueRoleTests;FFixture F;if (!F.Init()) return false;
    F.Player(FVector(-2000,0,96));auto* E=F.Enemy(ERogueEnemyType::AuraHealer);
    auto* Weak=F.Enemy(ERogueEnemyType::Rifleman,FVector(1000,0,72));Weak->GetRogueAttributeSet()->SetHealth(200);
    auto* Wall=F.World->SpawnActor<AActor>();auto* Box=NewObject<UBoxComponent>(Wall);Wall->SetRootComponent(Box);
    Box->SetBoxExtent(FVector(30,80,150));Box->SetCollisionObjectType(ECC_WorldStatic);Box->SetCollisionResponseToAllChannels(ECR_Block);Box->RegisterComponent();Wall->SetActorLocation(FVector(400,0,150));
    const FVector Direction=E->CalculateArchetypeMovement(.5f);
    TestTrue(TEXT("Occluded wounded ally remains a movement goal"),E->FindComponentByClass<URogueEnemySupportComponent>()->GetMovementTarget()==Weak);
    TestFalse(TEXT("Healer starts approaching rather than idling forever"),Direction.IsNearlyZero());return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHealerFrontlineRegression,"Rogue.Gameplay.HealingRegression.FrontlineAllyCanBeApproached",RogueRoleTests::Flags)
bool FHealerFrontlineRegression::RunTest(const FString&)
{
    using namespace RogueRoleTests;FFixture F;if (!F.Init()) return false;
    F.Player(FVector(0,0,96));auto* E=F.Enemy(ERogueEnemyType::AuraHealer,FVector(900,0,72));
    auto* Weak=F.Enemy(ERogueEnemyType::Rifleman,FVector(50,0,72));Weak->GetRogueAttributeSet()->SetHealth(200);
    const FVector Direction=E->CalculateArchetypeMovement(.5f);
    TestTrue(TEXT("Frontline wounded ally is not excluded by a longer retreat distance"),E->FindComponentByClass<URogueEnemySupportComponent>()->GetMovementTarget()==Weak);
    TestTrue(TEXT("Approaches into useful range while preserving emergency spacing"),Direction.X<-.3f);return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHealerUncappedTest,"Rogue.Gameplay.HealingRegression.HealAmountIsNotLimitedByCasterHealth",RogueRoleTests::Flags)
bool FHealerUncappedTest::RunTest(const FString&)
{
    using namespace RogueRoleTests;FFixture F;if (!F.Init()) return false;
    auto* Caster=F.Enemy(ERogueEnemyType::LinkHealer,FVector(0,0,72),100);
    auto* Target=F.Enemy(ERogueEnemyType::Rifleman,FVector(200,0,72),10000);
    Target->GetRogueAttributeSet()->SetHealth(5000);
    Caster->FindComponentByClass<URogueEnemySupportComponent>()->TickComponent(.01f,LEVELTICK_All,nullptr);
    TestEqual(TEXT("Six percent of ally max HP, independent of caster HP"),Target->GetCurrentHealth(),5600.f);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHealerNumbersTest,"Rogue.Gameplay.HealingRegression.GreenNumbersAreActualHealingAndSurviveNetHealthLoss",RogueRoleTests::Flags)
bool FHealerNumbersTest::RunTest(const FString&)
{
    using namespace RogueRoleTests;FFixture F;if (!F.Init()) return false;
    auto* Player=F.Player(FVector(-2000,0,96));auto* PC=Player->GetController<APlayerController>();
    auto* HUD=F.World->SpawnActor<ARogueHUD>();
    auto* HUDProperty=FindFProperty<FObjectPropertyBase>(APlayerController::StaticClass(),TEXT("MyHUD"));
    if (!TestNotNull(TEXT("HUD property"),HUDProperty)) return false;
    HUDProperty->SetObjectPropertyValue_InContainer(PC,HUD);if (!TestNotNull(TEXT("Local healing HUD"),HUD)) return false;
    auto* Caster=F.Enemy(ERogueEnemyType::LinkHealer,FVector(0,0,72),100);
    auto* Target=F.Enemy(ERogueEnemyType::Rifleman,FVector(200,0,72),10000);
    Target->GetRogueAttributeSet()->SetHealth(5000);FRogueEnemyHealingTestAccess::Clear(HUD);
    auto* S=Caster->FindComponentByClass<URogueEnemySupportComponent>();S->TickComponent(.01f,LEVELTICK_All,nullptr);
    TestEqual(TEXT("One green number for real heal"),FRogueEnemyHealingTestAccess::GreenCount(HUD),1);
    TestEqual(TEXT("Correct actual heal amount"),FRogueEnemyHealingTestAccess::Feedback(Target).TotalHealing,600.f);
    TestEqual(TEXT("Green text contains plus and correct amount"),FRogueEnemyHealingTestAccess::Numbers(HUD).Last().CachedText,FString(TEXT("+600")));
    RogueAbilitySystem::ApplyDamageToTarget(Caster,Target,800,RogueGameplayTags::Damage_Source_Projectile);
    FRogueEnemyHealingTestAccess::Clear(HUD);FRogueEnemyHealingTestAccess::Consume(Target);
    TestEqual(TEXT("Client feedback still shows heal when net health falls"),FRogueEnemyHealingTestAccess::GreenCount(HUD),1);
    FRogueEnemyHealingTestAccess::Consume(Target);TestEqual(TEXT("Replicated feedback not duplicated"),FRogueEnemyHealingTestAccess::GreenCount(HUD),1);
    Target->DeactivateToPool();Target->ActivatePooledEnemy(nullptr,FVector(200,0,72),FRotator::ZeroRotator);
    FRogueEnemyHealingTestAccess::Clear(HUD);Target->InitializeEnemy({ERogueEnemyType::Rifleman,10000,0,0,0,false});FRogueEnemyHealingTestAccess::Consume(Target);
    TestEqual(TEXT("Pool and initial full health do not create fake heals"),FRogueEnemyHealingTestAccess::GreenCount(HUD),0);
    TestEqual(TEXT("Full target heals zero"),S->HealTarget(Target,50),0.f);
    TestEqual(TEXT("Full target produces no green number"),FRogueEnemyHealingTestAccess::GreenCount(HUD),0);
    Target->GetRogueAttributeSet()->SetHealth(9999.9f);FRogueEnemyHealingTestAccess::Clear(HUD);
    const float Actual=S->HealTarget(Target,50);
    TestTrue(TEXT("Final partial heal is clipped only to missing health"),Actual>0&&Actual<.11f);
    TestTrue(TEXT("Sub-one heal retains decimal precision"),FRogueEnemyHealingTestAccess::Numbers(HUD).Last().CachedText.Contains(TEXT(".")));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHealerWalkingTest,"Rogue.Gameplay.HealingRegression.WalksToOccludedAllyAndHeals",RogueRoleTests::Flags)
bool FHealerWalkingTest::RunTest(const FString&)
{
    using namespace RogueRoleTests;FFixture F;if (!F.Init()) return false;
    F.Player(FVector(-2000,0,96));auto* E=F.Enemy(ERogueEnemyType::AuraHealer);
    E->InitializeEnemy({ERogueEnemyType::AuraHealer,1000,230,0,0,false});
    auto* Weak=F.Enemy(ERogueEnemyType::Rifleman,FVector(850,0,72));Weak->GetRogueAttributeSet()->SetHealth(200);
    auto AddBox=[&](FVector Pos,FVector Extent)
    {
        auto* A=F.World->SpawnActor<AActor>();auto* B=NewObject<UBoxComponent>(A);A->SetRootComponent(B);
        B->SetBoxExtent(Extent);B->SetCollisionObjectType(ECC_WorldStatic);B->SetCollisionResponseToAllChannels(ECR_Block);B->RegisterComponent();A->SetActorLocation(Pos);
    };
    AddBox(FVector(0,0,-30),FVector(4000,4000,30));AddBox(FVector(300,0,120),FVector(25,75,120));
    E->SetActorTickEnabled(true);
    for (int32 I=0;I<160;++I) F.Test.TickTestWorld(.05f);
    TestTrue(TEXT("Actor makes progress around obstruction"),E->GetActorLocation().X>350);
    TestTrue(TEXT("Wounded ally actually receives healing"),Weak->GetCurrentHealth()>200);
    return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHealerHealthyFollowTest,"Rogue.Gameplay.HealerFormation.BothBlueprintsWalkWithHealthyGroup",RogueRoleTests::Flags)
bool FHealerHealthyFollowTest::RunTest(const FString&)
{
    using namespace RogueRoleTests;
    for (const TCHAR* Name:{TEXT("AuraHealer"),TEXT("LinkHealer")})
    {
        FFixture F;if (!F.Init()) return false;
        F.Player(FVector(0,0,96));
        auto* Floor=F.World->SpawnActor<AActor>();auto* Box=NewObject<UBoxComponent>(Floor);Floor->SetRootComponent(Box);
        Box->SetBoxExtent(FVector(6000,6000,30));Box->SetCollisionObjectType(ECC_WorldStatic);
        Box->SetCollisionResponseToAllChannels(ECR_Block);Box->RegisterComponent();Floor->SetActorLocation(FVector(0,0,-30));
        auto* C=Class(Name);if (!TestNotNull(TEXT("Actual healer Blueprint"),C)) return false;
        const auto Type=FCString::Strcmp(Name,TEXT("AuraHealer"))==0?ERogueEnemyType::AuraHealer:ERogueEnemyType::LinkHealer;
        auto* E=F.Enemy(Type,FVector(2200,0,72),1000,false,C);
        E->InitializeEnemy({Type,1000,220,0,0,false});E->SetActorTickEnabled(true);
        auto* Ally=F.Enemy(ERogueEnemyType::Rifleman,FVector(1000,0,72));
        auto* S=E->FindComponentByClass<URogueEnemySupportComponent>();
        TestTrue(TEXT("Fresh full-health group gives a follow goal"),E->CalculateArchetypeMovement(.01f).X<-.8f);
        for (int32 I=0;I<50;++I) F.Test.TickTestWorld(.05f);
        TestTrue(TEXT("Actual CharacterMovement closes the distance"),E->GetActorLocation().X<2000);
        TestTrue(TEXT("Healthy ally remains the formation anchor"),S->GetMovementTarget()==Ally);
        TestNull(TEXT("Following healthy ally does not create a healing link"),S->GetHealingTarget());
        TestEqual(TEXT("Full health is preserved without overhealing"),Ally->GetCurrentHealth(),1000.f);
        TestTrue(TEXT("Formation stays behind its ally"),E->GetActorLocation().X>Ally->GetActorLocation().X+150);
    }
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHealerFormationWoundTest,"Rogue.Gameplay.HealerFormation.WoundedAllyOverridesHealthyFollow",RogueRoleTests::Flags)
bool FHealerFormationWoundTest::RunTest(const FString&)
{
    using namespace RogueRoleTests;FFixture F;if (!F.Init()) return false;
    F.Player(FVector(-2000,0,96));auto* E=F.Enemy(ERogueEnemyType::LinkHealer);
    auto* Near=F.Enemy(ERogueEnemyType::Rifleman,FVector(400,0,72));
    auto* Far=F.Enemy(ERogueEnemyType::Rifleman,FVector(1200,0,72));
    auto* S=E->FindComponentByClass<URogueEnemySupportComponent>();E->CalculateArchetypeMovement(.5f);
    TestTrue(TEXT("Idle follow chooses nearby healthy ally"),S->GetMovementTarget()==Near);
    Far->GetRogueAttributeSet()->SetHealth(200);E->CalculateArchetypeMovement(.5f);
    TestTrue(TEXT("Far injured ally takes priority over healthy anchor"),S->GetMovementTarget()==Far);
    TestNull(TEXT("Out-of-range approach is not a remote heal"),S->GetHealingTarget());
    TestEqual(TEXT("Cannot heal outside DT range"),S->HealTarget(Far,100),0.f);
    Far->GetRogueAttributeSet()->SetHealth(1000);E->CalculateArchetypeMovement(.5f);
    TestTrue(TEXT("Once healed, returns to nearest healthy formation"),S->GetMovementTarget()==Near);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHealerFormationSafetyTest,"Rogue.Gameplay.HealerFormation.BacklineSpacingAndRetreat",RogueRoleTests::Flags)
bool FHealerFormationSafetyTest::RunTest(const FString&)
{
    using namespace RogueRoleTests;FFixture F;if (!F.Init()) return false;
    auto* Player=F.Player(FVector(0,0,96));auto* E=F.Enemy(ERogueEnemyType::AuraHealer,FVector(900,0,72));
    auto* Ally=F.Enemy(ERogueEnemyType::Rifleman,FVector(100,0,72));
    auto* S=E->FindComponentByClass<URogueEnemySupportComponent>();
    TestTrue(TEXT("Healthy frontline ally does not pull healer toward player"),E->CalculateArchetypeMovement(.5f).IsNearlyZero());
    TestTrue(TEXT("Safe holding still retains a valid group anchor"),S->GetMovementTarget()==Ally);
    E->SetActorLocation(FVector(250,0,72));
    TestTrue(TEXT("Close player overrides healthy formation"),E->CalculateArchetypeMovement(.5f).X>.8f);
    E->SetActorLocation(FVector(900,0,72));Ally->SetActorLocation(FVector(1300,0,72));
    TestTrue(TEXT("Following resumes when healthy group moves"),E->CalculateArchetypeMovement(.5f).X>.8f);
    Ally->DeactivateToPool();
    TestTrue(TEXT("No group and safe spacing holds without chasing player"),E->CalculateArchetypeMovement(.01f).IsNearlyZero());
    TestNull(TEXT("Pooled group anchor is removed immediately"),S->GetMovementTarget());
    // 其它治疗者不能相互跟随形成远离战斗的治疗怪小团。
    F.Enemy(ERogueEnemyType::LinkHealer,FVector(1000,0,72));E->CalculateArchetypeMovement(.5f);
    TestNull(TEXT("Other healers are not formation anchors"),S->GetMovementTarget());
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHealerFormationPoolTest,"Rogue.Gameplay.HealerFormation.ReuseReplansHealthyFollow",RogueRoleTests::Flags)
bool FHealerFormationPoolTest::RunTest(const FString&)
{
    using namespace RogueRoleTests;FFixture F;if (!F.Init()) return false;
    F.Player(FVector(0,0,96));auto* E=F.Enemy(ERogueEnemyType::LinkHealer,FVector(2000,0,72));
    auto* Ally=F.Enemy(ERogueEnemyType::Rifleman,FVector(1000,0,72));
    auto* S=E->FindComponentByClass<URogueEnemySupportComponent>();
    TestTrue(TEXT("Initially follows healthy ally to the left"),E->CalculateArchetypeMovement(.01f).X<-.8f);
    Ally->DeactivateToPool();Ally->ActivatePooledEnemy(nullptr,FVector(3000,0,72),FRotator::ZeroRotator);
    Ally->InitializeEnemy({ERogueEnemyType::Rifleman,1000,0,0,0,false});
    F.World->GetSubsystem<URogueEnemyTrackerSubsystem>()->RegisterEnemy(Ally);
    TestTrue(TEXT("New pool cycle immediately replans opposite direction"),E->CalculateArchetypeMovement(.01f).X>.8f);
    RogueAbilitySystem::ApplyDamageToTarget(E,Ally,2000,RogueGameplayTags::Damage_Source_Projectile);
    E->CalculateArchetypeMovement(.01f);TestNull(TEXT("Zero-health pending death cannot anchor group"),S->GetMovementTarget());
    E->DeactivateToPool();TestNull(TEXT("Healer pool clears follow state"),S->GetMovementTarget());
    E->ActivatePooledEnemy(nullptr,FVector(2000,0,72),FRotator::ZeroRotator);E->InitializeEnemy({ERogueEnemyType::LinkHealer,1000,200,0,0,false});
    TestTrue(TEXT("Reused healer has no residual movement without group"),E->CalculateArchetypeMovement(.01f).IsNearlyZero());
    return true;
}



IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOpeningOffenseTest,"Rogue.Gameplay.SurvivorFeel.FreshProfileOffenseAndPermissions",RogueRoleTests::Flags)
bool FOpeningOffenseTest::RunTest(const FString&)
{
    using namespace RogueRoleTests;FFixture F;if (!F.Init()) return false;
    auto* P=F.Player(FVector(0,0,96));FRogueUpgradeSystem System;
    System.SetDefinitionAsset(LoadObject<URogueUpgradeDefinitionAsset>(nullptr,TEXT("/Game/Data/Upgrades/DA_UpgradeDefinitions.DA_UpgradeDefinitions")));
    System.SetRuleAsset(LoadObject<URogueUpgradeRuleAsset>(nullptr,TEXT("/Game/Data/Upgrades/DA_UpgradeRules.DA_UpgradeRules")));
    for(int I=0;I<100;++I)
    {
        TArray<FRogueUpgradeOption> Options;System.BuildRandomUpgradeOptions(P,3,Options);
        TestEqual(TEXT("Three choices"),Options.Num(),3);
        bool Offense=false;TSet<ERogueUpgradeType> Seen;
        for(const auto& O:Options)
        {
            Offense|=O.Type==ERogueUpgradeType::ProjectileCount||O.Type==ERogueUpgradeType::AttackPower||O.Type==ERogueUpgradeType::AttackSpeed;
            TestFalse(TEXT("No duplicates"),Seen.Contains(O.Type));Seen.Add(O.Type);
            TestFalse(TEXT("Locked burst never granted"),O.Type==ERogueUpgradeType::ProjectileBurst);
            TestFalse(TEXT("Locked laser never granted"),O.Type==ERogueUpgradeType::LaserCount);
        }
        TestTrue(TEXT("Every early roll contains immediate offense"),Offense);
    }
    TArray<FRogueUpgradeOption> Empty;System.BuildRandomUpgradeOptions(P,0,Empty);TestTrue(TEXT("Zero desired remains empty"),Empty.IsEmpty());
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSurvivorRolesTest,"Rogue.Gameplay.SurvivorFeel.NormalContactBossRangedAndOneHit",RogueRoleTests::Flags)
bool FSurvivorRolesTest::RunTest(const FString&)
{
    auto* T=LoadObject<UDataTable>(nullptr,TEXT("/Game/Data/Enemies/DT_EnemyArchetypes.DT_EnemyArchetypes"));
    auto* B=URogueGameBalanceAsset::GetDefaultGameBalance();
    for(auto Type:{ERogueEnemyType::Rifleman,ERogueEnemyType::Assault,ERogueEnemyType::Skirmisher})
    {
        const auto Normal=RogueEnemyArchetypes::BuildEnemyArchetypeFromDataTable(T,Type,false);
        const auto Boss=RogueEnemyArchetypes::BuildEnemyArchetypeFromDataTable(T,Type,true);
        TestFalse(TEXT("Normal is contact-only"),Normal.Ranged.bUsesRangedAttack);
        TestTrue(TEXT("Boss keeps ranged identity"),Boss.Ranged.bUsesRangedAttack);
        TestEqual(TEXT("Fodder approaches instead of kiting"),Normal.Movement.Model,ERogueEnemyMovementModel::Direct);
        TestTrue(TEXT("Wave-one fodder is below starter damage"),RogueGameModeRules::BuildEnemyProfile(1,Type,B).Health<=28);
    }
    return true;
}
#endif
