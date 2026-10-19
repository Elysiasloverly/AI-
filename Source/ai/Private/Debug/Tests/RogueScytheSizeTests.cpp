#if WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystem/RogueAttributeSet.h"
#include "Combat/Pooling/RogueCombatPoolSubsystem.h"
#include "Combat/Projectiles/RogueOrbitingBlade.h"
#include "Combat/Weapons/RogueWeapon_Scythe.h"
#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Enemies/RogueEnemy.h"
#include "Framework/RogueGameMode.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/WorldSettings.h"
#include "Misc/AutomationTest.h"
#include "Player/RogueCharacter.h"
#include "Player/RoguePlayerBalanceAsset.h"
#include "Player/RoguePlayerController.h"
#include "Player/RoguePlayerState.h"
#include "Tests/AutomationCommon.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

namespace RogueScytheSizeTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;

	struct FFixture
	{
		FTestConsoleVariable WeaponSelfTest{TEXT("Rogue.SelfTest.WeaponUpgradeChain")};
		FTestConsoleVariable EnemyStress{TEXT("Rogue.SelfTest.EnemyStress")};
		TStrongObjectPtr<URoguePlayerBalanceAsset> Balance{NewObject<URoguePlayerBalanceAsset>()};
		FTestWorldWrapper TestWorld;
		UWorld* World = nullptr;
		ARogueCharacter* Players[2] = {};
		ARogueWeapon_Scythe* Scythes[2] = {};

		bool Initialize(FAutomationTestBase& Test, int32 PlayerCount = 1)
		{
			WeaponSelfTest.Set(TEXT("0"));
			EnemyStress.Set(TEXT("0"));
			if (!TestWorld.CreateTestWorld(EWorldType::Game))
			{
				TestWorld.ForwardErrorMessages(&Test);
				return false;
			}
			World = TestWorld.GetTestWorld();
			World->GetWorldSettings()->DefaultGameMode = ARogueGameMode::StaticClass();
			if (!TestWorld.BeginPlayInTestWorld())
			{
				TestWorld.ForwardErrorMessages(&Test);
				return false;
			}
			ARogueGameMode* Mode = World->GetAuthGameMode<ARogueGameMode>();
			if (!Test.TestNotNull(TEXT("The authoritative GameMode exists"), Mode)) { return false; }
			Mode->SetActorTickEnabled(false);

			for (int32 Index = 0; Index < PlayerCount; ++Index)
			{
				ARoguePlayerController* Controller = World->SpawnActor<ARoguePlayerController>();
				const FTransform Transform(FVector(Index * 1500.0f, 0.0f, 120.0f));
				Players[Index] = World->SpawnActorDeferred<ARogueCharacter>(ARogueCharacter::StaticClass(),
					Transform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
				if (!Test.TestNotNull(TEXT("The real player controller exists"), Controller)
					|| !Test.TestNotNull(TEXT("The deferred player exists"), Players[Index])) { return false; }

				// Keep the real initialization path, with defaults isolated from editable project assets.
				FSoftObjectProperty* BalanceProperty = FindFProperty<FSoftObjectProperty>(ARogueCharacter::StaticClass(), TEXT("PlayerBalanceAsset"));
				FObjectProperty* TableProperty = FindFProperty<FObjectProperty>(ARogueCharacter::StaticClass(), TEXT("WeaponDataTable"));
				if (!Test.TestNotNull(TEXT("Player balance can be configured before BeginPlay"), BalanceProperty)
					|| !Test.TestNotNull(TEXT("The weapon table can be configured before BeginPlay"), TableProperty)) { return false; }
				*BalanceProperty->ContainerPtrToValuePtr<TSoftObjectPtr<URoguePlayerBalanceAsset>>(Players[Index]) = Balance.Get();
				FRogueWeaponTableRow Config;
				Config.WeaponClass = ARogueWeapon_Scythe::StaticClass();
				Config.CountUpgradeType = ERogueUpgradeType::ScytheCount;
				Config.BladeClass = ARogueOrbitingBlade::StaticClass();
				Config.Count = 1;
				Config.OrbitRadius = 200.0f;
				Config.RotationSpeed = 0.0f;
				Config.Damage = 17.0f;
				UDataTable* Table = NewObject<UDataTable>(World);
				Table->RowStruct = FRogueWeaponTableRow::StaticStruct();
				Table->AddRow(FName(TEXT("Scythe")), Config);
				TableProperty->SetObjectPropertyValue_InContainer(Players[Index], Table);
				Players[Index]->AutoPossessPlayer = EAutoReceiveInput::Disabled;
				Players[Index]->FinishSpawning(Transform);
				Controller->Possess(Players[Index]);
				Players[Index]->SetActorTickEnabled(false);
				Players[Index]->GetCharacterMovement()->SetComponentTickEnabled(false);
				Players[Index]->SetActorEnableCollision(false);
				Scythes[Index] = Players[Index]->FindWeapon<ARogueWeapon_Scythe>();
				if (!Test.TestNotNull(TEXT("Normal initialization creates the scythe"), Scythes[Index])
					|| !Test.TestNotNull(TEXT("The player owns real GAS attributes through PlayerState"), Players[Index]->GetRogueAttributeSet())
					|| !Test.TestNotNull(TEXT("Possession creates the real PlayerState"), Players[Index]->GetPlayerState<ARoguePlayerState>())) { return false; }
				if (!Test.TestTrue(TEXT("The test player is alive"), Players[Index]->IsAlive())) { return false; }
			}
			return !Test.HasAnyErrors();
		}

		void UpgradeSize(int32 PlayerIndex = 0) const
		{
			FRogueUpgradeOption Upgrade;
			Upgrade.Type = ERogueUpgradeType::ScytheSize;
			Upgrade.Magnitude = 0.2f;
			Players[PlayerIndex]->ApplyUpgrade(Upgrade);
		}

		TArray<ARogueOrbitingBlade*> ActiveBlades(int32 PlayerIndex = 0) const
		{
			TArray<ARogueOrbitingBlade*> Result;
			for (TActorIterator<ARogueOrbitingBlade> It(World); It; ++It)
			{
				if (!It->IsAvailableInPool() && It->GetOwner() == Players[PlayerIndex]) { Result.Add(*It); }
			}
			return Result;
		}
	};

	bool CheckSize(FAutomationTestBase& Test, ARogueOrbitingBlade* Blade, const FVector& BaseMeshScale,
		float BaseSphereRadius, float Multiplier, const TCHAR* Stage)
	{
		UStaticMeshComponent* Mesh = Blade->FindComponentByClass<UStaticMeshComponent>();
		USphereComponent* Sphere = Blade->FindComponentByClass<USphereComponent>();
		if (!Test.TestNotNull(TEXT("The blade has its real mesh"), Mesh)
			|| !Test.TestNotNull(TEXT("The blade has its real collision sphere"), Sphere)) { return false; }
		const bool bMeshMatches = Mesh->GetRelativeScale3D().Equals(BaseMeshScale * Multiplier, 0.0001f);
		const bool bSphereMatches = FMath::IsNearlyEqual(Sphere->GetUnscaledSphereRadius(), BaseSphereRadius * Multiplier, 0.0001f);
		Test.TestTrue(FString::Printf(TEXT("%s: the actual mesh uses its baseline times the size multiplier"), Stage), bMeshMatches);
		Test.TestTrue(FString::Printf(TEXT("%s: the actual collision radius uses the same multiplier"), Stage), bSphereMatches);
		return bMeshMatches && bSphereMatches;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueScytheSizeGASAndBladeTest,
	"Rogue.Gameplay.ScytheSize.RealGASUpgradeScalesVisualAndCollision", RogueScytheSizeTests::Flags)

bool FRogueScytheSizeGASAndBladeTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RogueScytheSizeTests;
	FFixture Fixture;
	if (!Fixture.Initialize(*this)) { return false; }
	ARogueWeapon_Scythe* Scythe = Fixture.Scythes[0];
	Scythe->WeaponTick(0.0f);
	TArray<ARogueOrbitingBlade*> Blades = Fixture.ActiveBlades();
	if (!TestEqual(TEXT("A real weapon tick creates the configured single blade"), Blades.Num(), 1)) { return false; }
	ARogueOrbitingBlade* Blade = Blades[0];
	UStaticMeshComponent* Mesh = Blade->FindComponentByClass<UStaticMeshComponent>();
	USphereComponent* Sphere = Blade->FindComponentByClass<USphereComponent>();
	if (!TestNotNull(TEXT("The blade mesh exists"), Mesh) || !TestNotNull(TEXT("The blade sphere exists"), Sphere)) { return false; }
	const FVector BaseScale = Mesh->GetRelativeScale3D();
	const float BaseRadius = Sphere->GetUnscaledSphereRadius();
	const FVector BaseLocation = Blade->GetActorLocation();
	const float OrbitRadius = Scythe->GetEffectiveOrbitRadius();
	const float Damage = Scythe->GetEffectiveDamage();
	const float RotationSpeed = Scythe->GetEffectiveRotationSpeed();
	TestEqual(TEXT("An unupgraded scythe starts at normal size"), Scythe->GetEffectiveSizeMultiplier(), 1.0f);

	for (int32 UpgradeIndex = 1; UpgradeIndex <= 2; ++UpgradeIndex)
	{
		Fixture.UpgradeSize();
		const float ExpectedBonus = UpgradeIndex * 0.2f;
		const float ExpectedSize = 1.0f + ExpectedBonus;
		TestTrue(TEXT("The actual upgrade GameplayEffect changes the GAS size attribute"),
			FMath::IsNearlyEqual(Fixture.Players[0]->GetRogueAttributeSet()->GetScytheSizeBonus(), ExpectedBonus));
		TestTrue(TEXT("The weapon snapshot receives the size upgrade in the same call stack"),
			FMath::IsNearlyEqual(Fixture.Players[0]->GetWeaponBonusSnapshot().ScytheSize, ExpectedBonus));
		TestTrue(TEXT("Size increases additively to 1.2 then 1.4"), FMath::IsNearlyEqual(Scythe->GetEffectiveSizeMultiplier(), ExpectedSize));
		Scythe->WeaponTick(0.0f);
		CheckSize(*this, Blade, BaseScale, BaseRadius, ExpectedSize, TEXT("After real GAS upgrade"));
		TestEqual(TEXT("Growing does not add blades"), Fixture.ActiveBlades().Num(), 1);
		TestEqual(TEXT("Growing does not change the orbit radius"), Scythe->GetEffectiveOrbitRadius(), OrbitRadius);
		TestEqual(TEXT("Growing does not change damage per hit"), Scythe->GetEffectiveDamage(), Damage);
		TestEqual(TEXT("Growing does not change rotation speed"), Scythe->GetEffectiveRotationSpeed(), RotationSpeed);
		TestTrue(TEXT("The actual blade stays on the same orbit position"), Blade->GetActorLocation().Equals(BaseLocation, 0.0001f));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueScytheSizeExtendedContactTest,
	"Rogue.Gameplay.ScytheSize.ExpandedCollisionDealsRealDamage", RogueScytheSizeTests::Flags)

bool FRogueScytheSizeExtendedContactTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RogueScytheSizeTests;
	FFixture Fixture;
	if (!Fixture.Initialize(*this)) { return false; }
	Fixture.Scythes[0]->WeaponTick(0.0f);
	const TArray<ARogueOrbitingBlade*> Blades = Fixture.ActiveBlades();
	if (!TestEqual(TEXT("The contact test has exactly one real blade"), Blades.Num(), 1)) { return false; }
	ARogueOrbitingBlade* Blade = Blades[0];
	USphereComponent* Sphere = Blade->FindComponentByClass<USphereComponent>();
	if (!TestNotNull(TEXT("The blade collision exists"), Sphere)) { return false; }
	const float OldRadius = Sphere->GetScaledSphereRadius();

	FActorSpawnParameters Spawn;
	Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ARogueEnemy* Enemy = Fixture.World->SpawnActor<ARogueEnemy>(ARogueEnemy::StaticClass(),
		FVector(5000.0f, 0.0f, 180.0f), FRotator::ZeroRotator, Spawn);
	if (!TestNotNull(TEXT("The contact target is a real enemy"), Enemy)) { return false; }
	FRogueEnemyProfile Profile;
	Profile.Health = 1000.0f;
	Profile.Speed = 0.0f;
	Profile.Damage = 0.0f;
	Enemy->ActivatePooledEnemy(nullptr, Enemy->GetActorLocation(), FRotator::ZeroRotator);
	Enemy->InitializeEnemy(Profile);
	Enemy->SetActorTickEnabled(false);
	Enemy->GetCharacterMovement()->SetComponentTickEnabled(false);
	Enemy->SetActorEnableCollision(false);
	Enemy->SetActorScale3D(FVector::OneVector);
	UCapsuleComponent* Capsule = Enemy->GetCapsuleComponent();
	Capsule->SetCapsuleSize(5.0f, 5.0f);
	Capsule->SetCollisionObjectType(ECC_Pawn);
	Capsule->SetCollisionResponseToAllChannels(ECR_Ignore);
	Capsule->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
	Capsule->SetGenerateOverlapEvents(true);
	Capsule->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	// The target is separated from the old sphere by 10% of its radius and lies
	// 10% inside the upgraded sphere. No world tick or AI movement can alter this geometry.
	const float Distance = OldRadius * 1.1f + Capsule->GetScaledCapsuleRadius();
	Enemy->SetActorLocation(Blade->GetActorLocation() + FVector(Distance, 0.0f, 0.0f), false, nullptr, ETeleportType::TeleportPhysics);
	Enemy->SetActorEnableCollision(true);
	Capsule->UpdateOverlaps();
	Sphere->UpdateOverlaps();
	URogueAttributeSet* EnemyAttributes = Enemy->GetRogueAttributeSet();
	if (!TestNotNull(TEXT("The enemy owns real authoritative GAS attributes"), EnemyAttributes)) { return false; }
	const float BeforeHealth = EnemyAttributes->GetHealth();
	TestFalse(TEXT("The target lies outside the original collision volume"), Sphere->IsOverlappingActor(Enemy));
	Blade->Tick(0.0f);
	TestEqual(TEXT("The original blade cannot damage that target"), EnemyAttributes->GetHealth(), BeforeHealth);

	Fixture.UpgradeSize();
	Fixture.Scythes[0]->WeaponTick(0.0f);
	Sphere->UpdateOverlaps();
	if (!TestTrue(TEXT("The same unmoved target overlaps the upgraded collision volume"), Sphere->IsOverlappingActor(Enemy))) { return false; }
	Blade->Tick(0.0f);
	TestEqual(TEXT("Expanded contact reaches the actual damage GameplayEffect"),
		EnemyAttributes->GetHealth(), BeforeHealth - Fixture.Scythes[0]->GetEffectiveDamage());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueScytheSizeBlueprintPoolReuseTest,
	"Rogue.Gameplay.ScytheSize.BlueprintBaselineAndDifferentOwnerReuse", RogueScytheSizeTests::Flags)

