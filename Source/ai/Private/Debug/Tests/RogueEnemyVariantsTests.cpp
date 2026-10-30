#if WITH_DEV_AUTOMATION_TESTS

#include "Combat/Projectiles/RogueEnemyProjectile.h"
#include "Enemies/Data/RogueEnemyAssetPaths.h"
#include "Enemies/Data/RogueEnemyArchetypes.h"
#include "Enemies/Data/RogueEnemyDataTable.h"
#include "Enemies/RogueEnemy.h"
#include "Engine/DataTable.h"
#include "Framework/RogueGameMode.h"
#include "Misc/AutomationTest.h"
#include "UObject/UObjectGlobals.h"

namespace RogueEnemyVariantsTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;
	const TCHAR* TablePath = TEXT("/Game/Data/Enemies/DT_EnemyArchetypes.DT_EnemyArchetypes");
	const TCHAR* GameModePath = TEXT("/Game/BP/Framework/BP_RogueGameMode.BP_RogueGameMode_C");

	struct FEnemyVariant
	{
		ERogueEnemyType Type;
		ERogueEnemyVisualKey VisualKey;
		const TCHAR* BlueprintName;
		const TCHAR* ProjectileName;
	};

	const FEnemyVariant Variants[] =
	{
		{ ERogueEnemyType::Sniper, ERogueEnemyVisualKey::Sniper, TEXT("Sniper"), TEXT("Sniper") },
		{ ERogueEnemyType::Shotgunner, ERogueEnemyVisualKey::Shotgunner, TEXT("Shotgunner"), TEXT("Shotgunner") },
		{ ERogueEnemyType::Repeater, ERogueEnemyVisualKey::Repeater, TEXT("Repeater"), TEXT("Repeater") },
		{ ERogueEnemyType::Ringcaster, ERogueEnemyVisualKey::Ringcaster, TEXT("Ringcaster"), TEXT("Ringcaster") },
		{ ERogueEnemyType::Marksman, ERogueEnemyVisualKey::Marksman, TEXT("Marksman"), TEXT("Marksman") },
		{ ERogueEnemyType::Cannoneer, ERogueEnemyVisualKey::Cannoneer, TEXT("Cannoneer"), TEXT("Cannoneer") },
		{ ERogueEnemyType::Scatterer, ERogueEnemyVisualKey::Scatterer, TEXT("Scatterer"), TEXT("Scatterer") },
		{ ERogueEnemyType::ShockPillar, ERogueEnemyVisualKey::ShockPillar, TEXT("ShockPillar"), TEXT("ShockPillar") },
		// 各类型保留弹体与BP映射，普通个体可由DT切换为接触攻击。
		{ ERogueEnemyType::Rifleman, ERogueEnemyVisualKey::Rifleman, TEXT("Rifleman"), TEXT("Rifleman") },
		{ ERogueEnemyType::Assault, ERogueEnemyVisualKey::Assault, TEXT("Assault"), TEXT("Assault") },
		{ ERogueEnemyType::Juggernaut, ERogueEnemyVisualKey::Juggernaut, TEXT("Juggernaut"), TEXT("Juggernaut") },
		{ ERogueEnemyType::Orbiter, ERogueEnemyVisualKey::Orbiter, TEXT("Orbiter"), TEXT("Orbiter") },
		{ ERogueEnemyType::Skirmisher, ERogueEnemyVisualKey::Skirmisher, TEXT("Skirmisher"), TEXT("Skirmisher") },
        { ERogueEnemyType::SkyMoth, ERogueEnemyVisualKey::SkyMoth, TEXT("SkyMoth"), TEXT("SkyMoth") },
        { ERogueEnemyType::WindReaver, ERogueEnemyVisualKey::WindReaver, TEXT("WindReaver"), TEXT("WindReaver") },
        { ERogueEnemyType::StarLantern, ERogueEnemyVisualKey::StarLantern, TEXT("StarLantern"), TEXT("StarLantern") }
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueNewRangedArchetypesTest,
	"Rogue.Gameplay.EnemyVariants.RangedArchetypesAndBlueprints", RogueEnemyVariantsTests::Flags)
