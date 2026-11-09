#if WITH_DEV_AUTOMATION_TESTS
#include "Enemies/Components/RogueBossSkillComponent.h"
#include "AbilitySystem/Abilities/RogueBossSkillAbility.h"
#include "AbilitySystem/RogueAbilitySystemLibrary.h"
#include "AbilitySystem/RogueGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/RogueAttributeSet.h"
#include "Enemies/RogueEnemy.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/RogueGameMode.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/AutomationTest.h"
#include "Player/RogueCharacter.h"
#include "Player/RoguePlayerController.h"
#include "Player/RoguePlayerState.h"
#include "Progression/Meta/RogueProgressionSaveGame.h"
#include "Combat/Weapons/RogueWeaponBase.h"
#include "Combat/Projectiles/RogueBossAttackActor.h"
#include "RogueTestWorld.h"
#include "World/RogueArenaBoundary.h"
#include "Tests/AutomationCommon.h"
#include "UObject/UnrealType.h"

namespace RogueBossSkillTests
{
    constexpr auto Flags=EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter;
    struct FFixture
    {
        FTestConsoleVariable SelfTest{TEXT("Rogue.SelfTest.WeaponUpgradeChain")};
        FTestConsoleVariable Stress{TEXT("Rogue.SelfTest.EnemyStress")};
        FTestWorldWrapper Scope; UWorld* World=nullptr; ARogueCharacter* Player=nullptr; UDataTable* Skills=nullptr;
        bool Init(FRogueBossSkillRow Row)
        {
            SelfTest.Set(TEXT("0")); Stress.Set(TEXT("0"));
            if (!Scope.CreateTestWorld(EWorldType::Game)) return false;
            World=Scope.GetTestWorld();World->GetWorldSettings()->DefaultGameMode=ARogueGameMode::StaticClass();
            if (!Scope.BeginPlayInTestWorld()) return false;
            World->GetAuthGameMode()->SetActorTickEnabled(false);
            auto* PC=World->SpawnActor<ARoguePlayerController>();
            const FTransform Transform(FVector(600,0,200));
            Player=World->SpawnActorDeferred<ARogueCharacter>(ARogueCharacter::StaticClass(),Transform,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
            auto* Weapons=NewObject<UDataTable>(World); Weapons->RowStruct=FRogueWeaponTableRow::StaticStruct();
            FindFProperty<FObjectProperty>(ARogueCharacter::StaticClass(),TEXT("WeaponDataTable"))->SetObjectPropertyValue_InContainer(Player,Weapons);
            Player->AutoPossessPlayer=EAutoReceiveInput::Disabled;Player->FinishSpawning(Transform);
            PC->GetPlayerState<ARoguePlayerState>()->SetProgressionSnapshotForRun(FRogueProgressionSnapshot());
            PC->Possess(Player);Player->SetActorTickEnabled(false);Player->SetActorEnableCollision(false);
            Player->GetCharacterMovement()->SetComponentTickEnabled(false);Player->GetAbilitySystemComponent()->CancelAllAbilities();
            Skills=NewObject<UDataTable>(World);Skills->RowStruct=FRogueBossSkillRow::StaticStruct();
            Row.AbilityClass=URogueBossSkillAbility::StaticClass();Row.InitialDelay=0;
            Skills->AddRow(TEXT("First"),Row);Skills->AddRow(TEXT("Second"),Row);
            return true;
        }
        ARogueEnemy* Enemy(bool Boss=true,ERogueEnemyType Type=ERogueEnemyType::Rifleman)
        {
            FActorSpawnParameters P;P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            auto* E=World->SpawnActor<ARogueEnemy>(ARogueEnemy::StaticClass(),FVector(0,0,200),FRotator::ZeroRotator,P);
            E->BossSkills->bAutoUseSkills=false;
            for (FName Name:{FName(TEXT("First")),FName(TEXT("Second"))})
            { FDataTableRowHandle H;H.DataTable=Skills;H.RowName=Name;E->BossSkills->SkillSlots.Add(H); }
            E->ActivatePooledEnemy(nullptr,FVector(0,0,200),FRotator::ZeroRotator);
            InitEnemy(E,Boss,Type);return E;
        }
        void InitEnemy(ARogueEnemy* E,bool Boss,ERogueEnemyType Type=ERogueEnemyType::Rifleman)
        {
            FRogueEnemyProfile P;P.bIsBoss=Boss;P.EnemyType=Type;P.Health=1000;P.Damage=10;P.Speed=200;
            E->InitializeEnemy(P);E->SetActorTickEnabled(false);E->SetActorEnableCollision(false);
            E->GetCharacterMovement()->SetComponentTickEnabled(false);
        }
        void Step(float Seconds) { for (int32 I=0;I<FMath::CeilToInt(Seconds/.01f);++I) Scope.TickTestWorld(.01f); }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBossSlotsRoleTest,"Rogue.Gameplay.BossSkills.BossOnlyGrantAndIndependentOrdinaryAbilities",RogueBossSkillTests::Flags)
bool FBossSlotsRoleTest::RunTest(const FString&)
{
    using namespace RogueBossSkillTests;FFixture F;if(!F.Init(FRogueBossSkillRow()))return false;
    auto* Normal=F.Enemy(false);auto* Boss=F.Enemy();auto* Healer=F.Enemy(true,ERogueEnemyType::AuraHealer);
    TestNull(TEXT("Normal enemy receives no Boss ability"),Normal->GetAbilitySystemComponent()->FindAbilitySpecFromClass(URogueBossSkillAbility::StaticClass()));
    TestFalse(TEXT("Normal enemy cannot activate a slot"),Normal->BossSkills->TryStartSkill(0));
    auto* Spec=Boss->GetAbilitySystemComponent()->FindAbilitySpecFromClass(URogueBossSkillAbility::StaticClass());
    if(!TestNotNull(TEXT("Boss receives its configured GAS skill"),Spec))return false;
    TestFalse(TEXT("Direct GAS activation cannot bypass coordinator eligibility"),Boss->GetAbilitySystemComponent()->TryActivateAbility(Spec->Handle));
    TestTrue(TEXT("Boss slot activates through coordinator"),Boss->BossSkills->TryStartSkill(0));
    TestTrue(TEXT("Charging suppresses ordinary attack entry"),Boss->AreNormalAttacksSuppressed());
    TestFalse(TEXT("Blueprint ordinary fire entry is blocked during exclusive skill"),Boss->ExecuteRangedAttack());
    TestTrue(TEXT("Movement is paused during the configured skill"),Boss->IsBossSkillBlockingMovement());
    TestTrue(TEXT("Healer Boss can receive skills independently of ordinary healer exclusions"),Healer->BossSkills->TryStartSkill(0));
    auto* Flyer=F.Enemy(true,ERogueEnemyType::SkyMoth);
    Flyer->SetActorTickEnabled(true);
    const FVector HoverPosition=Flyer->GetActorLocation();
    TestTrue(TEXT("Flying boss starts charging"),Flyer->BossSkills->TryStartSkill(0));
    F.Step(.24f);
    TestTrue(TEXT("Charging freezes flight bob and horizontal movement"),Flyer->GetActorLocation().Equals(HoverPosition,.1f));
    Boss->BossSkills->CancelCurrentSkill();
    TestFalse(TEXT("Cancellation releases ordinary attack gate"),Boss->AreNormalAttacksSuppressed());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBossChargeBurstTest,"Rogue.Gameplay.BossSkills.ChargeBurstRecoveryAndPerSlotCooldown",RogueBossSkillTests::Flags)
bool FBossChargeBurstTest::RunTest(const FString&)
{
    using namespace RogueBossSkillTests;FRogueBossSkillRow R;R.ChargeSeconds=.12f;R.DurationSeconds=.26f;R.PulseInterval=.1f;R.RecoverySeconds=.12f;R.Cooldown=.5f;
    FFixture F;if(!F.Init(R))return false;auto* E=F.Enemy();auto* C=E->BossSkills.Get();
    TestTrue(TEXT("Real GAS ability enters its charge"),C->TryStartSkill(0));
    TestFalse(TEXT("Second slot cannot interrupt an active cast"),C->TryStartSkill(1));
    F.Step(.08f);TestEqual(TEXT("No attack during warning"),C->GetSkillState().PulseIndex,0);
    F.Step(.07f);TestEqual(TEXT("First pulse after charge"),C->GetSkillState().PulseIndex,1);
    F.Step(.11f);TestEqual(TEXT("Second pulse is delayed"),C->GetSkillState().PulseIndex,2);
    F.Step(.11f);TestEqual(TEXT("Every configured pulse executes"),C->GetSkillState().PulseIndex,3);
    TestEqual(TEXT("The cast stays active after its third pulse until the deadline"),C->GetSkillState().Phase,ERogueBossSkillPhase::Firing);
    F.Step(.06f);
    TestEqual(TEXT("Duration expiry enters recovery"),C->GetSkillState().Phase,ERogueBossSkillPhase::Recovery);
    F.Step(.15f);TestFalse(TEXT("Recovery finishes the ability"),C->IsCasting());
    TestFalse(TEXT("Successful completion is distinguished from cancellation"),C->GetSkillState().bCancelled);
    TestTrue(TEXT("Cooldown starts after completion"),C->GetSkillCooldownRemaining(0)>.3f);
    TestFalse(TEXT("Used slot respects cooldown"),C->TryStartSkill(0));
    TestTrue(TEXT("Another slot has its own cooldown"),C->TryStartSkill(1));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBossCancelLifecycleTest,"Rogue.Gameplay.BossSkills.CancelChargeAndBurstOnPoolReuseOrDeath",RogueBossSkillTests::Flags)
bool FBossCancelLifecycleTest::RunTest(const FString&)
{
    using namespace RogueBossSkillTests;FRogueBossSkillRow R;R.ChargeSeconds=.15f;R.DurationSeconds=.8f;R.PulseInterval=.1f;
    FFixture F;if(!F.Init(R))return false;auto* E=F.Enemy();auto* C=E->BossSkills.Get();
    C->TryStartSkill(0);const int32 OldId=C->GetSkillState().CastId;
    E->DeactivateToPool();F.Step(.5f);
    TestFalse(TEXT("Pooling immediately ends charge"),C->IsCasting());
    TestEqual(TEXT("Cancelled charge never fires later"),C->GetSkillState().PulseIndex,0);
    E->ActivatePooledEnemy(nullptr,FVector(0,0,200),FRotator::ZeroRotator);F.InitEnemy(E,false);
    TestFalse(TEXT("A reused normal enemy has no Boss slots"),C->TryStartSkill(0));
    TestNull(TEXT("Old Boss GAS specs are removed"),E->GetAbilitySystemComponent()->FindAbilitySpecFromClass(URogueBossSkillAbility::StaticClass()));
    E->ActivatePooledEnemy(nullptr,FVector(0,0,200),FRotator::ZeroRotator);F.InitEnemy(E,true);
    C->TryStartSkill(0);F.Step(.18f);
    TestEqual(TEXT("New lifecycle starts a fresh burst"),C->GetSkillState().PulseIndex,1);
    TestTrue(TEXT("Cast identifiers distinguish pooled lifecycles"),C->GetSkillState().CastId>OldId);
    E->GetAbilitySystemComponent()->SetNumericAttributeBase(URogueAttributeSet::GetHealthAttribute(),0);
    F.Step(.5f);
    TestFalse(TEXT("Zero-health enemy cannot continue a sustained skill"),C->IsCasting());
    TestEqual(TEXT("Remaining shots are not emitted after death"),C->GetSkillState().PulseIndex,1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBossHitchAndTargetTest,"Rogue.Gameplay.BossSkills.NoHitchCatchUpAndTargetLossCancellation",RogueBossSkillTests::Flags)
bool FBossHitchAndTargetTest::RunTest(const FString&)
{
    using namespace RogueBossSkillTests;FRogueBossSkillRow R;R.ChargeSeconds=0;R.DurationSeconds=3.f;R.PulseInterval=.1f;
    FFixture F;if(!F.Init(R))return false;auto* E=F.Enemy();auto* C=E->BossSkills.Get();
    TestTrue(TEXT("Zero-charge sustained skill starts"),C->TryStartSkill(0));
    TestEqual(TEXT("First shot is immediate"),C->GetSkillState().PulseIndex,1);
    F.Step(.02f); // 将新建的 pending timer 转入运行中，再模拟一次长帧。
    F.Scope.TickTestWorld(1.f);
    TestEqual(TEXT("One long frame does not dump the remaining volley"),C->GetSkillState().PulseIndex,2);
    F.Player->GetAbilitySystemComponent()->SetNumericAttributeBase(URogueAttributeSet::GetHealthAttribute(),0);
    F.Step(.2f);TestFalse(TEXT("A dead or downed target cancels sustained fire"),C->IsCasting());
    TestEqual(TEXT("No later pulse targets a dead player"),C->GetSkillState().PulseIndex,2);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBossOwnedResourceTest,"Rogue.Gameplay.BossSkills.ForcedStopCleansResourcesAndInstantSkillCompletes",RogueBossSkillTests::Flags)
bool FBossOwnedResourceTest::RunTest(const FString&)
{
    using namespace RogueBossSkillTests;FFixture F;if(!F.Init(FRogueBossSkillRow()))return false;
    auto* E=F.Enemy();auto* C=E->BossSkills.Get();C->TryStartSkill(0);
    auto* Spec=E->GetAbilitySystemComponent()->FindAbilitySpecFromClass(URogueBossSkillAbility::StaticClass());
    auto* Ability=Spec?Cast<URogueBossSkillAbility>(Spec->GetPrimaryInstance()):nullptr;
    if(!TestNotNull(TEXT("Real GAS skill instance exists"),Ability))return false;
    auto* Area=F.World->SpawnActor<AActor>();Ability->TrackActorUntilSkillEnds(Area);
    Ability->SetCanBeCanceled(false);C->CancelCurrentSkill();
    TestFalse(TEXT("Lifecycle stop overrides ordinary uninterruptible setting"),C->IsCasting());
    TestTrue(TEXT("Owned sustained area is destroyed on stop"),Area->IsActorBeingDestroyed());
    F.Step(.3f);TestEqual(TEXT("Forced stop does not leave the charge timer running"),C->GetSkillState().PulseIndex,0);
    FRogueBossSkillRow Channel;Channel.AbilityClass=URogueBossSkillAbility::StaticClass();Channel.InitialDelay=0;
    Channel.ChargeSeconds=.1f;Channel.DurationSeconds=.3f;Channel.RecoverySeconds=.2f;
    F.Skills->AddRow(TEXT("First"),Channel);F.InitEnemy(E,true);C->TryStartSkill(0);
    auto* Beam=F.World->SpawnActor<AActor>();
    Spec=E->GetAbilitySystemComponent()->FindAbilitySpecFromClass(URogueBossSkillAbility::StaticClass());
    CastChecked<URogueBossSkillAbility>(Spec->GetPrimaryInstance())->TrackActorUntilSkillEnds(Beam);
    F.Step(.2f);TestEqual(TEXT("A channel can hold after its last pulse"),C->GetSkillState().Phase,ERogueBossSkillPhase::Firing);
    TestFalse(TEXT("Owned area remains through the active channel"),Beam->IsActorBeingDestroyed());
    F.Step(.25f);TestEqual(TEXT("Channel moves into recovery"),C->GetSkillState().Phase,ERogueBossSkillPhase::Recovery);
    TestTrue(TEXT("Sustained area is removed before the recovery window"),Beam->IsActorBeingDestroyed());
    C->CancelCurrentSkill();
    FRogueBossSkillRow Instant;Instant.AbilityClass=URogueBossSkillAbility::StaticClass();Instant.InitialDelay=0;
    Instant.ChargeSeconds=0;Instant.DurationSeconds=0;Instant.RecoverySeconds=0;F.Skills->AddRow(TEXT("First"),Instant);
    F.InitEnemy(E,true);
    TestTrue(TEXT("Synchronous zero-duration skill is considered a successful activation"),C->TryStartSkill(0));
    TestFalse(TEXT("Instant skill leaves no occupied slot"),C->IsCasting());
    TestEqual(TEXT("Instant skill still executes exactly one pulse"),C->GetSkillState().PulseIndex,1);
    TestFalse(TEXT("Instant completion is not marked cancelled"),C->GetSkillState().bCancelled);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBossEmergencySkillsTest,"Rogue.Gameplay.BossSkills.NineSkills.HealOnceAndConvertExactlyThreeTimes",RogueBossSkillTests::Flags)
bool FBossEmergencySkillsTest::RunTest(const FString&)
{
    using namespace RogueBossSkillTests;
    FFixture F; if (!F.Init(FRogueBossSkillRow())) return false;
    const auto* Catalog=LoadObject<UDataTable>(nullptr,TEXT("/Game/Data/Enemies/DT_BossSkills.DT_BossSkills"));
    if (!TestNotNull(TEXT("Shipped catalog"),Catalog)) return false;
    auto* E=F.Enemy(); auto* C=E->BossSkills.Get();
    E->DeactivateToPool();
    const FName Heal(TEXT("LastBloom")),Ward(TEXT("DiamondReversal"));
    for (const FName Name:{Heal,Ward})
    {
        const auto* Actual=Catalog->FindRow<FRogueBossSkillRow>(Name,TEXT("Low-health skill test"));
        if (!TestNotNull(TEXT("Real skill definition"),Actual)) return false;
        auto Row=*Actual; Row.Cooldown=0; Row.RecoverySeconds=.05f;
        Row.DurationSeconds=Name==Ward ? .35f : .2f;
        F.Skills->AddRow(Name,Row);
        FDataTableRowHandle H; H.DataTable=F.Skills;H.RowName=Name;C->SkillSlots.Add(H);
    }
    E->ActivatePooledEnemy(nullptr,FVector(0,0,200),FRotator::ZeroRotator); F.InitEnemy(E,true);
    auto* ASC=E->GetAbilitySystemComponent();
    const float Max=E->GetMaxHealth();
    auto SetHP=[&](float Fraction){ ASC->SetNumericAttributeBase(URogueAttributeSet::GetHealthAttribute(),Max*Fraction); };
    TestFalse(TEXT("Cannot heal or convert at full health"),C->TryStartSkill(2) || C->TryStartSkill(3));
    SetHP(.5f); TestFalse(TEXT("Half health does not satisfy strict ward threshold"),C->TryStartSkill(3));
    SetHP(.26f); TestTrue(TEXT("Real heal can start below a third"),C->TryStartSkill(2));
    F.Step(2.6f); TestNearlyEqual(TEXT("Heals all the way to the real maximum"),E->GetCurrentHealth(),Max,.01f);
    SetHP(.24f); TestFalse(TEXT("Spent heal cannot start again"),C->TryStartSkill(2));
    for (int32 Use=0; Use<3; ++Use)
    {
        SetHP(.42f);
        TestTrue(TEXT("One of three ward uses starts"),C->TryStartSkill(3));F.Step(.29f);
        TestTrue(TEXT("Ward grants inversion only during release"),ASC->HasMatchingGameplayTag(RogueGameplayTags::State_BossDamageInversion));
        const float Before=E->GetCurrentHealth();
        RogueAbilitySystem::ApplyDamageToTarget(F.Player,E,25,RogueGameplayTags::Damage_Source_Projectile);
        TestNearlyEqual(TEXT("A genuine player damage GE restores the same amount"),E->GetCurrentHealth(),Before+25,.01f);
        F.Step(.65f);TestFalse(TEXT("Inversion tag is cleared before the next cast"),ASC->HasMatchingGameplayTag(RogueGameplayTags::State_BossDamageInversion));
    }
    SetHP(.42f);TestFalse(TEXT("Fourth ward attempt is rejected"),C->TryStartSkill(3));
    const float Before=E->GetCurrentHealth();
    RogueAbilitySystem::ApplyDamageToTarget(F.Player,E,25,RogueGameplayTags::Damage_Source_Projectile);
    TestTrue(TEXT("Damage after ward reduces health again"),E->GetCurrentHealth()<Before);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBossBoundaryLaserTest,"Rogue.Gameplay.BossSkills.NineSkills.BeamReachesOuterBoundaryThroughCover",RogueBossSkillTests::Flags)
bool FBossBoundaryLaserTest::RunTest(const FString&)
{
    using namespace RogueBossSkillTests;
    FFixture F;
    if (!F.Init(FRogueBossSkillRow())) return false;
    auto* Boundary=F.World->SpawnActor<ARogueArenaBoundary>();
    FRogueBoundaryLoop Hole;
    Hole.Points={{300,-100,0},{390,-100,0},{390,100,0},{300,100,0}};
    FRogueBoundaryLoop Outer;
    Outer.Points={{-1200,-800,0},{1200,-800,0},{1200,800,0},{-1200,800,0}};
    Boundary->Loops={Hole,Outer};
    Boundary->RebuildWalls();
    RogueTestWorld::StaticBox(F.World, FVector(220,0,200), FVector(28,110,120),
        FRotator::ZeroRotator, ECollisionEnabled::QueryOnly);

    const FVector Origin(0,0,200);
    double Distance=0.;
    TestTrue(TEXT("The ray intersects the outer outline"),Boundary->TryGetOuterRayDistance(Origin,FVector::ForwardVector,Distance));
    TestNearlyEqual(TEXT("The nearer closed water hole is skipped"),Distance,1200.,.1);

    auto* Boss=F.Enemy();
    auto* Beam=F.World->SpawnActor<ARogueBossAttackActor>();
    FRogueBossAttackSettings Attack;
    Attack.BeamRange=140.f; // Only used when a level has no authored boundary.
    Attack.BeamHalfWidth=55.f;
    const float Health=F.Player->GetAbilitySystemComponent()->GetNumericAttribute(URogueAttributeSet::GetHealthAttribute());
    Beam->InitializeAttack(Boss,ERogueBossAttackShape::Beam,Attack,Origin,FVector::ForwardVector,5.f,5.f,true);
    F.Step(.1f);
    TestTrue(TEXT("A player behind a prop and beyond the fallback length receives damage"),F.Player->GetAbilitySystemComponent()->GetNumericAttribute(URogueAttributeSet::GetHealthAttribute())<Health);
    const float AfterFirstHit=F.Player->GetAbilitySystemComponent()->GetNumericAttribute(URogueAttributeSet::GetHealthAttribute());
    F.Player->SetActorLocation(FVector(1350,0,200));
    F.Step(.45f);
    TestNearlyEqual(TEXT("A player beyond the map boundary receives no further damage"),F.Player->GetAbilitySystemComponent()->GetNumericAttribute(URogueAttributeSet::GetHealthAttribute()),AfterFirstHit,.01f);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBossFlameWaveGrowthTest,"Rogue.Gameplay.BossSkills.NineSkills.FlameWaveGrowsWithTravel",RogueBossSkillTests::Flags)
bool FBossFlameWaveGrowthTest::RunTest(const FString&)
{
    using namespace RogueBossSkillTests;
    FFixture F;
    if (!F.Init(FRogueBossSkillRow())) return false;
    auto* Boss=F.Enemy();
    auto* Wave=F.World->SpawnActor<ARogueBossAttackActor>();
    FRogueBossAttackSettings Attack;
    Attack.Speed=1000.f;
    Attack.WaveHalfWidth=100.f;
    Attack.WaveGrowthDistance=1000.f;
    Attack.WaveMaxScale=2.f;
    F.Player->SetActorLocation(FVector(190,180,200));
    const float Health=F.Player->GetAbilitySystemComponent()->GetNumericAttribute(URogueAttributeSet::GetHealthAttribute());
    Wave->InitializeAttack(Boss,ERogueBossAttackShape::FlameWave,Attack,FVector(0,0,200),FVector::ForwardVector,5.f,30.f,true);
    F.Step(.31f);
    TestNearlyEqual(TEXT("The narrow wave misses a player off to the side"),F.Player->GetAbilitySystemComponent()->GetNumericAttribute(URogueAttributeSet::GetHealthAttribute()),Health,.01f);
    F.Player->SetActorLocation(FVector(920,180,200));
    F.Step(.78f);
    TestTrue(TEXT("The grown wave hits the same lateral offset farther downrange"),F.Player->GetAbilitySystemComponent()->GetNumericAttribute(URogueAttributeSet::GetHealthAttribute())<Health);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBossDurationDeadlineTest,"Rogue.Gameplay.BossSkills.DurationDeadlineAndNoPulseCap",RogueBossSkillTests::Flags)
bool FBossDurationDeadlineTest::RunTest(const FString&)
{
    using namespace RogueBossSkillTests;
    FRogueBossSkillRow Row;
    Row.ChargeSeconds=0; Row.DurationSeconds=.25f; Row.PulseInterval=.1f;
    FFixture F;
    if (!F.Init(Row)) return false;
    auto* Boss=F.Enemy(); auto* Skills=Boss->BossSkills.Get();
    TestTrue(TEXT("A short timed cast starts"),Skills->TryStartSkill(0));
    F.Step(.02f);
    F.Scope.TickTestWorld(1.f);
    TestEqual(TEXT("A hitch beyond the deadline emits no late attack"),Skills->GetSkillState().PulseIndex,1);
    TestEqual(TEXT("An expired duration immediately enters recovery"),Skills->GetSkillState().Phase,ERogueBossSkillPhase::Recovery);
    Skills->CancelCurrentSkill();

    Row.DurationSeconds=12.f; Row.PulseInterval=.02f; Row.InitialDelay=0; Row.RecoverySeconds=0;
    Row.AbilityClass=URogueBossSkillAbility::StaticClass();
    F.Skills->AddRow(TEXT("First"),Row); F.InitEnemy(Boss,true);
    TestTrue(TEXT("A long fast cast starts"),Skills->TryStartSkill(0));
    F.Step(12.1f);
    TestTrue(TEXT("Duration-based casting can emit more than 256 pulses"),Skills->GetSkillState().PulseIndex>256);
    TestFalse(TEXT("The long cast ends at its duration"),Skills->IsCasting());

    FRogueBossSkillRow Legacy;
    Legacy.PulseCount=4; Legacy.PulseInterval=.5f; Legacy.ReleaseHoldSeconds=.5f;
    TestNearlyEqual(TEXT("Old asset timings migrate to a duration"),Legacy.Sanitized().DurationSeconds,2.f,.001f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBossSustainedDurationTest,"Rogue.Gameplay.BossSkills.SustainedEffectsRespectDuration",RogueBossSkillTests::Flags)
bool FBossSustainedDurationTest::RunTest(const FString&)
{
    using namespace RogueBossSkillTests;
    const auto* Catalog=LoadObject<UDataTable>(nullptr,TEXT("/Game/Data/Enemies/DT_BossSkills.DT_BossSkills"));
    if (!TestNotNull(TEXT("Real skill catalog"),Catalog)) return false;
    for (const FName Name : {FName(TEXT("RollingSuns")),FName(TEXT("JudgmentRay")),FName(TEXT("LastBloom")),FName(TEXT("DiamondReversal"))})
    {
        FFixture F;
        if (!F.Init(FRogueBossSkillRow())) return false;
        const auto* Actual=Catalog->FindRow<FRogueBossSkillRow>(Name,TEXT("Duration regression"));
        if (!TestNotNull(Name.ToString(),Actual)) return false;
        auto Row=*Actual;
        // 时间线测试使用独立配置，策划调整正式 DT 的数值不会让生命周期测试失效。
        Row.DurationSeconds=10.f;
        Row.PulseInterval=.5f;
        Row.ChargeSeconds=0; Row.InitialDelay=0; Row.RecoverySeconds=0;
        Row.Attack.DamageScale=0; // 只观测生命周期，让玩家留在场上直到技能结束。
        F.Skills->AddRow(TEXT("First"),Row);
        auto* Boss=F.Enemy(); auto* Skills=Boss->BossSkills.Get();
        auto* ASC=Boss->GetAbilitySystemComponent();
        ASC->SetNumericAttributeBase(URogueAttributeSet::GetHealthAttribute(),200.f);
        TestTrue(TEXT("A real sustained skill starts without charge"),Skills->TryStartSkill(0));
        F.Step(6.f);
        TestTrue(TEXT("The effect is still casting beyond the old five-second duration"),Skills->IsCasting());
        if (Name==TEXT("RollingSuns"))
        {
            int32 Count=0;
            for (TActorIterator<ARogueBossAttackActor> It(F.World); It; ++It)
                if (It->GetOwner()==Boss && !It->IsActorBeingDestroyed()) ++Count;
            TestTrue(TEXT("Rolling fireballs continue in successive three-ball groups"),Count>3);
            TestTrue(TEXT("Rolling fireballs keep pulsing during the duration"),Skills->GetSkillState().PulseIndex>2);
        }
        if (Name==TEXT("JudgmentRay"))
        {
            int32 Count=0;
            for (TActorIterator<ARogueBossAttackActor> It(F.World); It; ++It)
                if (It->GetOwner()==Boss && !It->IsActorBeingDestroyed()) ++Count;
            TestEqual(TEXT("A continuous laser keeps exactly one beam"),Count,1);
            TestEqual(TEXT("A continuous laser only starts once"),Skills->GetSkillState().PulseIndex,1);
        }
        if (Name==TEXT("LastBloom"))
        {
            TestTrue(TEXT("Healing follows elapsed time"),Boss->GetCurrentHealth()>200.f);
            TestTrue(TEXT("Healing has not prematurely completed"),Boss->GetCurrentHealth()<Boss->GetMaxHealth());
        }
        if (Name==TEXT("DiamondReversal"))
            TestTrue(TEXT("The ward remains active beyond its old four-second lifetime"),ASC->HasMatchingGameplayTag(RogueGameplayTags::State_BossDamageInversion));
        F.Step(4.2f);
        TestFalse(TEXT("The sustained skill ends at its configured deadline"),Skills->IsCasting());
        if (Name==TEXT("LastBloom"))
            TestNearlyEqual(TEXT("Normal completion finishes healing"),Boss->GetCurrentHealth(),Boss->GetMaxHealth(),.01f);
        TestFalse(TEXT("No inversion tag survives completion"),ASC->HasMatchingGameplayTag(RogueGameplayTags::State_BossDamageInversion));
    }
    return true;
}
#endif
