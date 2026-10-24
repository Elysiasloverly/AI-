#if WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystem/RogueAttributeSet.h"
#include "Combat/Weapons/RogueWeapon_Laser.h"
#include "Combat/Weapons/RogueWeapon_Mortar.h"
#include "Combat/Weapons/RogueWeapon_Scythe.h"
#include "Progression/Upgrades/RogueUpgradeSystem.h"
#include "Progression/Meta/RogueUpgradeTreeDefinition.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "Framework/RogueGameMode.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/WorldSettings.h"
#include "Misc/AutomationTest.h"
#include "Player/RogueCharacter.h"
#include "Player/RoguePlayerBalanceAsset.h"
#include "Player/RoguePlayerController.h"
#include "Player/RoguePlayerState.h"
#include "Progression/Upgrades/RogueUpgradeRuleAsset.h"
#include "Progression/Upgrades/RogueUpgradeSystemRules.h"
#include "Tests/AutomationCommon.h"
#include "UObject/UnrealType.h"

namespace RogueUpgradePrerequisiteTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;

	struct FUpgradeFixture
	{
		FTestWorldWrapper TestWorld;
		UWorld* World = nullptr;
		ARogueCharacter* Player = nullptr;
		URogueAttributeSet* Attributes = nullptr;
		ARogueWeapon_Laser* Laser = nullptr;
		ARogueWeapon_Mortar* Mortar = nullptr;
		ARogueWeapon_Scythe* Scythe = nullptr;

		bool Initialize(FAutomationTestBase& Test, int32 MaximumWeaponCount = -1)
		{
			if (!TestWorld.CreateTestWorld(EWorldType::Game)) { return false; }
			World = TestWorld.GetTestWorld();
			World->GetWorldSettings()->DefaultGameMode = ARogueGameMode::StaticClass();
			if (!TestWorld.BeginPlayInTestWorld()) { return false; }
			ARogueGameMode* GameMode = World->GetAuthGameMode<ARogueGameMode>();
			if (!Test.TestNotNull(TEXT("The authoritative GameMode exists"), GameMode)) { return false; }
			GameMode->SetActorTickEnabled(false);

			ARoguePlayerController* Controller = World->SpawnActor<ARoguePlayerController>();
			Player = World->SpawnActorDeferred<ARogueCharacter>(ARogueCharacter::StaticClass(),
				FTransform(FVector(0.0f, 0.0f, 120.0f)), nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
			if (!Test.TestNotNull(TEXT("The controller exists"), Controller)
				|| !Test.TestNotNull(TEXT("The player exists"), Player)) { return false; }

			FRogueWeaponTableRow LaserConfig;
			LaserConfig.WeaponClass = ARogueWeapon_Laser::StaticClass();
			LaserConfig.CountUpgradeType = ERogueUpgradeType::LaserCount;
			LaserConfig.Count = 0;
			LaserConfig.MaxCount = MaximumWeaponCount;
			FRogueWeaponTableRow MortarConfig;
			MortarConfig.WeaponClass = ARogueWeapon_Mortar::StaticClass();
			MortarConfig.CountUpgradeType = ERogueUpgradeType::MortarCount;
			MortarConfig.Count = 0;
			MortarConfig.MaxCount = MaximumWeaponCount;

			FRogueWeaponTableRow ScytheConfig;
			ScytheConfig.WeaponClass = ARogueWeapon_Scythe::StaticClass();
			ScytheConfig.CountUpgradeType = ERogueUpgradeType::ScytheCount;
			ScytheConfig.Count = 0;
			ScytheConfig.MaxCount = MaximumWeaponCount;

			// Configure a transient table before BeginPlay; normal character initialization creates its weapons.
			UDataTable* WeaponTable = NewObject<UDataTable>(World);
			WeaponTable->RowStruct = FRogueWeaponTableRow::StaticStruct();
			WeaponTable->AddRow(FName(TEXT("Laser")), LaserConfig);
			WeaponTable->AddRow(FName(TEXT("Mortar")), MortarConfig);
			WeaponTable->AddRow(FName(TEXT("Scythe")), ScytheConfig);
			FObjectProperty* WeaponTableProperty = FindFProperty<FObjectProperty>(ARogueCharacter::StaticClass(), TEXT("WeaponDataTable"));
			if (!Test.TestNotNull(TEXT("The reflected weapon table configuration exists"), WeaponTableProperty)) { return false; }
			WeaponTableProperty->SetObjectPropertyValue_InContainer(Player, WeaponTable);
			Player->AutoPossessPlayer = EAutoReceiveInput::Disabled;
			Player->FinishSpawning(FTransform(FVector(0.0f, 0.0f, 120.0f)));
			// These tests exercise in-run weapon prerequisites after permanent access has been purchased.
			// Inject that profile before Possess freezes the run; no permanent stat bonuses are granted.
			ARoguePlayerState* ControllerState = Controller->GetPlayerState<ARoguePlayerState>();
			if (!Test.TestNotNull(TEXT("The controller owns a PlayerState before possession"), ControllerState)) { return false; }
			FRogueProgressionSnapshot PermanentAccess;
			for (const FRogueUpgradeTreeNode& Node : URogueUpgradeTreeDefinition::GetDefaultDefinition()->GetNodes())
			{
				if (Node.Kind != ERogueMetaNodeKind::BaseStat) PermanentAccess.NodeRanks.Add(Node.NodeId, 1);
			}
			if (!Test.TestTrue(TEXT("Permanent weapon and card access is injected before the run"),
				ControllerState->SetProgressionSnapshotForRun(PermanentAccess))) { return false; }
			Controller->Possess(Player);
			Player->SetActorTickEnabled(false);
			Player->GetCharacterMovement()->SetComponentTickEnabled(false);
			Player->SetActorEnableCollision(false);

			ARoguePlayerState* PlayerState = Player->GetPlayerState<ARoguePlayerState>();
			if (!Test.TestNotNull(TEXT("Possession created a real Rogue PlayerState"), PlayerState)
				|| !Test.TestNotNull(TEXT("The PlayerState owns a real ASC"), PlayerState->GetRogueAbilitySystemComponent())) { return false; }
			Attributes = Player->GetRogueAttributeSet();
			Laser = Player->FindWeapon<ARogueWeapon_Laser>();
			Mortar = Player->FindWeapon<ARogueWeapon_Mortar>();
			Scythe = Player->FindWeapon<ARogueWeapon_Scythe>();
			if (!Test.TestNotNull(TEXT("The player attribute set exists"), Attributes)
				|| !Test.TestNotNull(TEXT("Normal BeginPlay created the laser"), Laser)
				|| !Test.TestNotNull(TEXT("Normal BeginPlay created the mortar"), Mortar)
				|| !Test.TestNotNull(TEXT("Normal BeginPlay created the scythe"), Scythe)) { return false; }

			// Keep this fixture independent of caps in the project's editable balance asset.
			Attributes->SetWeaponBonusCaps(FRogueWeaponBonusCaps());
			Laser->InitializeWeapon(Player, LaserConfig);
			Mortar->InitializeWeapon(Player, MortarConfig);
			Scythe->InitializeWeapon(Player, ScytheConfig);
			Test.TestEqual(TEXT("Laser starts with no GAS quantity bonus"), Attributes->GetLaserCountBonus(), 0.0f);
			Test.TestEqual(TEXT("Mortar starts with no GAS quantity bonus"), Attributes->GetMortarCountBonus(), 0.0f);
			Test.TestEqual(TEXT("Laser base count is zero"), Player->GetLaserCannonCount(), 0);
			Test.TestEqual(TEXT("Mortar base count is zero"), Player->GetMortarCount(), 0);
			return Test.TestTrue(TEXT("The possessed player is alive"), Player->IsAlive());
		}

		void ApplyQuantityUpgrade(ERogueUpgradeType Type) const
		{
			FRogueUpgradeOption Upgrade;
			Upgrade.Type = Type;
			Upgrade.Magnitude = 1.0f;
			// Exercise the real upgrade -> GameplayEffect -> attribute path without ticking or writing a snapshot.
			Player->ApplyUpgrade(Upgrade);
		}

		TArray<ERogueUpgradeType> BuildPool(const URogueUpgradeRuleAsset* Rules) const
		{
			TArray<ERogueUpgradeType> Pool;
			RogueUpgradeSystemRules::BuildUpgradePool(Pool, Player, Rules);
			return Pool;
		}
	};

	bool CheckImmediateUnlocks(FAutomationTestBase& Test, FUpgradeFixture& Fixture, const URogueUpgradeRuleAsset* Rules)
	{
		// Arrange: locked weapons must not offer upgrades that depend on owning them.
		TArray<ERogueUpgradeType> Pool = Fixture.BuildPool(Rules);
		Test.TestFalse(TEXT("A locked laser cannot offer refraction"), Pool.Contains(ERogueUpgradeType::LaserRefraction));
		Test.TestFalse(TEXT("A locked mortar cannot offer blast radius"), Pool.Contains(ERogueUpgradeType::MortarBlastRadius));
		const double BeforeUpgradeTime = Fixture.World->GetTimeSeconds();

		// Act and assert in the same call stack as the upgrade, as queued level-up choices do.
		Fixture.ApplyQuantityUpgrade(ERogueUpgradeType::LaserCount);
		Test.TestEqual(TEXT("The laser upgrade changed the GAS attribute"), Fixture.Attributes->GetLaserCountBonus(), 1.0f);
		Test.TestEqual(TEXT("Laser base count stays zero after the GAS upgrade"), Fixture.Player->GetLaserCannonCount(), 0);
		Test.TestEqual(TEXT("The effective laser count changes without a tick"), Fixture.Player->GetEffectiveLaserCannonCount(), 1);
		Pool = Fixture.BuildPool(Rules);
		Test.TestTrue(TEXT("Refraction enters the next pool immediately after unlocking the laser"), Pool.Contains(ERogueUpgradeType::LaserRefraction));
		Test.TestFalse(TEXT("Unlocking the laser does not unlock mortar blast radius"), Pool.Contains(ERogueUpgradeType::MortarBlastRadius));

		Fixture.ApplyQuantityUpgrade(ERogueUpgradeType::MortarCount);
		Test.TestEqual(TEXT("The mortar upgrade changed the GAS attribute"), Fixture.Attributes->GetMortarCountBonus(), 1.0f);
		Test.TestEqual(TEXT("Mortar base count stays zero after the GAS upgrade"), Fixture.Player->GetMortarCount(), 0);
		Test.TestEqual(TEXT("The effective mortar count changes without a tick"), Fixture.Player->GetEffectiveMortarCount(), 1);
		Pool = Fixture.BuildPool(Rules);
		Test.TestTrue(TEXT("Mortar blast radius enters the next pool immediately after unlocking the mortar"), Pool.Contains(ERogueUpgradeType::MortarBlastRadius));
		Test.TestTrue(TEXT("Refraction remains eligible after unlocking another weapon"), Pool.Contains(ERogueUpgradeType::LaserRefraction));
		Test.TestEqual(TEXT("No world tick was needed to unlock either prerequisite"), static_cast<double>(Fixture.World->GetTimeSeconds()), BeforeUpgradeTime);
		return true;
	}

	URogueUpgradeRuleAsset* MakeRulesWithoutMortar(UObject* Outer)
	{
		URogueUpgradeRuleAsset* Rules = NewObject<URogueUpgradeRuleAsset>(Outer);
		Rules->WeaponUpgradeRules.RemoveAll([](const FRogueWeaponUpgradeRuleRow& Rule)
		{
			return Rule.Source == ERogueWeaponUpgradeSource::Mortar || Rule.UpgradeType == ERogueUpgradeType::MortarCount;
		});
		return Rules;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueDefaultUpgradePrerequisiteTest,
	"Rogue.Gameplay.UpgradePrerequisites.DefaultRulesUnlockImmediately", RogueUpgradePrerequisiteTests::Flags)

bool FRogueDefaultUpgradePrerequisiteTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	RogueUpgradePrerequisiteTests::FUpgradeFixture Fixture;
	if (!Fixture.Initialize(*this)) { return false; }
	return RogueUpgradePrerequisiteTests::CheckImmediateUnlocks(*this, Fixture, nullptr);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueAssetUpgradePrerequisiteTest,
	"Rogue.Gameplay.UpgradePrerequisites.AssetRulesUnlockImmediately", RogueUpgradePrerequisiteTests::Flags)

bool FRogueAssetUpgradePrerequisiteTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	RogueUpgradePrerequisiteTests::FUpgradeFixture Fixture;
	if (!Fixture.Initialize(*this)) { return false; }
	URogueUpgradeRuleAsset* Rules = NewObject<URogueUpgradeRuleAsset>(Fixture.World);
	TestEqual(TEXT("The default asset includes the three new exclusive card rules"), Rules->WeaponUpgradeRules.Num(), 9);
	return RogueUpgradePrerequisiteTests::CheckImmediateUnlocks(*this, Fixture, Rules);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueMortarFallbackUpgradePrerequisiteTest,
	"Rogue.Gameplay.UpgradePrerequisites.MissingMortarRuleFallbackUnlocksImmediately", RogueUpgradePrerequisiteTests::Flags)

bool FRogueMortarFallbackUpgradePrerequisiteTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	RogueUpgradePrerequisiteTests::FUpgradeFixture Fixture;
	if (!Fixture.Initialize(*this)) { return false; }
	URogueUpgradeRuleAsset* Rules = RogueUpgradePrerequisiteTests::MakeRulesWithoutMortar(Fixture.World);
	TestEqual(TEXT("The fixture leaves eight non-mortar asset rules"), Rules->WeaponUpgradeRules.Num(), 8);
	TestTrue(TEXT("The compatibility path still offers the mortar unlock"), Fixture.BuildPool(Rules).Contains(ERogueUpgradeType::MortarCount));
	return RogueUpgradePrerequisiteTests::CheckImmediateUnlocks(*this, Fixture, Rules);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueDisabledWeaponUpgradePrerequisiteTest,
	"Rogue.Gameplay.UpgradePrerequisites.ZeroMaximumCountBlocksSpecialUpgrades", RogueUpgradePrerequisiteTests::Flags)

