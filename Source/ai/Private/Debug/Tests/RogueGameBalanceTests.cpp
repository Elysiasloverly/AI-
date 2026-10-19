#if WITH_DEV_AUTOMATION_TESTS

#include "Framework/RogueGameBalanceAsset.h"
#include "Framework/RogueGameModeRules.h"
#include "HAL/FileManager.h"
#include "Math/UnrealMathUtility.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"

namespace RogueGameBalanceTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;
	const ERogueEnemyType EnemyTypes[] =
	{
		ERogueEnemyType::Hunter, ERogueEnemyType::Charger, ERogueEnemyType::Tank,
		ERogueEnemyType::Orbiter, ERogueEnemyType::Shooter, ERogueEnemyType::Skirmisher,
		ERogueEnemyType::Artillery, ERogueEnemyType::Spitter, ERogueEnemyType::ShockPillar
	};

	struct FIntOverride { const TCHAR* Name; int32 Value; };
	struct FFloatOverride { const TCHAR* Name; float Value; };

	bool SetLegacyInt(FAutomationTestBase& Test, URogueGameBalanceAsset* Asset, const TCHAR* Name, int32 Value)
	{
		FIntProperty* Property = FindFProperty<FIntProperty>(Asset->GetClass(), Name);
		if (!Test.TestNotNull(FString::Printf(TEXT("Legacy integer property %s remains readable for migration"), Name), Property)) { return false; }
		Property->SetPropertyValue_InContainer(Asset, Value);
		return true;
	}

	bool SetLegacyFloat(FAutomationTestBase& Test, URogueGameBalanceAsset* Asset, const TCHAR* Name, float Value)
	{
		FFloatProperty* Property = FindFProperty<FFloatProperty>(Asset->GetClass(), Name);
		if (!Test.TestNotNull(FString::Printf(TEXT("Legacy float property %s remains readable for migration"), Name), Property)) { return false; }
		Property->SetPropertyValue_InContainer(Asset, Value);
		return true;
	}

	void RouteLegacyPostLoad(URogueGameBalanceAsset* Asset)
	{
		// Route PostLoad after a fixture's legacy values and package version have been supplied.
		Asset->SetFlags(RF_NeedPostLoad);
		Asset->ConditionalPostLoad();
	}

	URogueGameBalanceAsset* MakeLegacyFixture(FAutomationTestBase& Test, int32 Variant)
	{
		// Load a real pre-migration package under a unique name so its linker supplies the old version.
		// NewObject alone reports the registered CURRENT version and cannot simulate a legacy load.
		FString LegacyFilename;
		if (!FParse::Value(FCommandLine::Get(), TEXT("RogueBalanceLegacyPackage="), LegacyFilename))
		{
			LegacyFilename = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(),
				TEXT("Source/ai/Private/Debug/Tests/Fixtures/DA_GameBalance.uasset")));
		}
		if (!IFileManager::Get().FileExists(*LegacyFilename))
		{
			Test.AddError(TEXT("The legacy game balance fixture is missing from Source/ai/Private/Debug/Tests/Fixtures."));
			return nullptr;
		}
		const FGuid LayoutVersion(0x7813CF72, 0x2EBB4E2A, 0xA614905F, 0xC30A8DD1);
		const FString BackupMount(TEXT("/RogueBalanceLegacyBackup/"));
		FString BackupDirectory = FPaths::ConvertRelativePathToFull(FPaths::GetPath(LegacyFilename));
		FPaths::NormalizeDirectoryName(BackupDirectory);
		BackupDirectory += TEXT("/");
		static FString MountedBackupDirectory;
		if (MountedBackupDirectory != BackupDirectory)
		{
			if (!MountedBackupDirectory.IsEmpty()) { FPackageName::UnRegisterMountPoint(BackupMount, MountedBackupDirectory); }
			FPackageName::RegisterMountPoint(BackupMount, BackupDirectory);
			MountedBackupDirectory = BackupDirectory;
		}
		const FString BackupPackageName = BackupMount + FPaths::GetBaseFilename(LegacyFilename);
		const FString PackageName = FString::Printf(TEXT("/Temp/RogueGameBalanceLegacy_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits));
		UPackage* LegacyPackage = LoadPackage(CreatePackage(*PackageName), *BackupPackageName, LOAD_None);
		if (!Test.TestNotNull(TEXT("The backed-up legacy package loads into its own package"), LegacyPackage)) { return nullptr; }
		LegacyPackage->SetFlags(RF_Transient);
		URogueGameBalanceAsset* Asset = FindObject<URogueGameBalanceAsset>(LegacyPackage, TEXT("DA_GameBalance"));
		if (!Test.TestNotNull(TEXT("The backed-up package contains DA_GameBalance"), Asset)) { return nullptr; }
		if (!Test.TestTrue(TEXT("The fixture package predates the grouped layout"), Asset->GetLinkerCustomVersion(LayoutVersion) < 1)) { return nullptr; }
		const FIntOverride IntOverrides[] =
		{
			{ TEXT("BossAliveMinMaxEnemies"), 9 }, { TEXT("BossAliveMaxEnemyReduction"), 6 },
			{ TEXT("MaxAliveEnemiesPerWave"), 3 }, { TEXT("MaxAliveEnemiesHardLimit"), 43 },
			{ TEXT("DoubleSpawnWave"), 7 }, { TEXT("DoubleSpawnSlack"), 11 },
			{ TEXT("TripleSpawnWave"), 3 }, { TEXT("TripleSpawnSlack"), 2 },
			{ TEXT("EarlyWaveSlowdownEndWave"), 6 },
			{ TEXT("BaseExperienceReward"), 4 }, { TEXT("ExperienceRewardWaveDivisor"), 3 }, { TEXT("MaxExperienceReward"), 8 },
			{ TEXT("MidGameHealthStartWave"), 12 }, { TEXT("LateGameHealthStartWave"), 4 },
			{ TEXT("BossExperiencePerWave"), 6 }, { TEXT("MinBossExperienceReward"), 17 }, { TEXT("MaxBossExperienceReward"), 67 },
			{ TEXT("LateEliteBossStartWave"), 9 },
			{ TEXT("EnemyCurrencyBaseReward"), 3 }, { TEXT("EnemyCurrencyWaveDivisor"), 4 }, { TEXT("MaxEnemyCurrencyReward"), 9 },
			{ TEXT("BossCurrencyBaseReward"), 21 }, { TEXT("BossCurrencyPerWave"), 3 }, { TEXT("MaxBossCurrencyReward"), 77 },
			{ TEXT("ShopOfferCount"), 9 }, { TEXT("ShopOfferCost"), 31 }, { TEXT("ShopRefreshCost"), 19 }
		};
		const FFloatOverride FloatOverrides[] =
		{
			{ TEXT("DifficultyScalePerWave"), 0.13f }, { TEXT("MaxDifficultyScale"), 3.7f },
			{ TEXT("EarlyWaveSpawnMultiplierAtWaveOne"), 1.6f }, { TEXT("EarlyWaveSpawnMultiplierAtEnd"), 0.85f },
			{ TEXT("MinimumSpawnDelay"), 0.27f },
			{ TEXT("BaseEnemyHealth"), 48.5f }, { TEXT("EnemyHealthPerWave"), 6.25f },
			{ TEXT("BaseEnemySpeed"), 198.5f }, { TEXT("EnemySpeedPerWave"), 5.75f },
			{ TEXT("BaseEnemyDamage"), 11.25f }, { TEXT("EnemyDamagePerWave"), 0.875f },
			{ TEXT("MidGameHealthPerWave"), 0.125f }, { TEXT("MidGameHealthCap"), 0.0f },
			{ TEXT("LateGameHealthPerWave"), 0.0375f }, { TEXT("LateGameHealthCap"), -0.5f },
			{ TEXT("MaxEnemyHealth"), 675.5f },
			{ TEXT("BaseBossHealth"), 777.5f }, { TEXT("BossHealthPerWave"), 83.25f },
			{ TEXT("BaseBossSpeed"), 313.5f }, { TEXT("BossSpeedPerWave"), 6.25f },
			{ TEXT("BaseBossDamage"), 27.5f }, { TEXT("BossDamagePerWave"), 3.125f },
			{ TEXT("EarlyEliteBossHealthMultiplier"), 1.27f }, { TEXT("LateEliteBossHealthMultiplier"), 2.13f },
			{ TEXT("EarlyEliteBossDamageMultiplier"), 0.83f }, { TEXT("LateEliteBossDamageMultiplier"), 1.45f },
			{ TEXT("EliteBossSpeedMultiplier"), 1.07f }, { TEXT("ShopAutoRefreshInterval"), 83.0f }
		};
		bool bValid = true;
		for (const FIntOverride& Override : IntOverrides) { bValid &= SetLegacyInt(Test, Asset, Override.Name, Override.Value); }
		for (const FFloatOverride& Override : FloatOverrides) { bValid &= SetLegacyFloat(Test, Asset, Override.Name, Override.Value); }

		if (Variant == 1)
		{
			bValid &= SetLegacyInt(Test, Asset, TEXT("MaxAliveEnemiesPerWave"), 0);
			bValid &= SetLegacyInt(Test, Asset, TEXT("MaxAliveEnemiesHardLimit"), 0);
			bValid &= SetLegacyInt(Test, Asset, TEXT("ExperienceRewardWaveDivisor"), 0);
			bValid &= SetLegacyInt(Test, Asset, TEXT("EnemyCurrencyWaveDivisor"), -3);
			bValid &= SetLegacyInt(Test, Asset, TEXT("MidGameHealthStartWave"), 10);
			bValid &= SetLegacyInt(Test, Asset, TEXT("LateGameHealthStartWave"), 2);
			bValid &= SetLegacyFloat(Test, Asset, TEXT("MidGameHealthCap"), -1.0f);
			bValid &= SetLegacyFloat(Test, Asset, TEXT("LateGameHealthCap"), 0.0f);
			bValid &= SetLegacyFloat(Test, Asset, TEXT("MaxEnemyHealth"), -1.0f);
			bValid &= SetLegacyInt(Test, Asset, TEXT("LateEliteBossStartWave"), 0);
			bValid &= SetLegacyInt(Test, Asset, TEXT("ShopOfferCount"), 0);
			bValid &= SetLegacyInt(Test, Asset, TEXT("ShopOfferCost"), -3);
			bValid &= SetLegacyInt(Test, Asset, TEXT("ShopRefreshCost"), -4);
			bValid &= SetLegacyFloat(Test, Asset, TEXT("ShopAutoRefreshInterval"), 0.0f);
		}
		else if (Variant == 2)
		{
			bValid &= SetLegacyInt(Test, Asset, TEXT("MaxAliveEnemiesHardLimit"), 5);
			bValid &= SetLegacyInt(Test, Asset, TEXT("BossAliveMinMaxEnemies"), 19);
			bValid &= SetLegacyInt(Test, Asset, TEXT("DoubleSpawnWave"), 4);
			bValid &= SetLegacyInt(Test, Asset, TEXT("TripleSpawnWave"), 10);
			bValid &= SetLegacyInt(Test, Asset, TEXT("DoubleSpawnSlack"), 0);
			bValid &= SetLegacyInt(Test, Asset, TEXT("TripleSpawnSlack"), 1);
			bValid &= SetLegacyFloat(Test, Asset, TEXT("MaxEnemyHealth"), 0.0f);
			bValid &= SetLegacyFloat(Test, Asset, TEXT("MidGameHealthCap"), 0.25f);
			bValid &= SetLegacyFloat(Test, Asset, TEXT("LateGameHealthCap"), 0.15f);
			bValid &= SetLegacyInt(Test, Asset, TEXT("LateEliteBossStartWave"), 5);
		}
		if (!bValid) { return nullptr; }

		// Include repeated weighted entries, non-sorted start waves, type overrides, and fallback types.
		Asset->EnemyWaveEntries.Reset();
		Asset->EnemyBalanceRows.Reset();
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(EnemyTypes); ++Index)
		{
			Asset->EnemyWaveEntries.Add({ 1 + (Index % 4) * 3, EnemyTypes[Index], 1 + Index % 3 });
			if (Variant == 1 && Index != 0 && Index != 6) { continue; }
			FRogueEnemyBalanceRow Row;
			Row.EnemyType = EnemyTypes[Index];
			Row.HealthMultiplier = 0.75f + 0.125f * Index;
			Row.SpeedMultiplier = 0.6f + 0.1f * Index;
			Row.DamageMultiplier = 0.8f + 0.15f * Index;
			Row.ExperienceRewardBonus = Index % 4;
			Row.BossHealthMultiplier = 1.1f + 0.2f * Index;
			Row.BossSpeedMultiplier = 0.75f + 0.05f * Index;
			Row.BossDamageMultiplier = 1.05f + 0.075f * Index;
			Asset->EnemyBalanceRows.Add(Row);
		}
		Asset->EnemyWaveEntries.Add({ 8, ERogueEnemyType::Hunter, 5 });
		Asset->EnemyWaveEntries.Add({ 2, ERogueEnemyType::Shooter, 4 });
		RouteLegacyPostLoad(Asset);
		return Asset;
	}

	void AppendCaseSnapshot(FString& Output, const TCHAR* Label, const URogueGameBalanceAsset* Asset)
	{
		TArray<int32> Waves;
		for (int32 Wave = 1; Wave <= 64; ++Wave) { Waves.Add(Wave); }
		Waves.Append({ 80, 120, 256 });
		Output += FString::Printf(TEXT("%s,shop,%d,%d,%d,%.9g\n"), Label,
			RogueGameModeRules::GetShopOfferCount(Asset), RogueGameModeRules::GetShopOfferCost(Asset),
			RogueGameModeRules::GetShopRefreshCost(Asset), RogueGameModeRules::GetShopAutoRefreshInterval(Asset));

		for (const int32 Wave : Waves)
		{
			Output += FString::Printf(TEXT("%s,reward,%d,%d,%d\n"), Label, Wave,
				RogueGameModeRules::BuildCurrencyReward(Wave, false, Asset), RogueGameModeRules::BuildCurrencyReward(Wave, true, Asset));
			for (const ERogueEnemyType Type : EnemyTypes)
			{
				for (int32 Boss = 0; Boss < 2; ++Boss)
				{
					const FRogueEnemyProfile Profile = Boss != 0
						? RogueGameModeRules::BuildBossProfile(Wave, Type, Asset)
						: RogueGameModeRules::BuildEnemyProfile(Wave, Type, Asset);
					Output += FString::Printf(TEXT("%s,profile,%d,%d,%d,%.9g,%.9g,%.9g,%d\n"), Label, Wave,
						static_cast<int32>(Profile.EnemyType), Profile.bIsBoss ? 1 : 0,
						Profile.Health, Profile.Speed, Profile.Damage, Profile.ExperienceReward);
				}
			}

			const int32 BaseLimits[] = { 7, 45 };
			const float Intervals[] = { 0.5f, 1.25f };
			const int32 FreeSlotBoundaries[] = { 0, 1, 2, 3, 9, 10, 11, 12, 23, 24, 25, 50 };
			for (const int32 BaseLimit : BaseLimits)
			{
				for (const float Interval : Intervals)
				{
					for (int32 Boss = 0; Boss < 2; ++Boss)
					{
						const int32 Limit = RogueGameModeRules::BuildSpawnCadence(Wave, 0, BaseLimit, Boss != 0, Interval, Asset).EffectiveMaxAliveEnemies;
						TArray<int32> AliveCounts = { 0, 1, Limit, Limit + 1 };
						for (const int32 FreeSlots : FreeSlotBoundaries) { AliveCounts.AddUnique(FMath::Max(0, Limit - FreeSlots)); }
						for (const int32 AliveCount : AliveCounts)
						{
							const FRogueSpawnCadence Cadence = RogueGameModeRules::BuildSpawnCadence(Wave, AliveCount, BaseLimit, Boss != 0, Interval, Asset);
							Output += FString::Printf(TEXT("%s,cadence,%d,%d,%d,%d,%.9g,%d,%d,%.9g\n"), Label,
								Wave, AliveCount, BaseLimit, Boss, Interval,
								Cadence.EffectiveMaxAliveEnemies, Cadence.SpawnBatchSize, Cadence.NextSpawnDelay);
						}
					}
				}
			}

			for (int32 Mode = 0; Mode < 4; ++Mode)
			{
				FMath::RandInit(18737 + Wave * 17 + Mode * 101);
				FString Picks;
				auto CanSpawnType = [Mode](ERogueEnemyType Type)
				{
					if (Mode == 1) { return static_cast<int32>(Type) % 2 != 0; }
					if (Mode == 2) { return Type != ERogueEnemyType::Hunter && Type != ERogueEnemyType::Tank; }
					return Mode != 3;
				};
				for (int32 Sample = 0; Sample < 32; ++Sample)
				{
					const ERogueEnemyType Type = Mode == 0
						? RogueGameModeRules::PickWeightedEnemyTypeForWave(Wave, Asset)
						: RogueGameModeRules::PickWeightedEnemyTypeForWave(Wave, Asset, CanSpawnType);
					if (Sample != 0) { Picks += TEXT(";"); }
					Picks += FString::FromInt(static_cast<int32>(Type));
				}
				Output += FString::Printf(TEXT("%s,picks,%d,%d,%s\n"), Label, Wave, Mode, *Picks);
			}
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueGameBalanceSnapshotTest,
	"Rogue.Gameplay.GameBalance.RuleSnapshots", RogueGameBalanceTests::Flags)

bool FRogueGameBalanceSnapshotTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	TStrongObjectPtr<URogueGameBalanceAsset> Actual(LoadObject<URogueGameBalanceAsset>(nullptr,
		TEXT("/Game/Data/Core/DA_GameBalance.DA_GameBalance")));
	if (!TestNotNull(TEXT("The real DA_GameBalance loads from its current path"), Actual.Get())) { return false; }
	TStrongObjectPtr<URogueGameBalanceAsset> NativeDefaults(NewObject<URogueGameBalanceAsset>());
	RogueGameBalanceTests::RouteLegacyPostLoad(NativeDefaults.Get());
	TStrongObjectPtr<URogueGameBalanceAsset> Custom(RogueGameBalanceTests::MakeLegacyFixture(*this, 0));
	TStrongObjectPtr<URogueGameBalanceAsset> Uncapped(RogueGameBalanceTests::MakeLegacyFixture(*this, 1));
	TStrongObjectPtr<URogueGameBalanceAsset> ZeroCaps(RogueGameBalanceTests::MakeLegacyFixture(*this, 2));
	if (!Custom.IsValid() || !Uncapped.IsValid() || !ZeroCaps.IsValid()) { return false; }

	FString Snapshot(TEXT("RogueGameBalanceSnapshotV1\n"));
	RogueGameBalanceTests::AppendCaseSnapshot(Snapshot, TEXT("ActualAsset"), Actual.Get());
	RogueGameBalanceTests::AppendCaseSnapshot(Snapshot, TEXT("NullAsset"), nullptr);
	RogueGameBalanceTests::AppendCaseSnapshot(Snapshot, TEXT("NativeDefaults"), NativeDefaults.Get());
	RogueGameBalanceTests::AppendCaseSnapshot(Snapshot, TEXT("CustomMigration"), Custom.Get());
	RogueGameBalanceTests::AppendCaseSnapshot(Snapshot, TEXT("UncappedStages"), Uncapped.Get());
	RogueGameBalanceTests::AppendCaseSnapshot(Snapshot, TEXT("ZeroFinalCaps"), ZeroCaps.Get());

	FString SnapshotPath;
	if (FParse::Value(FCommandLine::Get(), TEXT("RogueBalanceSnapshot="), SnapshotPath))
	{
		if (!TestFalse(TEXT("The requested snapshot path is absolute"), FPaths::IsRelative(SnapshotPath))) { return false; }
		const FString ParentDirectory = FPaths::GetPath(SnapshotPath);
		if (!IFileManager::Get().MakeDirectory(*ParentDirectory, true))
		{
			AddError(FString::Printf(TEXT("Cannot create the snapshot output directory: %s"), *ParentDirectory));
			return false;
		}
		if (!TestTrue(TEXT("The stable rule snapshot is written"), FFileHelper::SaveStringToFile(
			Snapshot, *SnapshotPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))) { return false; }
		AddInfo(FString::Printf(TEXT("Rule snapshot saved to %s (%d characters)"), *SnapshotPath, Snapshot.Len()));
	}
	else
	{
		AddInfo(TEXT("Rule snapshot evaluated. Supply -RogueBalanceSnapshot=absolute_path to save it for a before/after comparison."));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueGameBalanceSemanticsTest,
	"Rogue.Gameplay.GameBalance.ExistingRuleSemantics", RogueGameBalanceTests::Flags)

bool FRogueGameBalanceSemanticsTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const int32 Wave = 10;
	const int32 Limit = RogueGameModeRules::BuildSpawnCadence(Wave, 0, 60, false, 1.0f).EffectiveMaxAliveEnemies;
	TestEqual(TEXT("Twenty-four free slots permit three spawn attempts"),
		RogueGameModeRules::BuildSpawnCadence(Wave, Limit - 24, 60, false, 1.0f).SpawnBatchSize, 3);
	TestEqual(TEXT("Twenty-three free slots fall back to two spawn attempts"),
		RogueGameModeRules::BuildSpawnCadence(Wave, Limit - 23, 60, false, 1.0f).SpawnBatchSize, 2);
	TestEqual(TEXT("Nine free slots permit only one spawn attempt"),
		RogueGameModeRules::BuildSpawnCadence(Wave, Limit - 9, 60, false, 1.0f).SpawnBatchSize, 1);
	TestEqual(TEXT("The null-asset minimum delay remains 0.18 seconds"),
		RogueGameModeRules::BuildSpawnCadence(100, 0, 60, false, 0.1f).NextSpawnDelay, 0.18f);

	TestEqual(TEXT("Ordinary enemies start at their base health in wave one"),
		RogueGameModeRules::BuildEnemyProfile(1, ERogueEnemyType::Hunter).Health, 35.0f);
	TestEqual(TEXT("The start wave includes the first health growth increment"),
		RogueGameModeRules::BuildEnemyProfile(8, ERogueEnemyType::Hunter).Health, (35.0f + 7.0f * 10.0f) * 1.08f);
	TestEqual(TEXT("The two health stages multiply rather than replace each other"),
		RogueGameModeRules::BuildEnemyProfile(25, ERogueEnemyType::Hunter).Health, (35.0f + 24.0f * 10.0f) * 1.72f * 1.5f, 0.001f);
	TestEqual(TEXT("Boss growth already adds one increment in wave one"),
		RogueGameModeRules::BuildBossProfile(1, ERogueEnemyType::Hunter).Health, (420.0f + 140.0f) * 1.58f, 0.001f);
	TestEqual(TEXT("The later boss phase replaces the earlier multiplier"),
		RogueGameModeRules::BuildBossProfile(20, ERogueEnemyType::Hunter).Health, (420.0f + 20.0f * 140.0f) * 1.72f, 0.001f);
	TestEqual(TEXT("Type experience bonus is added after the ordinary reward cap"),
		RogueGameModeRules::BuildEnemyProfile(20, ERogueEnemyType::Tank).ExperienceReward, 7);
	TestEqual(TEXT("Boss rewards do not receive the ordinary type experience bonus"),
		RogueGameModeRules::BuildBossProfile(1, ERogueEnemyType::Tank).ExperienceReward, 30);
	TestEqual(TEXT("Ordinary currency steps up at the divisor wave"), RogueGameModeRules::BuildCurrencyReward(6, false), 2);
	TestEqual(TEXT("Boss currency includes its base plus the current wave"), RogueGameModeRules::BuildCurrencyReward(1, true), 15);

	TStrongObjectPtr<URogueGameBalanceAsset> Custom(RogueGameBalanceTests::MakeLegacyFixture(*this, 0));
	TStrongObjectPtr<URogueGameBalanceAsset> Uncapped(RogueGameBalanceTests::MakeLegacyFixture(*this, 1));
	TStrongObjectPtr<URogueGameBalanceAsset> ZeroCaps(RogueGameBalanceTests::MakeLegacyFixture(*this, 2));
	if (!Custom.IsValid() || !Uncapped.IsValid() || !ZeroCaps.IsValid()) { return false; }
	TestEqual(TEXT("Three-spawn rule can match before the two-spawn rule starts"),
		RogueGameModeRules::BuildSpawnCadence(3, 0, 45, false, 1.0f, Custom.Get()).SpawnBatchSize, 3);
	TestEqual(TEXT("A boss floor can raise an otherwise smaller hard limit"),
		RogueGameModeRules::BuildSpawnCadence(1, 0, 45, true, 1.0f, ZeroCaps.Get()).EffectiveMaxAliveEnemies, 19);
	TestEqual(TEXT("Zero ordinary health cap means zero profile health"),
		RogueGameModeRules::BuildEnemyProfile(10, ERogueEnemyType::Tank, ZeroCaps.Get()).Health, 0.0f);
	TestEqual(TEXT("Shop counts are clamped to at least one"), RogueGameModeRules::GetShopOfferCount(Uncapped.Get()), 1);
	TestEqual(TEXT("Shop offer price is clamped to at least one"), RogueGameModeRules::GetShopOfferCost(Uncapped.Get()), 1);
	TestEqual(TEXT("Shop refresh cost is clamped to nonnegative"), RogueGameModeRules::GetShopRefreshCost(Uncapped.Get()), 0);
	TestEqual(TEXT("Shop refresh interval is clamped to at least five seconds"), RogueGameModeRules::GetShopAutoRefreshInterval(Uncapped.Get()), 5.0f);
	return true;
}

