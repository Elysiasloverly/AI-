#if WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystem/RogueAbilitySystemLibrary.h"
#include "AbilitySystem/RogueAttributeSet.h"
#include "AbilitySystem/RogueGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "Engine/World.h"
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

// Only arrange the configured asset before the real BeginPlay/Possess path.
// Assertions read public gameplay APIs and the real authoritative AttributeSet.
struct FRoguePlayerBalanceTestAccess
{
	static void SetAsset(ARogueCharacter* Character, URoguePlayerBalanceAsset* Asset)
	{
		Character->PlayerBalanceAsset = Asset;
	}
};

namespace RoguePlayerBalanceTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;

	struct FFixture
	{
		FTestConsoleVariable WeaponSelfTest{TEXT("Rogue.SelfTest.WeaponUpgradeChain")};
		FTestConsoleVariable EnemyStress{TEXT("Rogue.SelfTest.EnemyStress")};
		TStrongObjectPtr<URoguePlayerBalanceAsset> Config{NewObject<URoguePlayerBalanceAsset>()};
		FTestWorldWrapper TestWorld;
		UWorld* World = nullptr;
		ARoguePlayerController* Controllers[2] = {};
		ARogueCharacter* Pawns[2] = {};

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
			if (!Test.TestNotNull(TEXT("A real authoritative GameMode exists"), Mode)) { return false; }
			Mode->SetActorTickEnabled(false);
			for (int32 Index = 0; Index < PlayerCount; ++Index)
			{
				Controllers[Index] = World->SpawnActor<ARoguePlayerController>();
				if (!Test.TestNotNull(TEXT("A real player controller exists"), Controllers[Index])) { return false; }
				// This fixture verifies authored armor amounts, so explicitly purchase its permanent gate.
				// Leave stat ranks empty to keep the configured health, armor and progression expectations exact.
				ARoguePlayerState* PlayerState = Controllers[Index]->GetPlayerState<ARoguePlayerState>();
				if (!Test.TestNotNull(TEXT("The controller owns a PlayerState before possession"), PlayerState)) { return false; }
				FRogueProgressionSnapshot PermanentAccess;
				PermanentAccess.NodeRanks.Add(FName(TEXT("Armor.Unlock")), 1);
				if (!Test.TestTrue(TEXT("Armor access is injected before the configured run"),
					PlayerState->SetProgressionSnapshotForRun(PermanentAccess))) { return false; }
				const FTransform Transform(FVector(Index * 500.0f, 0.0f, 120.0f));
				Pawns[Index] = World->SpawnActorDeferred<ARogueCharacter>(ARogueCharacter::StaticClass(),
					Transform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
				if (!Test.TestNotNull(TEXT("The configured Pawn exists"), Pawns[Index])) { return false; }
				FRoguePlayerBalanceTestAccess::SetAsset(Pawns[Index], Config.Get());
				Pawns[Index]->FinishSpawning(Transform);
				Controllers[Index]->Possess(Pawns[Index]);
				Pawns[Index]->GetCharacterMovement()->SetComponentTickEnabled(false);
				if (!Test.TestNotNull(TEXT("The actual GAS attributes exist"), Pawns[Index]->GetRogueAttributeSet())) { return false; }
			}
			return !Test.HasAnyErrors();
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
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRoguePlayerConfiguredInitializationTest,
	"Rogue.Gameplay.PlayerBalance.ConfiguredInitialization", RoguePlayerBalanceTests::Flags)

bool FRoguePlayerConfiguredInitializationTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RoguePlayerBalanceTests;
	FFixture Fixture;
	URoguePlayerBalanceAsset* Config = Fixture.Config.Get();
	Config->BaseStats.MaxHealth = 240.0f;
	Config->BaseStats.MaxArmor = 80.0f;
	Config->BaseStats.DashCooldownDuration = 0.2f;
	Config->BaseStats.DashMinCooldown = 1.5f;
	Config->SpawnState.HealthRatio = 0.25f;
	Config->SpawnState.ArmorRatio = 0.5f;
	Config->Progression.StartingMoney = 73;
	Config->WeaponBonusCaps.AttackPower = 0.2f;
	Config->WeaponBonusCaps.ScytheSize = 0.35f;
	FRogueInitialWeaponBonusConfig& Bonuses = Config->InitialWeaponBonuses;
	Bonuses.AttackPower = 0.4f;
	Bonuses.AttackSpeed = 0.3f;
	Bonuses.AttackRange = 0.2f;
	Bonuses.ProjectileSpeed = 0.1f;
	Bonuses.ProjectileCount = 2;
	Bonuses.ScytheCount = 3;
	Bonuses.RocketCount = 4;
	Bonuses.LaserCount = 5;
	Bonuses.HellTowerCount = 6;
	Bonuses.LaserRefraction = 7;
	Bonuses.MortarCount = 8;
	Bonuses.MortarBlastRadius = 90.0f;
	Bonuses.ScytheSize = 0.6f;

	if (!Fixture.Initialize(*this)) { return false; }
	ARogueCharacter* Pawn = Fixture.Pawns[0];
	URogueAttributeSet* Attributes = Pawn->GetRogueAttributeSet();
	TestEqual(TEXT("DA maximum health is applied before the initial health ratio"), Attributes->GetMaxHealth(), 240.0f);
	TestEqual(TEXT("The player starts with the configured quarter health"), Attributes->GetHealth(), 60.0f);
	TestEqual(TEXT("The player starts with the configured half armor"), Attributes->GetArmor(), 40.0f);
	TestEqual(TEXT("The configured wallet is initialized"), Pawn->GetMoney(), 73);
	TestEqual(TEXT("The minimum dash cooldown clamps the initial cooldown"), Attributes->GetDashCooldownDuration(), 1.5f);
	const float Actual[] = {
		Attributes->GetAttackPowerBonus(), Attributes->GetAttackSpeedBonus(), Attributes->GetAttackRangeBonus(),
		Attributes->GetProjectileSpeedBonus(), Attributes->GetProjectileCountBonus(), Attributes->GetScytheCountBonus(),
		Attributes->GetRocketCountBonus(), Attributes->GetLaserCountBonus(), Attributes->GetHellTowerCountBonus(),
		Attributes->GetLaserRefractionBonus(), Attributes->GetMortarCountBonus(), Attributes->GetMortarBlastRadiusBonus(),
		Attributes->GetScytheSizeBonus()
	};
	// Generic starting bonuses still honor the DA and caps. Quantity and dedicated-card
	// bonuses now start at zero, preventing legacy DA values from bypassing the upgrade tree.
	const float Expected[] = {0.2f, 0.3f, 0.2f, 0.1f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Expected); ++Index)
	{
		TestEqual(FString::Printf(TEXT("Initial bonus %d reaches GAS and respects its cap"), Index), Actual[Index], Expected[Index]);
	}
	TestEqual(TEXT("The initial snapshot cannot grant a locked laser through a legacy DA bonus"), Pawn->GetWeaponBonusSnapshot().LaserCount, 0.0f);
	TestEqual(TEXT("The snapshot contains the clamped initial damage bonus"), Pawn->GetWeaponBonusSnapshot().AttackPower, 0.2f);
	TestEqual(TEXT("The initial snapshot cannot grant a dedicated scythe size bonus"), Pawn->GetWeaponBonusSnapshot().ScytheSize, 0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRoguePlayerConfiguredProgressionTest,
	"Rogue.Gameplay.PlayerBalance.ProgressionAndRepossess", RoguePlayerBalanceTests::Flags)

bool FRoguePlayerConfiguredProgressionTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RoguePlayerBalanceTests;
	FFixture Fixture;
	Fixture.Config->BaseStats.ExperienceToNextLevel = 10;
	Fixture.Config->Progression.ExperienceGrowthMultiplier = 1.5f;
	Fixture.Config->Progression.ExperienceGrowthAddend = 3;
	Fixture.Config->Progression.StartingMoney = 31;
	if (!Fixture.Initialize(*this)) { return false; }
	ARogueCharacter* Pawn = Fixture.Pawns[0];

	Pawn->AddExperience(10);
	TestEqual(TEXT("The first configured threshold upgrades the player"), Pawn->GetPlayerLevel(), 2);
	TestEqual(TEXT("Ten times 1.5 plus 3 requires eighteen experience"), Pawn->GetExperienceToNextLevel(), 18);
	Pawn->AddExperience(18);
	TestEqual(TEXT("The second level uses the updated experience threshold"), Pawn->GetPlayerLevel(), 3);
	TestEqual(TEXT("Eighteen times 1.5 plus 3 requires thirty experience"), Pawn->GetExperienceToNextLevel(), 30);
	TestTrue(TEXT("The wallet can be spent before repossession"), Pawn->TrySpendMoney(5));
	Pawn->SetCurrentHealth(37.0f);
	Fixture.Controllers[0]->UnPossess();
	Fixture.Controllers[0]->Possess(Pawn);
	TestEqual(TEXT("Repossession does not grant starting money again"), Pawn->GetMoney(), 26);
	TestEqual(TEXT("Repossession preserves the current experience requirement"), Pawn->GetExperienceToNextLevel(), 30);
	TestEqual(TEXT("Repossession preserves the player's level"), Pawn->GetPlayerLevel(), 3);
	TestEqual(TEXT("Repossession does not apply the spawn health ratio again"), Pawn->GetRogueAttributeSet()->GetHealth(), 37.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRoguePlayerConfiguredRevivalTest,
	"Rogue.Gameplay.PlayerBalance.RevivalAndReset", RoguePlayerBalanceTests::Flags)

bool FRoguePlayerConfiguredRevivalTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RoguePlayerBalanceTests;
	FFixture Fixture;
	URoguePlayerBalanceAsset* Config = Fixture.Config.Get();
	Config->BaseStats.MaxHealth = 200.0f;
	Config->BaseStats.MaxArmor = 0.0f;
	Config->Revival.DownedDuration = 12.0f;
	Config->Revival.HealthRatio = 0.25f;
	Config->Revival.InvulnerableSeconds = 0.4f;
	if (!Fixture.Initialize(*this, 2)) { return false; }
	ARogueCharacter* Pawn = Fixture.Pawns[0];
	TestTrue(TEXT("A real GAS lethal hit is accepted"), RogueAbilitySystem::ApplyDamageToTarget(
		Fixture.Pawns[1], Pawn, 1000000.0f, RogueGameplayTags::Damage_Source_Contact));
	Fixture.Advance(0.04f);
	TestTrue(TEXT("The hit enters the actual Downed state"), Pawn->IsDowned());
	TestTrue(TEXT("A twelve-second configured countdown is not truncated to ten"), Pawn->GetReviveSecondsRemaining() > 11.9f);
	Fixture.Advance(11.8f);
	TestTrue(TEXT("The player remains downed until the configured deadline"), Pawn->IsDowned());
	Fixture.Advance(0.3f);
	TestTrue(TEXT("The authoritative timer revives after twelve seconds"), Pawn->IsAlive());
	TestEqual(TEXT("The configured revival ratio restores fifty health"), Pawn->GetRogueAttributeSet()->GetHealth(), 50.0f);
	TestTrue(TEXT("Revival applies the configured protection effect"), Pawn->GetAbilitySystemComponent()->HasMatchingGameplayTag(RogueGameplayTags::State_Invulnerable));
	Fixture.Advance(0.5f);
	TestFalse(TEXT("Protection expires at the configured duration"), Pawn->GetAbilitySystemComponent()->HasMatchingGameplayTag(RogueGameplayTags::State_Invulnerable));

	Config->WeaponBonusCaps.AttackPower = 0.7f;
	Config->InitialWeaponBonuses.LaserCount = 2;
	Config->InitialWeaponBonuses.ScytheSize = 0.8f;
	Config->WeaponBonusCaps.ScytheSize = 1.0f;
	Config->Progression.StartingMoney = 99;
	Config->SpawnState.HealthRatio = 0.1f;
	Config->SpawnState.ArmorRatio = 0.1f;
	Config->ResetToDefaults();
	TestEqual(TEXT("Reset includes existing base stats"), Config->BaseStats.MaxHealth, 100.0f);
	TestEqual(TEXT("Reset includes existing weapon caps"), Config->WeaponBonusCaps.AttackPower, -1.0f);
	TestEqual(TEXT("Reset includes new initial weapon bonuses"), Config->InitialWeaponBonuses.LaserCount, 0);
	TestEqual(TEXT("Reset includes initial scythe size"), Config->InitialWeaponBonuses.ScytheSize, 0.0f);
	TestEqual(TEXT("Reset restores unlimited scythe size cap"), Config->WeaponBonusCaps.ScytheSize, -1.0f);
	TestEqual(TEXT("Reset restores the experience multiplier"), Config->Progression.ExperienceGrowthMultiplier, 1.3f);
	TestEqual(TEXT("Reset restores the experience addend"), Config->Progression.ExperienceGrowthAddend, 2);
	TestEqual(TEXT("Reset includes starting money"), Config->Progression.StartingMoney, 0);
	TestEqual(TEXT("Reset restores full starting health"), Config->SpawnState.HealthRatio, 1.0f);
	TestEqual(TEXT("Reset restores full starting armor"), Config->SpawnState.ArmorRatio, 1.0f);
	TestEqual(TEXT("Reset preserves the ten-second default"), Config->Revival.DownedDuration, 10.0f);
	TestEqual(TEXT("Reset restores half health revival"), Config->Revival.HealthRatio, 0.5f);
	TestEqual(TEXT("Reset restores normal revival protection"), Config->Revival.InvulnerableSeconds, 1.5f);
	return true;
}

#endif