bool FRogueDisabledWeaponUpgradePrerequisiteTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	RogueUpgradePrerequisiteTests::FUpgradeFixture Fixture;
	if (!Fixture.Initialize(*this, 0)) { return false; }

	Fixture.ApplyQuantityUpgrade(ERogueUpgradeType::LaserCount);
	Fixture.ApplyQuantityUpgrade(ERogueUpgradeType::MortarCount);
	TestEqual(TEXT("Laser GAS bonus exists despite the final weapon cap"), Fixture.Attributes->GetLaserCountBonus(), 1.0f);
	TestEqual(TEXT("Mortar GAS bonus exists despite the final weapon cap"), Fixture.Attributes->GetMortarCountBonus(), 1.0f);
	TestEqual(TEXT("MaxCount zero keeps the effective laser count at zero"), Fixture.Player->GetEffectiveLaserCannonCount(), 0);
	TestEqual(TEXT("MaxCount zero keeps the effective mortar count at zero"), Fixture.Player->GetEffectiveMortarCount(), 0);

	URogueUpgradeRuleAsset* AssetRules = NewObject<URogueUpgradeRuleAsset>(Fixture.World);
	URogueUpgradeRuleAsset* FallbackRules = RogueUpgradePrerequisiteTests::MakeRulesWithoutMortar(Fixture.World);
	const URogueUpgradeRuleAsset* RuleSets[] = { nullptr, AssetRules, FallbackRules };
	const TCHAR* RuleLabels[] = { TEXT("Default rules"), TEXT("Asset rules"), TEXT("Mortar fallback rules") };
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(RuleSets); ++Index)
	{
		const TArray<ERogueUpgradeType> Pool = Fixture.BuildPool(RuleSets[Index]);
		TestFalse(FString::Printf(TEXT("%s respect the laser final count cap"), RuleLabels[Index]), Pool.Contains(ERogueUpgradeType::LaserRefraction));
		TestFalse(FString::Printf(TEXT("%s respect the mortar final count cap"), RuleLabels[Index]), Pool.Contains(ERogueUpgradeType::MortarBlastRadius));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueScytheSizePrerequisiteTest,
	"Rogue.Gameplay.UpgradePrerequisites.ScytheSizeUnlocksAndStacks", RogueUpgradePrerequisiteTests::Flags)

bool FRogueScytheSizePrerequisiteTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RogueUpgradePrerequisiteTests;
	FUpgradeFixture Fixture;
	if (!Fixture.Initialize(*this)) { return false; }
	URogueUpgradeRuleAsset* Rules = NewObject<URogueUpgradeRuleAsset>(Fixture.World);
	// An accidental entry in the generic base pool must not bypass ownership.
	Rules->BaseUpgradeTypes.AddUnique(ERogueUpgradeType::ScytheSize);
	TestFalse(TEXT("A locked scythe has no size card under default rules"), Fixture.BuildPool(nullptr).Contains(ERogueUpgradeType::ScytheSize));
	TestFalse(TEXT("A locked scythe cannot bypass the prerequisite through a custom base pool"), Fixture.BuildPool(Rules).Contains(ERogueUpgradeType::ScytheSize));
	TArray<ERogueUpgradeType> NoCharacterPool;
	RogueUpgradeSystemRules::BuildUpgradePool(NoCharacterPool, nullptr, Rules);
	TestFalse(TEXT("A generic pool without a player cannot offer scythe size"), NoCharacterPool.Contains(ERogueUpgradeType::ScytheSize));
	const double BeforeUpgradeTime = Fixture.World->GetTimeSeconds();

	Fixture.ApplyQuantityUpgrade(ERogueUpgradeType::ScytheCount);
	TestEqual(TEXT("The scythe unlock reaches the real GAS attribute"), Fixture.Attributes->GetScytheCountBonus(), 1.0f);
	TestEqual(TEXT("The effective scythe count changes without a tick"), Fixture.Player->GetEffectiveScytheCount(), 1);
	TestTrue(TEXT("The size card becomes eligible immediately under default rules"), Fixture.BuildPool(nullptr).Contains(ERogueUpgradeType::ScytheSize));
	TestTrue(TEXT("The size card becomes eligible immediately under asset rules"), Fixture.BuildPool(Rules).Contains(ERogueUpgradeType::ScytheSize));
	FRogueUpgradeSystem System;
	TArray<FRogueUpgradeOption> AllOptions;
	System.BuildRandomUpgradeOptions(Fixture.Player, 100, AllOptions);
	const FRogueUpgradeOption* FoundCard = AllOptions.FindByPredicate([](const FRogueUpgradeOption& Option)
	{
		return Option.Type == ERogueUpgradeType::ScytheSize;
	});
	if (!TestNotNull(TEXT("The actual option generator includes the unlocked size card"), FoundCard)) { return false; }
	const FRogueUpgradeOption Card = *FoundCard;
	TestEqual(TEXT("The fallback card has the real Chinese title"), Card.Title, FString(TEXT("银刃巨化")));
	TestEqual(TEXT("The fallback card increases size by twenty percent of the base"), Card.Magnitude, 0.2f);
	Fixture.Player->ApplyUpgrade(Card);
	TestTrue(TEXT("The first card reaches the new continuous GAS attribute"), FMath::IsNearlyEqual(Fixture.Attributes->GetScytheSizeBonus(), 0.2f));
	Fixture.Player->ApplyUpgrade(Card);
	TestTrue(TEXT("Two cards stack to forty percent extra size"), FMath::IsNearlyEqual(Fixture.Attributes->GetScytheSizeBonus(), 0.4f));
	TestTrue(TEXT("The current frame weapon snapshot contains the size upgrade"), FMath::IsNearlyEqual(Fixture.Player->GetWeaponBonusSnapshot().ScytheSize, 0.4f));
	FRogueWeaponBonusCaps Caps;
	Caps.ScytheSize = 0.5f;
	Fixture.Attributes->SetWeaponBonusCaps(Caps);
	Fixture.Player->ApplyUpgrade(Card);
	TestEqual(TEXT("The optional size cap clamps the third card without rounding to an integer"), Fixture.Attributes->GetScytheSizeBonus(), 0.5f);
	TestEqual(TEXT("The capped continuous value reaches the weapon snapshot"), Fixture.Player->GetWeaponBonusSnapshot().ScytheSize, 0.5f);
	TestEqual(TEXT("Unlock and stacking do not require a world tick"), static_cast<double>(Fixture.World->GetTimeSeconds()), BeforeUpgradeTime);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueScytheSizeDisabledWeaponTest,
	"Rogue.Gameplay.UpgradePrerequisites.ScytheSizeRespectsFinalQuantityCap", RogueUpgradePrerequisiteTests::Flags)

bool FRogueScytheSizeDisabledWeaponTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RogueUpgradePrerequisiteTests;
	FUpgradeFixture Fixture;
	if (!Fixture.Initialize(*this, 0)) { return false; }
	Fixture.ApplyQuantityUpgrade(ERogueUpgradeType::ScytheCount);
	TestEqual(TEXT("The scythe quantity bonus exists"), Fixture.Attributes->GetScytheCountBonus(), 1.0f);
	TestEqual(TEXT("The final quantity cap keeps the actual weapon disabled"), Fixture.Player->GetEffectiveScytheCount(), 0);
	URogueUpgradeRuleAsset* Rules = NewObject<URogueUpgradeRuleAsset>(Fixture.World);
	Rules->BaseUpgradeTypes.AddUnique(ERogueUpgradeType::ScytheSize);
	TestFalse(TEXT("Default rules do not offer an unusable scythe size card"), Fixture.BuildPool(nullptr).Contains(ERogueUpgradeType::ScytheSize));
	TestFalse(TEXT("Asset rules cannot bypass the effective weapon count"), Fixture.BuildPool(Rules).Contains(ERogueUpgradeType::ScytheSize));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueScytheLifestealPrerequisiteTest,
	"Rogue.Gameplay.UpgradePrerequisites.ScytheLifestealUnlocksAndStacks", RogueUpgradePrerequisiteTests::Flags)

bool FRogueScytheLifestealPrerequisiteTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RogueUpgradePrerequisiteTests;
	FUpgradeFixture Fixture;
	if (!Fixture.Initialize(*this)) { return false; }
	URogueUpgradeRuleAsset* Rules = NewObject<URogueUpgradeRuleAsset>(Fixture.World);
	// 即使规则资产误把吸血卡放进通用池，没有实际镰刀也不能出现。
	Rules->BaseUpgradeTypes.AddUnique(ERogueUpgradeType::ScytheLifesteal);
	TestFalse(TEXT("A locked scythe has no lifesteal card under default rules"), Fixture.BuildPool(nullptr).Contains(ERogueUpgradeType::ScytheLifesteal));
	TestFalse(TEXT("A locked scythe cannot bypass the prerequisite through a custom base pool"), Fixture.BuildPool(Rules).Contains(ERogueUpgradeType::ScytheLifesteal));
	TArray<ERogueUpgradeType> NoCharacterPool;
	RogueUpgradeSystemRules::BuildUpgradePool(NoCharacterPool, nullptr, Rules);
	TestFalse(TEXT("A generic pool without a player cannot offer scythe lifesteal"), NoCharacterPool.Contains(ERogueUpgradeType::ScytheLifesteal));

	Fixture.ApplyQuantityUpgrade(ERogueUpgradeType::ScytheCount);
	TestTrue(TEXT("The lifesteal card becomes eligible immediately after unlocking the scythe"), Fixture.BuildPool(nullptr).Contains(ERogueUpgradeType::ScytheLifesteal));
	TestTrue(TEXT("The lifesteal card also appears under custom asset rules"), Fixture.BuildPool(Rules).Contains(ERogueUpgradeType::ScytheLifesteal));

	FRogueUpgradeSystem System;
	TArray<FRogueUpgradeOption> AllOptions;
	System.BuildRandomUpgradeOptions(Fixture.Player, 100, AllOptions);
	const FRogueUpgradeOption* FoundCard = AllOptions.FindByPredicate([](const FRogueUpgradeOption& Option)
	{
		return Option.Type == ERogueUpgradeType::ScytheLifesteal;
	});
	if (!TestNotNull(TEXT("The actual option generator includes the unlocked lifesteal card"), FoundCard)) { return false; }
	const FRogueUpgradeOption Card = *FoundCard;
	TestEqual(TEXT("The fallback card has the real Chinese title"), Card.Title, FString(TEXT("红雾吸血")));
	TestEqual(TEXT("The per-level magnitude is five percent"), Card.Magnitude, 0.05f);

	Fixture.Player->ApplyUpgrade(Card);
	TestTrue(TEXT("The first card grants twenty percent lifesteal"), FMath::IsNearlyEqual(Fixture.Attributes->GetScytheLifestealBonus(), 0.20f));
	TestTrue(TEXT("The character getter reports the first-card value"), FMath::IsNearlyEqual(Fixture.Player->GetScytheLifestealPercent(), 0.20f));

	Fixture.Player->ApplyUpgrade(Card);
	TestTrue(TEXT("The second card adds five percent"), FMath::IsNearlyEqual(Fixture.Attributes->GetScytheLifestealBonus(), 0.25f));

	FRogueWeaponBonusCaps Caps;
	Caps.ScytheLifesteal = 0.3f;
	Fixture.Attributes->SetWeaponBonusCaps(Caps);
	Fixture.Player->ApplyUpgrade(Card);
	TestEqual(TEXT("The optional lifesteal cap clamps the third card"), Fixture.Attributes->GetScytheLifestealBonus(), 0.3f);
	return true;
}

#endif
