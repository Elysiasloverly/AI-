#if WITH_DEV_AUTOMATION_TESTS
#include "Enemies/RogueEnemy.h"
#include "Framework/RogueGameMode.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/WorldSettings.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"

namespace RogueEnemyCollisionTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;
	struct FFixture
	{
		FTestConsoleVariable SelfTest{TEXT("Rogue.SelfTest.WeaponUpgradeChain")};
		FTestConsoleVariable Stress{TEXT("Rogue.SelfTest.EnemyStress")};
		FTestWorldWrapper TestWorld;
		UWorld* World = nullptr;
		bool Initialize()
		{
			SelfTest.Set(TEXT("0")); Stress.Set(TEXT("0"));
			if (!TestWorld.CreateTestWorld(EWorldType::Game)) { return false; }
			World = TestWorld.GetTestWorld();
			World->GetWorldSettings()->DefaultGameMode = ARogueGameMode::StaticClass();
			if (!TestWorld.BeginPlayInTestWorld()) { return false; }
			World->GetAuthGameMode()->SetActorTickEnabled(false);
			return true;
		}
		UBoxComponent* Box(FVector Center, FVector Extent, FRotator Rotation = FRotator::ZeroRotator)
		{
			AActor* Actor = World->SpawnActor<AActor>();
			UBoxComponent* Shape = NewObject<UBoxComponent>(Actor);
			Actor->SetRootComponent(Shape);
			Shape->SetBoxExtent(Extent);
			Shape->SetCollisionObjectType(ECC_WorldStatic);
			Shape->SetCollisionResponseToAllChannels(ECR_Block);
			Shape->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Shape->RegisterComponent();
			Actor->SetActorLocationAndRotation(Center, Rotation);
			return Shape;
		}
		ARogueEnemy* Enemy(FVector Location, UClass* Class = ARogueEnemy::StaticClass())
		{
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			ARogueEnemy* Result = World->SpawnActor<ARogueEnemy>(Class, Location, FRotator::ZeroRotator, Params);
			Result->ActivatePooledEnemy(nullptr, Location, FRotator::ZeroRotator);
			Result->SetActorTickEnabled(false);
			return Result;
		}
		void Step(int32 Frames) { for (int32 I = 0; I < Frames; ++I) { TestWorld.TickTestWorld(1.0f / 60.0f); } }
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueEnemyBlueprintCosmeticCollisionTest,
	"Rogue.Gameplay.EnemyCollision.ActualBlueprintMeshesCannotBlockOrSimulate", RogueEnemyCollisionTests::Flags)
