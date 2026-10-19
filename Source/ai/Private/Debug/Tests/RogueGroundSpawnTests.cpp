#if WITH_DEV_AUTOMATION_TESTS

#include "Combat/Pooling/RogueCombatPoolSubsystem.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Enemies/RogueEnemy.h"
#include "Enemies/Spawning/RogueSpawnSubsystem.h"
#include "Framework/RogueGameMode.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/WorldSettings.h"
#include "Misc/AutomationTest.h"
#include "Player/RogueCharacter.h"
#include "Player/RoguePlayerController.h"
#include "Tests/AutomationCommon.h"
#include "World/RogueEnemySpawnRegion.h"
#include "World/RogueShopTerminal.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SplineComponent.h"
#include "Engine/PointLight.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"

namespace RogueGroundSpawnTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;
	struct FFixture
	{
		FTestConsoleVariable SelfTest{TEXT("Rogue.SelfTest.WeaponUpgradeChain")};
		FTestConsoleVariable Stress{TEXT("Rogue.SelfTest.EnemyStress")};
		FTestWorldWrapper TestWorld;
		UWorld* World = nullptr;
		ARogueCharacter* Player = nullptr;
		ARogueEnemySpawnRegion* Region = nullptr;
		URogueSpawnSubsystem* Spawner = nullptr;
		UClass* EnemyClass = nullptr;
		FRogueSpawnSettings Settings;
		FRogueBossSettings BossSettings;
		FRogueRunState State;

		bool Initialize(FAutomationTestBase& Test)
		{
			SelfTest.Set(TEXT("0")); Stress.Set(TEXT("0"));
			if (!TestWorld.CreateTestWorld(EWorldType::Game)) { return false; }
			World = TestWorld.GetTestWorld();
			World->GetWorldSettings()->DefaultGameMode = ARogueGameMode::StaticClass();
			if (!TestWorld.BeginPlayInTestWorld()) { return false; }
			ARogueGameMode* Mode = World->GetAuthGameMode<ARogueGameMode>();
			if (!Test.TestNotNull(TEXT("The authoritative GameMode exists"), Mode)) { return false; }
			Mode->SetActorTickEnabled(false);
			ARoguePlayerController* Controller = World->SpawnActor<ARoguePlayerController>();
			const FTransform Transform(FVector(0.0f, 0.0f, 1000.0f));
			Player = World->SpawnActorDeferred<ARogueCharacter>(ARogueCharacter::StaticClass(), Transform,
				nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
			if (!Test.TestNotNull(TEXT("The player exists"), Player) || !Test.TestNotNull(TEXT("The controller exists"), Controller)) { return false; }
			Player->AutoPossessPlayer = EAutoReceiveInput::Disabled;
			Player->FinishSpawning(Transform);
			Controller->Possess(Player);
			Player->SetActorTickEnabled(false);
			Player->GetCharacterMovement()->SetComponentTickEnabled(false);
			Player->SetActorEnableCollision(false);
			Spawner = World->GetSubsystem<URogueSpawnSubsystem>();
			EnemyClass = LoadClass<ARogueEnemy>(nullptr, TEXT("/Game/BP/Enemy/BP_EnemyHunter.BP_EnemyHunter_C"));
			if (!Test.TestNotNull(TEXT("The actual spawner exists"), Spawner) || !Test.TestNotNull(TEXT("The actual enemy Blueprint loads"), EnemyClass)) { return false; }
			Settings.SpawnRadius = 1500.0f;
			Settings.EnemySpawnMinimumDistance = 300.0f;
			Settings.GroundTraceHeight = 2000.0f;
			Settings.GroundTraceDepth = 10000.0f;
			Settings.GroundClearance = 2.0f;
			BossSettings.BossSpawnDistance = 1500.0f;
			BossSettings.BossSpawnMinimumDistance = 300.0f;
			Region = World->SpawnActor<ARogueEnemySpawnRegion>();
			Region->SetActorLocation(FVector(0, 0, 1000));
			Region->GetSpawnBounds()->SetBoxExtent(FVector(5000, 5000, 6000));
			Configure();
			return Test.TestTrue(TEXT("The reference player is alive"), Player->IsAlive());
		}
		void Configure()
		{
			Spawner->Configure(EnemyClass, TMap<ERogueEnemyType, TSubclassOf<ARogueEnemy>>(), Settings, BossSettings, nullptr, nullptr);
		}
		UBoxComponent* Box(const FVector& Center, const FVector& Extent, const FRotator& Rotation = FRotator::ZeroRotator)
		{
			AActor* Actor = World->SpawnActor<AActor>();
			UBoxComponent* Shape = NewObject<UBoxComponent>(Actor);
			Actor->SetRootComponent(Shape);
			Shape->SetBoxExtent(Extent);
			Shape->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
			Shape->SetCollisionObjectType(ECC_WorldStatic);
			Shape->SetCollisionResponseToAllChannels(ECR_Block);
			Shape->RegisterComponent();
			Actor->SetActorLocationAndRotation(Center, Rotation);
			return Shape;
		}
		ARogueEnemy* ActiveEnemy() const
		{
			for (TActorIterator<ARogueEnemy> It(World); It; ++It)
			{
				if (!It->IsAvailableInPool()) { return *It; }
			}
			return nullptr;
		}
		ARogueEnemy* Spawn(FAutomationTestBase& Test)
		{
			if (!Test.TestTrue(TEXT("The real enemy spawn succeeds"), Spawner->SpawnEnemy(State, Player, Region))) { return nullptr; }
			ARogueEnemy* Enemy = ActiveEnemy();
			Test.TestNotNull(TEXT("A pooled enemy is active"), Enemy);
			return Enemy;
		}
		void CheckHeight(FAutomationTestBase& Test, ARogueEnemy* Enemy, float FloorZ) const
		{
			const float Expected = FloorZ + Enemy->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + Settings.GroundClearance;
			Test.TestTrue(TEXT("Actor height is the detected surface plus its actual capsule half height and clearance"),
				FMath::IsNearlyEqual(Enemy->GetActorLocation().Z, Expected, 0.15f));
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueGroundSpawnHeightReuseTest,
	"Rogue.Gameplay.GroundSpawn.RaisedLoweredGroundAndActualPooledCapsule", RogueGroundSpawnTests::Flags)
bool FRogueGroundSpawnHeightReuseTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RogueGroundSpawnTests;
	FFixture F; if (!F.Initialize(*this)) { return false; }
	UBoxComponent* Floor = F.Box(FVector(0.0f, 0.0f, 480.0f), FVector(20000.0f, 20000.0f, 20.0f));
	URogueCombatPoolSubsystem* Pools = F.World->GetSubsystem<URogueCombatPoolSubsystem>();
	if (!TestNotNull(TEXT("The actual pool exists"), Pools)) { return false; }
	ARogueEnemy* Prepared = Pools->AcquireEnemy(F.EnemyClass, nullptr, FVector::ZeroVector, FRotator::ZeroRotator);
	if (!TestNotNull(TEXT("A Blueprint enemy is acquired without activation"), Prepared)) { return false; }
	Prepared->GetCapsuleComponent()->SetCapsuleSize(50.0f, 150.0f);
	Prepared->SetActorScale3D(FVector(1.4f));
	ARogueEnemy* First = F.Spawn(*this); if (First == nullptr) { return false; }
	TestTrue(TEXT("The previously acquired Blueprint actor is used"), First == Prepared);
	F.CheckHeight(*this, First, 500.0f);
	First->DeactivateToPool();
	Floor->GetOwner()->SetActorLocation(FVector(0.0f, 0.0f, -370.0f));
	ARogueEnemy* Second = F.Spawn(*this); if (Second == nullptr) { return false; }
	TestTrue(TEXT("The same pooled actor is reused"), Second == First);
	F.CheckHeight(*this, Second, -350.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueGroundSpawnRejectionTest,
	"Rogue.Gameplay.GroundSpawn.VoidAndBlockedHeadroomDoNotSpawn", RogueGroundSpawnTests::Flags)
bool FRogueGroundSpawnRejectionTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RogueGroundSpawnTests;
	FFixture F; if (!F.Initialize(*this)) { return false; }
	TestFalse(TEXT("No floor never falls back to world Z 60"), F.Spawner->SpawnEnemy(F.State, F.Player, F.Region));
	TestNull(TEXT("A failed search leaves no active enemy"), F.ActiveEnemy());
	F.Spawner->SpawnBoss(F.State, F.Player, F.Region);
	TestFalse(TEXT("A failed Boss search does not mark a Boss alive"), F.State.bBossAlive);
	TestEqual(TEXT("A failed Boss search does not consume the wave"), F.State.LastBossWaveSpawned, 0);
	F.Box(FVector(0.0f, 0.0f, -20.0f), FVector(20000.0f, 20000.0f, 20.0f));
	UBoxComponent* Ceiling = F.Box(FVector(0.0f, 0.0f, 120.0f), FVector(20000.0f, 20000.0f, 20.0f));
	F.Player->SetActorLocation(FVector(0.0f, 0.0f, 80.0f));
	F.Settings.GroundTraceHeight = 0.0f; F.Configure();
	TestFalse(TEXT("A floor hit with insufficient capsule headroom is rejected"), F.Spawner->SpawnEnemy(F.State, F.Player, F.Region));
	Ceiling->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ARogueEnemy* Enemy = F.Spawn(*this);
	if (Enemy) { F.CheckHeight(*this, Enemy, 0.0f); }
	return Enemy != nullptr;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueGroundSpawnSlopeTest,
	"Rogue.Gameplay.GroundSpawn.WalkableSlopeFitsCapsuleAndSteepSlopeRejected", RogueGroundSpawnTests::Flags)
bool FRogueGroundSpawnSlopeTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RogueGroundSpawnTests;
	FFixture F; if (!F.Initialize(*this)) { return false; }
	F.Settings.SpawnRadius = 0.0f; F.Settings.EnemySpawnMinimumDistance = 0.0f; F.Configure();
	UBoxComponent* Floor = F.Box(FVector::ZeroVector, FVector(20000.0f, 20000.0f, 20.0f), FRotator(20.0f, 0.0f, 0.0f));
	ARogueEnemy* Enemy = F.Spawn(*this); if (Enemy == nullptr) { return false; }
	TestTrue(TEXT("Walkable slope placement remains at the requested XY"), Enemy->GetActorLocation().SizeSquared2D() < 1.0f);
	// Confirm actual collision geometry is clear, independently of the spawn helper.
	UCapsuleComponent* Capsule = Enemy->GetCapsuleComponent();
	FCollisionQueryParams Query(SCENE_QUERY_STAT(GroundSpawnSlopeTest), false, Enemy);
	Query.AddIgnoredActor(F.Player);
	TestFalse(TEXT("The actual capsule does not penetrate the slope"), F.World->OverlapBlockingTestByChannel(Enemy->GetActorLocation(),
		Capsule->GetComponentQuat(), ECC_Pawn, FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight()), Query));
	Enemy->DeactivateToPool();
	Floor->GetOwner()->SetActorRotation(FRotator(80.0f, 0.0f, 0.0f));
	TestFalse(TEXT("An unwalkable steep slope is rejected"), F.Spawner->SpawnEnemy(F.State, F.Player, F.Region));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueGroundSpawnBossPawnTest,
	"Rogue.Gameplay.GroundSpawn.PawnsIgnoredBossGroundedAndFallbackKeepsDistance", RogueGroundSpawnTests::Flags)
bool FRogueGroundSpawnBossPawnTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RogueGroundSpawnTests;
	FFixture F; if (!F.Initialize(*this)) { return false; }
	F.Box(FVector(0.0f, 0.0f, -20.0f), FVector(20000.0f, 20000.0f, 20.0f));
	F.Settings.SpawnRadius = 0.0f; F.Settings.EnemySpawnMinimumDistance = 0.0f; F.Configure();
	F.Player->SetActorLocation(FVector(0.0f, 0.0f, 500.0f));
	F.Player->SetActorEnableCollision(true);
	F.Player->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	ARogueEnemy* Enemy = F.Spawn(*this); if (Enemy == nullptr) { return false; }
	F.CheckHeight(*this, Enemy, 0.0f); // The player at Z 500 was not mistaken for ground.
	Enemy->DeactivateToPool();
	F.Settings.EnemySpawnMinimumDistance = 20000.0f; F.Configure();
	TestFalse(TEXT("The fixed fallback cannot bypass player separation"), F.Spawner->SpawnEnemy(F.State, F.Player, F.Region));
	F.Player->SetActorEnableCollision(false);
	F.Spawner->SpawnBoss(F.State, F.Player, F.Region);
	ARogueEnemy* Boss = F.ActiveEnemy();
	if (!TestNotNull(TEXT("A real Boss is spawned on the same ground"), Boss)) { return false; }
	TestTrue(TEXT("Boss state is committed only after a grounded spawn"), F.State.bBossAlive);
	F.CheckHeight(*this, Boss, 0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueGroundSpawnBossRetryTest,
	"Rogue.Gameplay.GroundSpawn.BossGroundFailureRetriesWithoutPerFrameTracing", RogueGroundSpawnTests::Flags)
bool FRogueGroundSpawnBossRetryTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RogueGroundSpawnTests;
	FFixture F; if (!F.Initialize(*this)) { return false; }
	F.Settings.SpawnInterval = 1.0f; F.Configure();
	F.State.CurrentWave = 10;
	F.State.RunTimeSeconds = F.Settings.WaveDuration * 9.0f;
	F.State.SpawnTimer = 1000.0f; // Isolate Boss scheduling from ordinary enemies.
	F.Spawner->TickSpawning(0.01f, F.State, F.Player, F.Region);
	TestFalse(TEXT("The first Boss attempt fails without a floor"), F.State.bBossAlive);
	F.Box(FVector(0.0f, 0.0f, -20.0f), FVector(20000.0f, 20000.0f, 20.0f));
	F.Spawner->TickSpawning(0.01f, F.State, F.Player, F.Region);
	TestFalse(TEXT("The next frame does not run another Boss search"), F.State.bBossAlive);
	TestNull(TEXT("No Boss is active before its retry interval"), F.ActiveEnemy());
	F.Spawner->TickSpawning(1.0f, F.State, F.Player, F.Region);
	TestTrue(TEXT("A later retry succeeds without consuming the failed wave attempt"), F.State.bBossAlive);
	ARogueEnemy* Boss = F.ActiveEnemy();
	if (!TestNotNull(TEXT("The retry creates a grounded Boss"), Boss)) { return false; }
	F.CheckHeight(*this, Boss, 0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueSpawnRegionTranslatedTest,
    "Rogue.Gameplay.SpawnRegion.TranslatedRotatedScaledRegionAndOutsidePlayer", RogueGroundSpawnTests::Flags)
