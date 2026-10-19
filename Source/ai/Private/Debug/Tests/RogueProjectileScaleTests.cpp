#if WITH_DEV_AUTOMATION_TESTS

#include "Combat/Projectiles/RogueProjectile.h"
#include "Combat/Projectiles/RogueRocketProjectile.h"
#include "Combat/Projectiles/RogueMortarProjectile.h"
#include "Combat/Projectiles/RogueEnemyProjectile.h"
#include "Combat/Projectiles/RogueOrbitingBlade.h"
#include "Combat/Targeting/RogueEnemyTrackerSubsystem.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Enemies/RogueEnemy.h"
#include "Framework/RogueGameMode.h"
#include "GameFramework/WorldSettings.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"

namespace RogueProjectileScaleTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;
	using FScales = TMap<UStaticMeshComponent*, FVector>;
	const FVector Location(10000.0f, 0.0f, 5000.0f);

	struct FFixture
	{
		FTestConsoleVariable SelfTest{TEXT("Rogue.SelfTest.WeaponUpgradeChain")};
		FTestConsoleVariable EnemyStress{TEXT("Rogue.SelfTest.EnemyStress")};
		FTestWorldWrapper TestWorld;
		UWorld* World = nullptr;
		AActor* Owner = nullptr;
		URogueEnemyTrackerSubsystem* Tracker = nullptr;
		bool Initialize(FAutomationTestBase& Test)
		{
			SelfTest.Set(TEXT("0")); EnemyStress.Set(TEXT("0"));
			if (!TestWorld.CreateTestWorld(EWorldType::Game)) { return false; }
			World = TestWorld.GetTestWorld();
			World->GetWorldSettings()->DefaultGameMode = ARogueGameMode::StaticClass();
			if (!TestWorld.BeginPlayInTestWorld()) { return false; }
			ARogueGameMode* Mode = World->GetAuthGameMode<ARogueGameMode>();
			if (!Test.TestNotNull(TEXT("The test GameMode exists"), Mode)) { return false; }
			Mode->SetActorTickEnabled(false);
			Owner = World->SpawnActor<AActor>();
			Tracker = World->GetSubsystem<URogueEnemyTrackerSubsystem>();
			return Test.TestNotNull(TEXT("The owner exists"), Owner) && Test.TestNotNull(TEXT("The real enemy tracker exists"), Tracker);
		}
		void SetHighLoad(bool bEnabled)
		{
			FRogueRuntimePerformanceSettings Settings;
			// Threshold 1 with a real registered enemy exercises the actual culling path.
			Settings.CombatEffectsCullEnemyCount = bEnabled ? 1 : 0;
			Tracker->Configure(Settings);
			if (bEnabled && Tracker->GetActiveEnemyCount() == 0)
			{
				ARogueEnemy* Enemy = World->SpawnActor<ARogueEnemy>();
				Enemy->ActivatePooledEnemy(Owner, FVector(20000.0f, 0.0f, 5000.0f), FRotator::ZeroRotator);
				FRogueEnemyProfile Profile; Profile.Health = 10000.0f; Profile.Damage = 0.0f;
				Enemy->InitializeEnemy(Profile);
				Tracker->RegisterEnemy(Enemy);
			}
		}
	};

	FScales Capture(AActor* Actor, bool bUseNonuniformScale)
	{
		FScales Result;
		TInlineComponentArray<UStaticMeshComponent*> Meshes;
		Actor->GetComponents(Meshes);
		for (int32 Index = 0; Index < Meshes.Num(); ++Index)
		{
			UStaticMeshComponent* Mesh = Meshes[Index];
			// The landing ring represents damage range, not the flying projectile model.
			if (Mesh->GetFName() == TEXT("LandingIndicatorMesh")) { continue; }
			if (bUseNonuniformScale) { Mesh->SetRelativeScale3D(FVector(0.31f + Index * 0.1f, 0.57f, 0.83f)); }
			Result.Add(Mesh, Mesh->GetRelativeScale3D());
		}
		return Result;
	}
	void Check(FAutomationTestBase& Test, const FScales& Expected, float Multiplier = 1.0f)
	{
		for (const auto& Entry : Expected)
		{
			Test.TestTrue(FString::Printf(TEXT("%s retains its authored proportions (multiplier %.1f)"), *Entry.Key->GetName(), Multiplier),
				Entry.Key->GetRelativeScale3D().Equals(Entry.Value * Multiplier, 0.0001f));
		}
	}
	void Activate(AActor* Actor, AActor* Owner)
	{
		if (ARogueMortarProjectile* Mortar = Cast<ARogueMortarProjectile>(Actor))
		{
			Mortar->ActivatePooledMortar(Owner, nullptr, Location, Location + FVector(500.0f, 0.0f, -100.0f), 0.0f, 420.0f, 800.0f);
		}
		else if (ARogueRocketProjectile* Rocket = Cast<ARogueRocketProjectile>(Actor))
		{
			Rocket->ActivatePooledRocket(Owner, nullptr, Location, FRotator::ZeroRotator, FVector::ForwardVector, 800.0f, 0.0f, 200.0f);
		}
		else if (ARogueProjectile* Projectile = Cast<ARogueProjectile>(Actor))
		{
			Projectile->ActivatePooledProjectile(Owner, nullptr, Location, FRotator::ZeroRotator, FVector::ForwardVector, 800.0f, 0.0f);
		}
	}
	void Deactivate(AActor* Actor)
	{
		if (ARogueProjectile* Projectile = Cast<ARogueProjectile>(Actor)) { Projectile->DeactivateToPool(); }
		else if (ARogueRocketProjectile* Rocket = Cast<ARogueRocketProjectile>(Actor)) { Rocket->DeactivateToPool(); }
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRoguePlayerProjectileScaleTest,
	"Rogue.Gameplay.ProjectileScale.PlayerBlueprintsAcrossFlightLoadAndReuse", RogueProjectileScaleTests::Flags)
bool FRoguePlayerProjectileScaleTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RogueProjectileScaleTests;
	FFixture Fixture; if (!Fixture.Initialize(*this)) { return false; }
	for (const TCHAR* Path : { TEXT("/Game/BP/Projectile/BP_RogueProjectile.BP_RogueProjectile_C"),
		TEXT("/Game/BP/Projectile/BP_RogueRocketProjectile.BP_RogueRocketProjectile_C"),
		TEXT("/Game/BP/Projectile/BP_RogueMortarProjectile.BP_RogueMortarProjectile_C") })
	{
		UClass* Class = LoadClass<AActor>(nullptr, Path);
		if (!TestNotNull(TEXT("The actual projectile Blueprint loads"), Class)) { return false; }
		FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AActor* Actor = Fixture.World->SpawnActor<AActor>(Class, Location, FRotator::ZeroRotator, Spawn);
		if (!TestNotNull(TEXT("The actual Blueprint instance exists"), Actor)) { return false; }
		const FScales Authored = Capture(Actor, false);
		TestTrue(TEXT("The Blueprint provides a flying model"), Authored.Num() > 0);
		Activate(Actor, Fixture.Owner); Actor->Tick(0.17f); Check(*this, Authored);
		Deactivate(Actor); Check(*this, Authored);
		// Nonuniform proportions detect replacement with a uniform hardcoded pulse value.
		const FScales Custom = Capture(Actor, true);
		for (bool bHighLoad : {true, false})
		{
			Fixture.SetHighLoad(bHighLoad);
			TestEqual(TEXT("The requested load state is exercised"), Fixture.Tracker->ShouldCullCombatEffects(), bHighLoad);
			Activate(Actor, Fixture.Owner);
			for (int32 Tick = 0; Tick < 4; ++Tick) { Actor->Tick(0.17f); Check(*this, Custom); }
			Deactivate(Actor); Check(*this, Custom);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueEnemyProjectileScaleTest,
	"Rogue.Gameplay.ProjectileScale.EnemyModesPreserveMeshAndCollisionRules", RogueProjectileScaleTests::Flags)
bool FRogueEnemyProjectileScaleTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RogueProjectileScaleTests;
	FFixture Fixture; if (!Fixture.Initialize(*this)) { return false; }
	ARogueEnemyProjectile* Actor = Fixture.World->SpawnActor<ARogueEnemyProjectile>();
	if (!TestNotNull(TEXT("The enemy bullet exists"), Actor)) { return false; }
	const FScales Authored = Capture(Actor, true);
	USphereComponent* Sphere = Actor->FindComponentByClass<USphereComponent>();
	if (!TestNotNull(TEXT("The real collision sphere exists"), Sphere)) { return false; }
	for (bool bHighLoad : {false, true})
	{
		Fixture.SetHighLoad(bHighLoad);
		for (bool bBoss : {false, true})
		{
			Actor->ActivatePooledProjectile(Fixture.Owner, nullptr, Location, FRotator::ZeroRotator,
				FVector::ForwardVector, 800.0f, 0.0f, bBoss, 2.0f, 0.4f, !bBoss);
			const float CollisionRadius = Sphere->GetUnscaledSphereRadius();
			TestTrue(TEXT("The original collision multiplier remains active"), CollisionRadius > 0.0f);
			Check(*this, Authored);
			for (int32 Tick = 0; Tick < 3; ++Tick) { Actor->Tick(0.17f); Check(*this, Authored); }
			TestEqual(TEXT("Visual updates do not change the damage collision radius"), Sphere->GetUnscaledSphereRadius(), CollisionRadius);
			Actor->DeactivateToPool(); Check(*this, Authored);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueBladeBlueprintScaleTest,
	"Rogue.Gameplay.ProjectileScale.ScytheBlueprintBaselineUpgradeAndPoolReset", RogueProjectileScaleTests::Flags)
bool FRogueBladeBlueprintScaleTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RogueProjectileScaleTests;
	FFixture Fixture; if (!Fixture.Initialize(*this)) { return false; }
	UClass* Class = LoadClass<ARogueOrbitingBlade>(nullptr, TEXT("/Game/BP/Projectile/BP_RogueOrbitingBlade.BP_RogueOrbitingBlade_C"));
	if (!TestNotNull(TEXT("The actual scythe Blueprint loads"), Class)) { return false; }
	ARogueOrbitingBlade* Actor = Fixture.World->SpawnActor<ARogueOrbitingBlade>(Class);
	if (!TestNotNull(TEXT("The scythe Blueprint instance exists"), Actor)) { return false; }
	const FScales Authored = Capture(Actor, false);
	TestTrue(TEXT("The scythe Blueprint contains a model"), Authored.Num() > 0);
	USphereComponent* Sphere = Actor->FindComponentByClass<USphereComponent>();
	if (!TestNotNull(TEXT("The scythe damage sphere exists"), Sphere)) { return false; }
	const float BaseRadius = Sphere->GetUnscaledSphereRadius();
	Actor->ActivatePooledBlade(Fixture.Owner);
	Actor->ConfigureBlade(Fixture.Owner, 0, 1, 200.0f, 0.0f, 0.0f, 0.0f, 1.4f);
	Actor->Tick(0.2f); Check(*this, Authored, 1.4f);
	TestTrue(TEXT("Scythe growth still scales the actual damage range"), FMath::IsNearlyEqual(Sphere->GetUnscaledSphereRadius(), BaseRadius * 1.4f));
	Actor->DeactivateToPool(); Check(*this, Authored);
	Actor->ActivatePooledBlade(Fixture.Owner);
	Actor->ConfigureBlade(Fixture.Owner, 0, 1, 200.0f, 0.0f, 0.0f, 0.0f);
	Actor->Tick(0.2f); Check(*this, Authored);
	TestEqual(TEXT("Reuse restores the original damage range"), Sphere->GetUnscaledSphereRadius(), BaseRadius);
	return true;
}
#endif
