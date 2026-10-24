#if WITH_DEV_AUTOMATION_TESTS

#include "Combat/Pooling/RogueCombatPoolSubsystem.h"
#include "Combat/Visuals/RogueLaserBeam.h"
#include "Combat/Weapons/RogueWeapon_HellTower.h"
#include "Combat/Weapons/RogueWeapon_Laser.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/RogueGameMode.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/WorldSettings.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/AutomationTest.h"
#include "Player/RogueCharacter.h"
#include "Player/RoguePlayerController.h"
#include "Tests/AutomationCommon.h"
#include "UObject/UnrealType.h"

namespace RogueHellTowerBeamTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;

	struct FFixture
	{
		FTestConsoleVariable WeaponSelfTest{TEXT("Rogue.SelfTest.WeaponUpgradeChain")};
		FTestConsoleVariable EnemyStress{TEXT("Rogue.SelfTest.EnemyStress")};
		FTestWorldWrapper TestWorld;
		UWorld* World = nullptr;
		ARogueCharacter* Player = nullptr;
		const FRogueWeaponTableRow* Laser = nullptr;
		const FRogueWeaponTableRow* HellTower = nullptr;

		bool Initialize(FAutomationTestBase& Test)
		{
			WeaponSelfTest.Set(TEXT("0"));
			EnemyStress.Set(TEXT("0"));
			if (!TestWorld.CreateTestWorld(EWorldType::Game)) { return false; }
			World = TestWorld.GetTestWorld();
			World->GetWorldSettings()->DefaultGameMode = ARogueGameMode::StaticClass();
			if (!TestWorld.BeginPlayInTestWorld()) { return false; }
			ARogueGameMode* Mode = World->GetAuthGameMode<ARogueGameMode>();
			if (!Test.TestNotNull(TEXT("A real GameMode exists"), Mode)) { return false; }
			Mode->SetActorTickEnabled(false);
			ARoguePlayerController* Controller = World->SpawnActor<ARoguePlayerController>();
			Player = World->SpawnActorDeferred<ARogueCharacter>(ARogueCharacter::StaticClass(),
				FTransform(FVector(0.0f, 0.0f, 120.0f)), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
			if (!Test.TestNotNull(TEXT("A controller exists"), Controller) || !Test.TestNotNull(TEXT("A player exists"), Player)) { return false; }
			Player->AutoPossessPlayer = EAutoReceiveInput::Disabled;
			Player->FinishSpawning(FTransform(FVector(0.0f, 0.0f, 120.0f)));
			Controller->Possess(Player);
			Player->SetActorTickEnabled(false);
			Player->GetCharacterMovement()->SetComponentTickEnabled(false);
			UDataTable* Table = LoadObject<UDataTable>(nullptr, TEXT("/Game/Data/Weapons/DT_WeaponConfig.DT_WeaponConfig"));
			if (!Test.TestNotNull(TEXT("The actual weapon table loads"), Table)) { return false; }
			Laser = Table->FindRow<FRogueWeaponTableRow>(TEXT("Laser"), TEXT("HellTowerBeamTests"));
			HellTower = Table->FindRow<FRogueWeaponTableRow>(TEXT("HellTower"), TEXT("HellTowerBeamTests"));
			return Test.TestNotNull(TEXT("The Laser row exists"), Laser)
				&& Test.TestNotNull(TEXT("The HellTower row exists"), HellTower);
		}

		void Advance(float Seconds)
		{
			while (Seconds > KINDA_SMALL_NUMBER)
			{
				const float Step = FMath::Min(Seconds, 0.02f);
				TestWorld.TickTestWorld(Step);
				Seconds -= Step;
			}
		}
	};

	UStaticMeshComponent* Mesh(ARogueLaserBeam* Beam, const TCHAR* Name)
	{
		TInlineComponentArray<UStaticMeshComponent*> Components;
		Beam->GetComponents(Components);
		for (UStaticMeshComponent* Component : Components)
		{
			if (Component->GetFName() == FName(Name)) { return Component; }
		}
		return nullptr;
	}

	bool UseComponentMaterials(FAutomationTestBase& Test, ARogueLaserBeam* Beam, bool bEnabled)
	{
		FBoolProperty* Property = FindFProperty<FBoolProperty>(ARogueLaserBeam::StaticClass(), TEXT("bUseBlueprintComponentMaterials"));
		if (!Test.TestNotNull(TEXT("The material source setting exists"), Property)) { return false; }
		Property->SetPropertyValue_InContainer(Beam, bEnabled);
		return true;
	}

	bool SetMaterial(FAutomationTestBase& Test, ARogueLaserBeam* Beam, const TCHAR* Name, UMaterialInterface* Material)
	{
		FObjectProperty* Property = FindFProperty<FObjectProperty>(ARogueLaserBeam::StaticClass(), Name);
		if (!Test.TestNotNull(TEXT("The material configuration exists"), Property)) { return false; }
		Test.TestTrue(TEXT("The material is editable in Blueprint class defaults"),
			Property->HasAllPropertyFlags(CPF_Edit | CPF_DisableEditOnInstance));
		Property->SetObjectPropertyValue_InContainer(Beam, Material);
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueHellTowerIndependentBeamTest,
	"Rogue.Gameplay.HellTowerBeam.IndependentBlueprintAndCosmeticRouting", RogueHellTowerBeamTests::Flags)

bool FRogueHellTowerIndependentBeamTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RogueHellTowerBeamTests;
	FFixture Fixture;
	if (!Fixture.Initialize(*this)) { return false; }
	UClass* LaserClass = Fixture.Laser->BeamClass.Get();
	UClass* HellClass = Fixture.HellTower->BeamClass.Get();
	if (!TestNotNull(TEXT("Laser retains its configured Blueprint"), LaserClass)
		|| !TestNotNull(TEXT("HellTower has its own Blueprint"), HellClass)) { return false; }
	TestEqual(TEXT("Laser retains its original class"), LaserClass->GetPathName(),
		FString(TEXT("/Game/BP/Projectiles/BP_RogueLaserBeam.BP_RogueLaserBeam_C")));
	TestEqual(TEXT("HellTower uses the separate class"), HellClass->GetPathName(),
		FString(TEXT("/Game/BP/Projectiles/BP_RogueHellTowerBeam.BP_RogueHellTowerBeam_C")));
	TestFalse(TEXT("HellTower is independent of the editable laser Blueprint"), HellClass->IsChildOf(LaserClass));
	TestTrue(TEXT("Both share only the native beam implementation"), HellClass->GetSuperClass() == ARogueLaserBeam::StaticClass());

	ARogueWeapon_HellTower* HellWeapon = Fixture.World->SpawnActor<ARogueWeapon_HellTower>();
	ARogueWeapon_Laser* LaserWeapon = Fixture.World->SpawnActor<ARogueWeapon_Laser>();
	if (!TestNotNull(TEXT("A real HellTower weapon exists"), HellWeapon)
		|| !TestNotNull(TEXT("A real laser weapon exists"), LaserWeapon)) { return false; }
	HellWeapon->InitializeWeapon(Fixture.Player, *Fixture.HellTower);
	LaserWeapon->InitializeWeapon(Fixture.Player, *Fixture.Laser);
	FRogueWeaponCosmeticFire Fire;
	Fire.Start = FVector(0.0f, 0.0f, 180.0f);
	Fire.End = FVector(500.0f, 0.0f, 180.0f);
	Fire.Param = 0.25f;
	HellWeapon->PlayCosmeticFire(Fire);
	LaserWeapon->PlayCosmeticFire(Fire);
	int32 HellBeams = 0;
	int32 LaserBeams = 0;
	for (TActorIterator<ARogueLaserBeam> It(Fixture.World); It; ++It)
	{
		if (It->IsAvailableInPool()) { continue; }
		HellBeams += It->GetClass() == HellClass ? 1 : 0;
		LaserBeams += It->GetClass() == LaserClass ? 1 : 0;
	}
	TestEqual(TEXT("HellTower cosmetic replay creates its own beam class exactly once"), HellBeams, 1);
	TestEqual(TEXT("Laser cosmetic replay continues to create the original class"), LaserBeams, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueHellTowerBeamMaterialPoolTest,
	"Rogue.Gameplay.HellTowerBeam.EditableMaterialsAndPoolReset", RogueHellTowerBeamTests::Flags)

bool FRogueHellTowerBeamMaterialPoolTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RogueHellTowerBeamTests;
	FFixture Fixture;
	if (!Fixture.Initialize(*this)) { return false; }
	URogueCombatPoolSubsystem* Pools = Fixture.World->GetSubsystem<URogueCombatPoolSubsystem>();
	if (!TestNotNull(TEXT("The real combat pool exists"), Pools)) { return false; }
	ARogueLaserBeam* Beam = Pools->AcquireLaserBeam(Fixture.HellTower->BeamClass, Fixture.Player, FVector::ZeroVector, FRotator::ZeroRotator);
	if (!TestNotNull(TEXT("The pool returns a HellTower beam"), Beam)) { return false; }
	TestTrue(TEXT("The pooled class matches HellTower rather than laser"), Beam->GetClass() == Fixture.HellTower->BeamClass.Get());
	if (!UseComponentMaterials(*this, Beam, false)) { return false; } // Explicitly test the retained legacy mode.
	UMaterialInterface* BaseMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (!TestNotNull(TEXT("The test base material exists"), BaseMaterial)) { return false; }
	UMaterialInstanceDynamic* Custom = UMaterialInstanceDynamic::Create(BaseMaterial, Beam);
	if (!TestNotNull(TEXT("A distinct configurable material exists"), Custom)) { return false; }
	for (const TCHAR* Name : { TEXT("InfernoBeamMaterial"), TEXT("InfernoGlowMaterial"), TEXT("InfernoRingMaterial") })
	{
		if (!SetMaterial(*this, Beam, Name, Custom)) { return false; }
	}
	const FVector Start(0.0f, 0.0f, 180.0f);
	const FVector End(500.0f, 0.0f, 180.0f);
	Beam->ActivatePooledBeam(Fixture.Player, Start, End, true, false, 0.05f);
	for (const TCHAR* Name : { TEXT("BeamMesh"), TEXT("GlowMesh"), TEXT("StartRingMesh"), TEXT("EndRingMesh") })
	{
		UStaticMeshComponent* Component = Mesh(Beam, Name);
		if (!TestNotNull(TEXT("The real beam mesh component exists"), Component)) { return false; }
		TestTrue(TEXT("Activation uses the configured material rather than constructor defaults"), Component->GetMaterial(0) == Custom);
	}
	Fixture.Advance(0.02f);
	for (const TCHAR* Name : { TEXT("PlayerBeamMaterial"), TEXT("PlayerGlowMaterial"), TEXT("PlayerRingMaterial"),
		TEXT("InfernoBeamMaterial"), TEXT("InfernoGlowMaterial"), TEXT("InfernoRingMaterial") })
	{
		if (!SetMaterial(*this, Beam, Name, nullptr)) { return false; }
	}
	// Reuse the exact actor; a different pristine pool actor cannot hide stale materials/timers.
	Beam->ActivatePooledBeam(Fixture.Player, Start, End, false, false, 0.30f);
	for (const TCHAR* Name : { TEXT("BeamMesh"), TEXT("GlowMesh"), TEXT("StartRingMesh"), TEXT("EndRingMesh") })
	{
		TestTrue(TEXT("Empty configuration clears the previous activation's material"), Mesh(Beam, Name)->GetMaterial(0) != Custom);
	}
	Fixture.Advance(0.08f);
	TestFalse(TEXT("An old short-lived activation cannot hide the reactivated beam"), Beam->IsAvailableInPool());
	Beam->ActivatePooledBeam(Fixture.Player, Start, End, true, false, 0.30f);
	TestTrue(TEXT("Returning to Inferno style with empty material does not restore stale material"), Mesh(Beam, TEXT("BeamMesh"))->GetMaterial(0) != Custom);
	Fixture.Advance(0.35f);
	TestTrue(TEXT("The current activation returns on its own lifetime"), Beam->IsAvailableInPool());
	TestTrue(TEXT("Returned beams are hidden"), Beam->IsHidden());
	TestNull(TEXT("Returning clears the previous owner"), Beam->GetOwner());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueHellTowerComponentMaterialTest,
	"Rogue.Gameplay.HellTowerBeam.ComponentSlotsSurviveActivationStylesAndReuse", RogueHellTowerBeamTests::Flags)

bool FRogueHellTowerComponentMaterialTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RogueHellTowerBeamTests;
	FFixture Fixture;
	if (!Fixture.Initialize(*this)) { return false; }
	ARogueLaserBeam* Beam = Fixture.World->SpawnActor<ARogueLaserBeam>(Fixture.HellTower->BeamClass);
	if (!TestNotNull(TEXT("The actual HellTower Blueprint instance exists"), Beam)
		|| !UseComponentMaterials(*this, Beam, true)) { return false; }
	UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (!TestNotNull(TEXT("The test material loads"), Base)) { return false; }
	UMaterialInstanceDynamic* AuthoredBeam = UMaterialInstanceDynamic::Create(Base, Beam);
	UMaterialInstanceDynamic* AuthoredGlow = UMaterialInstanceDynamic::Create(Base, Beam);
	UMaterialInstanceDynamic* StaleStyle = UMaterialInstanceDynamic::Create(Base, Beam);
	TArray<UStaticMeshComponent*> Components;
	for (const TCHAR* Name : { TEXT("BeamMesh"), TEXT("GlowMesh"), TEXT("StartRingMesh"), TEXT("EndRingMesh") })
	{
		UStaticMeshComponent* Component = Mesh(Beam, Name);
		if (!TestNotNull(TEXT("The real Blueprint mesh component exists"), Component)) { return false; }
		Component->SetMaterial(0, FName(Name) == TEXT("GlowMesh") ? AuthoredGlow : AuthoredBeam);
		Components.Add(Component);
	}
	// A stale class-default value must not mask a component-slot edit in this mode.
	for (const TCHAR* Name : { TEXT("PlayerBeamMaterial"), TEXT("PlayerGlowMaterial"), TEXT("PlayerRingMaterial"),
		TEXT("InfernoBeamMaterial"), TEXT("InfernoGlowMaterial"), TEXT("InfernoRingMaterial") })
	{
		if (!SetMaterial(*this, Beam, Name, StaleStyle)) { return false; }
	}
	for (bool bInferno : {true, false, true})
	{
		Beam->ActivatePooledBeam(Fixture.Player, FVector(0.0f, 0.0f, 180.0f), FVector(500.0f, 0.0f, 180.0f), bInferno, false, 0.05f);
		for (UStaticMeshComponent* Component : Components)
		{
			TestTrue(TEXT("Activation preserves the authored component material slot"),
				Component->GetMaterial(0) == (Component->GetFName() == TEXT("GlowMesh") ? AuthoredGlow : AuthoredBeam));
		}
		Fixture.Advance(0.08f);
		TestTrue(TEXT("The beam still returns on schedule"), Beam->IsAvailableInPool());
		TestTrue(TEXT("Pooling preserves the authored glow slot"), Mesh(Beam, TEXT("GlowMesh"))->GetMaterial(0) == AuthoredGlow);
	}
	// None is also a valid component choice: use the mesh's own material, not a stale style override.
	Mesh(Beam, TEXT("BeamMesh"))->SetMaterial(0, nullptr);
	UMaterialInterface* MeshDefault = Mesh(Beam, TEXT("BeamMesh"))->GetMaterial(0);
	Beam->ActivatePooledBeam(Fixture.Player, FVector::ZeroVector, FVector(500.0f, 0.0f, 0.0f), true, false, 0.10f);
	TestTrue(TEXT("An empty component slot stays on the mesh default"), Mesh(Beam, TEXT("BeamMesh"))->GetMaterial(0) == MeshDefault);
	return true;
}

#endif