bool FRogueEnemyBlueprintCosmeticCollisionTest::RunTest(const FString& Parameters)
{
	using namespace RogueEnemyCollisionTests;
	FFixture F; if (!F.Initialize()) { return false; }
	const TCHAR* Names[] = {TEXT("Base"),TEXT("Rifleman"),TEXT("Assault"),TEXT("Juggernaut"),TEXT("Orbiter"),TEXT("Marksman"),TEXT("Scatterer"),TEXT("Skirmisher"),TEXT("Cannoneer"),TEXT("ShockPillar"),TEXT("Sniper"),TEXT("Shotgunner"),TEXT("Repeater"),TEXT("Ringcaster")};
	for (const TCHAR* Name : Names)
	{
		const FString Path = FString::Printf(TEXT("/Game/BP/Enemy/BP_Enemy%s.BP_Enemy%s_C"), Name, Name);
		UClass* Class = LoadClass<ARogueEnemy>(nullptr, *Path);
		if (!TestNotNull(*Path, Class)) { return false; }
		ARogueEnemy* Enemy = F.Enemy(FVector(0, 0, 500), Class);
		TestTrue(TEXT("Legacy zero gravity is repaired after Blueprint construction"), Enemy->GetCharacterMovement()->GravityScale > 0);
		TestFalse(TEXT("Ground movement does not push physics accessories"), Enemy->GetCharacterMovement()->bEnablePhysicsInteraction);
		for (UMeshComponent* Mesh : TInlineComponentArray<UMeshComponent*>(Enemy))
		{
			TestEqual(*Mesh->GetPathName(), Mesh->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
			TestFalse(TEXT("Cosmetics cannot simulate independently"), Mesh->IsSimulatingPhysics());
			TestFalse(TEXT("Cosmetics cannot trigger duplicate hits"), Mesh->GetGenerateOverlapEvents());
			TestTrue(TEXT("Cosmetics cannot become another character's step"), Mesh->CanCharacterStepUpOn == ECB_No);
		}
		TestTrue(TEXT("The capsule still blocks the scene"), Enemy->GetCapsuleComponent()->GetCollisionResponseToChannel(ECC_WorldStatic) == ECR_Block);
		TestTrue(TEXT("Enemy capsules still ignore other pawns"), Enemy->GetCapsuleComponent()->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Ignore);
		Enemy->Destroy();
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueEnemyCollisionPoolReuseTest,
	"Rogue.Gameplay.EnemyCollision.PoolReuseRepairsNewAccessoryAndPreservesAppearance", RogueEnemyCollisionTests::Flags)
bool FRogueEnemyCollisionPoolReuseTest::RunTest(const FString& Parameters)
{
	using namespace RogueEnemyCollisionTests;
	FFixture F; if (!F.Initialize()) { return false; }
	ARogueEnemy* Enemy = F.Enemy(FVector(0, 0, 300));
	UStaticMeshComponent* Accessory = NewObject<UStaticMeshComponent>(Enemy);
	Accessory->SetupAttachment(Enemy->GetRootComponent());
	Accessory->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
	Accessory->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	Accessory->SetGenerateOverlapEvents(true);
	Accessory->RegisterComponent();
	Accessory->SetRelativeLocation(FVector(23, 45, 67));
	Accessory->SetRelativeScale3D(FVector(3, 2, 1));
	const FTransform Authored = Accessory->GetRelativeTransform();
	UStaticMesh* AuthoredMesh = Accessory->GetStaticMesh();
	UMaterialInterface* AuthoredMaterial = Accessory->GetMaterial(0);
	Enemy->GetCharacterMovement()->GravityScale = 0;
	Enemy->DeactivateToPool();
	Enemy->ActivatePooledEnemy(nullptr, FVector(0, 0, 300), FRotator::ZeroRotator);
	TestEqual(TEXT("A newly added blocking accessory is repaired on reuse"), Accessory->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
	TestTrue(TEXT("Authored accessory transform is retained"), Accessory->GetRelativeTransform().Equals(Authored));
	TestTrue(TEXT("Authored mesh and material are retained"), Accessory->GetStaticMesh() == AuthoredMesh && Accessory->GetMaterial(0) == AuthoredMaterial);
	TestTrue(TEXT("Old zero-gravity override is repaired on reuse"), Enemy->GetCharacterMovement()->GravityScale > 0);
	Enemy->GetCharacterMovement()->GravityScale = 1.5f;
	Enemy->DeactivateToPool(); Enemy->ActivatePooledEnemy(nullptr, FVector(0, 0, 300), FRotator::ZeroRotator);
	TestEqual(TEXT("A valid authored positive gravity is retained"), Enemy->GetCharacterMovement()->GravityScale, 1.5f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueEnemySlopeRecoveryTest,
	"Rogue.Gameplay.EnemyCollision.SlopeDeflectionAndLedgeDropReturnToGround", RogueEnemyCollisionTests::Flags)
bool FRogueEnemySlopeRecoveryTest::RunTest(const FString& Parameters)
{
	using namespace RogueEnemyCollisionTests;
	FFixture F; if (!F.Initialize()) { return false; }
	F.Box(FVector(0, 0, -50), FVector(2500, 2500, 50));
	F.Box(FVector(0, 0, 180), FVector(300, 180, 30), FRotator(25, 0, 0));
	ARogueEnemy* Enemy = F.Enemy(FVector(0, 0, 600));
	UCharacterMovementComponent* Movement = Enemy->GetCharacterMovement();
	Movement->SetMovementMode(MOVE_Falling);
	F.Step(180);
	TestTrue(TEXT("The enemy lands on a walkable ramp"), Movement->IsMovingOnGround());
	const float OnRamp = Enemy->GetActorLocation().Z;
	TestTrue(TEXT("The ramp actually raises the capsule above the floor"), OnRamp > 200);
	// Exercise the same swept planar movement used by Blueprint behavior and native AI.
	for (int32 I = 0; I < 180; ++I)
	{
		Enemy->MoveInDirection(FVector(0, 1, 0), 1, 1.0f / 60.0f);
		F.Step(1);
	}
	const float GroundZ = Enemy->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	TestTrue(TEXT("Leaving the ramp does not leave a zero-gravity hovering enemy"), FMath::Abs(Enemy->GetActorLocation().Z - GroundZ) < 5);
	TestTrue(TEXT("The enemy is walking after dropping off the ramp"), Movement->IsMovingOnGround());
	// Reproduce a slope/contact upward deflection and require recovery instead of permanent flight.
	Movement->SetMovementMode(MOVE_Falling);
	Movement->Velocity.Z = 350;
	F.Step(180);
	TestTrue(TEXT("An upward contact deflection returns to the scene floor"), FMath::Abs(Enemy->GetActorLocation().Z - GroundZ) < 5);
	TestTrue(TEXT("The capsule still cannot pass through the floor"), Enemy->GetActorLocation().Z >= GroundZ - 1);
	return true;
}
#endif
