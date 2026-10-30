#if WITH_DEV_AUTOMATION_TESTS
#include "AbilitySystemComponent.h"
#include "AbilitySystem/RogueAttributeSet.h"
#include "Combat/Projectiles/RogueReturningTalisman.h"
#include "Combat/Weapons/RogueWeapon_Talisman.h"
#include "Combat/Targeting/RogueEnemyTrackerSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SplineMeshComponent.h"
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
#endif