bool FRogueNewRangedArchetypesTest::RunTest(const FString& Parameters)
{
	using namespace RogueEnemyVariantsTests;

	UDataTable* Table = LoadObject<UDataTable>(nullptr, TablePath);
	if (!TestNotNull(TEXT("Enemy archetype table loads"), Table)) { return false; }

	UClass* GameModeClass = LoadClass<ARogueGameMode>(nullptr, GameModePath);
	if (!TestNotNull(TEXT("BP_RogueGameMode loads"), GameModeClass)) { return false; }
	const ARogueGameMode* GameModeCDO = GameModeClass->GetDefaultObject<ARogueGameMode>();
	if (!TestNotNull(TEXT("BP_RogueGameMode CDO exists"), GameModeCDO)) { return false; }

	for (const FEnemyVariant& Variant : Variants)
	{
		const FName RowName = RogueEnemyArchetypes::FindRowNameForEnemyType(Table, Variant.Type);
		if (!TestFalse(*FString::Printf(TEXT("%s has a data row"), Variant.BlueprintName), RowName.IsNone())) { continue; }

		const FRogueEnemyArchetype Archetype = RogueEnemyArchetypes::BuildEnemyArchetypeFromDataTable(Table, Variant.Type, false);
		TestEqual(*FString::Printf(TEXT("%s keeps its visual key"), Variant.BlueprintName), Archetype.Visual.VisualKey, Variant.VisualKey);
		const auto* Row=Table->FindRow<FRogueEnemyArchetypeRow>(RowName,TEXT("Role test"));
        TestEqual(TEXT("Runtime attack follows explicit normal role"),Archetype.Ranged.bUsesRangedAttack,!Row->bContactOnlyNormal);
		TestTrue(*FString::Printf(TEXT("%s has a projectile class"), Variant.BlueprintName), Archetype.ProjectileClass != nullptr);
		if (Archetype.ProjectileClass != nullptr)
		{
			TestTrue(*FString::Printf(TEXT("%s uses its configured projectile"), Variant.BlueprintName),
				Archetype.ProjectileClass->GetName().Contains(Variant.ProjectileName));
		}

		const FString BlueprintPath = RogueEnemyAssetPaths::EnemyClassPath(Variant.BlueprintName);
		UClass* EnemyClass = LoadClass<ARogueEnemy>(nullptr, *BlueprintPath);
		TestNotNull(*FString::Printf(TEXT("%s Blueprint loads"), Variant.BlueprintName), EnemyClass);

		if (EnemyClass != nullptr)
		{
			TestEqual(TEXT("Blueprint default enemy type agrees with its variant"), EnemyClass->GetDefaultObject<ARogueEnemy>()->GetEnemyType(), Variant.Type);
		}

		TestTrue(*FString::Printf(TEXT("%s is mapped in BP_RogueGameMode"), Variant.BlueprintName),
			GameModeCDO->HasEnemyClassForType(Variant.Type));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueFormerMeleeRangedTest,
	"Rogue.Gameplay.EnemyVariants.BaselineRangedPatternsRemainAvailable", RogueEnemyVariantsTests::Flags)
bool FRogueFormerMeleeRangedTest::RunTest(const FString& Parameters)
{
	using namespace RogueEnemyVariantsTests;

	UDataTable* Table = LoadObject<UDataTable>(nullptr, TablePath);
	if (!TestNotNull(TEXT("Enemy archetype table loads"), Table)) { return false; }

	const FRogueEnemyArchetype Rifleman = RogueEnemyArchetypes::BuildEnemyArchetype(ERogueEnemyType::Rifleman, false);
	TestTrue(TEXT("Rifleman now has a ranged attack"), Rifleman.Ranged.bUsesRangedAttack);
	TestEqual(TEXT("Rifleman keeps a single-shot ranged pattern"), Rifleman.Ranged.ShotPattern, ERogueEnemyShotPattern::Single);
	TestEqual(TEXT("Rifleman keeps kiting movement"), Rifleman.Movement.Model, ERogueEnemyMovementModel::RangedKite);

	const FRogueEnemyArchetype Assault = RogueEnemyArchetypes::BuildEnemyArchetype(ERogueEnemyType::Assault, false);
	TestTrue(TEXT("Assault now has a ranged attack"), Assault.Ranged.bUsesRangedAttack);
	TestEqual(TEXT("Assault fires a close-range spread"), Assault.Ranged.ShotPattern, ERogueEnemyShotPattern::Spread3);
	TestEqual(TEXT("Assault keeps its Repeater-charge movement"), Assault.Movement.Model, ERogueEnemyMovementModel::BurstCharge);

	const FRogueEnemyArchetype Juggernaut = RogueEnemyArchetypes::BuildEnemyArchetype(ERogueEnemyType::Juggernaut, false);
	TestTrue(TEXT("Juggernaut now has a ranged attack"), Juggernaut.Ranged.bUsesRangedAttack);
	TestEqual(TEXT("Juggernaut fires a heavy single shot"), Juggernaut.Ranged.ShotPattern, ERogueEnemyShotPattern::Single);
	TestTrue(TEXT("Juggernaut shot is slow but hard-hitting"), Juggernaut.Ranged.ProjectileDamageMultiplier > 1.0f);

	const FRogueEnemyArchetype Orbiter = RogueEnemyArchetypes::BuildEnemyArchetype(ERogueEnemyType::Orbiter, false);
	TestTrue(TEXT("Orbiter now has a ranged attack"), Orbiter.Ranged.bUsesRangedAttack);
	TestEqual(TEXT("Orbiter keeps orbiting movement"), Orbiter.Movement.Model, ERogueEnemyMovementModel::Orbit);

	const FRogueEnemyArchetype Skirmisher = RogueEnemyArchetypes::BuildEnemyArchetype(ERogueEnemyType::Skirmisher, false);
	TestTrue(TEXT("Skirmisher now has a ranged attack"), Skirmisher.Ranged.bUsesRangedAttack);
	TestEqual(TEXT("Skirmisher fires a spread"), Skirmisher.Ranged.ShotPattern, ERogueEnemyShotPattern::Spread3);
	TestEqual(TEXT("Skirmisher keeps zig-zag movement"), Skirmisher.Movement.Model, ERogueEnemyMovementModel::ZigZagChase);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueNewRangedShotPatternsTest,
	"Rogue.Gameplay.EnemyVariants.DistinctRangedPatterns", RogueEnemyVariantsTests::Flags)
bool FRogueNewRangedShotPatternsTest::RunTest(const FString& Parameters)
{
	using namespace RogueEnemyVariantsTests;

	UDataTable* Table = LoadObject<UDataTable>(nullptr, TablePath);
	if (!TestNotNull(TEXT("Enemy archetype table loads"), Table)) { return false; }

	const FRogueEnemyArchetype Sniper = RogueEnemyArchetypes::BuildEnemyArchetypeFromDataTable(Table, ERogueEnemyType::Sniper, false);
	TestEqual(TEXT("Sniper fires a single high-speed shot"), Sniper.Ranged.ShotPattern, ERogueEnemyShotPattern::Single);
	TestTrue(TEXT("Sniper projectile is fast"), Sniper.Ranged.ProjectileSpeed >= 1600.0f);
	TestTrue(TEXT("Sniper has a long cooldown"), Sniper.Ranged.NextShotCooldown >= 1.5f);

	const FRogueEnemyArchetype Repeater = RogueEnemyArchetypes::BuildEnemyArchetypeFromDataTable(Table, ERogueEnemyType::Repeater, false);
	TestEqual(TEXT("Repeater fires rapidly"), Repeater.Ranged.ShotPattern, ERogueEnemyShotPattern::Single);
	TestTrue(TEXT("Repeater fires more frequently than Sniper"), Repeater.Ranged.NextShotCooldown > 0.f && Repeater.Ranged.NextShotCooldown < Sniper.Ranged.NextShotCooldown);
	TestTrue(TEXT("Repeater damage is lower than Sniper"), Repeater.Ranged.ProjectileDamageMultiplier < Sniper.Ranged.ProjectileDamageMultiplier);

	const FRogueEnemyArchetype Shotgunner = RogueEnemyArchetypes::BuildEnemyArchetypeFromDataTable(Table, ERogueEnemyType::Shotgunner, false);
	TestEqual(TEXT("Shotgunner uses spread fire"), Shotgunner.Ranged.ShotPattern, ERogueEnemyShotPattern::Spread3);
	TestTrue(TEXT("Shotgunner fires five pellets"), Shotgunner.Ranged.SpreadShotCount >= 5);

	const FRogueEnemyArchetype Ringcaster = RogueEnemyArchetypes::BuildEnemyArchetypeFromDataTable(Table, ERogueEnemyType::Ringcaster, false);
	TestEqual(TEXT("Ringcaster uses a radial ring"), Ringcaster.Ranged.ShotPattern, ERogueEnemyShotPattern::RadialRing);
	TestEqual(TEXT("Ringcaster ring has 16 bullets"), Ringcaster.Ranged.BulletRing.ProjectileCount, 16);
	TestEqual(TEXT("Ringcaster is mobile, unlike the ShockPillar"), Ringcaster.Movement.Model, ERogueEnemyMovementModel::RangedKite);

	return true;
}

#endif