#if WITH_EDITOR

namespace RogueGameBalanceTests
{
	void CheckEditableMetadata(FAutomationTestBase& Test, const UStruct* Struct, TSet<const UStruct*>& Visited)
	{
		if (Visited.Contains(Struct)) { return; }
		Visited.Add(Struct);
		for (TFieldIterator<FProperty> It(Struct, EFieldIteratorFlags::ExcludeSuper); It; ++It)
		{
			const FProperty* Property = *It;
			if (!Property->HasAnyPropertyFlags(CPF_Edit)) { continue; }
			const FString Label = Struct->GetName() + TEXT(".") + Property->GetName();
			Test.TestFalse(Label + TEXT(" has an explicit display name"), Property->GetMetaData(TEXT("DisplayName")).TrimStartAndEnd().IsEmpty());
			Test.TestFalse(Label + TEXT(" has a configuration tooltip"), Property->GetMetaData(TEXT("ToolTip")).TrimStartAndEnd().IsEmpty());

			if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
			{
				CheckEditableMetadata(Test, StructProperty->Struct, Visited);
			}
			else if (const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property))
			{
				if (const FStructProperty* ElementProperty = CastField<FStructProperty>(ArrayProperty->Inner))
				{
					CheckEditableMetadata(Test, ElementProperty->Struct, Visited);
				}
			}
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueGroupedGameBalanceLayoutTest,
	"Rogue.Gameplay.GameBalance.GroupedLayoutMetadataAndRules", RogueGameBalanceTests::Flags)

bool FRogueGroupedGameBalanceLayoutTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const UClass* AssetClass = URogueGameBalanceAsset::StaticClass();
	struct FExpectedCategory { const TCHAR* PropertyName; const TCHAR* Category; };
	const FExpectedCategory ExpectedCategories[] =
	{
		{ TEXT("EnemyWaveEntries"), TEXT("01.敌人种类") },
		{ TEXT("EnemyBalanceRows"), TEXT("01.敌人种类") },
		{ TEXT("Spawn"), TEXT("02.刷怪数量与速度") },
		{ TEXT("EnemyStats"), TEXT("03.普通敌人属性") },
		{ TEXT("BossStats"), TEXT("04.首领属性") },
		{ TEXT("Rewards"), TEXT("05.击杀奖励") },
		{ TEXT("Shop"), TEXT("06.商店") }
	};
	TSet<FString> Categories;
	for (const FExpectedCategory& Expected : ExpectedCategories)
	{
		const FProperty* Property = FindFProperty<FProperty>(AssetClass, Expected.PropertyName);
		if (!TestNotNull(FString::Printf(TEXT("The grouped field %s exists"), Expected.PropertyName), Property)) { continue; }
		TestTrue(Property->GetName() + TEXT(" is editable"), Property->HasAnyPropertyFlags(CPF_Edit));
		TestEqual(Property->GetName() + TEXT(" uses its Chinese category"), Property->GetMetaData(TEXT("Category")), FString(Expected.Category));
		Categories.Add(Property->GetMetaData(TEXT("Category")));
		if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
		{
			TestTrue(Property->GetName() + TEXT(" displays its members in the parent category"), Property->HasMetaData(TEXT("ShowOnlyInnerProperties")));
			for (TFieldIterator<FProperty> Inner(StructProperty->Struct, EFieldIteratorFlags::ExcludeSuper); Inner; ++Inner)
			{
				if (!Inner->HasAnyPropertyFlags(CPF_Edit)) { continue; }
				const FString InnerCategory = Inner->GetMetaData(TEXT("Category"));
				// UE's detail layout retains the parent category when a member's category is its owning struct name.
				TestTrue(Property->GetName() + TEXT(".") + Inner->GetName() + TEXT(" inherits the parent category or names it explicitly"),
					InnerCategory == StructProperty->Struct->GetName() || InnerCategory == Expected.Category);
			}
		}
	}
	TestEqual(TEXT("Seven top-level configuration fields occupy six Chinese categories"), Categories.Num(), 6);

