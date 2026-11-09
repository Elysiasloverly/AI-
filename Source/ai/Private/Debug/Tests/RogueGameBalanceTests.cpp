#if WITH_DEV_AUTOMATION_TESTS

#include "Framework/RogueGameBalanceAsset.h"
#include "Framework/RogueGameModeRules.h"
#include "HAL/FileManager.h"
#include "Math/UnrealMathUtility.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"

namespace RogueGameBalanceTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;
	const ERogueEnemyType EnemyTypes[] =
	{
		ERogueEnemyType::Rifleman, ERogueEnemyType::Assault, ERogueEnemyType::Juggernaut,
		ERogueEnemyType::Orbiter, ERogueEnemyType::Marksman, ERogueEnemyType::Skirmisher,
		ERogueEnemyType::Cannoneer, ERogueEnemyType::Scatterer, ERogueEnemyType::ShockPillar,
		ERogueEnemyType::Sniper, ERogueEnemyType::Shotgunner, ERogueEnemyType::Repeater,
		ERogueEnemyType::Ringcaster
	};

	// 覆盖乱序批次、零/负上限、分母边界及缺失敌人类型；直接写当前分组字段。
	URogueGameBalanceAsset* MakeRuleFixture(int32 Variant)
	{
		URogueGameBalanceAsset* Asset = NewObject<URogueGameBalanceAsset>();
		Asset->Spawn.EarlyWaves.EndWave = 6;
		Asset->Rewards.EnemyExperience.BaseAmount = 4;
		Asset->Rewards.EnemyExperience.WaveInterval = 3;
		Asset->Rewards.EnemyExperience.MaximumAmount = 8;
		Asset->Rewards.BossExperience.PerWave = 6;
		Asset->Rewards.BossExperience.MinimumAmount = 17;
		Asset->Rewards.BossExperience.MaximumAmount = 67;
		Asset->BossStats.LateStageStartWave = 9;
		Asset->Rewards.EnemyGold.BaseAmount = 3;
		Asset->Rewards.EnemyGold.WaveInterval = 4;
		Asset->Rewards.EnemyGold.MaximumAmount = 9;
		Asset->Rewards.BossGold.BaseAmount = 21;
		Asset->Rewards.BossGold.PerWave = 3;
		Asset->Rewards.BossGold.MaximumAmount = 77;
		Asset->Shop.OfferCount = 9;
		Asset->Shop.OfferCost = 31;
		Asset->Shop.RefreshCost = 19;
		Asset->Spawn.AccelerationPerWave = 0.13f;
		Asset->Spawn.MaxAcceleration = 3.7f;
		Asset->Spawn.EarlyWaves.FirstWaveIntervalMultiplier = 1.6f;
		Asset->Spawn.EarlyWaves.LastWaveIntervalMultiplier = 0.85f;
		Asset->Spawn.MinimumInterval = 0.27f;
		Asset->EnemyStats.Health.BaseValue = 48.5f;
		Asset->EnemyStats.Health.PerWave = 6.25f;
		Asset->EnemyStats.Speed.BaseValue = 198.5f;
		Asset->EnemyStats.Speed.PerWave = 5.75f;
		Asset->EnemyStats.Damage.BaseValue = 11.25f;
		Asset->EnemyStats.Damage.PerWave = 0.875f;
		Asset->EnemyStats.MaxHealth = 675.5f;
		Asset->BossStats.Health.BaseValue = 777.5f;
		Asset->BossStats.Health.PerWave = 83.25f;
		Asset->BossStats.Speed.BaseValue = 313.5f;
		Asset->BossStats.Speed.PerWave = 6.25f;
		Asset->BossStats.Damage.BaseValue = 27.5f;
		Asset->BossStats.Damage.PerWave = 3.125f;
		Asset->BossStats.EarlyStage.HealthMultiplier = 1.27f;
		Asset->BossStats.LateStage.HealthMultiplier = 2.13f;
		Asset->BossStats.EarlyStage.DamageMultiplier = 0.83f;
		Asset->BossStats.LateStage.DamageMultiplier = 1.45f;
		Asset->BossStats.SpeedMultiplier = 1.07f;
		Asset->Shop.AutoRefreshInterval = 83.0f;
		Asset->Spawn.BatchRules = {{7, 2}, {3, 3}};
		Asset->EnemyStats.HealthGrowthStages = {{12, 0.125f, 0.0f}, {4, 0.0375f, -0.5f}};
		if (Variant == 1)
		{
			Asset->Rewards.EnemyExperience.WaveInterval = 0;
			Asset->Rewards.EnemyGold.WaveInterval = -3;
			Asset->EnemyStats.MaxHealth = -1.0f;
			Asset->BossStats.LateStageStartWave = 0;
			Asset->Shop.OfferCount = 0;
			Asset->Shop.OfferCost = -3;
			Asset->Shop.RefreshCost = -4;
			Asset->Shop.AutoRefreshInterval = 0.0f;
			Asset->EnemyStats.HealthGrowthStages = {{10, 0.125f, -1.0f}, {2, 0.0375f, 0.0f}};
		}
		else if (Variant == 2)
		{
			Asset->EnemyStats.MaxHealth = 0.0f;
			Asset->BossStats.LateStageStartWave = 5;
			Asset->Spawn.BatchRules = {{4, 2}, {10, 3}};
			Asset->EnemyStats.HealthGrowthStages = {{12, 0.125f, 0.25f}, {4, 0.0375f, 0.15f}};
		}

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
		Asset->EnemyWaveEntries.Add({ 8, ERogueEnemyType::Rifleman, 5 });
		Asset->EnemyWaveEntries.Add({ 2, ERogueEnemyType::Marksman, 4 });
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

            for (const float Interval : { .5f, 1.25f })
            {
                const FRogueSpawnCadence Cadence = RogueGameModeRules::BuildSpawnCadence(Wave, Interval, Asset);
                Output += FString::Printf(TEXT("%s,cadence,%d,%.9g,%d,%.9g\n"), Label,
                    Wave, Interval, Cadence.SpawnBatchSize, Cadence.NextSpawnDelay);
            }

			for (int32 Mode = 0; Mode < 4; ++Mode)
			{
				FMath::RandInit(18737 + Wave * 17 + Mode * 101);
				FString Picks;
				auto CanSpawnType = [Mode](ERogueEnemyType Type)
				{
					if (Mode == 1) { return static_cast<int32>(Type) % 2 != 0; }
					if (Mode == 2) { return Type != ERogueEnemyType::Rifleman && Type != ERogueEnemyType::Juggernaut; }
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
	TStrongObjectPtr<URogueGameBalanceAsset> Custom(RogueGameBalanceTests::MakeRuleFixture(0));
	TStrongObjectPtr<URogueGameBalanceAsset> Uncapped(RogueGameBalanceTests::MakeRuleFixture(1));
	TStrongObjectPtr<URogueGameBalanceAsset> ZeroCaps(RogueGameBalanceTests::MakeRuleFixture(2));
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
    TestEqual(TEXT("Wave ten permits three attempts without a global free-slot check"),
        RogueGameModeRules::BuildSpawnCadence(Wave, 1.0f).SpawnBatchSize, 3);
    TestEqual(TEXT("Wave five permits two attempts"),
        RogueGameModeRules::BuildSpawnCadence(5, 1.0f).SpawnBatchSize, 2);
    TestEqual(TEXT("Early waves permit one attempt"),
        RogueGameModeRules::BuildSpawnCadence(1, 1.0f).SpawnBatchSize, 1);
    TestEqual(TEXT("The null-asset minimum delay remains 0.18 seconds"),
        RogueGameModeRules::BuildSpawnCadence(100, 0.1f).NextSpawnDelay, 0.18f);

	TestEqual(TEXT("Ordinary enemies start at their base health in wave one"),
		RogueGameModeRules::BuildEnemyProfile(1, ERogueEnemyType::Rifleman).Health, 35.0f);
	TestEqual(TEXT("The start wave includes the first health growth increment"),
		RogueGameModeRules::BuildEnemyProfile(8, ERogueEnemyType::Rifleman).Health, (35.0f + 7.0f * 10.0f) * 1.08f);
	TestEqual(TEXT("The two health stages multiply rather than replace each other"),
		RogueGameModeRules::BuildEnemyProfile(25, ERogueEnemyType::Rifleman).Health, (35.0f + 24.0f * 10.0f) * 1.72f * 1.5f, 0.001f);
	TestEqual(TEXT("Boss growth already adds one increment in wave one"),
		RogueGameModeRules::BuildBossProfile(1, ERogueEnemyType::Rifleman).Health, (420.0f + 140.0f) * 1.58f, 0.001f);
	TestEqual(TEXT("The later boss phase replaces the earlier multiplier"),
		RogueGameModeRules::BuildBossProfile(20, ERogueEnemyType::Rifleman).Health, (420.0f + 20.0f * 140.0f) * 1.72f, 0.001f);
	TestEqual(TEXT("Type experience bonus is added after the ordinary reward cap"),
		RogueGameModeRules::BuildEnemyProfile(20, ERogueEnemyType::Juggernaut).ExperienceReward, 7);
	TestEqual(TEXT("Boss rewards do not receive the ordinary type experience bonus"),
		RogueGameModeRules::BuildBossProfile(1, ERogueEnemyType::Juggernaut).ExperienceReward, 30);
	TestEqual(TEXT("Ordinary currency steps up at the divisor wave"), RogueGameModeRules::BuildCurrencyReward(6, false), 2);
	URogueGameBalanceAsset* Gold = NewObject<URogueGameBalanceAsset>();
	Gold->Rewards.EnemyGold = {3, 6, 8};
	Gold->Rewards.EnemyGoldStepAmount = 2;
	TestEqual(TEXT("Gold stays at the base before the first step"), RogueGameModeRules::BuildCurrencyReward(5, false, Gold), 3);
	TestEqual(TEXT("Gold uses the configured step at wave six"), RogueGameModeRules::BuildCurrencyReward(6, false, Gold), 5);
	TestEqual(TEXT("Gold adds another step at wave twelve"), RogueGameModeRules::BuildCurrencyReward(12, false, Gold), 7);
	TestEqual(TEXT("Gold respects its cap"), RogueGameModeRules::BuildCurrencyReward(18, false, Gold), 8);
	Gold->Rewards.EnemyGoldStepAmount = 0;
	TestEqual(TEXT("A zero step keeps the base reward"), RogueGameModeRules::BuildCurrencyReward(90, false, Gold), 3);
	TestEqual(TEXT("Boss currency includes its base plus the current wave"), RogueGameModeRules::BuildCurrencyReward(1, true), 15);

	TStrongObjectPtr<URogueGameBalanceAsset> Custom(RogueGameBalanceTests::MakeRuleFixture(0));
	TStrongObjectPtr<URogueGameBalanceAsset> Uncapped(RogueGameBalanceTests::MakeRuleFixture(1));
	TStrongObjectPtr<URogueGameBalanceAsset> ZeroCaps(RogueGameBalanceTests::MakeRuleFixture(2));
	if (!Custom.IsValid() || !Uncapped.IsValid() || !ZeroCaps.IsValid()) { return false; }
	TestEqual(TEXT("Three-spawn rule can match before the two-spawn rule starts"),
		RogueGameModeRules::BuildSpawnCadence(3, 1.0f, Custom.Get()).SpawnBatchSize, 3);
	TestEqual(TEXT("Zero ordinary health cap means zero profile health"),
		RogueGameModeRules::BuildEnemyProfile(10, ERogueEnemyType::Juggernaut, ZeroCaps.Get()).Health, 0.0f);
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
        { TEXT("GrowthCurveTable"), TEXT("00.成长曲线") },
		{ TEXT("EnemyWaveEntries"), TEXT("01.敌人种类") },
		{ TEXT("EnemyBalanceRows"), TEXT("01.敌人种类") },
		{ TEXT("Spawn"), TEXT("02.刷怪数量与速度") },
		{ TEXT("EnemyStats"), TEXT("03.普通敌人属性") },
		{ TEXT("BossStats"), TEXT("04.首领属性") },
		{ TEXT("BossSpawn"), TEXT("04.首领属性") },
		{ TEXT("Rewards"), TEXT("05.击杀奖励") },
		{ TEXT("Shop"), TEXT("06.商店") },
		{ TEXT("Pools"), TEXT("07.运行时") },
		{ TEXT("Performance"), TEXT("07.运行时") },
		{ TEXT("Scene"), TEXT("07.运行时") },
        { TEXT("BattleStage"), TEXT("08.昼夜阶段") }
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
	TestEqual(TEXT("Current configuration fields occupy nine Chinese categories"), Categories.Num(), 9);

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
	TestEqual(TEXT("Only current grouped top-level fields remain editable"), EditableFieldCount, static_cast<int32>(UE_ARRAY_COUNT(ExpectedCategories)));
	TestEqual(TEXT("Completed migration leaves no deprecated scalar properties"), DeprecatedFieldCount, 0);
    for (const TCHAR* Name : { TEXT("BaseMaxAliveEnemies"), TEXT("MaxAlivePerWave"), TEXT("MaxAliveLimit"), TEXT("BossAliveReduction"), TEXT("BossAliveMinimumLimit") })
    {
        const FProperty* Property = FindFProperty<FProperty>(FRogueSpawnBalanceSettings::StaticStruct(), Name);
        TestNull(FString(Name) + TEXT(" is removed after migration"), Property);
    }
    TestNull(TEXT("Removed global slack cannot appear as a working option"),
        FindFProperty<FProperty>(FRogueSpawnBatchRule::StaticStruct(), TEXT("RequiredFreeSlots")));
	TSet<const UStruct*> Visited;
	RogueGameBalanceTests::CheckEditableMetadata(*this, AssetClass, Visited);

	TStrongObjectPtr<URogueGameBalanceAsset> Asset(NewObject<URogueGameBalanceAsset>());


	Asset->Spawn.BatchRules.Reset();
	TestEqual(TEXT("An intentionally empty batch list attempts one enemy per spawn"),
		RogueGameModeRules::BuildSpawnCadence(50, 1.0f, Asset.Get()).SpawnBatchSize, 1);
	Asset->Spawn.BatchRules = { {1, 5}, {1, 2} };
	TestEqual(TEXT("The last matching batch rule wins even when its count is smaller"),
		RogueGameModeRules::BuildSpawnCadence(50, 1.0f, Asset.Get()).SpawnBatchSize, 2);

	FRogueEnemyBalanceRow Rifleman;
	Rifleman.EnemyType = ERogueEnemyType::Rifleman;
	Asset->EnemyBalanceRows = { Rifleman };
	Asset->EnemyStats.Health = {100.0f, 10.0f};
	Asset->EnemyStats.MaxHealth = -1.0f;
	Asset->EnemyStats.HealthGrowthStages.Reset();
	TestEqual(TEXT("An empty health stage list retains only base and linear wave growth"),
		RogueGameModeRules::BuildEnemyProfile(4, ERogueEnemyType::Rifleman, Asset.Get()).Health, 130.0f);
	Asset->EnemyStats.HealthGrowthStages = { {1, 0.1f, 0.0f}, {3, 0.15f, 0.0f} };
	TestEqual(TEXT("Both started health stages multiply the running health in list order"),
		RogueGameModeRules::BuildEnemyProfile(4, ERogueEnemyType::Rifleman, Asset.Get()).Health,
		(130.0f * 1.4f) * 1.3f, 0.001f);
	return true;
}

#endif

#endif