bool FRogueSpawnRegionTranslatedTest::RunTest(const FString& Parameters)
{
    using namespace RogueGroundSpawnTests;
    FFixture F; if (!F.Initialize(*this)) { return false; }
    F.Region->SetActorLocation(FVector(20000, 30000, 1000));
    F.Region->SetActorRotation(FRotator(0, 37, 0));
    F.Region->SetActorScale3D(FVector(1.3, 0.7, 1));
    F.Region->GetSpawnBounds()->SetBoxExtent(FVector(1000, 1600, 700));
    F.Player->SetActorLocation(FVector(25000, 30000, 1000));
    F.Box(FVector(20000, 30000, 480), FVector(5000, 5000, 20));
    for (int32 Index = 0; Index < 24; ++Index)
    {
        ARogueEnemy* Enemy = F.Spawn(*this); if (Enemy == nullptr) { return false; }
        const UCapsuleComponent* Capsule = Enemy->GetCapsuleComponent();
        TestTrue(TEXT("Entire actual capsule remains inside the placed region"), F.Region->ContainsCapsule(
            Enemy->GetActorLocation(), Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight()));
        TestTrue(TEXT("No fallback to world origin"), Enemy->GetActorLocation().X > 18000);
        F.CheckHeight(*this, Enemy, 500);
        Enemy->DeactivateToPool();
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueSpawnRegionHeightTest,
    "Rogue.Gameplay.SpawnRegion.MissingRegionAndGroundOutsideBoxAreRejected", RogueGroundSpawnTests::Flags)
bool FRogueSpawnRegionHeightTest::RunTest(const FString& Parameters)
{
    using namespace RogueGroundSpawnTests;
    FFixture F; if (!F.Initialize(*this)) { return false; }
    UBoxComponent* Floor = F.Box(FVector(0, 0, -20), FVector(20000, 20000, 20));
    TestFalse(TEXT("No region cannot use hidden default boundaries"), F.Spawner->SpawnEnemy(F.State, F.Player, nullptr));
    F.Region->SetActorLocation(FVector(0, 0, 1000));
    F.Region->GetSpawnBounds()->SetBoxExtent(FVector(5000, 5000, 600));
    TestFalse(TEXT("Ground below the selected height is rejected"), F.Spawner->SpawnEnemy(F.State, F.Player, F.Region));
    F.Spawner->SpawnBoss(F.State, F.Player, F.Region);
    TestFalse(TEXT("Boss uses the same region boundary"), F.State.bBossAlive);
    Floor->GetOwner()->SetActorLocation(FVector(0, 0, 480));
    F.Box(FVector(0, 0, 2000), FVector(20000, 20000, 20));
    ARogueEnemy* Enemy = F.Spawn(*this); if (Enemy == nullptr) { return false; }
    F.CheckHeight(*this, Enemy, 500); // Roof above the box must not intercept the clipped ray.
    Enemy->DeactivateToPool();
    F.Region->GetSpawnBounds()->SetBoxExtent(FVector(10, 10, 600));
    TestFalse(TEXT("A box smaller than the capsule does not spawn"), F.Spawner->SpawnEnemy(F.State, F.Player, F.Region));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueSpawnRegionGeometryTest,
    "Rogue.Gameplay.SpawnRegion.CapsuleMarginsAndTiltedRayIntersection", RogueGroundSpawnTests::Flags)
bool FRogueSpawnRegionGeometryTest::RunTest(const FString& Parameters)
{
    using namespace RogueGroundSpawnTests;
    FFixture F; if (!F.Initialize(*this)) { return false; }
    F.Region->SetActorLocation(FVector(4000, -8000, 600));
    F.Region->GetSpawnBounds()->SetBoxExtent(FVector(500, 500, 300));
    TestTrue(TEXT("Capsule fits at center"), F.Region->ContainsCapsule(FVector(4000, -8000, 600), 34, 72));
    TestFalse(TEXT("Side margin includes radius and edge padding"), F.Region->ContainsCapsule(FVector(4400, -8000, 600), 34, 72));
    TestFalse(TEXT("Capsule head must fit in selected height"), F.Region->ContainsCapsule(FVector(4000, -8000, 850), 34, 72));
    F.Region->SetActorRotation(FRotator(20, 32, 15));
    F.Region->SetActorScale3D(FVector(1.5, 0.8, 1.2));
    FVector Start, End;
    TestTrue(TEXT("Vertical line intersects a tilted nonuniformly scaled box"),
        F.Region->ClipGroundTrace(FVector(4000, -8000, 600), 10000, 10000, Start, End));
    const FTransform Transform = F.Region->GetSpawnBounds()->GetComponentTransform();
    const FVector Extent = F.Region->GetSpawnBounds()->GetUnscaledBoxExtent() + FVector(0.02);
    for (const FVector& Point : {Start, End})
    {
        const FVector Local = Transform.InverseTransformPosition(Point).GetAbs();
        TestTrue(TEXT("Clipped trace ends stay on the oriented box"), Local.X <= Extent.X && Local.Y <= Extent.Y && Local.Z <= Extent.Z);
    }
    TestFalse(TEXT("A distant vertical ray misses the box"), F.Region->ClipGroundTrace(FVector::ZeroVector, 10000, 10000, Start, End));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueMapAuthoringTest,
    "Rogue.Gameplay.MapAuthoring.ShopComponentDefaultsAndLevelActorsPreserved", RogueGroundSpawnTests::Flags)
bool FRogueMapAuthoringTest::RunTest(const FString& Parameters)
{
    using namespace RogueGroundSpawnTests;
    FTestConsoleVariable SelfTest(TEXT("Rogue.SelfTest.WeaponUpgradeChain")); SelfTest.Set(TEXT("0"));
    FTestConsoleVariable Stress(TEXT("Rogue.SelfTest.EnemyStress")); Stress.Set(TEXT("0"));
    FTestWorldWrapper TestWorld;
    if (!TestWorld.CreateTestWorld(EWorldType::Game)) { return false; }
    UWorld* World = TestWorld.GetTestWorld();
    World->GetWorldSettings()->DefaultGameMode = ARogueGameMode::StaticClass();
    World->SpawnActor<ARogueEnemySpawnRegion>();
    APointLight* Light = World->SpawnActor<APointLight>();
    ARogueShopTerminal* Shop = World->SpawnActor<ARogueShopTerminal>();
    Shop->SetActorTransform(FTransform(FRotator(0, 38, 0), FVector(2300, -1200, 400), FVector(1.2)));
    UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    if (!TestNotNull(TEXT("Test mesh loads"), Mesh) || !TestNotNull(TEXT("Test material loads"), Material)) { return false; }
    TArray<UStaticMeshComponent*> Meshes; Shop->GetComponents(Meshes);
    for (UStaticMeshComponent* Component : Meshes)
    {
        Component->SetStaticMesh(Mesh);
        Component->SetMaterial(0, Material);
        Component->SetRelativeScale3D(FVector(0.7, 1.4, 2.1));
    }
    const FTransform Before = Shop->GetActorTransform();
    if (!TestWorld.BeginPlayInTestWorld()) { return false; }
    int32 ShopCount = 0;
    for (TActorIterator<ARogueShopTerminal> It(World); It; ++It) { ++ShopCount; }
    TestEqual(TEXT("GameMode does not auto-spawn another shop"), ShopCount, 1);
    TestTrue(TEXT("Map shop transform is preserved"), Shop->GetActorTransform().Equals(Before));
    TestFalse(TEXT("Map lighting is not automatically hidden"), Light->IsHidden());
    for (UStaticMeshComponent* Component : Meshes)
    {
        TestTrue(TEXT("Configured component mesh is preserved"), Component->GetStaticMesh() == Mesh);
        TestTrue(TEXT("Configured component material is preserved"), Component->GetMaterial(0) == Material);
        TestTrue(TEXT("Configured component scale is preserved"), Component->GetRelativeScale3D().Equals(FVector(0.7, 1.4, 2.1)));
    }
    TestTrue(TEXT("Prompt follows the placed shop transform"), Shop->GetPromptWorldLocation().Equals(
        Before.TransformPosition(FVector(0, 0, 220)), 0.01));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueCanopyGroundBandTest,
    "Rogue.Gameplay.SpawnRegion.GroundHeightBandSkipsCanopyAndPreservesGroundClearance", RogueGroundSpawnTests::Flags)
bool FRogueCanopyGroundBandTest::RunTest(const FString& Parameters)
{
    using namespace RogueGroundSpawnTests;
    FFixture F; if (!F.Initialize(*this)) { return false; }
    F.Box(FVector(0,0,480), FVector(20000,20000,20)); // Ground surface Z 500.
    UBoxComponent* Canopy = F.Box(FVector(0,0,1200), FVector(20000,20000,40));
    ARogueEnemy* Before = F.Spawn(*this); if (!Before) { return false; }
    F.CheckHeight(*this, Before, 1240); // Reproduce the old walkable-canopy bug.
    Before->DeactivateToPool();
    F.Region->ConfigureGroundHeightRange(true, -550, -450); // World Z 450..550.
    ARogueEnemy* After = F.Spawn(*this); if (!After) { return false; }
    F.CheckHeight(*this, After, 500);
    TestTrue(TEXT("The tall capsule is allowed above the surface-only band"), After->GetActorLocation().Z > 550);
    After->DeactivateToPool();
    // Move foliage low enough that it blocks a capsule, but remains above the allowed ground band.
    Canopy->GetOwner()->SetActorLocation(FVector(0,0,650));
    TestFalse(TEXT("Starting below a canopy cannot bypass blocked capsule headroom"), F.Spawner->SpawnEnemy(F.State,F.Player,F.Region));
    F.Spawner->SpawnBoss(F.State,F.Player,F.Region);
    TestFalse(TEXT("A blocked Boss spawn does not consume its wave"), F.State.bBossAlive);
    Canopy->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    F.Spawner->SpawnBoss(F.State,F.Player,F.Region);
    ARogueEnemy* Boss = F.ActiveEnemy(); if (!TestNotNull(TEXT("Boss spawns after headroom is cleared"),Boss)) { return false; }
    F.CheckHeight(*this,Boss,500);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueGroundBandVoidTest,
    "Rogue.Gameplay.SpawnRegion.GroundHeightBandRejectsRaisedPlatformsAndInvalidBands", RogueGroundSpawnTests::Flags)
bool FRogueGroundBandVoidTest::RunTest(const FString& Parameters)
{
    using namespace RogueGroundSpawnTests;
    FFixture F; if (!F.Initialize(*this)) { return false; }
    F.Box(FVector(0,0,780),FVector(20000,20000,20));
    F.Region->ConfigureGroundHeightRange(true,-550,-450);
    TestFalse(TEXT("A platform outside the authored ground band is not a fallback surface"),F.Spawner->SpawnEnemy(F.State,F.Player,F.Region));
    TestNull(TEXT("A failed search leaves no activated pooled enemy"),F.ActiveEnemy());
    F.Region->ConfigureGroundHeightRange(true,50,-50);
    TestFalse(TEXT("An inverted band fails safely"),F.Spawner->SpawnEnemy(F.State,F.Player,F.Region));
    F.Region->ConfigureGroundHeightRange(false,50,-50);
    ARogueEnemy* Enemy=F.Spawn(*this); if (!Enemy) { return false; }
    F.CheckHeight(*this,Enemy,800);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueGroundBandTransformTest,
    "Rogue.Gameplay.SpawnRegion.GroundHeightBandFollowsRegionTransform", RogueGroundSpawnTests::Flags)
bool FRogueGroundBandTransformTest::RunTest(const FString& Parameters)
{
    using namespace RogueGroundSpawnTests;
    FFixture F; if (!F.Initialize(*this)) { return false; }
    F.Region->SetActorLocation(FVector(10000,-20000,800));
    F.Region->SetActorRotation(FRotator(20,37,15));
    F.Region->SetActorScale3D(FVector(1.5,.8,2));
    F.Region->ConfigureGroundHeightRange(true,-50,50);
    const FTransform Transform=F.Region->GetSpawnBounds()->GetComponentTransform();
    TestTrue(TEXT("A surface in the local band follows movement, rotation and scaling"),F.Region->IsGroundHeightAllowed(Transform.TransformPosition(FVector(0,0,40))));
    TestFalse(TEXT("A raised surface is rejected in transformed space"),F.Region->IsGroundHeightAllowed(Transform.TransformPosition(FVector(0,0,60))));
    FVector Start,End;
    TestTrue(TEXT("A world-vertical ray is clipped to the tilted local band"),F.Region->ClipGroundTrace(F.Region->GetActorLocation(),10000,10000,Start,End));
    TestTrue(TEXT("Upper endpoint lies on the allowed upper surface"),FMath::IsNearlyEqual(Transform.InverseTransformPosition(Start).Z,50,.01));
    TestTrue(TEXT("Lower endpoint lies on the allowed lower surface"),FMath::IsNearlyEqual(Transform.InverseTransformPosition(End).Z,-50,.01));
    return true;
}

namespace RogueGroundSpawnTests
{
    void Outline(ARogueEnemySpawnRegion* Region,const TArray<FVector>& Points)
    {
        USplineComponent* Spline=Region->GetSpawnOutline(); Spline->ClearSplinePoints(false);
        for (const FVector& Point : Points)
        {
            Spline->AddSplinePoint(Point,ESplineCoordinateSpace::Local,false);
            Spline->SetSplinePointType(Spline->GetNumberOfSplinePoints()-1,ESplinePointType::Linear,false);
        }
        Spline->SetClosedLoop(true,true); Region->SetSpawnOutlineEnabled(true);
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueConcaveSpawnOutlineTest,
    "Rogue.Gameplay.SpawnRegion.ConcaveOutlineRejectsForestNotchAndCapsuleEdge", RogueGroundSpawnTests::Flags)
bool FRogueConcaveSpawnOutlineTest::RunTest(const FString& Parameters)
{
    using namespace RogueGroundSpawnTests;
    FFixture F; if (!F.Initialize(*this)) { return false; }
    Outline(F.Region,{FVector(-3000,-3000,0),FVector(3000,-3000,0),FVector(3000,-500,0),FVector(500,-500,0),FVector(500,3000,0),FVector(-3000,3000,0)});
    TestTrue(TEXT("A point in the L-shaped clearing is allowed"),F.Region->IsInsideSpawnOutline(FVector(-1000,1000,0),34));
    TestFalse(TEXT("The forest notch is rejected even inside the outer rectangle"),F.Region->IsInsideSpawnOutline(FVector(1500,1500,0),34));
    TestFalse(TEXT("The capsule and safety gap cannot cross the concave edge"),F.Region->IsInsideSpawnOutline(FVector(450,1000,0),34));
    F.Box(FVector(0,0,480),FVector(20000,20000,20));
    for (int32 Index=0;Index<30;++Index)
    {
        ARogueEnemy* Enemy=F.Spawn(*this); if (!Enemy) { return false; }
        const FVector Location=Enemy->GetActorLocation();
        TestFalse(TEXT("Actual spawned enemy is outside the excluded notch"),Location.X>500 && Location.Y>-500);
        TestTrue(TEXT("The full capsule fits the outline"),F.Region->IsInsideSpawnOutline(Location,Enemy->GetCapsuleComponent()->GetScaledCapsuleRadius()));
        Enemy->DeactivateToPool();
    }
    F.Spawner->SpawnBoss(F.State,F.Player,F.Region);
    TestTrue(TEXT("Boss uses the same concave outline"),F.State.bBossAlive);
    ARogueEnemy* Boss=F.ActiveEnemy(); if (!Boss) { return false; }
    TestFalse(TEXT("Boss never uses the forest notch"),Boss->GetActorLocation().X>500 && Boss->GetActorLocation().Y>-500);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueSpawnOutlineInvalidTest,
    "Rogue.Gameplay.SpawnRegion.InvalidOutlineCannotFallbackToRectangle", RogueGroundSpawnTests::Flags)
bool FRogueSpawnOutlineInvalidTest::RunTest(const FString& Parameters)
{
    using namespace RogueGroundSpawnTests;
    FFixture F; if (!F.Initialize(*this)) { return false; }
    F.Box(FVector(0,0,480),FVector(20000,20000,20));
    Outline(F.Region,{FVector(-1000,-1000,0),FVector(1000,1000,0),FVector(-1000,1000,0),FVector(1000,-1000,0)});
    TestFalse(TEXT("A self-intersecting outline does not spawn outside intended space"),F.Spawner->SpawnEnemy(F.State,F.Player,F.Region));
    Outline(F.Region,{FVector(-1000,-1000,0),FVector(1000,-1000,0),FVector(1000,1000,0),FVector(-1000,1000,0)});
    F.Region->GetSpawnOutline()->SetClosedLoop(false,true);
    TestFalse(TEXT("An open outline cannot fall back to its bounding rectangle"),F.Spawner->SpawnEnemy(F.State,F.Player,F.Region));
    F.Region->SetSpawnOutlineEnabled(false);
    return F.Spawn(*this)!=nullptr;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueSpawnOutlineTransformTest,
    "Rogue.Gameplay.SpawnRegion.OutlineFollowsMovedRotatedScaledActor", RogueGroundSpawnTests::Flags)
bool FRogueSpawnOutlineTransformTest::RunTest(const FString& Parameters)
{
    using namespace RogueGroundSpawnTests;
    FFixture F; if (!F.Initialize(*this)) { return false; }
    Outline(F.Region,{FVector(-2000,-2000,0),FVector(2000,-2000,0),FVector(2000,0,0),FVector(0,0,0),FVector(0,2000,0),FVector(-2000,2000,0)});
    F.Region->SetActorTransform(FTransform(FRotator(0,37,0),FVector(12000,-9000,1000),FVector(1.5,.7,1)));
    const FTransform Transform=F.Region->GetSpawnOutline()->GetComponentTransform();
    TestTrue(TEXT("Inside follows authored transform"),F.Region->IsInsideSpawnOutline(Transform.TransformPosition(FVector(-1000,1000,0)),34));
    TestFalse(TEXT("Excluded notch follows authored transform"),F.Region->IsInsideSpawnOutline(Transform.TransformPosition(FVector(1000,1000,0)),34));
    F.Region->OnConstruction(F.Region->GetActorTransform());
    TestEqual(TEXT("Rerunning construction preserves edited vertices"),F.Region->GetSpawnOutline()->GetNumberOfSplinePoints(),6);
    return true;
}

#endif