	int32 EditableFieldCount = 0;
	int32 DeprecatedFieldCount = 0;
	for (TFieldIterator<FProperty> It(AssetClass, EFieldIteratorFlags::ExcludeSuper); It; ++It)
	{
		if (It->HasAnyPropertyFlags(CPF_Edit)) { ++EditableFieldCount; }
		if (It->HasAnyPropertyFlags(CPF_Deprecated))
		{
			++DeprecatedFieldCount;
			TestFalse(It->GetName() + TEXT(" is hidden from editing and blueprint field access"), It->HasAnyPropertyFlags(CPF_Edit | CPF_BlueprintVisible));
		}
	}
	TestEqual(TEXT("Only the seven grouped top-level fields remain editable"), EditableFieldCount, 7);
	TestEqual(TEXT("All 55 legacy scalar fields retain real deprecated property flags"), DeprecatedFieldCount, 55);
	TSet<const UStruct*> Visited;
	RogueGameBalanceTests::CheckEditableMetadata(*this, AssetClass, Visited);

	TStrongObjectPtr<URogueGameBalanceAsset> Asset(NewObject<URogueGameBalanceAsset>());
	Asset->Spawn.MaxAlivePerWave = 0;
	Asset->Spawn.MaxAliveLimit = 0;
	Asset->Spawn.BatchRules.Reset();
	TestEqual(TEXT("An intentionally empty batch list attempts one enemy per spawn"),
		RogueGameModeRules::BuildSpawnCadence(50, 0, 50, false, 1.0f, Asset.Get()).SpawnBatchSize, 1);
	Asset->Spawn.BatchRules = { {1, 0, 5}, {1, 0, 2} };
	TestEqual(TEXT("The last matching batch rule wins even when its count is smaller"),
		RogueGameModeRules::BuildSpawnCadence(50, 0, 50, false, 1.0f, Asset.Get()).SpawnBatchSize, 2);

	FRogueEnemyBalanceRow Hunter;
	Hunter.EnemyType = ERogueEnemyType::Hunter;
	Asset->EnemyBalanceRows = { Hunter };
	Asset->EnemyStats.Health = {100.0f, 10.0f};
	Asset->EnemyStats.MaxHealth = -1.0f;
	Asset->EnemyStats.HealthGrowthStages.Reset();
	TestEqual(TEXT("An empty health stage list retains only base and linear wave growth"),
		RogueGameModeRules::BuildEnemyProfile(4, ERogueEnemyType::Hunter, Asset.Get()).Health, 130.0f);
	Asset->EnemyStats.HealthGrowthStages = { {1, 0.1f, 0.0f}, {3, 0.15f, 0.0f} };
	TestEqual(TEXT("Both started health stages multiply the running health in list order"),
		RogueGameModeRules::BuildEnemyProfile(4, ERogueEnemyType::Hunter, Asset.Get()).Health,
		(130.0f * 1.4f) * 1.3f, 0.001f);
	return true;
}

#endif

#endif
