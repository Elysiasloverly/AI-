#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Framework/RogueGameBalanceAsset.h"
#include "Framework/RogueGameModeRules.h"
#include "Engine/CurveTable.h"
#include "UObject/StrongObjectPtr.h"
#include <limits>

namespace RogueGrowthCurveTests
{
    constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;
    FRichCurve& Add(UCurveTable* Table, FName Name, float First, float Fifth)
    {
        FRichCurve& Curve = Table->AddRichCurve(Name);
        Curve.SetKeyInterpMode(Curve.AddKey(1.0f, First), RCIM_Linear);
        Curve.SetKeyInterpMode(Curve.AddKey(5.0f, Fifth), RCIM_Linear);
        Curve.PostInfinityExtrap = RCCE_Linear;
        return Curve;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueGrowthCurveOverrideTest,
    "Rogue.Gameplay.GrowthCurves.OverrideBeforeTypeAndKeepLimits", RogueGrowthCurveTests::Flags)
bool FRogueGrowthCurveOverrideTest::RunTest(const FString& Parameters)
{
    TStrongObjectPtr<URogueGameBalanceAsset> Asset(NewObject<URogueGameBalanceAsset>());
    TStrongObjectPtr<UCurveTable> Table(NewObject<UCurveTable>());
    Asset->GrowthCurveTable = Table.Get();
    Asset->EnemyStats.HealthGrowthStages = {{1, 0.5f, 0.5f}};
    Asset->BossStats.EarlyStage = {9.0f, 9.0f};
    Asset->BossStats.SpeedMultiplier = 9.0f;
    FRogueEnemyBalanceRow Type;
    Type.EnemyType = ERogueEnemyType::Rifleman;
    Type.HealthMultiplier = 2.0f; Type.SpeedMultiplier = 0.5f; Type.DamageMultiplier = 0.25f;
    Type.BossHealthMultiplier = 3.0f; Type.BossSpeedMultiplier = 2.0f; Type.BossDamageMultiplier = 4.0f;
    Asset->EnemyBalanceRows = {Type};
    using namespace RogueGrowthCurveTests;
    Add(Table.Get(), TEXT("EnemyHealth"), 100, 180);
    Add(Table.Get(), TEXT("EnemySpeed"), 120, 200);
    Add(Table.Get(), TEXT("EnemyDamage"), 10, 18);
    Add(Table.Get(), TEXT("BossHealth"), 1000, 1400);
    Add(Table.Get(), TEXT("BossSpeed"), 200, 240);
    Add(Table.Get(), TEXT("BossDamage"), 20, 24);
    Add(Table.Get(), TEXT("EnemiesPerWave"), 10.2f, 30.2f);
    Add(Table.Get(), TEXT("SpawnInterval"), 0.1f, 0.3f);
    const auto Normal = RogueGameModeRules::BuildEnemyProfile(5, Type.EnemyType, Asset.Get());
    const auto Boss = RogueGameModeRules::BuildBossProfile(5, Type.EnemyType, Asset.Get());
    TestEqual(TEXT("Normal HP does not multiply stages twice"), Normal.Health, 360.0f);
    TestEqual(TEXT("Normal speed keeps type factor"), Normal.Speed, 100.0f);
    TestEqual(TEXT("Normal damage keeps type factor"), Normal.Damage, 4.5f);
    TestEqual(TEXT("Boss HP replaces generic stage result"), Boss.Health, 4200.0f);
    TestEqual(TEXT("Boss speed curve includes generic multiplier already"), Boss.Speed, 480.0f);
    TestEqual(TEXT("Boss damage keeps type factor once"), Boss.Damage, 96.0f);
    TestEqual(TEXT("Count curve rounds to nearest integer"), RogueGameModeRules::BuildWaveEnemyCount(5, Asset.Get()), 30);
    Asset->Spawn.EnemiesPerWaveLimit = 12;
    TestEqual(TEXT("Per-wave cumulative cap remains effective"), RogueGameModeRules::BuildWaveEnemyCount(5, Asset.Get()), 12);
    Asset->Spawn.MinimumInterval = 0.45f;
    TestEqual(TEXT("Curve interval respects configured minimum"), RogueGameModeRules::BuildSpawnCadence(5, 1, Asset.Get()).NextSpawnDelay, 0.45f);
    Asset->EnemyStats.MaxHealth = 250;
    TestEqual(TEXT("Final HP cap applies after type multiplier"), RogueGameModeRules::BuildEnemyProfile(5, Type.EnemyType, Asset.Get()).Health, 250.0f);
    float Value = 0;
    TestTrue(TEXT("Fractional wave evaluates native interpolation"), Asset->TryEvaluateGrowthCurve(TEXT("EnemyHealth"), 3.0f, Value));
    TestEqual(TEXT("Native curve interpolates"), Value, 140.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueGrowthCurveFallbackTest,
    "Rogue.Gameplay.GrowthCurves.MissingInvalidAndExtremeValues", RogueGrowthCurveTests::Flags)
bool FRogueGrowthCurveFallbackTest::RunTest(const FString& Parameters)
{
    TStrongObjectPtr<URogueGameBalanceAsset> Asset(NewObject<URogueGameBalanceAsset>());
    const float Original = RogueGameModeRules::BuildEnemyProfile(5, ERogueEnemyType::Rifleman, Asset.Get()).Health;
    const int32 OriginalCount = RogueGameModeRules::BuildWaveEnemyCount(5, Asset.Get());
    float Value = 99;
    TestFalse(TEXT("Unset table requests fallback"), Asset->TryEvaluateGrowthCurve(TEXT("EnemyHealth"), 5, Value));
    TStrongObjectPtr<UCurveTable> Table(NewObject<UCurveTable>());
    Asset->GrowthCurveTable = Table.Get();
    Table->AddRichCurve(TEXT("EnemyHealth"));
    TestEqual(TEXT("Empty row keeps formula"), RogueGameModeRules::BuildEnemyProfile(5, ERogueEnemyType::Rifleman, Asset.Get()).Health, Original);
    Table->FindRichCurve(TEXT("EnemyHealth"), TEXT("Test"))->AddKey(5, std::numeric_limits<float>::quiet_NaN());
    TestEqual(TEXT("NaN curve keeps valid formula"), RogueGameModeRules::BuildEnemyProfile(5, ERogueEnemyType::Rifleman, Asset.Get()).Health, Original);
    RogueGrowthCurveTests::Add(Table.Get(), TEXT("EnemiesPerWave"), -1, -1);
    TestEqual(TEXT("Negative count keeps formula"), RogueGameModeRules::BuildWaveEnemyCount(5, Asset.Get()), OriginalCount);
    Table->RemoveRow(TEXT("EnemiesPerWave"));
    RogueGrowthCurveTests::Add(Table.Get(), TEXT("EnemiesPerWave"), MAX_flt, MAX_flt);
    TestEqual(TEXT("Huge count saturates without integer overflow"), RogueGameModeRules::BuildWaveEnemyCount(5, Asset.Get()), MAX_int32);
    TestFalse(TEXT("Unknown row requests fallback"), Asset->TryEvaluateGrowthCurve(TEXT("Missing"), 5, Value));
    TestFalse(TEXT("Non-finite X requests fallback"), Asset->TryEvaluateGrowthCurve(TEXT("EnemiesPerWave"), std::numeric_limits<float>::infinity(), Value));
    TestEqual(TEXT("Null asset path remains valid"), RogueGameModeRules::BuildWaveEnemyCount(1, nullptr), GetDefault<URogueGameBalanceAsset>()->Spawn.BaseEnemiesPerWave);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueGrowthCurveAssetTest,
    "Rogue.Gameplay.GrowthCurves.FormalAssetRowsAndRuntimeRouting", RogueGrowthCurveTests::Flags)
bool FRogueGrowthCurveAssetTest::RunTest(const FString& Parameters)
{
    TStrongObjectPtr<URogueGameBalanceAsset> Asset(LoadObject<URogueGameBalanceAsset>(nullptr, TEXT("/Game/Data/Core/DA_GameBalance.DA_GameBalance")));
    if (!TestNotNull(TEXT("Formal balance loads"), Asset.Get())) { return false; }
    if (!TestNotNull(TEXT("Formal curve table is referenced for cooking"), Asset->GrowthCurveTable.Get())) { return false; }
    for (int32 Wave = 1; Wave <= 100; ++Wave)
    {
        for (FName Row : {FName(TEXT("EnemiesPerWave")), FName(TEXT("SpawnInterval")), FName(TEXT("EnemyHealth")), FName(TEXT("EnemySpeed")),
            FName(TEXT("EnemyDamage")), FName(TEXT("BossHealth")), FName(TEXT("BossSpeed")), FName(TEXT("BossDamage"))})
        {
            float Value = 0;
            if (!TestTrue(TEXT("Curve row is finite and nonnegative"), Asset->TryEvaluateGrowthCurve(Row, static_cast<float>(Wave), Value))) { return false; }
        }
        float Health = 0;
        Asset->TryEvaluateGrowthCurve(TEXT("EnemyHealth"), static_cast<float>(Wave), Health);
        for (const FRogueEnemyBalanceRow& Type : Asset->EnemyBalanceRows)
        {
            float Expected = Health * Type.HealthMultiplier;
            if (Asset->EnemyStats.MaxHealth >= 0) { Expected = FMath::Min(Expected, Asset->EnemyStats.MaxHealth); }
            const auto Profile = RogueGameModeRules::BuildEnemyProfile(Wave, Type.EnemyType, Asset.Get());
            if (!TestTrue(TEXT("All types route curve before type scaling"), FMath::IsNearlyEqual(Profile.Health, Expected, 0.02f))) { return false; }
        }
        float BossHealth = 0;
        Asset->TryEvaluateGrowthCurve(TEXT("BossHealth"), static_cast<float>(Wave), BossHealth);
        const auto Boss = RogueGameModeRules::BuildBossProfile(Wave, ERogueEnemyType::Rifleman, Asset.Get());
        const auto* Type = Asset->FindEnemyBalanceRow(ERogueEnemyType::Rifleman);
        TestTrue(TEXT("Boss uses curve once"), FMath::IsNearlyEqual(Boss.Health, BossHealth * (Type ? Type->BossHealthMultiplier : 1.0f), 0.02f));
    }
    return true;
}
#endif
