#if WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystemComponent.h"
#include "Enemies/Data/RogueEnemyAssetPaths.h"
#include "AbilitySystem/RogueAbilitySystemLibrary.h"
#include "AbilitySystem/RogueAttributeSet.h"
#include "AbilitySystem/RogueGameplayTags.h"
#include "Combat/Targeting/RogueEnemyTrackerSubsystem.h"
#include "Combat/Weapons/RogueWeapon_HellTower.h"
#include "Enemies/RogueEnemy.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "Framework/RogueGameBalanceAsset.h"
#include "Framework/RogueGameMode.h"
#include "Framework/RogueGameModeRules.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/WorldSettings.h"
#include "Misc/AutomationTest.h"
#include "Player/RogueCharacter.h"
#include "Player/RoguePlayerController.h"
#include "Player/RoguePlayerState.h"
#include "Progression/Meta/RogueUpgradeTreeDefinition.h"
#include "Progression/Upgrades/RogueUpgradeDefinitionAsset.h"
#include "Tests/AutomationCommon.h"

namespace RogueBossUpgradeTests
{
    constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;

    struct FFixture
    {
        FTestConsoleVariable SelfTest{TEXT("Rogue.SelfTest.WeaponUpgradeChain")};
        FTestConsoleVariable Stress{TEXT("Rogue.SelfTest.EnemyStress")};
        FTestWorldWrapper TestWorld;
        UWorld* World = nullptr;
        ARogueGameMode* Mode = nullptr;
        TArray<ARogueCharacter*> Players;
        TArray<ARoguePlayerState*> States;
        URogueGameBalanceAsset* Balance = nullptr;
        UDataTable* Definitions = nullptr;

