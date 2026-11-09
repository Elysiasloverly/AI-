#if WITH_DEV_AUTOMATION_TESTS
#include "RogueTestWorld.h"
#include "Enemies/RogueEnemy.h"
#include "Enemies/Components/RogueEnemyObstacleMovementComponent.h"
#include "Enemies/Components/RogueEnemySupportComponent.h"
#include "Enemies/Spawning/RogueEnemyNavigationBudget.h"
#include "Combat/Targeting/RogueEnemyTrackerSubsystem.h"
#include "Enemies/Data/RogueEnemyAssetPaths.h"
#include "Enemies/Data/RogueEnemyDataTable.h"
#include "Engine/DataTable.h"
#include "UObject/UnrealType.h"
#include "AbilitySystem/RogueAttributeSet.h"
#include "Framework/RogueGameMode.h"
#include "Player/RogueCharacter.h"
#include "Player/RoguePlayerController.h"
#include "Player/RoguePlayerState.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/WorldSettings.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"

namespace RogueObstacleTests
{
constexpr auto Flags=EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter;
struct FFixture
{
    FTestWorldWrapper Test;UWorld* World=nullptr;
    bool Init(bool bFloor=true)
    {
        if (!RogueTestWorld::Begin(Test, World)) { return false; }
        if(bFloor)Box(FVector(0,0,-50),FVector(6000,6000,50));return true;
    }
    UBoxComponent* Box(FVector P, FVector Extent, FRotator Rotation=FRotator::ZeroRotator)
    {
        return RogueTestWorld::StaticBox(World, P, Extent, Rotation);
    }
    ARogueEnemy* Enemy(FVector P,ERogueEnemyType Type=ERogueEnemyType::Juggernaut,UClass* Class=ARogueEnemy::StaticClass())
    {
        FActorSpawnParameters Params;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* E=World->SpawnActor<ARogueEnemy>(Class,P,FRotator::ZeroRotator,Params);
        E->ActivatePooledEnemy(nullptr,P,FRotator::ZeroRotator);E->InitializeEnemy({Type,1000,240,0,0,false});E->SetActorTickEnabled(false);
        World->GetSubsystem<URogueEnemyTrackerSubsystem>()->RegisterEnemy(E);return E;
    }
    ARogueCharacter* Player(FVector P)
    {
        auto* PC = RogueTestWorld::SpawnFreshController(World);
        auto* C=World->SpawnActorDeferred<ARogueCharacter>(ARogueCharacter::StaticClass(),FTransform(P),nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        C->AutoPossessPlayer=EAutoReceiveInput::Disabled;C->FinishSpawning(FTransform(P));PC->Possess(C);C->SetActorTickEnabled(false);return C;
    }
    void Step(){Test.TickTestWorld(1.f/60);}
    bool Reach(ARogueEnemy* E,FVector Goal,int32 Frames=1800)
    {
        for(int32 I=0;I<Frames;++I)
        {
            if(FVector::Dist2D(E->GetActorLocation(),Goal)<75)return true;
            E->MoveTowardsLocation(Goal,1,1.f/60);Step();
        }
        return false;
    }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyLongWallTest,"Rogue.Gameplay.EnemyObstacles.LegacyReproAndLongBuildingDetour",RogueObstacleTests::Flags)
bool FEnemyLongWallTest::RunTest(const FString&)
{
    using namespace RogueObstacleTests;FFixture F;if(!F.Init())return false;
    F.Box(FVector(0,0,250),FVector(80,650,250));
    auto* E=F.Enemy(FVector(-650,0,74));const FVector Goal(650,0,74);
    // Reproduce the old movement against real collision, then exercise the shared action.
    for(int32 I=0;I<300;++I)
    {
        E->AddActorWorldOffset((Goal-E->GetActorLocation()).GetSafeNormal2D()*240/60,true);
        E->GetCharacterMovement()->bForceNextFloorCheck=true;F.Step();
    }
    TestTrue(TEXT("Legacy direct sweep remains stuck at the building"),E->GetActorLocation().X<-100);
    E->DeactivateToPool();E->ActivatePooledEnemy(nullptr,FVector(-650,0,74),FRotator::ZeroRotator);E->InitializeEnemy({ERogueEnemyType::Juggernaut,1000,240,0,0,false});E->SetActorTickEnabled(false);
    TestTrue(TEXT("Enemy reaches the far side by walking around the long building"),F.Reach(E,Goal));
    TestTrue(TEXT("Detour remains on the floor"),FMath::Abs(E->GetActorLocation().Z-E->GetCapsuleComponent()->GetScaledCapsuleHalfHeight())<5);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyConcaveWallTest,"Rogue.Gameplay.EnemyObstacles.ConcaveUShapeAndRotatedBuilding",RogueObstacleTests::Flags)
bool FEnemyConcaveWallTest::RunTest(const FString&)
{
    using namespace RogueObstacleTests;
    {
        FFixture F;if(!F.Init())return false;
        F.Box(FVector(0,0,250),FVector(80,550,250));
        F.Box(FVector(-450,500,250),FVector(450,50,250));
        F.Box(FVector(-450,-500,250),FVector(450,50,250));
        auto* E=F.Enemy(FVector(-400,0,74));
        TestTrue(TEXT("A concave courtyard is exited instead of switching sides forever"),F.Reach(E,FVector(650,0,74),3000));
    }
    {
        FFixture F;if(!F.Init())return false;
        F.Box(FVector(0,0,250),FVector(100,450,250),FRotator(0,35,0));
        auto* E=F.Enemy(FVector(-650,0,74));
        TestTrue(TEXT("Non-axis-aligned collision is routed around"),F.Reach(E,FVector(650,0,74)));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyFlightObstacleTest,"Rogue.Gameplay.EnemyObstacles.FlightKeepsAltitudeAndAvoidsTallWalls",RogueObstacleTests::Flags)
bool FEnemyFlightObstacleTest::RunTest(const FString&)
{
    using namespace RogueObstacleTests;FFixture F;if(!F.Init())return false;
    auto* E=F.Enemy(FVector(-650,0,74),ERogueEnemyType::SkyMoth);const float Height=E->GetActorLocation().Z;
    const float LowTop=FMath::Max(10.f,Height-E->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()-20);
    F.Box(FVector(0,0,LowTop*.5f),FVector(80,550,LowTop*.5f));
    const bool LowReached=F.Reach(E,FVector(650,0,Height),400);
    AddInfo(FString::Printf(TEXT("Flight startZ=%.1f final=%s speed=%.1f"),Height,*E->GetActorLocation().ToString(),E->GetMoveSpeed()));
    TestTrue(TEXT("Flying enemy passes above a low obstacle"),LowReached);
    TestTrue(TEXT("Low-obstacle route retains flight height"),FMath::Abs(E->GetActorLocation().Z-Height)<1);
    E->DeactivateToPool();E->ActivatePooledEnemy(nullptr,FVector(-650,1500,74),FRotator::ZeroRotator);E->InitializeEnemy({ERogueEnemyType::SkyMoth,1000,240,0,0,false});E->SetActorTickEnabled(false);
    const float TallHeight=E->GetActorLocation().Z;
    F.Box(FVector(0,1500,TallHeight),FVector(80,450,400));
    TestTrue(TEXT("Tall walls are avoided instead of trapping flying enemies"),F.Reach(E,FVector(650,1500,TallHeight)));
    TestTrue(TEXT("Tall-wall detour does not climb or fall"),FMath::Abs(E->GetActorLocation().Z-TallHeight)<1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyActualBlueprintObstacleTest,"Rogue.Gameplay.EnemyObstacles.ActualBlueprintNativeBehaviorAndStationary",RogueObstacleTests::Flags)
bool FEnemyActualBlueprintObstacleTest::RunTest(const FString&)
{
    using namespace RogueObstacleTests;FFixture F;if(!F.Init())return false;
    auto* Player=F.Player(FVector(650,0,96));F.Box(FVector(0,0,250),FVector(80,450,250));
    auto* Class=LoadClass<ARogueEnemy>(nullptr,*RogueEnemyAssetPaths::EnemyClassPath(TEXT("Juggernaut")));
    if(!TestNotNull(TEXT("Actual enemy Blueprint"),Class))return false;
    auto* E=F.Enemy(FVector(-650,0,74),ERogueEnemyType::Juggernaut,Class);
    if(!TestNotNull(TEXT("Existing child BP inherits obstacle component"),E->ObstacleMovement.Get()))return false;
    bool Reached=false;
    for(int32 I=0;I<1800;++I)
    {
        E->Tick(1.f/60);F.Step();
        if(FVector::Dist2D(E->GetActorLocation(),Player->GetActorLocation())<100){Reached=true;break;}
    }
    TestTrue(TEXT("Real Blueprint/native AI reaches the player round a building"),Reached);
    auto* Pillar=F.Enemy(FVector(-650,1200,74),ERogueEnemyType::ShockPillar);
    const FVector Fixed=Pillar->GetActorLocation();
    for(int32 I=0;I<60;++I){Pillar->Tick(1.f/60);F.Step();}
    TestTrue(TEXT("Stationary enemies do not acquire autonomous movement"),FVector::Dist2D(Fixed,Pillar->GetActorLocation())<1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyHealerObstacleTest,"Rogue.Gameplay.EnemyObstacles.HealerRoutesToWoundedAlly",RogueObstacleTests::Flags)
bool FEnemyHealerObstacleTest::RunTest(const FString&)
{
    using namespace RogueObstacleTests;FFixture F;if(!F.Init())return false;
    F.Player(FVector(-3000,-2000,74));F.Box(FVector(0,0,250),FVector(80,500,250));
    auto* Ally=F.Enemy(FVector(650,0,74));Ally->GetRogueAttributeSet()->SetHealth(300);
    auto* E=F.Enemy(FVector(-650,0,74),ERogueEnemyType::LinkHealer);
    auto* Support=E->FindComponentByClass<URogueEnemySupportComponent>();
    bool Reached=false;
    for(int32 I=0;I<1800;++I)
    {
        const FVector Direction=Support->CalculateSupportMovement(1.f/60);
        E->MoveInDirection(Direction,1,1.f/60);F.Step();
        if(E->GetActorLocation().X>150&&Ally->GetCurrentHealth()>300){Reached=true;break;}
    }
    AddInfo(FString::Printf(TEXT("Healer final=%s target=%s allyHP=%.1f"),*E->GetActorLocation().ToString(),*GetNameSafe(Support->GetMovementTarget()),Ally->GetCurrentHealth()));
    TestTrue(TEXT("Healer walks round the building and resumes actual healing"),Reached);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyObstaclePoolBudgetTest,"Rogue.Gameplay.EnemyObstacles.BudgetPoolResetAndAuthority",RogueObstacleTests::Flags)
bool FEnemyObstaclePoolBudgetTest::RunTest(const FString&)
{
    using namespace RogueObstacleTests;FFixture F;if(!F.Init())return false;
    F.Box(FVector(0,0,250),FVector(80,500,250));
    auto* Budget=F.World->GetSubsystem<URogueEnemyNavigationBudget>();
    ARogueEnemy* Last=nullptr;
    for(int32 I=0;I<80;++I)
    {
        Last=F.Enemy(FVector(-150,0,74));FHitResult Hit;Hit.bBlockingHit=true;Hit.ImpactNormal=-FVector::ForwardVector;
        Last->ObstacleMovement->NotifyBlocked(Hit,FVector::ForwardVector);
        Last->MoveTowardsLocation(FVector(650,0,74),1,1.f/60);
    }
    TestEqual(TEXT("Large crowds share a bounded per-frame steering budget"),Budget->GetDecisionsUsed(),URogueEnemyNavigationBudget::MaxDecisionsPerFrame);
    TestTrue(TEXT("Path queries are bounded independently"),Budget->GetPathsUsed()<=URogueEnemyNavigationBudget::MaxPathsPerFrame);
    TestTrue(TEXT("Blocked enemy is in a detour"),Last->ObstacleMovement->IsDetouring());
    Last->DeactivateToPool();TestFalse(TEXT("Pool return discards previous detour"),Last->ObstacleMovement->IsDetouring());
    Last->ActivatePooledEnemy(nullptr,FVector(1800,1800,74),FRotator::ZeroRotator);
    TestFalse(TEXT("Pool reuse starts without an old route"),Last->ObstacleMovement->IsDetouring());
    const FVector Before=Last->GetActorLocation();Last->SetRole(ROLE_SimulatedProxy);
    Last->MoveTowardsLocation(FVector(2500,1800,74),1,1.f/60);
    TestTrue(TEXT("Client proxies do not run their own movement"),Last->GetActorLocation().Equals(Before));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyStaleWallRegression,"Rogue.Gameplay.EnemyObstacles.KnockbackDoesNotLeaveStaleWallOrbit",RogueObstacleTests::Flags)
bool FEnemyStaleWallRegression::RunTest(const FString&)
{
    using namespace RogueObstacleTests;FFixture F;if(!F.Init())return false;
    F.Box(FVector(0,0,250),FVector(80,650,250));
    auto* E=F.Enemy(FVector(-150,0,74));const FVector Goal(650,0,74);
    for(int32 I=0;I<30;++I){E->MoveTowardsLocation(Goal,1,1.f/60);F.Step();}
    TestTrue(TEXT("A real wall collision started a detour"),E->ObstacleMovement->IsDetouring());
    // Knockback can move an enemy beyond the short side probe while its target is still occluded.
    E->SetActorLocation(FVector(-750,0,74),false);
    TestTrue(TEXT("After displacement the enemy finds a route, rather than orbiting in empty space"),F.Reach(E,Goal,1800));
    return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyRoutingStaggeredTest,"Rogue.Gameplay.EnemyObstacles.StaggeredBuildingsAndLowTickRate",RogueObstacleTests::Flags)
bool FEnemyRoutingStaggeredTest::RunTest(const FString&)
{
    using namespace RogueObstacleTests;FFixture F;if(!F.Init())return false;
    F.Box(FVector(-250,220,250),FVector(100,550,250),FRotator(0,12,0));
    F.Box(FVector(300,-200,250),FVector(100,550,250),FRotator(0,-12,0));
    auto* E=F.Enemy(FVector(-1100,0,74));const FVector Goal(1100,0,74);
    bool Reached=false;float Travel=0;
    for(int32 I=0;I<1600;++I)
    {
        const FVector Before=E->GetActorLocation();E->MoveTowardsLocation(Goal,1,.08f);F.Step();Travel+=FVector::Dist2D(Before,E->GetActorLocation());
        if(FVector::Dist2D(E->GetActorLocation(),Goal)<55){Reached=true;break;}
    }
    if(!Reached)AddInfo(FString::Printf(TEXT("Staggered final=%s travel=%.1f %s"),*E->GetActorLocation().ToCompactString(),Travel,*E->ObstacleMovement->DescribeRoute()));
    TestTrue(TEXT("Low-frequency movement passes staggered angled buildings"),Reached);
    TestTrue(TEXT("The path does not accumulate repeated loops"),Travel<8000);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyRoutingReplanTest,"Rogue.Gameplay.EnemyObstacles.MovingGoalAndNewCollisionInvalidateRoute",RogueObstacleTests::Flags)
bool FEnemyRoutingReplanTest::RunTest(const FString&)
{
    using namespace RogueObstacleTests;FFixture F;if(!F.Init())return false;
    F.Box(FVector(0,0,250),FVector(80,500,250));auto* E=F.Enemy(FVector(-650,0,74));
    for(int32 I=0;I<100;++I){E->MoveTowardsLocation(FVector(650,0,74),1,1.f/60);F.Step();}
    const FVector ChangedGoal(-1000,850,74);
    TestTrue(TEXT("A moved target does not preserve an obsolete detour"),F.Reach(E,ChangedGoal,800));
    E->ObstacleMovement->ResetForPool();E->SetActorLocation(FVector(-650,0,74));
    for(int32 I=0;I<100;++I){E->MoveTowardsLocation(FVector(650,0,74),1,1.f/60);F.Step();}
    F.Box(E->GetActorLocation()+FVector(80,0,250),FVector(20,220,250),FRotator(0,25,0));
    TestTrue(TEXT("A new blocking object causes replanning, not a permanent orbit"),F.Reach(E,FVector(650,0,74),1800));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyRoutingUnreachableTest,"Rogue.Gameplay.EnemyObstacles.SealedGoalWaitsWithoutSpinningAndCanResume",RogueObstacleTests::Flags)
bool FEnemyRoutingUnreachableTest::RunTest(const FString&)
{
    using namespace RogueObstacleTests;FFixture F;if(!F.Init())return false;
    F.Box(FVector(0,0,250),FVector(400,400,250));auto* E=F.Enemy(FVector(-650,0,74));
    const FVector SealedGoal(0,0,74);float LateTravel=0,LateTurn=0;
    for(int32 I=0;I<1500;++I)
    {
        const FVector Before=E->GetActorLocation();const float Yaw=E->GetActorRotation().Yaw;
        E->MoveTowardsLocation(SealedGoal,1,1.f/60);F.Step();
        if(I>=1200){LateTravel+=FVector::Dist2D(Before,E->GetActorLocation());LateTurn+=FMath::Abs(FMath::FindDeltaAngleDegrees(Yaw,E->GetActorRotation().Yaw));}
    }
    AddInfo(FString::Printf(TEXT("Sealed goal late travel=%.1f turn=%.1f"),LateTravel,LateTurn));
    TestTrue(TEXT("An unreachable target does not induce perpetual orbiting"),LateTravel<40&&LateTurn<30);
    TestTrue(TEXT("An accessible new destination wakes the waiting enemy"),F.Reach(E,FVector(-900,1000,74),900));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyRoutingCrowdTest,"Rogue.Gameplay.EnemyObstacles.CrowdQueriesAreBoundedAndEveryEnemyProgresses",RogueObstacleTests::Flags)
bool FEnemyRoutingCrowdTest::RunTest(const FString&)
{
    using namespace RogueObstacleTests;FFixture F;if(!F.Init())return false;
    F.Box(FVector(0,0,250),FVector(80,550,250));const FVector Goal(650,0,74);
    TArray<ARogueEnemy*> Enemies;for(int32 I=0;I<80;++I)Enemies.Add(F.Enemy(FVector(-450-(I%4)*20,(I%9-4)*12,74)));
    auto* Budget=F.World->GetSubsystem<URogueEnemyNavigationBudget>();int32 Reached=0,PeakQueries=0;int64 Queries=0;
    const double Start=FPlatformTime::Seconds();
    for(int32 Frame=0;Frame<2100;++Frame)
    {
        for (ARogueEnemy* Enemy : Enemies)
        {
            if (Enemy->IsAvailableInPool()) { continue; }
            if (FVector::Dist2D(Enemy->GetActorLocation(), Goal) < 75.f)
            {
                // The finish area has limited space. Clear each arrival to measure route throughput.
                ++Reached;
                Enemy->DeactivateToPool();
            }
            else
            {
                Enemy->MoveTowardsLocation(Goal, 1, 1.f / 60.f);
            }
        }
        PeakQueries=FMath::Max(PeakQueries,Budget->GetCollisionQueriesUsed());Queries+=Budget->GetCollisionQueriesUsed();
        if(Reached==Enemies.Num())break;F.Step();
    }
    AddInfo(FString::Printf(TEXT("Crowd reached=%d/80 queries=%lld peak=%d CPU wall=%.3fs"),Reached,Queries,PeakQueries,FPlatformTime::Seconds()-Start));
    TestTrue(TEXT("Collision steering work obeys a hard world/frame budget"),PeakQueries<=URogueEnemyNavigationBudget::MaxCollisionQueriesPerFrame);
    if(Reached<80)for(auto* E:Enemies)if(!E->IsAvailableInPool())AddInfo(FString::Printf(TEXT("Waiting crowd %s %s"),*E->GetActorLocation().ToCompactString(),*E->ObstacleMovement->DescribeRoute()));
    TestEqual(TEXT("Budget sharing does not starve late enemies"),Reached,80);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyRoutingPenetrationTest,"Rogue.Gameplay.EnemyObstacles.InitialOverlapAndFastArrivalDoNotOscillate",RogueObstacleTests::Flags)
bool FEnemyRoutingPenetrationTest::RunTest(const FString&)
{
    using namespace RogueObstacleTests;FFixture F;if(!F.Init())return false;
    F.Box(FVector(0,0,250),FVector(80,500,250));auto* E=F.Enemy(FVector(-100,0,74));
    TestTrue(TEXT("A shallow initial wall overlap is resolved before routing"),F.Reach(E,FVector(650,0,74),1800));
    E->ObstacleMovement->ResetForPool();E->SetActorLocation(FVector(1500,0,74));const FVector Goal(1530,0,74);
    for(int32 I=0;I<30;++I){E->MoveTowardsLocation(Goal,8,.12f);F.Step();}
    TestTrue(TEXT("A fast last movement step stops at the destination instead of overshooting"),FVector::Dist2D(E->GetActorLocation(),Goal)<1);
    return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyFastHealerRoutingTest,"Rogue.Gameplay.EnemyObstacles.FastHealerBrakesAtCornersInsteadOfOrbiting",RogueObstacleTests::Flags)
bool FEnemyFastHealerRoutingTest::RunTest(const FString&)
{
    using namespace RogueObstacleTests;FFixture F;if(!F.Init())return false;
    F.Box(FVector(0,0,250),FVector(80,650,250));
    auto* E=F.Enemy(FVector(-650,0,74),ERogueEnemyType::LinkHealer);E->InitializeEnemy({ERogueEnemyType::LinkHealer,1000,900,0,0,false});E->SetActorTickEnabled(false);
    TestTrue(TEXT("High-speed CharacterMovement follows a corner route without orbiting its waypoint"),F.Reach(E,FVector(650,0,74),1600));
    return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyGroundGapRoutingTest,"Rogue.Gameplay.EnemyObstacles.GroundGapUsesBridgeInsteadOfShortcutIntoVoid",RogueObstacleTests::Flags)
bool FEnemyGroundGapRoutingTest::RunTest(const FString&)
{
    using namespace RogueObstacleTests;FFixture F;if(!F.Init(false))return false;
    F.Box(FVector(-600,0,-50),FVector(600,1000,50));F.Box(FVector(700,0,-50),FVector(500,1000,50));
    F.Box(FVector(100,650,-50),FVector(150,100,50));
    for(auto Type:{ERogueEnemyType::Juggernaut,ERogueEnemyType::LinkHealer})
    {
        auto* E=F.Enemy(FVector(-600,0,74),Type);float MinZ=74;bool Reached=false;TArray<FString> Trace;
        for(int32 I=0;I<1800;++I)
        {
            E->MoveTowardsLocation(FVector(600,0,74),1,1.f/60);F.Step();if(I%300==0)Trace.Add(FString::Printf(TEXT("frame=%d P=%s %s"),I,*E->GetActorLocation().ToCompactString(),*E->ObstacleMovement->DescribeRoute()));MinZ=FMath::Min(MinZ,float(E->GetActorLocation().Z));
            if(FVector::Dist2D(E->GetActorLocation(),FVector(600,0,74))<75){Reached=true;break;}
        }
        if(!Reached){AddInfo(FString::Printf(TEXT("Bridge type=%d P=%s %s"),int32(Type),*E->GetActorLocation().ToCompactString(),*E->ObstacleMovement->DescribeRoute()));for(const FString& Point:Trace)AddInfo(Point);}
        TestTrue(TEXT("Ground routing finds the actual connecting bridge"),Reached);
        TestTrue(TEXT("The enemy does not fall into the gap"),MinZ>65);
        E->DeactivateToPool();
    }
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyNarrowRoutingTest,"Rogue.Gameplay.EnemyObstacles.NarrowPassageRefinesGridWithoutOrbiting",RogueObstacleTests::Flags)
bool FEnemyNarrowRoutingTest::RunTest(const FString&)
{
    using namespace RogueObstacleTests;FFixture F;if(!F.Init())return false;
    F.Box(FVector(0,-150,250),FVector(2500,50,250));F.Box(FVector(0,250,250),FVector(2500,50,250));
    F.Box(FVector(0,-50.5,250),FVector(80,49.5,250));F.Box(FVector(0,148.5,250),FVector(80,51.5,250));
    auto* E=F.Enemy(FVector(-600,0,74));
    const bool Reached=F.Reach(E,FVector(600,0,74),2400);
    if(!Reached)AddInfo(FString::Printf(TEXT("Narrow P=%s %s"),*E->GetActorLocation().ToCompactString(),*E->ObstacleMovement->DescribeRoute()));
    TestTrue(TEXT("An off-grid 98cm opening is found after refining the collision grid"),Reached);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyCrowdChaseTest,
    "Rogue.Gameplay.EnemyObstacles.OverlappingPackSpreadsWhileChasing", RogueObstacleTests::Flags)
bool FEnemyCrowdChaseTest::RunTest(const FString&)
{
    using namespace RogueObstacleTests;
    FFixture F;
    if (!F.Init()) { return false; }
    auto* Player = F.Player(FVector(0, 0, 98));
    auto* Archetypes = NewObject<UDataTable>(F.World);
    Archetypes->RowStruct = FRogueEnemyArchetypeRow::StaticStruct();
    FRogueEnemyArchetypeRow Melee;
    Melee.EnemyType = ERogueEnemyType::Juggernaut;
    Melee.bContactOnlyNormal = true;
    Archetypes->AddRow(TEXT("Juggernaut"), Melee);
    const auto* ArchetypeProperty = FindFProperty<FObjectPropertyBase>(ARogueEnemy::StaticClass(), TEXT("EnemyArchetypeDataTable"));
    if (!TestNotNull(TEXT("The enemy uses its authored archetype property"), ArchetypeProperty)) { return false; }
    TArray<ARogueEnemy*> Enemies;
    for (int32 Index = 0; Index < 12; ++Index)
    {
        auto* Enemy = F.Enemy(FVector(-550, 0, 74));
        ArchetypeProperty->SetObjectPropertyValue_InContainer(Enemy, Archetypes);
        Enemy->InitializeEnemy({ERogueEnemyType::Juggernaut, 1000, 240, 0, 0, false});
        Enemies.Add(Enemy);
    }

    for (int32 Frame = 0; Frame < 300; ++Frame)
    {
        for (ARogueEnemy* Enemy : Enemies)
            Enemy->Tick(1.f / 60.f);
        F.Step();
    }

    float ClosestToPlayer = TNumericLimits<float>::Max();
    float LowestY = TNumericLimits<float>::Max();
    float HighestY = -LowestY;
    int32 OverlappingPairs = 0;
    for (int32 Index = 0; Index < Enemies.Num(); ++Index)
    {
        const auto* Enemy = Enemies[Index];
        const FVector Position = Enemy->GetActorLocation();
        const auto* Capsule = Enemy->GetCapsuleComponent();
        LowestY = FMath::Min(LowestY, float(Position.Y));
        HighestY = FMath::Max(HighestY, float(Position.Y));
        const float PlayerDistance = FVector::Dist2D(Position, Player->GetActorLocation());
        ClosestToPlayer = FMath::Min(ClosestToPlayer, PlayerDistance);
        TestTrue(TEXT("Melee crowd keeps outside the player capsule"), PlayerDistance >=
            Capsule->GetScaledCapsuleRadius() + Player->GetCapsuleComponent()->GetScaledCapsuleRadius() - 1.f);
        TestTrue(TEXT("Crowd steering keeps each enemy on the floor"),
            FMath::Abs(Position.Z - Capsule->GetScaledCapsuleHalfHeight()) < 5.f);
        for (int32 OtherIndex = Index + 1; OtherIndex < Enemies.Num(); ++OtherIndex)
        {
            const auto* Other = Enemies[OtherIndex];
            const float PairRadius = Capsule->GetScaledCapsuleRadius() + Other->GetCapsuleComponent()->GetScaledCapsuleRadius();
            if (FVector::DistSquared2D(Position, Other->GetActorLocation()) < FMath::Square(PairRadius - 5.f))
                ++OverlappingPairs;
        }
    }
    AddInfo(FString::Printf(TEXT("Pack width %.1f cm, overlapping pairs %d, closest player distance %.1f cm"),
        HighestY - LowestY, OverlappingPairs, ClosestToPlayer));
    TestTrue(TEXT("The crowd spreads laterally while sharing one target"), HighestY - LowestY > 225.f);
    TestEqual(TEXT("The original stack no longer intersects"), OverlappingPairs, 0);
    TestTrue(TEXT("At least one enemy still reaches contact range"),
        Enemies.ContainsByPredicate([](const ARogueEnemy* Enemy) { return Enemy->HasTouchingPlayer(); }));
    return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyWideBlockerRecoveryTest,
    "Rogue.Gameplay.EnemyObstacles.Recovery.WideBlockerBeyondOriginalSearchBounds",RogueObstacleTests::Flags)
bool FEnemyWideBlockerRecoveryTest::RunTest(const FString&)
{
    using namespace RogueObstacleTests;FFixture F;if(!F.Init())return false;
    F.Box(FVector(0,0,250),FVector(80,3100,250));
    auto* E=F.Enemy(FVector(-600,0,74));const FVector Goal(600,0,74);
    const bool Reached=F.Reach(E,Goal,4000);
    AddInfo(FString::Printf(TEXT("Wide blocker final=%s %s"),*E->GetActorLocation().ToCompactString(),*E->ObstacleMovement->DescribeRoute()));
    return TestTrue(TEXT("A reachable route beyond the initial local search is eventually found"),Reached);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyStairsRecoveryTest,
    "Rogue.Gameplay.EnemyObstacles.Recovery.WalkableStepsUseFullCapsuleClearance",RogueObstacleTests::Flags)
bool FEnemyStairsRecoveryTest::RunTest(const FString&)
{
    using namespace RogueObstacleTests;FFixture F;if(!F.Init())return false;
    // Eight 20cm steps connect the floor to a 160cm platform; no route exists around the raised target.
    for(int32 I=0;I<8;++I)
        F.Box(FVector(-400+I*100,0,(I+1)*10),FVector(50,450,(I+1)*10));
    F.Box(FVector(750,0,80),FVector(400,450,80));
    auto* E=F.Enemy(FVector(-700,0,74));const FVector Goal(750,0,234);
    const bool Reached=F.Reach(E,Goal,1800);
    AddInfo(FString::Printf(TEXT("Stairs final=%s %s"),*E->GetActorLocation().ToCompactString(),*E->ObstacleMovement->DescribeRoute()));
    TestTrue(TEXT("Ordinary enemies walk up valid steps"),Reached);
    return TestTrue(TEXT("Arrival is on the platform, not below it"),E->GetActorLocation().Z>225);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyStationaryGoalRecoveryTest,
    "Rogue.Gameplay.EnemyObstacles.Recovery.ZeroProgressDoesNotRemainOnStalePath",RogueObstacleTests::Flags)
bool FEnemyStationaryGoalRecoveryTest::RunTest(const FString&)
{
    using namespace RogueObstacleTests;FFixture F;if(!F.Init())return false;
    F.Box(FVector(0,0,250),FVector(80,650,250));
    auto* E=F.Enemy(FVector(-650,0,74));const FVector Goal(650,0,74);
    for(int32 I=0;I<120;++I){E->MoveTowardsLocation(Goal,1,1.f/60);F.Step();}
    auto* Block=F.Box(E->GetActorLocation()+FVector(0,0,120),FVector(150,150,120));
    for(int32 I=0;I<180;++I){E->MoveTowardsLocation(Goal,1,1.f/60);F.Step();}
    Block->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    return TestTrue(TEXT("A temporarily jammed route resumes after the blocker is removed"),F.Reach(E,Goal,2400));
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyLowCeilingRecoveryTest,
    "Rogue.Gameplay.EnemyObstacles.Recovery.LowCeilingCannotUseSmallerPlanningCapsule",RogueObstacleTests::Flags)
bool FEnemyLowCeilingRecoveryTest::RunTest(const FString&)
{
    using namespace RogueObstacleTests;FFixture F;if(!F.Init())return false;
    F.Box(FVector(0,0,244),FVector(200,300,100));
    auto* E=F.Enemy(FVector(-650,0,74));
    return TestTrue(TEXT("A beam that clears the old short probe but clips the real capsule is routed around"),F.Reach(E,FVector(650,0,74),2000));
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnemyContinuouslyMovingGoalTest,
    "Rogue.Gameplay.EnemyObstacles.Recovery.MovingPlayerDoesNotCancelEverySearch",RogueObstacleTests::Flags)
bool FEnemyContinuouslyMovingGoalTest::RunTest(const FString&)
{
    using namespace RogueObstacleTests;FFixture F;if(!F.Init())return false;
    F.Box(FVector(0,0,250),FVector(80,1100,250));
    auto* E=F.Enemy(FVector(-650,0,74));bool Crossed=false;
    for(int32 I=0;I<2400;++I)
    {
        const FVector Goal(650,650*FMath::Sin(I/60.f*1.5f),74);
        E->MoveTowardsLocation(Goal,1,1.f/60);F.Step();
        if(E->GetActorLocation().X>200){Crossed=true;break;}
    }
    AddInfo(FString::Printf(TEXT("Moving target final=%s %s"),*E->GetActorLocation().ToCompactString(),*E->ObstacleMovement->DescribeRoute()));
    return TestTrue(TEXT("Continuous player movement still lets the enemy finish planning round the building"),Crossed);
}

#endif
