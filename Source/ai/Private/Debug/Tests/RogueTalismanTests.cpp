#if WITH_DEV_AUTOMATION_TESTS
#include "AbilitySystemComponent.h"
#include "AbilitySystem/RogueAttributeSet.h"
#include "Combat/Projectiles/RogueReturningTalisman.h"
#include "Combat/Weapons/RogueWeapon_Talisman.h"
#include "Combat/Weapons/RogueWeapon_Projectile.h"
#include "Combat/Weapons/RogueWeapon_Laser.h"
#include "Combat/Weapons/RogueWeapon_HellTower.h"
#include "Combat/Projectiles/RogueProjectile.h"
#include "Combat/Projectiles/RogueRocketProjectile.h"
#include "Combat/Projectiles/RogueOrbitingBlade.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Components/SphereComponent.h"

#include "Combat/Targeting/RogueEnemyTrackerSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Materials/Material.h"
#include "Enemies/RogueEnemy.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/RogueGameMode.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/WorldSettings.h"
#include "Misc/AutomationTest.h"
#include "Player/RogueCharacter.h"
#include "Player/RoguePlayerController.h"
#include "Player/RoguePlayerState.h"
#include "Progression/Meta/RogueUpgradeTreeDefinition.h"
#include "Progression/Upgrades/RogueUpgradeDefinitionAsset.h"
#include "Progression/Upgrades/RogueUpgradeRuleAsset.h"
#include "Progression/Upgrades/RogueUpgradeSystem.h"
#include "Progression/Shop/RogueShopSystem.h"
#include "Tests/AutomationCommon.h"
#include "UObject/UnrealType.h"