        bool Initialize(FAutomationTestBase& Test, int32 Count = 1)
        {
            SelfTest.Set(TEXT("0")); Stress.Set(TEXT("0"));
            Balance = LoadObject<URogueGameBalanceAsset>(nullptr, TEXT("/Game/Data/Core/DA_GameBalance.DA_GameBalance"));
            Definitions = LoadObject<UDataTable>(nullptr, TEXT("/Game/Data/Upgrades/DT_UpgradeDefinitions.DT_UpgradeDefinitions"));
            if (!Test.TestNotNull(TEXT("Current game balance loads"), Balance)
                || !Test.TestNotNull(TEXT("Current upgrade table loads"), Definitions)) { return false; }
            if (!TestWorld.CreateTestWorld(EWorldType::Game)) { return false; }
            World = TestWorld.GetTestWorld();
            World->GetWorldSettings()->DefaultGameMode = ARogueGameMode::StaticClass();
            if (!TestWorld.BeginPlayInTestWorld()) { return false; }
            Mode = World->GetAuthGameMode<ARogueGameMode>();
            if (!Test.TestNotNull(TEXT("Authoritative mode exists"), Mode)) { return false; }
            Mode->SetActorTickEnabled(false); // Control waves; combat is stepped explicitly below.
            for (int32 Index = 0; Index < Count; ++Index)
            {
                auto* PC = World->SpawnActor<ARoguePlayerController>();
                if (!Test.TestNotNull(TEXT("Player controller exists"), PC)) { return false; }
                auto* State = PC->GetPlayerState<ARoguePlayerState>();
                if (!Test.TestNotNull(TEXT("Player state exists"), State)) { return false; }
                // No LocalPlayer or connection: the tests cannot write a person's progression save.
                if (!Test.TestNull(TEXT("No local save owner in this fixture"), PC->GetLocalPlayer())) { return false; }
                FRogueProgressionSnapshot Snapshot;
                for (const auto& Node : URogueUpgradeTreeDefinition::GetDefaultDefinition()->GetNodes())
                {
                    if (Node.Kind != ERogueMetaNodeKind::BaseStat) { Snapshot.NodeRanks.Add(Node.NodeId, 1); }
                }
                State->SetProgressionSnapshotForRun(Snapshot);
                const FTransform Transform(FVector(0, Index * 3000, 200));
                auto* Player = World->SpawnActorDeferred<ARogueCharacter>(ARogueCharacter::StaticClass(), Transform,
                    nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
                if (!Test.TestNotNull(TEXT("Player pawn exists"), Player)) { return false; }
                Player->AutoPossessPlayer = EAutoReceiveInput::Disabled;
                Player->FinishSpawning(Transform);
                PC->Possess(Player);
                Player->SetActorTickEnabled(false);
                Player->GetCharacterMovement()->SetComponentTickEnabled(false);
                Player->SetActorEnableCollision(false);
                Player->GetAbilitySystemComponent()->CancelAllAbilities();
                Players.Add(Player); States.Add(State);
            }
            return true;
        }

        ARogueEnemy* SpawnBoss(FAutomationTestBase& Test, const TCHAR* Variant = TEXT("Rifleman"), int32 Wave = 10)
        {
            const FString Path = RogueEnemyAssetPaths::EnemyClassPath(Variant);
            UClass* Class = LoadClass<ARogueEnemy>(nullptr, *Path);
            if (!Test.TestNotNull(*Path, Class)) { return nullptr; }
            FActorSpawnParameters Params;
            Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            auto* Enemy = World->SpawnActor<ARogueEnemy>(Class, FVector(700, 0, 200), FRotator::ZeroRotator, Params);
            if (!Test.TestNotNull(TEXT("Actual enemy Blueprint spawns"), Enemy)) { return nullptr; }
            Enemy->ActivatePooledEnemy(Mode, FVector(700, 0, 200), FRotator::ZeroRotator);
            Enemy->InitializeEnemy(RogueGameModeRules::BuildBossProfile(Wave, Enemy->GetEnemyType(), Balance));
            Enemy->SetActorTickEnabled(false);
            Enemy->GetCharacterMovement()->SetComponentTickEnabled(false);
            Enemy->SetActorEnableCollision(false);
            Enemy->GetAbilitySystemComponent()->CancelAllAbilities();
            return Enemy;
        }

        void Step(float Seconds) { TestWorld.TickTestWorld(Seconds); }

        FRogueUpgradeOption Card(ERogueUpgradeType Type) const
        {
            FRogueUpgradeOption Result; Result.Type = Type;
            for (const auto& Entry : Definitions->GetRowMap())
            {
                const auto* Row = reinterpret_cast<const FRogueUpgradeDefinitionRow*>(Entry.Value);
                if (Row->Type != Type) { continue; }
                Result.Title = Row->Title; Result.Description = Row->Description; Result.Magnitude = Row->Magnitude;
                Result.FirstMagnitude = Row->FirstMagnitude; Result.MaxMagnitude = Row->MaxMagnitude;
                break;
            }
            return Result;
        }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueBossUpgradeIsolationTest,
    "Rogue.Gameplay.BossUpgrade.XPAndEveryCardDoNotDirectlyDamageBoss", RogueBossUpgradeTests::Flags)
bool FRogueBossUpgradeIsolationTest::RunTest(const FString& Parameters)
{
    using namespace RogueBossUpgradeTests;
    FFixture F; if (!F.Initialize(*this, 2)) { return false; }
    const TCHAR* Variants[] = {TEXT("Rifleman"), TEXT("Assault"), TEXT("Juggernaut"), TEXT("Orbiter"),
        TEXT("Skirmisher"), TEXT("Sniper"), TEXT("Shotgunner"), TEXT("Repeater"), TEXT("Ringcaster"),
        TEXT("Marksman"), TEXT("Cannoneer"), TEXT("Scatterer"), TEXT("ShockPillar")};
    TArray<ARogueEnemy*> Bosses;
    TArray<float> Health;
    for (const auto* Variant : Variants)
    {
        auto* Boss = F.SpawnBoss(*this, Variant); if (!Boss) { return false; }
        Bosses.Add(Boss); Health.Add(Boss->GetCurrentHealth());
        TestTrue(TEXT("The configured boss has positive health"), Boss->GetCurrentHealth() > 0);
        TestTrue(TEXT("Player and boss have separate ASCs"), Boss->GetAbilitySystemComponent() != F.Players[0]->GetAbilitySystemComponent());
    }
    const float OtherPlayerPower = F.Players[1]->GetRogueAttributeSet()->GetAttackPowerBonus();
    const int32 BeforeLevel = F.Players[0]->GetPlayerLevel();
    F.Players[0]->AddExperience(F.Players[0]->GetExperienceToNextLevel());
    TestTrue(TEXT("XP really levels the player"), F.Players[0]->GetPlayerLevel() > BeforeLevel);
    TestFalse(TEXT("XP banks rewards without forcing a menu"),F.States[0]->IsAwaitingUpgradeChoice());
    F.States[0]->Server_ToggleUpgradeSelection();
    TestTrue(TEXT("Explicit input opens the earned selection"),F.States[0]->IsAwaitingUpgradeChoice());
    auto CheckBosses = [&]()
    {
        for (int32 Index = 0; Index < Bosses.Num(); ++Index)
        {
            if (!TestTrue(*FString::Printf(TEXT("Boss %d still exists"), Index), IsValid(Bosses[Index]))) { continue; }
            if (!TestNotNull(*FString::Printf(TEXT("%s retains its attributes"), *Bosses[Index]->GetName()), Bosses[Index]->GetRogueAttributeSet())) { continue; }
            TestEqual(TEXT("Boss GAS health is unchanged without a hit"), Bosses[Index]->GetRogueAttributeSet()->GetHealth(), Health[Index]);
            TestFalse(TEXT("No death was scheduled by upgrading"), Bosses[Index]->IsDead());
        }
    };
    F.Step(.02f); CheckBosses();
    // Use current saved magnitudes and the same GameMode entry used by a valid selection.
    for (const auto& Entry : F.Definitions->GetRowMap())
    {
        const auto* Row = reinterpret_cast<const FRogueUpgradeDefinitionRow*>(Entry.Value);
        for (int32 Pick = 0; Pick < 3; ++Pick)
        {
            F.Mode->HandlePlayerUpgradeSelected(F.States[0], F.Card(Row->Type));
            F.Step(.02f); CheckBosses();
        }
    }
    TestEqual(TEXT("A player's upgrades do not change a teammate's attack power"),
        F.Players[1]->GetRogueAttributeSet()->GetAttackPowerBonus(), OtherPlayerPower);
    AddInfo(FString::Printf(TEXT("Checked XP plus %d saved card definitions x3 against %d Blueprint boss variants."),
        F.Definitions->GetRowMap().Num(), Bosses.Num()));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueMenuDamageProtectionTest,
    "Rogue.Gameplay.PlayerMenus.ShopAndUpgradeRemainVulnerable", RogueBossUpgradeTests::Flags)
bool FRogueMenuDamageProtectionTest::RunTest(const FString&)
{
    using namespace RogueBossUpgradeTests;
    FFixture F;
    if (!F.Initialize(*this, 2)) { return false; }
    auto* Player = F.Players[0];
    auto* State = F.States[0];
    auto* Source = F.World->SpawnActor<AActor>();
    auto* ASC = Player->GetAbilitySystemComponent();
    ASC->SetNumericAttributeBase(URogueAttributeSet::GetMaxArmorAttribute(), 10.f);
    ASC->SetNumericAttributeBase(URogueAttributeSet::GetArmorAttribute(), 5.f);
    const auto* Attributes = Player->GetRogueAttributeSet();
    const float InitialHealth = Attributes->GetHealth();
    const float InitialArmor = Attributes->GetArmor();
    if (!TestEqual(TEXT("The protected player starts with armor"), InitialArmor, 5.f)) { return false; }

    F.Mode->TryOpenShop(Player);
    if (!TestTrue(TEXT("The server actually opens the shop"), State->IsShopOpen())) { return false; }
    float Applied = -1.f;
    TestTrue(TEXT("Enemy projectile damage still applies while shopping"),RogueAbilitySystem::ApplyDamageToTarget(
        Source,Player,7.f,RogueGameplayTags::Damage_Source_Projectile,&Applied));
    TestTrue(TEXT("Shopping damage reports the real amount"),Applied>0.f);
    TestTrue(TEXT("Shopping can consume armor"),Attributes->GetArmor()<InitialArmor);
    TestTrue(TEXT("Shopping can consume health after armor"),Attributes->GetHealth()<InitialHealth);
    TestTrue(TEXT("Shopping hits delay armor regeneration normally"),ASC->HasMatchingGameplayTag(RogueGameplayTags::State_RecentlyDamaged));
    TestTrue(TEXT("A teammate without a menu remains vulnerable"), RogueAbilitySystem::ApplyDamageToTarget(
        Source, F.Players[1], 1.f, RogueGameplayTags::Damage_Source_Projectile));

    F.Mode->StartUpgradeSelection(Player);
    F.Mode->StartUpgradeSelection(Player);
    F.Mode->CloseShop(Player);
    State->Server_ToggleUpgradeSelection();
    for (int32 Choice = 0; Choice < 2; ++Choice)
    {
        if (!TestTrue(TEXT("The requested upgrade menu is active"), State->IsAwaitingUpgradeChoice())) { return false; }
        TestTrue(TEXT("Contact damage still applies throughout queued choices"), RogueAbilitySystem::ApplyDamageToTarget(
            Source, Player, 7.f, RogueGameplayTags::Damage_Source_Contact));
        const auto& Pending = State->GetPendingUpgrades();
        if (!TestTrue(TEXT("There is an upgrade to select"), !Pending.IsEmpty())) { return false; }
        State->Server_RequestUpgradeSelection(0, Pending[0].SelectionId);
    }
    TestFalse(TEXT("Submitting the last upgrade closes its menu"), State->IsAwaitingUpgradeChoice());
    TestTrue(TEXT("The next damage applies immediately after the menus close"), RogueAbilitySystem::ApplyDamageToTarget(
        Source, Player, 1.f, RogueGameplayTags::Damage_Source_Area, &Applied));
    TestTrue(TEXT("The next hit removes armor or health"), Applied > 0.f);
    return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRoguePauseMenuDamageProtectionTest,
    "Rogue.Gameplay.PlayerMenus.PauseDamageAndOwnership", RogueBossUpgradeTests::Flags)
bool FRoguePauseMenuDamageProtectionTest::RunTest(const FString&)
{
    using namespace RogueBossUpgradeTests;
    FFixture F; if (!F.Initialize(*this,2)) return false;
    auto* Player=F.Players[0]; auto* State=F.States[0];
    auto* Source=F.World->SpawnActor<AActor>();
    auto* ASC=Player->GetAbilitySystemComponent();
    ASC->SetNumericAttributeBase(URogueAttributeSet::GetMaxArmorAttribute(),10.f);
    ASC->SetNumericAttributeBase(URogueAttributeSet::GetArmorAttribute(),5.f);
    const float Health=Player->GetCurrentHealth();
    State->Server_SetPauseMenuOpen(true);
    TestTrue(TEXT("Owning player opens server menu protection"),State->IsPauseMenuOpen());
    float Applied=-1;
    const FGameplayTag Tags[] = {RogueGameplayTags::Damage_Source_Projectile,RogueGameplayTags::Damage_Source_Contact,RogueGameplayTags::Damage_Source_Area};
    for (const auto& Tag : Tags)
        TestFalse(TEXT("Pause/settings blocks all shared damage paths"),RogueAbilitySystem::ApplyDamageToTarget(Source,Player,7.f,Tag,&Applied));
    TestEqual(TEXT("Blocked damage reports zero"),Applied,0.f);
    TestEqual(TEXT("Health stays intact"),Player->GetCurrentHealth(),Health);
    TestEqual(TEXT("Armor stays intact"),ASC->GetSet<URogueAttributeSet>()->GetArmor(),5.f);
    TestFalse(TEXT("Blocked hits do not delay armor regeneration"),ASC->HasMatchingGameplayTag(RogueGameplayTags::State_RecentlyDamaged));
    TestTrue(TEXT("Teammate remains vulnerable"),RogueAbilitySystem::ApplyDamageToTarget(Source,F.Players[1],1.f,RogueGameplayTags::Damage_Source_Area));
    F.Mode->StartUpgradeSelection(Player);
    TestFalse(TEXT("A reward earned while paused does not force another menu"),State->IsAwaitingUpgradeChoice());
    State->Server_SetPauseMenuOpen(false);
    State->Server_ToggleUpgradeSelection();
    TestTrue(TEXT("A selection opened after pause remains vulnerable"),RogueAbilitySystem::ApplyDamageToTarget(Source,Player,1.f,RogueGameplayTags::Damage_Source_Area));
    State->CloseUpgradeSelection();
    TestTrue(TEXT("Damage resumes after the last menu closes"),RogueAbilitySystem::ApplyDamageToTarget(Source,Player,1.f,RogueGameplayTags::Damage_Source_Area));
    State->Server_SetPauseMenuOpen(true);
    auto* PC=Player->GetController();
    PC->UnPossess();
    TestFalse(TEXT("Unpossessing clears protection"),State->IsPauseMenuOpen());
    State->Server_SetPauseMenuOpen(true);
    TestFalse(TEXT("Cannot acquire protection without a live pawn"),State->IsPauseMenuOpen());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueBossUpgradeContinuousDamageTest,
    "Rogue.Gameplay.BossUpgrade.ActiveBeamContinuesWithoutUpgradeBurst", RogueBossUpgradeTests::Flags)
bool FRogueBossUpgradeContinuousDamageTest::RunTest(const FString& Parameters)
{
    using namespace RogueBossUpgradeTests;
    FFixture F; if (!F.Initialize(*this)) { return false; }
    auto* Boss = F.SpawnBoss(*this); if (!Boss) { return false; }
    auto* Table = LoadObject<UDataTable>(nullptr, TEXT("/Game/Data/Weapons/DT_WeaponConfig.DT_WeaponConfig"));
    if (!TestNotNull(TEXT("Current weapon table loads"), Table)) { return false; }
    const auto* Row = Table->FindRow<FRogueWeaponTableRow>(TEXT("HellTower"), TEXT("BossUpgrade"));
    if (!TestNotNull(TEXT("Current continuous beam configuration exists"), Row)) { return false; }
    auto* Tower = F.World->SpawnActor<ARogueWeapon_HellTower>();
    Tower->InitializeWeapon(F.Players[0], *Row);
    Tower->GetConfig().Count = 1; // Acquire this one weapon; all other attacks remain disabled.
    F.World->GetSubsystem<URogueEnemyTrackerSubsystem>()->RegisterEnemy(Boss);
    float LastDamage = 0;
    const float Interval = Tower->GetEffectiveDamageTickInterval();
    for (int32 Tick = 0; Tick < 40; ++Tick)
    {
        const float Before = Boss->GetCurrentHealth();
        Tower->WeaponTick(Interval);
        LastDamage = Before - Boss->GetCurrentHealth();
    }
    const float BeforeUpgrade = Boss->GetCurrentHealth();
    const float OldRamp = Tower->GetEffectiveDamageRampPerTick();
    F.Players[0]->AddExperience(F.Players[0]->GetExperienceToNextLevel());
    TestEqual(TEXT("Opening XP selection causes no extra beam hit"), Boss->GetCurrentHealth(), BeforeUpgrade);
    F.Mode->HandlePlayerUpgradeSelected(F.States[0], F.Card(ERogueUpgradeType::AttackPower));
    TestEqual(TEXT("Applying the attack card causes no extra beam hit"), Boss->GetCurrentHealth(), BeforeUpgrade);
    Tower->WeaponTick(Interval);
    const float FirstDamage = BeforeUpgrade - Boss->GetCurrentHealth();
    TestTrue(TEXT("Next scheduled tick continues the existing ramp once"), FMath::IsNearlyEqual(FirstDamage, LastDamage + OldRamp, .02f));
    const float BeforeNext = Boss->GetCurrentHealth();
    Tower->WeaponTick(Interval);
    const float NextDamage = BeforeNext - Boss->GetCurrentHealth();
    TestTrue(TEXT("The card changes subsequent ramp by its configured value, without duplicate ticks"),
        FMath::IsNearlyEqual(NextDamage - FirstDamage, Tower->GetEffectiveDamageRampPerTick(), .02f));
    TestFalse(TEXT("Boss survives the controlled level-up and two hits"), Boss->IsDead());
    AddInfo(FString::Printf(TEXT("Boss HP %.2f -> %.2f; next hits %.2f, %.2f; ramp %.2f -> %.2f."),
        BeforeUpgrade, Boss->GetCurrentHealth(), FirstDamage, NextDamage, OldRamp, Tower->GetEffectiveDamageRampPerTick()));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueBossDeathRewardOrderTest,
    "Rogue.Gameplay.BossUpgrade.DeathBanksRewardUntilRequested", RogueBossUpgradeTests::Flags)
bool FRogueBossDeathRewardOrderTest::RunTest(const FString& Parameters)
{
    using namespace RogueBossUpgradeTests;
    FFixture F; if (!F.Initialize(*this)) { return false; }
    auto* Boss = F.SpawnBoss(*this); if (!Boss) { return false; }
    const int32 Level = F.Players[0]->GetPlayerLevel();
    TestFalse(TEXT("No selection before the lethal hit"), F.States[0]->IsAwaitingUpgradeChoice());
    const float HP = Boss->GetCurrentHealth();
    TestTrue(TEXT("A real lethal GAS hit is applied"), RogueAbilitySystem::ApplyDamageToTarget(F.Players[0], Boss, HP + 1,
        RogueGameplayTags::Damage_Source_Projectile));
    TestEqual(TEXT("Health reaches zero before reward UI opens"), Boss->GetRogueAttributeSet()->GetHealth(), 0.f);
    TestFalse(TEXT("Reward has not opened inside the damage call"), F.States[0]->IsAwaitingUpgradeChoice());
    F.Step(.02f);
    TestTrue(TEXT("Deferred boss death finishes"), Boss->IsDead());
    TestFalse(TEXT("Boss death does not interrupt combat with a menu"),F.States[0]->IsAwaitingUpgradeChoice());
    TestTrue(TEXT("Boss bonus selections are banked"),F.States[0]->GetPendingUpgradeCount()>0);
    F.States[0]->Server_ToggleUpgradeSelection();
    TestTrue(TEXT("The boss reward opens when requested"),F.States[0]->IsAwaitingUpgradeChoice());
    TestEqual(TEXT("No XP level-up was necessary to show that selection"), F.Players[0]->GetPlayerLevel(), Level);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueBossBlueprintHealthTest,
    "Rogue.Gameplay.BossUpgrade.AllBlueprintBossesInitializeGASHealth", RogueBossUpgradeTests::Flags)
bool FRogueBossBlueprintHealthTest::RunTest(const FString& Parameters)
{
    using namespace RogueBossUpgradeTests;
    FFixture F; if (!F.Initialize(*this)) { return false; }
    const TCHAR* Variants[] = {TEXT("Rifleman"), TEXT("Assault"), TEXT("Juggernaut"), TEXT("Orbiter"),
        TEXT("Skirmisher"), TEXT("Sniper"), TEXT("Shotgunner"), TEXT("Repeater"), TEXT("Ringcaster"),
        TEXT("Marksman"), TEXT("Cannoneer"), TEXT("Scatterer"), TEXT("ShockPillar")};
    for (const auto* Variant : Variants)
    {
        auto* Boss = F.SpawnBoss(*this, Variant); if (!Boss) { return false; }
        auto* ASC = Boss->GetAbilitySystemComponent();
        if (!TestNotNull(TEXT("Boss ASC exists"), ASC)) { continue; }
        const auto* RegisteredSet = ASC->GetSet<URogueAttributeSet>();
        const float Mirror = Boss->GetCurrentHealth();
        const float Actual = ASC->GetNumericAttribute(URogueAttributeSet::GetHealthAttribute());
        const float ActualMax = ASC->GetNumericAttribute(URogueAttributeSet::GetMaxHealthAttribute());
        AddInfo(FString::Printf(TEXT("%s: accessor=%s registered=%s defaultSubobject=%s mirror=%.2f GAS=%.2f/%.2f"),
            Variant, *GetNameSafe(Boss->GetRogueAttributeSet()), *GetNameSafe(RegisteredSet),
            *GetNameSafe(Boss->GetDefaultSubobjectByName(TEXT("AttributeSet"))), Mirror, Actual, ActualMax));
        TestNotNull(*FString::Printf(TEXT("%s has an accessible attribute set"), Variant), Boss->GetRogueAttributeSet());
        TestTrue(*FString::Printf(TEXT("%s GAS health matches its boss profile"), Variant), FMath::IsNearlyEqual(Actual, Mirror));
        TestTrue(*FString::Printf(TEXT("%s GAS max health matches its boss profile"), Variant), FMath::IsNearlyEqual(ActualMax, Boss->GetMaxHealth()));
        RogueAbilitySystem::ApplyDamageToTarget(F.Players[0], Boss, 1.f, RogueGameplayTags::Damage_Source_Projectile);
        F.Step(.02f);
        AddInfo(FString::Printf(TEXT("%s after 1 damage: GAS=%.2f mirror=%.2f dead=%d pooled=%d"),
            Variant, ASC->GetNumericAttribute(URogueAttributeSet::GetHealthAttribute()), Boss->GetCurrentHealth(), Boss->IsDead(), Boss->IsAvailableInPool()));
        TestFalse(*FString::Printf(TEXT("%s survives one point of damage"), Variant), Boss->IsDead());
        Boss->DeactivateToPool();
    }
    return true;
}

#endif