bool FRogueScytheSizeBlueprintPoolReuseTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RogueScytheSizeTests;
	FFixture Fixture;
	if (!Fixture.Initialize(*this, 2)) { return false; }
	UClass* BladeClass = LoadClass<ARogueOrbitingBlade>(nullptr,
		TEXT("/Game/BP/Projectile/BP_RogueOrbitingBlade.BP_RogueOrbitingBlade_C"));
	if (!TestNotNull(TEXT("The real project blade Blueprint loads"), BladeClass)) { return false; }
	AActor* Defaults = BladeClass->GetDefaultObject<AActor>();
	const UStaticMeshComponent* DefaultMesh = Cast<UStaticMeshComponent>(Defaults->GetDefaultSubobjectByName(TEXT("Mesh")));
	const USphereComponent* DefaultSphere = Cast<USphereComponent>(Defaults->GetDefaultSubobjectByName(TEXT("Collision")));
	if (!TestNotNull(TEXT("The Blueprint retains the native mesh component"), DefaultMesh)
		|| !TestNotNull(TEXT("The Blueprint retains the native collision component"), DefaultSphere)) { return false; }
	const FVector BlueprintScale = DefaultMesh->GetRelativeScale3D();
	const float BlueprintRadius = DefaultSphere->GetUnscaledSphereRadius();
	URogueCombatPoolSubsystem* Pool = Fixture.World->GetSubsystem<URogueCombatPoolSubsystem>();
	if (!TestNotNull(TEXT("The real combat pool exists"), Pool)) { return false; }
	ARogueOrbitingBlade* Blade = Pool->AcquireOrbitingBlade(BladeClass, Fixture.Players[0],
		Fixture.Players[0]->GetActorLocation(), FRotator::ZeroRotator);
	if (!TestNotNull(TEXT("The Blueprint blade is acquired through the real pool"), Blade)) { return false; }
	Blade->ActivatePooledBlade(Fixture.Players[0]);
	CheckSize(*this, Blade, BlueprintScale, BlueprintRadius, 1.0f, TEXT("Blueprint activation"));
	Fixture.UpgradeSize();
	Fixture.UpgradeSize();
	Blade->ConfigureBlade(Fixture.Players[0], 0, 1, 200.0f, 0.0f, 17.0f, 0.0f,
		Fixture.Scythes[0]->GetEffectiveSizeMultiplier());
	CheckSize(*this, Blade, BlueprintScale, BlueprintRadius, 1.4f, TEXT("Blueprint upgraded owner"));
	Blade->DeactivateToPool();
	TestTrue(TEXT("The same Blueprint blade returns to the pool"), Blade->IsAvailableInPool());
	CheckSize(*this, Blade, BlueprintScale, BlueprintRadius, 1.0f, TEXT("Pooled baseline"));

	// Reuse this exact pooled object, so acquiring another pristine actor cannot hide stale state.
	Blade->ActivatePooledBlade(Fixture.Players[1]);
	CheckSize(*this, Blade, BlueprintScale, BlueprintRadius, 1.0f, TEXT("Different owner activation"));
	TestTrue(TEXT("Activation installs the new owner on the same actor"), Blade->GetOwner() == Fixture.Players[1]);
	TestEqual(TEXT("The other player's real GAS size remains unupgraded"), Fixture.Players[1]->GetRogueAttributeSet()->GetScytheSizeBonus(), 0.0f);
	Blade->ConfigureBlade(Fixture.Players[1], 0, 1, 200.0f, 0.0f, 17.0f, 0.0f);
	CheckSize(*this, Blade, BlueprintScale, BlueprintRadius, 1.0f, TEXT("Legacy configure call defaults to normal size"));
	return true;
}

#endif