namespace RogueTalismanTests
{
    constexpr EAutomationTestFlags Flags=EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter;
    struct FFixture
    {
        FTestConsoleVariable SelfTest{TEXT("Rogue.SelfTest.WeaponUpgradeChain")};
        FTestConsoleVariable Stress{TEXT("Rogue.SelfTest.EnemyStress")};
        FTestWorldWrapper TestWorld; UWorld* World=nullptr; ARogueCharacter* Player=nullptr;
        bool Initialize(const FRogueProgressionSnapshot& Profile=FRogueProgressionSnapshot())
        {
            SelfTest.Set(TEXT("0")); Stress.Set(TEXT("0"));
            if (!TestWorld.CreateTestWorld(EWorldType::Game)) { return false; }
            World=TestWorld.GetTestWorld();World->GetWorldSettings()->DefaultGameMode=ARogueGameMode::StaticClass();
            if (!TestWorld.BeginPlayInTestWorld()) { return false; }
            World->GetAuthGameMode()->SetActorTickEnabled(false);
            auto* PC=World->SpawnActor<ARoguePlayerController>();
            const FTransform Transform(FVector(0,0,200));
            Player=World->SpawnActorDeferred<ARogueCharacter>(ARogueCharacter::StaticClass(),Transform,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
            auto* Table=NewObject<UDataTable>(World);Table->RowStruct=FRogueWeaponTableRow::StaticStruct();
            FRogueWeaponTableRow Row;Row.Count=0;Row.WeaponClass=ARogueWeapon_Talisman::StaticClass();Row.Damage=10;Row.Range=2000;
            Row.CountUpgradeType=ERogueUpgradeType::TalismanCount;Table->AddRow(TEXT("Talisman"),Row);
            FindFProperty<FObjectProperty>(ARogueCharacter::StaticClass(),TEXT("WeaponDataTable"))->SetObjectPropertyValue_InContainer(Player,Table);
            Player->AutoPossessPlayer=EAutoReceiveInput::Disabled; Player->FinishSpawning(Transform);
            PC->GetPlayerState<ARoguePlayerState>()->SetProgressionSnapshotForRun(Profile); PC->Possess(Player);
            Player->SetActorTickEnabled(false);Player->GetCharacterMovement()->SetComponentTickEnabled(false);Player->SetActorEnableCollision(false);
            Player->GetAbilitySystemComponent()->CancelAllAbilities();Player->GetRogueAttributeSet()->SetWeaponBonusCaps(FRogueWeaponBonusCaps());
            return Player->FindWeapon<ARogueWeapon_Talisman>()!=nullptr;
        }
        ARogueEnemy* Enemy(FVector Location)
        {
            FActorSpawnParameters Params;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            auto* E=World->SpawnActor<ARogueEnemy>(ARogueEnemy::StaticClass(),Location,FRotator::ZeroRotator,Params);
            E->ActivatePooledEnemy(nullptr,Location,FRotator::ZeroRotator);FRogueEnemyProfile P;P.Health=1000;E->InitializeEnemy(P);
            E->SetActorTickEnabled(false);E->GetCharacterMovement()->SetComponentTickEnabled(false);E->SetActorEnableCollision(false);
            World->GetSubsystem<URogueEnemyTrackerSubsystem>()->RegisterEnemy(E);return E;
        }
        ARogueReturningTalisman* Charm(int32 Targets=1,int32 Repeats=1)
        {
            auto* C=World->SpawnActor<ARogueReturningTalisman>(); C->Configure(Player,0,1,10,2000,1,Targets,Repeats);return C;
        }
        void Flight(ARogueReturningTalisman* C,float Time=4.f,float Step=.01f)
        { for(float T=0;T<Time && IsValid(C);T+=Step) { C->Tick(Step); } }
        void Upgrade(ERogueUpgradeType Type)
        { FRogueUpgradeOption O;O.Type=Type;O.Magnitude=1;Player->ApplyUpgrade(O); }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueTalismanBasicTest,"Rogue.Gameplay.Talisman.OneTargetSweepReturnAndBlueprintScale",RogueTalismanTests::Flags)
bool FRogueTalismanBasicTest::RunTest(const FString& Parameters)
{
    using namespace RogueTalismanTests;FFixture F;if(!F.Initialize())return false;
    auto* A=F.Enemy(FVector(600,0,200));auto* B=F.Enemy(FVector(600,0,200));auto* C=F.Charm();
    const FVector Custom(.9f,.07f,1.7f);C->Mesh->SetRelativeScale3D(Custom);C->OrbitSizeMultiplier=.6f;
    TestTrue(TEXT("Launch finds a real enemy"),C->TryLaunch());
    for(int I=0;I<200 && C->GetSortieHitCount()==0;++I)C->Tick(.01f);
    TestEqual(TEXT("Contact consumes one hit"),C->GetSortieHitCount(),1);
    TestTrue(TEXT("Finishes physical pass instead of turning at capsule edge"),C->GetFlightPhase()==ERogueTalismanPhase::Striking);
    F.Flight(C,4,.12f); // 单步180cm，大于普通胶囊直径，必须线段扫掠而非端点检查。
    const float Damage=2000-A->GetCurrentHealth()-B->GetCurrentHealth();
    TestEqual(TEXT("Overlapping enemies consume only one distinct target"),Damage,10.f);
    TestEqual(TEXT("Exactly one damage pass, return adds none"),C->GetSortieHitCount(),1);
    TestTrue(TEXT("Returns to orbit"),C->GetFlightPhase()==ERogueTalismanPhase::Orbiting);
    TestTrue(TEXT("Runtime never rewrites BP mesh scale"),C->Mesh->GetRelativeScale3D().Equals(Custom));
    TestTrue(TEXT("Orbit size multiplier is separate"),C->VisualRoot->GetRelativeScale3D().Equals(FVector(.6f)));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueTalismanRepeatTest,"Rogue.Gameplay.Talisman.TwoTargetsThreeActualPassesEach",RogueTalismanTests::Flags)
bool FRogueTalismanRepeatTest::RunTest(const FString& Parameters)
{
    using namespace RogueTalismanTests;FFixture F;if(!F.Initialize())return false;
    auto* A=F.Enemy(FVector(650,0,200));auto* B=F.Enemy(FVector(1000,150,200));auto* C=F.Charm(2,3);
    TestTrue(TEXT("Launch succeeds"),C->TryLaunch());F.Flight(C,8);
    TestEqual(TEXT("First enemy takes three full hits"),A->GetCurrentHealth(),970.f);
    TestEqual(TEXT("Second enemy takes three full hits"),B->GetCurrentHealth(),970.f);
    TestEqual(TEXT("Two distinct victims"),C->GetSortieTargetCount(),2);
    TestEqual(TEXT("Six physical passes"),C->GetSortieHitCount(),6);
    TestTrue(TEXT("Ends in orbit"),C->GetFlightPhase()==ERogueTalismanPhase::Orbiting);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueTalismanLostTargetTest,"Rogue.Gameplay.Talisman.PooledTargetAndOwnerCleanup",RogueTalismanTests::Flags)
bool FRogueTalismanLostTargetTest::RunTest(const FString& Parameters)
{
    using namespace RogueTalismanTests;FFixture F;if(!F.Initialize())return false;
    auto* Enemy=F.Enemy(FVector(800,0,200));auto* C=F.Charm(1,3);TestTrue(TEXT("Launch"),C->TryLaunch());
    for(int I=0;I<100 && C->GetSortieHitCount()==0;++I)C->Tick(.01f);
    TestEqual(TEXT("First hit landed"),C->GetSortieHitCount(),1);
    Enemy->DeactivateToPool();Enemy->ActivatePooledEnemy(nullptr,FVector(800,0,200),FRotator::ZeroRotator);
    FRogueEnemyProfile P;P.Health=1000;Enemy->InitializeEnemy(P);Enemy->SetActorTickEnabled(false);
    F.Flight(C,3);
    TestEqual(TEXT("New pool cycle does not receive remaining old strikes"),Enemy->GetCurrentHealth(),1000.f);
    TestTrue(TEXT("Lost target returns safely"),C->GetFlightPhase()==ERogueTalismanPhase::Orbiting);
    F.Upgrade(ERogueUpgradeType::TalismanCount);auto* W=F.Player->FindWeapon<ARogueWeapon_Talisman>();W->WeaponTick(0);
    W->OnOwnerDied();int Active=0;
    for(TActorIterator<ARogueReturningTalisman> It(F.World);It;++It)if(!It->IsActorBeingDestroyed() && *It!=C)++Active;
    TestEqual(TEXT("Weapon cleans its persistent actors"),Active,0);
    C->Destroy();return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueTalismanGASTest,"Rogue.Gameplay.Talisman.GASQuantityAndBothUpgradeStacks",RogueTalismanTests::Flags)
bool FRogueTalismanGASTest::RunTest(const FString& Parameters)
{
    using namespace RogueTalismanTests;FFixture F;if(!F.Initialize())return false;
    auto* W=F.Player->FindWeapon<ARogueWeapon_Talisman>();TestEqual(TEXT("Starts unowned"),W->GetEffectiveCount(),0);
    for(int I=0;I<3;++I){F.Upgrade(ERogueUpgradeType::TalismanCount);F.Upgrade(ERogueUpgradeType::TalismanPierce);F.Upgrade(ERogueUpgradeType::TalismanRepeat);}
    TestEqual(TEXT("Three quantity choices create three talismans"),W->GetEffectiveCount(),3);
    TestEqual(TEXT("Pierce starts one and each choice adds one"),W->GetEffectiveTargetCount(),4);
    TestEqual(TEXT("Repeat starts one and each choice adds one"),W->GetEffectiveHitsPerTarget(),4);
    W->WeaponTick(0);int Active=0;for(TActorIterator<ARogueReturningTalisman> It(F.World);It;++It)if(!It->IsActorBeingDestroyed())++Active;
    TestEqual(TEXT("Real persistent count matches GAS"),Active,3);
    W->Destroy();return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueTalismanPoolTest,"Rogue.Gameplay.Talisman.SavedTreeSequentialUnlockAndUpgradeShopPermissions",RogueTalismanTests::Flags)
bool FRogueTalismanPoolTest::RunTest(const FString& Parameters)
{
    using namespace RogueTalismanTests;
    const auto* Tree=URogueUpgradeTreeDefinition::GetDefaultDefinition();
    const FName Ids[]={TEXT("Weapon.Talisman"),TEXT("Card.TalismanCount"),TEXT("Card.TalismanPierce"),TEXT("Card.TalismanRepeat")};
    FRogueProgressionSnapshot Profile;
    for(int I=0;I<4;++I)
    {
        const auto* Node=Tree->FindNode(Ids[I]);if(!TestNotNull(TEXT("Saved node exists"),Node))return false;
        TestEqual(TEXT("Five ice bricks per node"),Node->Cost,5);TestFalse(TEXT("New nodes are paid"),Node->bInitiallyUnlocked);
        if(I>0)TestTrue(TEXT("Cannot skip previous node"),!Tree->GetPurchaseFailure({},100,Ids[I]).IsEmpty());
        TestTrue(TEXT("Can buy next node"),Tree->GetPurchaseFailure(Profile,5,Ids[I]).IsEmpty());Profile.NodeRanks.Add(Ids[I],1);
    }
    FRogueUpgradeSystem System;
    auto* Definitions=LoadObject<URogueUpgradeDefinitionAsset>(nullptr,TEXT("/Game/Data/Upgrades/DA_UpgradeDefinitions.DA_UpgradeDefinitions"));
    System.SetDefinitionAsset(Definitions);System.SetRuleAsset(LoadObject<URogueUpgradeRuleAsset>(nullptr,TEXT("/Game/Data/Upgrades/DA_UpgradeRules.DA_UpgradeRules")));
    const ERogueUpgradeType Types[]={ERogueUpgradeType::TalismanCount,ERogueUpgradeType::TalismanPierce,ERogueUpgradeType::TalismanRepeat};
    { FFixture Fresh;if(!Fresh.Initialize())return false;
      for(auto Type:Types)TestFalse(TEXT("Fresh profile cannot roll new weapon/cards"),System.IsUpgradeAvailable(Fresh.Player,Type)); }
    FFixture F;if(!F.Initialize(Profile))return false;
    TestTrue(TEXT("Weapon acquisition can roll after permanent unlock"),System.IsUpgradeAvailable(F.Player,Types[0]));
    TestFalse(TEXT("Exclusive still needs actual weapon this run"),System.IsUpgradeAvailable(F.Player,Types[1]));
    F.Upgrade(Types[0]);FRogueShopSystem Shop;Shop.Configure(100,1,0,30);Shop.OpenShop(F.Player,System);
    for(auto Type:Types)
    {
        FRogueUpgradeDefinitionRow Row;TestTrue(TEXT("Data table has real card"),Definitions->FindDefinition(Type,Row));
        TestEqual(TEXT("Each card adds one"),Row.Magnitude,1.f);
        TestTrue(TEXT("Owned unlocked card is available"),System.IsUpgradeAvailable(F.Player,Type));
        TestTrue(TEXT("Shop shares this player's permissions"),Shop.GetOffers().ContainsByPredicate([Type](const auto& O){return O.GetUpgrade().Type==Type;}));
    }
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueTalismanBlueprintTickTest,"Rogue.Gameplay.Talisman.SavedBlueprintAutomaticWorldTick",RogueTalismanTests::Flags)
bool FRogueTalismanBlueprintTickTest::RunTest(const FString& Parameters)
{
    using namespace RogueTalismanTests;FFixture F;if(!F.Initialize())return false;
    auto* E=F.Enemy(FVector(650,0,200));
    auto* Class=LoadClass<ARogueReturningTalisman>(nullptr,TEXT("/Game/BP/Projectiles/BP_RogueReturningTalisman.BP_RogueReturningTalisman_C"));
    if(!TestNotNull(TEXT("Saved projectile BP loads"),Class))return false;
    auto* C=F.World->SpawnActor<ARogueReturningTalisman>(Class);
    C->Configure(F.Player,0,1,10,2000,1,1,1);C->TryLaunch();
    TestTrue(TEXT("Saved blueprint enables native tick"),C->IsActorTickEnabled());
    for(int I=0;I<300;++I)F.TestWorld.TickTestWorld(.01f);
    TestEqual(TEXT("Automatic world ticks actually deal damage"),E->GetCurrentHealth(),990.f);
    TestTrue(TEXT("Automatic world ticks return it to orbit"),C->GetFlightPhase()==ERogueTalismanPhase::Orbiting);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueTalismanFacingTrailTest,"Rogue.Gameplay.Talisman.ModelFacingAndTimeBasedTrailAtMultipleFrameRates",RogueTalismanTests::Flags)
bool FRogueTalismanFacingTrailTest::RunTest(const FString& Parameters)
{
    using namespace RogueTalismanTests;FFixture F;if(!F.Initialize())return false;
    auto* E=F.Enemy(FVector(8000,1400,200));
    auto* Class=LoadClass<ARogueReturningTalisman>(nullptr,TEXT("/Game/BP/Projectiles/BP_RogueReturningTalisman.BP_RogueReturningTalisman_C"));
    if(!TestNotNull(TEXT("Saved projectile blueprint loads"),Class))return false;
    for(const float Rate:{30.f,120.f,240.f})
    {
        auto* C=F.World->SpawnActor<ARogueReturningTalisman>(Class);
        const FVector Scale=C->Mesh->GetRelativeScale3D();
        C->Configure(F.Player,0,1,10,20000,1,1,1);
        if(!TestTrue(TEXT("Launch to distant target"),C->TryLaunch()))return false;
        F.Flight(C,2.f,1.f/Rate);
        const FVector PaperHead=C->Mesh->GetComponentQuat().RotateVector(FVector::YAxisVector);
        TestTrue(TEXT("Paper head points along real flight direction"),FVector::DotProduct(PaperHead,C->GetActorForwardVector())>.999f);
        TestTrue(TEXT("Mesh scale remains the authored blueprint scale"),C->Mesh->GetRelativeScale3D().Equals(Scale));
        TArray<USplineMeshComponent*> Segments;C->GetComponents(Segments);float Length=0;
        for(const auto* Segment:Segments)if(Segment->IsVisible())Length+=FVector::Distance(Segment->GetStartPosition(),Segment->GetEndPosition());
        TestTrue(FString::Printf(TEXT("Long trail preserved at %.0f FPS (%.1f cm)"),Rate,Length),Length >= C->FlightSpeed * FMath::Clamp(C->TrailLifetime,.03f,1.f) * .8f && Length <= C->FlightSpeed * FMath::Clamp(C->TrailLifetime,.03f,1.f) * 1.08f);
        C->FlightRotationOffset=FRotator(0,25,0);C->Tick(1.f/Rate);
        const FVector Expected=(C->GetActorQuat()*C->FlightRotationOffset.Quaternion()).RotateVector(FVector::ForwardVector);
        TestTrue(TEXT("Designer flight angle correction is applied"),FVector::DotProduct(C->Mesh->GetComponentQuat().RotateVector(FVector::YAxisVector),Expected)>.999f);
        C->Destroy();
    }
    E->DeactivateToPool();F.Enemy(FVector(650,0,200));F.World->GetSubsystem<URogueEnemyTrackerSubsystem>()->Tick(1.f);
    auto* C=F.World->SpawnActor<ARogueReturningTalisman>(Class);C->Configure(F.Player,0,1,10,2000,1,1,1);C->TryLaunch();
    bool Returned=false;
    for(int I=0;I<200;++I)
    {
        const auto Before=C->GetFlightPhase();const FVector Old=C->GetActorLocation();C->Tick(.01f);
        if(Before!=ERogueTalismanPhase::Returning || C->GetFlightPhase()!=ERogueTalismanPhase::Returning)continue;
        const FVector Movement=(C->GetActorLocation()-Old).GetSafeNormal();
        TestTrue(TEXT("Paper also faces movement on return"),FVector::DotProduct(C->Mesh->GetComponentQuat().RotateVector(FVector::YAxisVector),Movement)>.999f);
        TArray<USplineMeshComponent*> Segments;C->GetComponents(Segments);
        TestTrue(TEXT("Trail survives turn into return phase"),Segments.ContainsByPredicate([](const auto* S){return S->IsVisible() && FVector::DistSquared(S->GetStartPosition(),S->GetEndPosition())>1.f;}));
        Returned=true;break;
    }
    TestTrue(TEXT("Return phase was exercised"),Returned);C->Destroy();return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueTableTalismanSettingsTest,"Rogue.Gameplay.WeaponConfiguration.TalismanFlightBudgetAndBlueprintVisuals",RogueTalismanTests::Flags)
bool FRogueTableTalismanSettingsTest::RunTest(const FString&)
{
    using namespace RogueTalismanTests;FFixture F;if(!F.Initialize())return false;
    F.Enemy(FVector(1000,0,200));auto* Weapon=F.Player->FindWeapon<ARogueWeapon_Talisman>();
    auto& Row=Weapon->GetConfig();Row.Count=1;Row.TalismanCombat.FlightSpeed=250;Row.TalismanCombat.ReturnSpeed=350;
    Row.TalismanCombat.HitRadius=51;Row.TalismanCombat.BaseTargetCount=2;Row.TalismanCombat.BaseHitsPerTarget=3;
    Row.TalismanCombat.MaxSortieSeconds=2;Row.TalismanCombat.MaxReturnSeconds=.5f;
    Weapon->WeaponTick(.01f);ARogueReturningTalisman* Charm=nullptr;
    for(TActorIterator<ARogueReturningTalisman> It(F.World);It;++It)if(It->GetOwner()==F.Player){Charm=*It;break;}
    if(!TestNotNull(TEXT("Weapon spawns its configured projectile"),Charm))return false;
    const FVector Start=Charm->GetActorLocation();Charm->Tick(.1f);
    TestTrue(TEXT("Real sortie moves 25cm at table speed 250cm/s"),FMath::IsNearlyEqual(FVector::Distance(Start,Charm->GetActorLocation()),25.f,.01f));
    TestEqual(TEXT("Return speed comes from row"),Charm->ReturnSpeed,350.f);
    TestEqual(TEXT("Sweep radius comes from row"),Charm->HitRadius,51.f);
    TestEqual(TEXT("Base target budget comes from row"),Weapon->GetEffectiveTargetCount(),2);
    TestEqual(TEXT("Base repeat budget comes from row"),Weapon->GetEffectiveHitsPerTarget(),3);
    const FVector Scale(.4f,.3f,1.2f);Charm->Mesh->SetRelativeScale3D(Scale);Charm->OrbitRadius=95;
    Charm->FlightSpeed=9000;Weapon->WeaponTick(.01f);
    TestEqual(TEXT("Hidden legacy blueprint values cannot override row"),Charm->FlightSpeed,250.f);
    TestTrue(TEXT("Blueprint mesh scale survives combat reconfiguration"),Charm->Mesh->GetRelativeScale3D().Equals(Scale));
    TestEqual(TEXT("Orbit presentation stays blueprint controlled"),Charm->OrbitRadius,95.f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueTableProjectileSettingsTest,"Rogue.Gameplay.WeaponConfiguration.RealWeaponsApplyHitRadiusLifetimeAndMortarArc",RogueTalismanTests::Flags)
bool FRogueTableProjectileSettingsTest::RunTest(const FString&)
{
    using namespace RogueTalismanTests;FFixture F;if(!F.Initialize())return false;
    auto* Enemy=F.Enemy(FVector(1000,0,200));
    const auto* Table=LoadObject<UDataTable>(nullptr,TEXT("/Game/Data/Weapons/DT_WeaponConfig.DT_WeaponConfig"));
    if(!TestNotNull(TEXT("Saved weapon table loads"),Table))return false;
    for(FName Id:{FName(TEXT("Projectile")),FName(TEXT("Rocket")),FName(TEXT("Mortar"))})
    {
        const auto* Saved=Table->FindRow<FRogueWeaponTableRow>(Id,TEXT("Config test"));if(!Saved)return false;
        FRogueWeaponTableRow Row=*Saved;Row.Count=1;Row.Damage=0;Row.bEnableSpeedRandomization=false;
        Row.ProjectileCombat.CollisionRadius=77;Row.ProjectileCombat.Lifetime=.3f;
        Row.MortarFlight.GravityScale=1.8f;Row.MortarFlight.MinimumFlightTime=1.1f;Row.MortarFlight.MaximumFlightTime=1.1f;
        auto* Weapon=F.World->SpawnActor<ARogueWeaponBase>(Row.WeaponClass);Weapon->InitializeWeapon(F.Player,Row);
        TArray<ARogueEnemy*> Targets{Enemy};Weapon->FireAtTargets(Targets);
        AActor* Projectile=nullptr;
        if(Id==TEXT("Projectile"))
        {for(TActorIterator<ARogueProjectile> It(F.World);It;++It)if(!It->IsAvailableInPool()){Projectile=*It;break;}}
        else
        {for(TActorIterator<ARogueRocketProjectile> It(F.World);It;++It)if(!It->IsAvailableInPool()){Projectile=*It;break;}}
        if(!TestNotNull(TEXT("Actual weapon fires"),Projectile))return false;
        const auto* Sphere=Projectile->FindComponentByClass<USphereComponent>();
        TestEqual(TEXT("Weapon passes DT hit radius to actual projectile"),Sphere->GetUnscaledSphereRadius(),77.f);
        if(Id==TEXT("Mortar"))
        {
            const auto* Move=Projectile->FindComponentByClass<UProjectileMovementComponent>();
            TestEqual(TEXT("Mortar gravity comes from DT"),Move->ProjectileGravityScale,1.8f);
            const float ExpectedZ=(Enemy->GetActorLocation().Z+18.f-Projectile->GetActorLocation().Z)/1.1f+.5f*FMath::Abs(F.World->GetGravityZ()*1.8f)*1.1f;
            TestTrue(TEXT("Mortar flight-time settings change actual launch arc"),FMath::IsNearlyEqual(Move->Velocity.Z,ExpectedZ,.1f));
        }
        Projectile->Tick(.31f);
        TestTrue(TEXT("Weapon DT lifetime retires live projectile"),Cast<ARogueProjectile>(Projectile)?Cast<ARogueProjectile>(Projectile)->IsAvailableInPool():Cast<ARogueRocketProjectile>(Projectile)->IsAvailableInPool());
        // 旧实例再次出池必须采用新行参数，不能记住首次激活配置。
        Row.ProjectileCombat.CollisionRadius=33;Row.ProjectileCombat.Lifetime=2;
        Weapon->InitializeWeapon(F.Player,Row);Weapon->FireAtTargets(Targets);
        TestEqual(TEXT("Pooled reuse resets radius from DT"),Sphere->GetUnscaledSphereRadius(),33.f);
        Projectile->Tick(.31f);
        TestFalse(TEXT("Pooled reuse resets lifetime from DT"),Cast<ARogueProjectile>(Projectile)?Cast<ARogueProjectile>(Projectile)->IsAvailableInPool():Cast<ARogueRocketProjectile>(Projectile)->IsAvailableInPool());
        if(auto* P=Cast<ARogueProjectile>(Projectile))P->DeactivateToPool();else Cast<ARogueRocketProjectile>(Projectile)->DeactivateToPool();
        Weapon->Destroy();
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueTableBladeSettingsTest,"Rogue.Gameplay.WeaponConfiguration.BladeRadiusGrowthAndDamageInterval",RogueTalismanTests::Flags)
bool FRogueTableBladeSettingsTest::RunTest(const FString&)
{
    using namespace RogueTalismanTests;FFixture F;if(!F.Initialize())return false;
    auto* B=F.World->SpawnActor<ARogueOrbitingBlade>();B->ActivatePooledBlade(F.Player);
    B->ConfigureBlade(F.Player,0,1,200,0,10,0,1.5f,80,.8f);
    auto* Sphere=B->FindComponentByClass<USphereComponent>();TestEqual(TEXT("DT radius scales with giant-blade upgrade"),Sphere->GetUnscaledSphereRadius(),120.f);
    auto* E=F.Enemy(B->GetActorLocation());E->SetActorEnableCollision(true);Sphere->UpdateOverlaps();
    B->Tick(.01f);TestEqual(TEXT("Initial contact applies one hit"),E->GetCurrentHealth(),990.f);
    B->Tick(.4f);TestEqual(TEXT("DT cooldown prevents old 0.35s cadence"),E->GetCurrentHealth(),990.f);
    B->Tick(.41f);TestEqual(TEXT("DT cooldown expires at 0.8s"),E->GetCurrentHealth(),980.f);
    B->DeactivateToPool();B->ActivatePooledBlade(F.Player);B->ConfigureBlade(F.Player,0,1,200,0,0,0,1.f,32,.2f);
    TestEqual(TEXT("Reused blade receives replacement DT radius"),Sphere->GetUnscaledSphereRadius(),32.f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueTargetReservationsTest,"Rogue.Gameplay.WeaponTargeting.ScopeBalancedFallbackAndPoolCycle",RogueTalismanTests::Flags)
bool FRogueTargetReservationsTest::RunTest(const FString&)
{
    using namespace RogueTalismanTests;FFixture F;if(!F.Initialize())return false;
    auto* T=F.World->GetSubsystem<URogueEnemyTrackerSubsystem>();
    auto* A=F.Enemy(FVector(500,0,200));auto* B=F.Enemy(FVector(800,0,200));
    auto* W=F.World->SpawnActor<AActor>();auto* OtherWeapon=F.World->SpawnActor<AActor>();
    auto* P=F.World->SpawnActor<AActor>();auto* Q=F.World->SpawnActor<AActor>();auto* R=F.World->SpawnActor<AActor>();
    TArray<ARogueEnemy*> Candidates{A,B};
    TestTrue(TEXT("Equal load preserves nearest/preferred order"),T->SelectWeaponTarget(W,Candidates)==A);
    T->ReserveWeaponTarget(W,P,A);
    TestTrue(TEXT("Next same-weapon projectile picks an idle target"),T->SelectWeaponTarget(W,Candidates)==B);
    TestTrue(TEXT("Other weapon/player has independent assignments"),T->SelectWeaponTarget(OtherWeapon,Candidates)==A);
    T->ReserveWeaponTarget(W,Q,B);T->ReserveWeaponTarget(W,R,A);
    TestTrue(TEXT("No idle enemy falls back to least assigned enemy"),T->SelectWeaponTarget(W,Candidates)==B);
    TestTrue(TEXT("A returning selector excludes its own reservation"),T->SelectWeaponTarget(W,Candidates,Q)==B);
    T->ReleaseWeaponTarget(Q);TestTrue(TEXT("Pool return releases reservation"),T->GetWeaponTarget(Q)==nullptr);
    A->DeactivateToPool();A->ActivatePooledEnemy(nullptr,FVector(500,0,200),FRotator::ZeroRotator);
    FRogueEnemyProfile Profile;Profile.Health=1000;A->InitializeEnemy(Profile);
    TestTrue(TEXT("Actor reuse never inherits old-life targeting load"),T->SelectWeaponTarget(W,Candidates)==A);
    TestTrue(TEXT("Old cycle query is invalid"),T->GetWeaponTarget(P)==nullptr);
    T->ReserveWeaponTarget(W,P,A);P->Destroy();
    TestTrue(TEXT("Destroyed projectile does not retain its load"),T->SelectWeaponTarget(W,Candidates)==A);
    T->ReserveWeaponTarget(W,Q,B);T->ReleaseWeaponTargets(W);
    TestTrue(TEXT("Owner cleanup releases all its reservations"),T->GetWeaponTarget(Q)==nullptr);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueMissileDistributionTest,"Rogue.Gameplay.WeaponTargeting.RealProjectileRocketMortarVolleysAndRelease",RogueTalismanTests::Flags)
bool FRogueMissileDistributionTest::RunTest(const FString&)
{
    using namespace RogueTalismanTests;FFixture F;if(!F.Initialize())return false;
    auto* Tracker=F.World->GetSubsystem<URogueEnemyTrackerSubsystem>();TArray<ARogueEnemy*> Targets;
    for(int I=0;I<4;++I)Targets.Add(F.Enemy(FVector(1500+I*250,I*300,200)));
    auto* Table=LoadObject<UDataTable>(nullptr,TEXT("/Game/Data/Weapons/DT_WeaponConfig.DT_WeaponConfig"));
    if(!TestNotNull(TEXT("Saved table loads"),Table))return false;
    for(FName Id:{FName(TEXT("Projectile")),FName(TEXT("Rocket")),FName(TEXT("Mortar"))})
    {
        const auto* Saved=Table->FindRow<FRogueWeaponTableRow>(Id,TEXT("Targeting test"));if(!Saved)return false;
        FRogueWeaponTableRow Row=*Saved;Row.Count=2;Row.Damage=0;Row.Range=5000;Row.bEnableTrajectoryDeviation=false;Row.bEnableSpeedRandomization=false;
        auto* Weapon=F.World->SpawnActor<ARogueWeaponBase>(Row.WeaponClass);Weapon->InitializeWeapon(F.Player,Row);
        Weapon->FireAtTargets(Targets);Weapon->FireAtTargets(Targets);
        TSet<ARogueEnemy*> Assigned;TArray<AActor*> Projectiles;
        for(TActorIterator<ARogueProjectile> It(F.World);It;++It)
            if(auto* Target=Tracker->GetWeaponTarget(*It)){Assigned.Add(Target);Projectiles.Add(*It);}
        for(TActorIterator<ARogueRocketProjectile> It(F.World);It;++It)
            if(auto* Target=Tracker->GetWeaponTarget(*It)){Assigned.Add(Target);Projectiles.Add(*It);}
        TestEqual(FString::Printf(TEXT("%s fires four actual projectiles across two volleys"),*Id.ToString()),Projectiles.Num(),4);
        TestEqual(FString::Printf(TEXT("%s considers previous volley and covers four different targets"),*Id.ToString()),Assigned.Num(),4);
        for(AActor* Projectile:Projectiles)
        {
            if(auto* P=Cast<ARogueProjectile>(Projectile))P->DeactivateToPool();else Cast<ARogueRocketProjectile>(Projectile)->DeactivateToPool();
            TestTrue(TEXT("Real deactivation releases target assignment"),Tracker->GetWeaponTarget(Projectile)==nullptr);
        }
        Weapon->Destroy();
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueTalismanDistributionTest,"Rogue.Gameplay.WeaponTargeting.ThreeTalismansSeparateTargetsAndReleaseOnReturn",RogueTalismanTests::Flags)
bool FRogueTalismanDistributionTest::RunTest(const FString&)
{
    using namespace RogueTalismanTests;FFixture F;if(!F.Initialize())return false;
    F.Enemy(FVector(2200,0,200));F.Enemy(FVector(2600,400,200));F.Enemy(FVector(3000,-500,200));
    auto* Weapon=F.Player->FindWeapon<ARogueWeapon_Talisman>();Weapon->GetConfig().Count=3;Weapon->GetConfig().Range=4000;
    Weapon->WeaponTick(.01f);F.World->Tick(LEVELTICK_All,.3f);Weapon->WeaponTick(.01f);
    TArray<ARogueReturningTalisman*> Charms;TSet<ARogueEnemy*> Targets;
    auto* Tracker=F.World->GetSubsystem<URogueEnemyTrackerSubsystem>();
    for(TActorIterator<ARogueReturningTalisman> It(F.World);It;++It)
    {
        if(It->GetOwner()!=F.Player)continue;
        Charms.Add(*It);if(auto* Target=It->GetCurrentTarget())Targets.Add(Target);
        TestTrue(TEXT("In-flight talisman registers its actual selected target"),Tracker->GetWeaponTarget(*It)==It->GetCurrentTarget());
    }
    TestEqual(TEXT("Three live talismans"),Charms.Num(),3);TestEqual(TEXT("No nearest-enemy pileup"),Targets.Num(),3);
    for(auto* Charm:Charms)
    {
        // 每张只给一次预算；手动推进到返航，预约必须在归位前释放。
        for(int I=0;I<500 && Charm->GetFlightPhase()==ERogueTalismanPhase::Striking;++I)Charm->Tick(.02f);
        TestTrue(TEXT("Return/orbit releases attack target immediately"),Tracker->GetWeaponTarget(Charm)==nullptr);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueLaserDistributionTest,"Rogue.Gameplay.WeaponTargeting.LaserRefractionsPreferTargetsOutsideOtherBeams",RogueTalismanTests::Flags)
bool FRogueLaserDistributionTest::RunTest(const FString&)
{
    using namespace RogueTalismanTests;FFixture F;if(!F.Initialize())return false;
    TArray<ARogueEnemy*> Targets;for(int I=0;I<4;++I)Targets.Add(F.Enemy(FVector(700+I*200,0,200)));
    auto* Weapon=F.World->SpawnActor<ARogueWeapon_Laser>();FRogueWeaponTableRow Row;Row.Count=2;Row.Damage=10;Row.RefractionCount=1;Row.RefractionRange=2000;Row.CountUpgradeType=ERogueUpgradeType::LaserCount;
    Weapon->InitializeWeapon(F.Player,Row);Weapon->FireAtTargets(Targets);
    for(auto* Target:Targets)TestTrue(TEXT("Two main beams and two refractions cover four enemies"),Target->GetCurrentHealth()<1000);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueTowerDistributionTest,"Rogue.Gameplay.WeaponTargeting.TowerFallbackKeepsRampThenSeparatesNewEnemy",RogueTalismanTests::Flags)
bool FRogueTowerDistributionTest::RunTest(const FString&)
{
    using namespace RogueTalismanTests;FFixture F;if(!F.Initialize())return false;
    auto* Enemy=F.Enemy(FVector(600,0,200));auto* Weapon=F.World->SpawnActor<ARogueWeapon_HellTower>();
    FRogueWeaponTableRow Row;Row.Count=2;Row.BaseDamage=1;Row.DamageRampPerTick=1;Row.DamageTickInterval=.1f;
    Row.UpdateInterval=.05f;Row.HeavyCombatUpdateInterval=.05f;Row.CountUpgradeType=ERogueUpgradeType::HellTowerCount;
    Weapon->InitializeWeapon(F.Player,Row);Weapon->WeaponTick(.1f);
    const float First=1000-Enemy->GetCurrentHealth();TestEqual(TEXT("Only one enemy permits both towers to fire"),First,2.f);
    const float Previous=Enemy->GetCurrentHealth();Weapon->WeaponTick(.1f);
    TestTrue(TEXT("Sharing does not restart growing damage every update"),Previous-Enemy->GetCurrentHealth()>First);
    auto* NewEnemy=F.Enemy(FVector(800,300,200));Weapon->WeaponTick(.1f);
    TestTrue(TEXT("New enemy attracts the previously duplicated lock"),NewEnemy->GetCurrentHealth()<1000);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueTalismanStandbyPresentationTest,"Rogue.Gameplay.Talisman.StandbyMaterialRestoresFlightAndTrailFadesAfterReturn",RogueTalismanTests::Flags)
bool FRogueTalismanStandbyPresentationTest::RunTest(const FString&)
{
    using namespace RogueTalismanTests;FFixture F;if(!F.Initialize())return false;
    F.Enemy(FVector(650,0,200));F.World->GetSubsystem<URogueEnemyTrackerSubsystem>()->Tick(1.f);
    auto* Class=LoadClass<ARogueReturningTalisman>(nullptr,TEXT("/Game/BP/Projectiles/BP_RogueReturningTalisman.BP_RogueReturningTalisman_C"));
    auto* C=F.World->SpawnActor<ARogueReturningTalisman>(Class);auto* Original=C->Mesh->GetMaterial(0);
    auto* Standby=NewObject<UMaterial>(F.World);C->OrbitMaterials={Standby};C->Configure(F.Player,0,1,10,2000,30,1,1);C->Tick(0);
    TestTrue(TEXT("Waiting paper can use a separate cosmetic material"),C->Mesh->GetMaterial(0)==Standby);
    if(!TestTrue(TEXT("Flight starts against a real target"),C->TryLaunch()))return false;C->Tick(.01f);
    TestTrue(TEXT("Launch restores the authored component material"),C->Mesh->GetMaterial(0)==Original);
    for(int I=0;I<600&&C->GetFlightPhase()!=ERogueTalismanPhase::Orbiting;++I)C->Tick(.01f);
    TestTrue(TEXT("The paper returns to its orbit"),C->GetFlightPhase()==ERogueTalismanPhase::Orbiting);
    TestTrue(TEXT("Return reapplies the waiting material"),C->Mesh->GetMaterial(0)==Standby);
    TArray<USplineMeshComponent*> Segments;C->GetComponents(Segments);
    TestTrue(TEXT("Old flight segments survive arrival for a natural tail"),Segments.ContainsByPredicate([](auto* S){return S->IsVisible();}));
    for(int I=0;I<120;++I)C->Tick(.01f);
    TestFalse(TEXT("Residual segments disappear within their original lifetime"),Segments.ContainsByPredicate([](auto* S){return S->IsVisible();}));
    C->Destroy();return true;
}
#endif
