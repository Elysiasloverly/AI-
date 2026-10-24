#if WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystemComponent.h"
#include "AbilitySystem/RogueAttributeSet.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Framework/RogueGameMode.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/WorldSettings.h"
#include "Misc/AutomationTest.h"
#include "Player/RogueCharacter.h"
#include "Player/RoguePlayerController.h"
#include "Player/RoguePlayerState.h"
#include "Player/RogueShieldOrbitComponent.h"
#include "Tests/AutomationCommon.h"

namespace RogueShieldOrbitTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;

	struct FFixture
	{
		FTestConsoleVariable SelfTest{TEXT("Rogue.SelfTest.WeaponUpgradeChain")};
		FTestConsoleVariable Stress{TEXT("Rogue.SelfTest.EnemyStress")};
		FTestWorldWrapper TestWorld;
		UWorld* World = nullptr;
		ARogueCharacter* Player = nullptr;
		URogueShieldOrbitComponent* Orbit = nullptr;
		URogueAttributeSet* Attributes = nullptr;

		bool Initialize(FAutomationTestBase& Test)
		{
			SelfTest.Set(TEXT("0")); Stress.Set(TEXT("0"));
			if (!TestWorld.CreateTestWorld(EWorldType::Game)) { return false; }
			World = TestWorld.GetTestWorld();
			World->GetWorldSettings()->DefaultGameMode = ARogueGameMode::StaticClass();
			if (!TestWorld.BeginPlayInTestWorld()) { return false; }
			World->GetAuthGameMode()->SetActorTickEnabled(false);
			ARoguePlayerController* Controller = World->SpawnActor<ARoguePlayerController>();
			Player = World->SpawnActorDeferred<ARogueCharacter>(ARogueCharacter::StaticClass(),
				FTransform(FVector(0, 0, 200)), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
			Player->AutoPossessPlayer = EAutoReceiveInput::Disabled;
			Player->FinishSpawning(FTransform(FVector(0, 0, 200)));
			Controller->GetPlayerState<ARoguePlayerState>()->SetProgressionSnapshotForRun({});
			Controller->Possess(Player);
			Player->SetActorTickEnabled(false);
			Player->GetCharacterMovement()->SetComponentTickEnabled(false);
			Player->GetAbilitySystemComponent()->CancelAllAbilities();
			Orbit = Player->FindComponentByClass<URogueShieldOrbitComponent>();
			Attributes = Player->GetRogueAttributeSet();
			if (!Test.TestNotNull(TEXT("Player has the inherited shield orbit component"), Orbit)
				|| !Test.TestNotNull(TEXT("The real player GAS attribute set exists"), Attributes)) { return false; }
			Orbit->ShieldMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
			return Test.TestNotNull(TEXT("Test-only mesh can be configured"), Orbit->ShieldMesh.Get());
		}

		void Shield(float Maximum, float Current)
		{
			UAbilitySystemComponent* ASC = Player->GetAbilitySystemComponent();
			ASC->SetNumericAttributeBase(URogueAttributeSet::GetMaxArmorAttribute(), Maximum);
			ASC->SetNumericAttributeBase(URogueAttributeSet::GetArmorAttribute(), Current);
			Orbit->RefreshShieldVisuals();
		}

		void Step(float Seconds)
		{
			const int32 Count = FMath::RoundToInt(Seconds / .02f);
			for (int32 Index = 0; Index < Count; ++Index) { TestWorld.TickTestWorld(.02f); }
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueShieldOrbitStateTest,
	"Rogue.Gameplay.ShieldOrbit.RequiresShieldAndPositiveCurrentValue", RogueShieldOrbitTests::Flags)
bool FRogueShieldOrbitStateTest::RunTest(const FString& Parameters)
{
	using namespace RogueShieldOrbitTests;
	FFixture F; if (!F.Initialize(*this)) { return false; }
	TestEqual(TEXT("Exactly three runtime mesh components exist"), F.Orbit->GetOrbitMeshes().Num(), 3);
	const auto OriginalMeshes = F.Orbit->GetOrbitMeshes();
	F.Shield(0, 0); TestFalse(TEXT("No shield is hidden"), F.Orbit->IsShieldVisible());
	// 模拟当前值先于上限到达客户端；仅有当前值不能视为已经拥有护盾。
	F.Attributes->Armor.SetCurrentValue(10); F.Orbit->RefreshShieldVisuals();
	TestFalse(TEXT("A positive current value alone cannot bypass owning a shield"), F.Orbit->IsShieldVisible());
	F.Shield(50, 0); TestFalse(TEXT("Unlocked but depleted shield is hidden"), F.Orbit->IsShieldVisible());
	F.Shield(50, 12); TestTrue(TEXT("Owned shield with positive current value shows all three"), F.Orbit->IsShieldVisible());
	for (UStaticMeshComponent* Mesh : OriginalMeshes) { TestTrue(TEXT("Each shield mesh is visible"), Mesh->IsVisible()); }
	F.Player->GetAbilitySystemComponent()->SetNumericAttributeBase(URogueAttributeSet::GetArmorAttribute(), 0);
	F.Step(.02f); TestFalse(TEXT("A GAS shield change is consumed by the component tick"), F.Orbit->IsShieldVisible());
	F.Shield(50, .01f); TestTrue(TEXT("Any positive restored shield becomes visible"), F.Orbit->IsShieldVisible());
	TestTrue(TEXT("Depletion and restoration reuse the same three meshes"), F.Orbit->GetOrbitMeshes() == OriginalMeshes);
	F.Player->FinalizeForGameOver(); F.Orbit->RefreshShieldVisuals();
	TestFalse(TEXT("Eliminated player hides shield even with shield points remaining"), F.Orbit->IsShieldVisible());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueShieldOrbitGeometryTest,
	"Rogue.Gameplay.ShieldOrbit.AuthoredRadiusSpeedScaleAndPlayerTurning", RogueShieldOrbitTests::Flags)
bool FRogueShieldOrbitGeometryTest::RunTest(const FString& Parameters)
{
	using namespace RogueShieldOrbitTests;
	FFixture F; if (!F.Initialize(*this)) { return false; }
	F.Orbit->OrbitRadius = 150; F.Orbit->OrbitSpeed = 90; F.Orbit->HeightOffset = 35;
	F.Orbit->MeshScale = FVector(.2, .3, .4); F.Shield(50, 25);
	const auto& Meshes = F.Orbit->GetOrbitMeshes();
	if (!TestEqual(TEXT("Three meshes are available"), Meshes.Num(), 3)) { return false; }
	const FVector Center = F.Player->GetActorLocation();
	for (int32 Index = 0; Index < 3; ++Index)
	{
		const FVector Offset = Meshes[Index]->GetComponentLocation() - Center;
		TestTrue(TEXT("Configured radius is measured in world centimeters"), FMath::IsNearlyEqual(Offset.Size2D(), 150.0, .01));
		TestTrue(TEXT("Configured height is applied"), FMath::IsNearlyEqual(Offset.Z, 35.0, .01));
		TestTrue(TEXT("Configured mesh scale is applied"), Meshes[Index]->GetComponentScale().Equals(FVector(.2, .3, .4), .001));
		const FVector Next = Meshes[(Index + 1) % 3]->GetComponentLocation() - Center;
		TestTrue(TEXT("The three meshes are spaced 120 degrees apart"), FMath::IsNearlyEqual(
			FVector::DotProduct(Offset.GetSafeNormal2D(), Next.GetSafeNormal2D()), -.5, .001));
	}
	F.Step(1.0f);
	TestTrue(TEXT("90 degrees per second advances a quarter orbit in one second"),
		(Meshes[0]->GetComponentLocation() - Center).Equals(FVector(0, 150, 35), .05));
	const FVector BeforeTurn = Meshes[0]->GetComponentLocation();
	F.Player->SetActorRotation(FRotator(0, 170, 0)); F.Orbit->RefreshShieldVisuals();
	TestTrue(TEXT("Player turning does not add an unwanted rotation to the orbit"), Meshes[0]->GetComponentLocation().Equals(BeforeTurn, .01));
	F.Orbit->OrbitSpeed = 0; F.Player->SetActorLocation(Center + FVector(100, 50, 0)); F.Step(.1f);
	TestTrue(TEXT("Zero speed still follows player movement"), Meshes[0]->GetComponentLocation().Equals(BeforeTurn + FVector(100, 50, 0), .05));
	F.Orbit->OrbitSpeed = -90; F.Step(1.0f);
	TestTrue(TEXT("Negative speed reverses the orbit"), (Meshes[0]->GetComponentLocation() - F.Player->GetActorLocation()).Equals(FVector(150, 0, 35), .05));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueShieldOrbitLifecycleTest,
	"Rogue.Gameplay.ShieldOrbit.CosmeticOnlyConfigurationAndCleanup", RogueShieldOrbitTests::Flags)
bool FRogueShieldOrbitLifecycleTest::RunTest(const FString& Parameters)
{
	using namespace RogueShieldOrbitTests;
	FFixture F; if (!F.Initialize(*this)) { return false; }
	F.Shield(50, 10);
	TArray<TWeakObjectPtr<UStaticMeshComponent>> Meshes;
	for (UStaticMeshComponent* Mesh : F.Orbit->GetOrbitMeshes())
	{
		Meshes.Add(Mesh);
		TestTrue(TEXT("The configured mesh is used without a hardcoded replacement"), Mesh->GetStaticMesh() == F.Orbit->ShieldMesh);
		TestTrue(TEXT("Shield mesh cannot collide with players or scenery"), Mesh->GetCollisionEnabled() == ECollisionEnabled::NoCollision);
		TestFalse(TEXT("Shield mesh generates no overlap events"), Mesh->GetGenerateOverlapEvents());
		TestFalse(TEXT("Shield mesh does not simulate physics"), Mesh->IsSimulatingPhysics());
		TestFalse(TEXT("Shield mesh cannot affect navigation"), Mesh->CanEverAffectNavigation());
		TestFalse(TEXT("Shield mesh transforms are not replicated"), Mesh->GetIsReplicated());
	}
	F.Orbit->Deactivate(); TestFalse(TEXT("Deactivating hides all models"), F.Orbit->IsShieldVisible());
	F.Orbit->Activate(); TestTrue(TEXT("Activating restores a still-valid shield"), F.Orbit->IsShieldVisible());
	F.Orbit->ShieldMesh = nullptr; F.Orbit->RefreshShieldVisuals();
	TestFalse(TEXT("An empty mesh setting hides rather than using a fallback mesh"), F.Orbit->IsShieldVisible());
	F.Orbit->DestroyComponent();
	for (auto Mesh : Meshes) { TestFalse(TEXT("Removing the orbit component destroys its runtime meshes"), Mesh.IsValid()); }
	TestTrue(TEXT("Removing a visual component preserves the character"), IsValid(F.Player));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueShieldOrbitBlueprintTest,
	"Rogue.Gameplay.ShieldOrbit.PlayerBlueprintInheritsEditableComponent", RogueShieldOrbitTests::Flags)
bool FRogueShieldOrbitBlueprintTest::RunTest(const FString& Parameters)
{
	UClass* PlayerClass = LoadClass<ARogueCharacter>(nullptr, TEXT("/Game/BP/Player/BP_RogueCharacter.BP_RogueCharacter_C"));
	if (!TestNotNull(TEXT("The real player Blueprint loads"), PlayerClass)) { return false; }
	ARogueCharacter* Defaults = PlayerClass->GetDefaultObject<ARogueCharacter>();
	const auto* Orbit = Cast<URogueShieldOrbitComponent>(Defaults->GetDefaultSubobjectByName(TEXT("ShieldOrbit")));
	if (!TestNotNull(TEXT("The existing Blueprint inherits the new component"), Orbit)) { return false; }
	TestTrue(TEXT("Inherited component is enabled"), Orbit->bOrbitEnabled);
	TestNull(TEXT("The new component does not hardcode a mesh"), Orbit->ShieldMesh.Get());
	for (const FName Name : { FName(TEXT("ShieldMesh")), FName(TEXT("OrbitRadius")), FName(TEXT("OrbitSpeed")) })
	{
		const FProperty* Property = FindFProperty<FProperty>(URogueShieldOrbitComponent::StaticClass(), Name);
		TestTrue(TEXT("Mesh, radius and speed are editable in Blueprint component details"), Property && Property->HasAnyPropertyFlags(CPF_Edit));
	}
	return true;
}

#endif
