#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/RogueAttributeSet.h"
#include "Framework/RogueGameMode.h"
#include "Framework/RogueGameModeRules.h"
#include "Framework/RogueGameBalanceAsset.h"
#include "Enemies/RogueEnemy.h"
#include "Player/RogueCharacter.h"
#include "Player/RoguePlayerController.h"
#include "Player/RoguePlayerState.h"
#include "Progression/Meta/RogueUpgradeTreeDefinition.h"
#include "Progression/Upgrades/RogueUpgradeDefinitionAsset.h"
#include "Progression/Upgrades/RogueUpgradeRuleAsset.h"
#include "Progression/Shop/RogueShopSystem.h"
#include "Combat/Weapons/RogueWeapon_Projectile.h"
#include "Combat/Weapons/RogueWeapon_Scythe.h"
#include "Combat/Projectiles/RogueProjectile.h"
#include "UObject/UnrealType.h"

namespace RogueNumericAuditTests
{
    constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;
    struct FFixture
    {
        FTestConsoleVariable SelfTest{TEXT("Rogue.SelfTest.WeaponUpgradeChain")};
        FTestConsoleVariable Stress{TEXT("Rogue.SelfTest.EnemyStress")};
        FTestWorldWrapper TestWorld;
        UWorld* World = nullptr;
        ARogueCharacter* Player = nullptr;
        ARoguePlayerState* State = nullptr;
        FRogueUpgradeSystem Upgrades;
        bool Initialize()
        {
            SelfTest.Set(TEXT("0")); Stress.Set(TEXT("0"));
            if (!TestWorld.CreateTestWorld(EWorldType::Game)) { return false; }
            World = TestWorld.GetTestWorld(); World->GetWorldSettings()->DefaultGameMode = ARogueGameMode::StaticClass();
            if (!TestWorld.BeginPlayInTestWorld()) { return false; }
            auto* Mode = Cast<ARogueGameMode>(World->GetAuthGameMode()); Mode->SetActorTickEnabled(false);
            auto* Controller = World->SpawnActor<ARoguePlayerController>();
            State = Controller->GetPlayerState<ARoguePlayerState>();
            FRogueProgressionSnapshot Profile;
            for (const auto& Node : URogueUpgradeTreeDefinition::GetDefaultDefinition()->GetNodes())
                if (Node.Kind != ERogueMetaNodeKind::BaseStat) { Profile.NodeRanks.Add(Node.NodeId, 1); }
            State->SetProgressionSnapshotForRun(Profile);
            const FTransform Transform(FVector(0, 0, 200));
            Player = World->SpawnActorDeferred<ARogueCharacter>(ARogueCharacter::StaticClass(), Transform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
            auto* Weapons = NewObject<UDataTable>(World); Weapons->RowStruct = FRogueWeaponTableRow::StaticStruct();
            FRogueWeaponTableRow Weapon; Weapon.Count = 1; Weapon.WeaponClass = ARogueWeapon_Projectile::StaticClass();
            Weapon.ProjectileClass = ARogueProjectile::StaticClass(); Weapon.Cooldown = 0.7f; Weapon.MinCooldown = 0.25f;
            Weapons->AddRow(TEXT("Projectile"), Weapon); Weapon.WeaponClass = ARogueWeapon_Scythe::StaticClass(); Weapon.Count = 0; Weapon.CountUpgradeType = ERogueUpgradeType::ScytheCount; Weapons->AddRow(TEXT("Scythe"), Weapon);
            FindFProperty<FObjectProperty>(ARogueCharacter::StaticClass(), TEXT("WeaponDataTable"))->SetObjectPropertyValue_InContainer(Player, Weapons);
            Player->AutoPossessPlayer = EAutoReceiveInput::Disabled; Player->FinishSpawning(Transform); Controller->Possess(Player);
            Player->SetActorTickEnabled(false); Player->GetCharacterMovement()->SetComponentTickEnabled(false); Player->SetActorEnableCollision(false);
            Player->GetAbilitySystemComponent()->CancelAllAbilities();
            auto* Definitions = NewObject<URogueUpgradeDefinitionAsset>(World);
            auto* Table = NewObject<UDataTable>(World); Table->RowStruct = FRogueUpgradeDefinitionRow::StaticStruct();
            FRogueUpgradeDefinitionRow Row; Row.Type = ERogueUpgradeType::Recovery; Row.Title = TEXT("Recovery"); Row.Magnitude = 0.5f; Row.MaxPicks = 1;
            Table->AddRow(TEXT("Recovery"), Row);
            Row.Type = ERogueUpgradeType::Armor; Row.Magnitude = 0.04f; Row.MaxPicks = 10; Table->AddRow(TEXT("Armor"), Row);
            Row.Type = ERogueUpgradeType::AttackSpeed; Row.Magnitude = 0.03f; Row.MaxPicks = 0; Table->AddRow(TEXT("AttackSpeed"), Row);
            Row.Type = ERogueUpgradeType::ScytheLifesteal; Row.Magnitude = 0.05f; Row.FirstMagnitude = 0.2f; Row.MaxMagnitude = 1; Table->AddRow(TEXT("ScytheLifesteal"), Row);
            Definitions->UpgradeDefinitionTable = Table;
            auto* Rules = NewObject<URogueUpgradeRuleAsset>(World);
            Rules->BaseUpgradeTypes = {ERogueUpgradeType::Recovery, ERogueUpgradeType::Armor, ERogueUpgradeType::AttackSpeed, ERogueUpgradeType::AttackPower};
            Upgrades.SetDefinitionAsset(Definitions); Upgrades.SetRuleAsset(Rules);
            *FindFProperty<FStructProperty>(ARogueGameMode::StaticClass(), TEXT("UpgradeSystem"))->ContainerPtrToValuePtr<FRogueUpgradeSystem>(Mode) = Upgrades;
            return Player->IsAlive() && State->IsProgressionReady();
        }
        void Apply(ERogueUpgradeType Type, float Magnitude)
        { FRogueUpgradeOption Option; Option.Type = Type; Option.Magnitude = Magnitude; Player->ApplyUpgrade(Option); }
        TArray<FRogueUpgradeOption> Options() const
        { TArray<FRogueUpgradeOption> Options; Upgrades.BuildRandomUpgradeOptions(Player, 100, Options); return Options; }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueStaleShopPickLimitTest,
    "Rogue.Gameplay.NumericAudit.StaleShopCannotExceedTenArmorCards", RogueNumericAuditTests::Flags)
bool FRogueStaleShopPickLimitTest::RunTest(const FString& Parameters)
{
    using namespace RogueNumericAuditTests; FFixture F; if (!F.Initialize()) { return false; }
    for (int32 I = 0; I < 9; ++I) { F.Apply(ERogueUpgradeType::Armor, 0.04f); }
    F.Player->AddMoney(100);
    FRogueShopSystem Shop; Shop.Configure(100, 2, 1, 60);
    if (!TestTrue(TEXT("Shop opens before the tenth card"), Shop.OpenShop(F.Player, F.Upgrades))) { return false; }
    const int32 Index = Shop.GetOffers().IndexOfByPredicate([](const FRogueShopOffer& Offer) { return Offer.GetUpgrade().Type == ERogueUpgradeType::Armor; });
    if (!TestTrue(TEXT("The ninth-pick state has an armor offer"), Index != INDEX_NONE)) { return false; }
    F.Apply(ERogueUpgradeType::Armor, 0.04f);
    const int32 Money = F.Player->GetMoney(); FRogueUpgradeOption Bought;
    TestFalse(TEXT("A stale quote cannot sell an eleventh armor card"), Shop.TryBuyOffer(Index, Shop.GetOffers()[Index].QuoteId, Shop.GetOffers()[Index].GetCost(), F.Player, F.Upgrades, Bought));
    TestEqual(TEXT("Rejected stale quote does not charge"), F.Player->GetMoney(), Money);
    TestFalse(TEXT("Rejected stale quote stays unpurchased"), Shop.GetOffers()[Index].IsPurchased());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueStalePendingPickLimitTest,
    "Rogue.Gameplay.NumericAudit.StalePendingCardKeepsChoice", RogueNumericAuditTests::Flags)
bool FRogueStalePendingPickLimitTest::RunTest(const FString& Parameters)
{
    using namespace RogueNumericAuditTests; FFixture F; if (!F.Initialize()) { return false; }
    const auto Options = F.Options();
    const auto* Recovery = Options.FindByPredicate([](const FRogueUpgradeOption& Option) { return Option.Type == ERogueUpgradeType::Recovery; });
    if (!TestNotNull(TEXT("A recovery choice exists"), Recovery)) { return false; }
    F.State->OpenUpgradeSelection({*Recovery}); F.Apply(ERogueUpgradeType::Recovery, 0.5f);
    FRogueUpgradeOption Selected;
    TestFalse(TEXT("An already capped pending card cannot be consumed"), F.State->TryConsumePendingUpgrade(0, F.State->GetPendingUpgrades()[0].SelectionId, Selected));
    TestTrue(TEXT("A rejected choice is not lost"), F.State->IsAwaitingUpgradeChoice());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueFinishedLifestealPoolTest,
    "Rogue.Gameplay.NumericAudit.FinishedLifestealLeavesPool", RogueNumericAuditTests::Flags)
bool FRogueFinishedLifestealPoolTest::RunTest(const FString& Parameters)
{
    using namespace RogueNumericAuditTests; FFixture F; if (!F.Initialize()) { return false; }
    F.Apply(ERogueUpgradeType::ScytheCount, 1);
    F.Player->GetRogueAttributeSet()->SetScytheLifestealBonus(1.0f);
    TestFalse(TEXT("A 100 percent lifesteal build is not offered a useless lifesteal card"), F.Options().ContainsByPredicate([](const FRogueUpgradeOption& Option) { return Option.Type == ERogueUpgradeType::ScytheLifesteal; }));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueFinishedAttackSpeedPoolTest,
    "Rogue.Gameplay.NumericAudit.SaturatedAttackSpeedLeavesPoolUntilNewWeapon", RogueNumericAuditTests::Flags)
bool FRogueFinishedAttackSpeedPoolTest::RunTest(const FString& Parameters)
{
    using namespace RogueNumericAuditTests; FFixture F; if (!F.Initialize()) { return false; }
    F.Apply(ERogueUpgradeType::AttackSpeed, 2);
    TestFalse(TEXT("Only cold bullet at minimum cooldown does not offer useless attack speed"), F.Options().ContainsByPredicate([](const FRogueUpgradeOption& Option) { return Option.Type == ERogueUpgradeType::AttackSpeed; }));
    F.Apply(ERogueUpgradeType::ScytheCount, 1);
    TestTrue(TEXT("Owning rotating blades makes attack speed useful again"), F.Options().ContainsByPredicate([](const FRogueUpgradeOption& Option) { return Option.Type == ERogueUpgradeType::AttackSpeed; }));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueQuantityRayPreservationTest,
    "Rogue.Gameplay.NumericAudit.QuantityAddsRaysWithoutLosingAim", RogueNumericAuditTests::Flags)
bool FRogueQuantityRayPreservationTest::RunTest(const FString& Parameters)
{
    using namespace RogueNumericAuditTests; FFixture F; if (!F.Initialize()) { return false; }
    auto* Weapon = F.Player->FindWeapon<ARogueWeapon_Projectile>();
    FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Enemy = F.World->SpawnActor<ARogueEnemy>(ARogueEnemy::StaticClass(), FVector(1000, 0, 200), FRotator::ZeroRotator, Spawn);
    Enemy->ActivatePooledEnemy(nullptr, FVector(1000, 0, 200), FRotator::ZeroRotator);
    FRogueEnemyProfile Profile; Profile.Health = 10000; Enemy->InitializeEnemy(Profile);
    Enemy->SetActorEnableCollision(false); Enemy->SetActorTickEnabled(false);
    TArray<float> Previous;
    for (int32 Count = 1; Count <= 8; ++Count)
    {
        for (TActorIterator<ARogueProjectile> It(F.World); It; ++It) { It->DeactivateToPool(); }
        if (Count > 1) { F.Apply(ERogueUpgradeType::ProjectileCount, 1); }
        Weapon->FireAtTargets({Enemy});
        TArray<float> Angles;
        for (TActorIterator<ARogueProjectile> It(F.World); It; ++It)
            if (!It->IsAvailableInPool()) { Angles.Add(It->GetActorForwardVector().Rotation().Yaw); }
        TestEqual(TEXT("Each quantity step spawns one more real projectile"), Angles.Num(), Count);
        TestTrue(TEXT("Every quantity keeps a projectile aimed at the target"), Angles.ContainsByPredicate([](float Angle) { return FMath::IsNearlyZero(Angle, 0.01f); }));
        for (float Old : Previous)
            TestTrue(TEXT("Quantity growth retains every existing direction"), Angles.ContainsByPredicate([Old](float Angle) { return FMath::IsNearlyEqual(Old, Angle, 0.01f); }));
        Previous = Angles;
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueFractionalExperienceAuditTest,
    "Rogue.Gameplay.NumericAudit.FractionalExperienceIsAccumulated", RogueNumericAuditTests::Flags)
bool FRogueFractionalExperienceAuditTest::RunTest(const FString& Parameters)
{
    using namespace RogueNumericAuditTests; FFixture F; if (!F.Initialize()) { return false; }
    F.Apply(ERogueUpgradeType::ExperienceGain, 0.5f);
    const int32 Before = F.Player->GetCurrentExperience();
    F.Player->AddExperience(1); F.Player->AddExperience(1);
    TestEqual(TEXT("Two one-point pickups at x1.5 grant three XP; fractions are not dropped"), F.Player->GetCurrentExperience() - Before, 3);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueAttackDamageReadoutAuditTest,
    "Rogue.Gameplay.NumericAudit.AttackDamageReadoutMatchesActualHit", RogueNumericAuditTests::Flags)
bool FRogueAttackDamageReadoutAuditTest::RunTest(const FString& Parameters)
{
    using namespace RogueNumericAuditTests; FFixture F; if (!F.Initialize()) { return false; }
    F.Apply(ERogueUpgradeType::AttackPower, 4);
    FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Enemy = F.World->SpawnActor<ARogueEnemy>(ARogueEnemy::StaticClass(), FVector(600, 0, 200), FRotator::ZeroRotator, Spawn);
    Enemy->ActivatePooledEnemy(nullptr, FVector(600, 0, 200), FRotator::ZeroRotator);
    FRogueEnemyProfile Profile; Profile.Health = 100; Enemy->InitializeEnemy(Profile);
    Enemy->SetActorTickEnabled(false); Enemy->GetCharacterMovement()->SetComponentTickEnabled(false);
    const auto* Attributes = Enemy->GetAbilitySystemComponent()->GetSet<URogueAttributeSet>();
    const float Before = Attributes->GetHealth();
    F.Player->FindWeapon<ARogueWeapon_Projectile>()->FireAtTargets({Enemy});
    for (int32 I = 0; I < 60; ++I) { F.TestWorld.TickTestWorld(0.01f); }
    const float Damage = Before - Attributes->GetHealth();
    TestEqual(TEXT("The real projectile deals base 20 plus bonus 4"), Damage, 24.0f);
    TestEqual(TEXT("The public damage query agrees with the actual hit"), F.Player->GetAttackDamage(), Damage);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueNightBudgetTimingAuditTest,
    "Rogue.Gameplay.NumericAudit.NightBudgetFitsTimedWaves", RogueNumericAuditTests::Flags)
bool FRogueNightBudgetTimingAuditTest::RunTest(const FString& Parameters)
{
    const auto* Asset = URogueGameBalanceAsset::GetDefaultGameBalance();
    if (!TestNotNull(TEXT("Current formal balance"), Asset)) { return false; }
    const float Duration = Asset->BattleStage.NightWaveDuration;
    for (int32 Wave = Asset->BattleStage.DayWaveCount + 1; Wave <= 150; ++Wave)
    {
        const int32 Budget = RogueGameModeRules::BuildWaveEnemyCount(Wave, Asset);
        const auto Cadence = RogueGameModeRules::BuildSpawnCadence(Wave, Asset->Spawn.SpawnInterval, Asset);
        for (int32 FPS : {20, 30, 60, 120})
        {
            float Timer = 0; float LastSpawn = 0; int32 Spawned = 0;
            const float Delta = 1.0f / FPS;
            // 与TickSpawning相同的一帧一批、重置间隔模型；假设落点有效且类型名额可用。
            for (int32 Frame = 0; Frame < FMath::FloorToInt(Duration * FPS); ++Frame)
            {
                Timer -= Delta;
                if (Timer <= 0 && Spawned < Budget)
                {
                    Spawned += FMath::Min(Cadence.SpawnBatchSize, Budget - Spawned);
                    Timer = Cadence.NextSpawnDelay; LastSpawn = (Frame + 1) * Delta;
                }
            }
            if (!TestEqual(TEXT("Night budget fits the timed window at target frame rates"), Spawned, Budget)) { return false; }
            if (!TestTrue(TEXT("Night pacing retains at least one second of spawn slack"), LastSpawn <= Duration - 1.0f)) { return false; }
        }
    }
    return true;
}
#endif
