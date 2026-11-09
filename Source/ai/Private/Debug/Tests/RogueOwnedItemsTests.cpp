#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Engine/World.h"
#include "Engine/DataTable.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Framework/RogueGameMode.h"
#include "Player/RogueCharacter.h"
#include "Player/RoguePlayerController.h"
#include "Player/RoguePlayerState.h"
#include "Combat/Weapons/RogueWeaponBase.h"
#include "AbilitySystemComponent.h"
#include "UI/RoguePresentationTheme.h"
#include "UI/Widgets/RogueOwnedItemsWidget.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/CanvasPanelSlot.h"
#include "UObject/UnrealType.h"
#include "Net/UnrealNetwork.h"

struct FRogueOwnedItemsTestAccess
{
    static void Mirror(ARoguePlayerState* To,const ARoguePlayerState* From){To->UpgradePickViews=From->UpgradePickViews;To->OnRep_UpgradePickViews();}
    static float Velocity(const URogueOwnedItemWidget* W){return W->Velocity;}
    static int32 Flights(const URogueOwnedItemsWidget* W){return W->AcquisitionQueue.Num();}
};
namespace RogueOwnedItemsTests
{
    constexpr auto Flags=EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter;
    struct FFixture
    {
        FTestWorldWrapper Test;UWorld* W=nullptr;ARogueCharacter* Player=nullptr;ARoguePlayerState* State=nullptr;
        bool Init()
        {
            if(!Test.CreateTestWorld(EWorldType::Game))return false;W=Test.GetTestWorld();W->GetWorldSettings()->DefaultGameMode=ARogueGameMode::StaticClass();if(!Test.BeginPlayInTestWorld())return false;W->GetAuthGameMode()->SetActorTickEnabled(false);
            auto* PC=W->SpawnActor<ARoguePlayerController>();Player=W->SpawnActorDeferred<ARogueCharacter>(ARogueCharacter::StaticClass(),FTransform(FVector(0,0,300)),nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
            auto* Table=LoadObject<UDataTable>(nullptr,TEXT("/Game/Data/Weapons/DT_WeaponConfig.DT_WeaponConfig"));if(!Table)return false;
            FindFProperty<FObjectProperty>(ARogueCharacter::StaticClass(),TEXT("WeaponDataTable"))->SetObjectPropertyValue_InContainer(Player,Table);
            Player->AutoPossessPlayer=EAutoReceiveInput::Disabled;Player->FinishSpawning(FTransform(FVector(0,0,300)));State=PC->GetPlayerState<ARoguePlayerState>();State->SetProgressionSnapshotForRun({});PC->Possess(Player);
            Player->SetActorTickEnabled(false);Player->GetCharacterMovement()->SetComponentTickEnabled(false);Player->GetAbilitySystemComponent()->CancelAllAbilities();State->ResetUpgradeState();
            for(ARogueWeaponBase* Weapon:Player->GetWeapons())if(Weapon)Weapon->GetConfig().Count=Weapon->GetCountUpgradeType()==ERogueUpgradeType::ProjectileCount?1:0;
            Player->ApplyWeaponBonusSnapshot(FRogueWeaponBonusValues());return true;
        }
        void Pick(ERogueUpgradeType Type,int32 Times=1)
        {for(int32 I=0;I<Times;++I){FRogueUpgradeOption U;U.Type=Type;U.Magnitude=1;Player->ApplyUpgrade(U);}Player->RefreshWeaponBonusSnapshotFromAttributes();}
    };
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOwnedItemsAcquisition,"Rogue.UI.OwnedItems.RealPicksMergeQuantityAndKeepOrder",RogueOwnedItemsTests::Flags)
bool FOwnedItemsAcquisition::RunTest(const FString&)
{
    using namespace RogueOwnedItemsTests;FFixture F;if(!F.Init())return false;
    auto Initial=RogueOwnedItems::BuildView(F.Player,F.State);TestEqual(TEXT("Initial loadout has one weapon entry"),Initial.Num(),1);if(Initial.IsEmpty())return false;
    TestTrue(TEXT("Initial entry is the actually held frost weapon"),Initial[0].bWeapon&&Initial[0].Type==ERogueUpgradeType::ProjectileCount&&Initial[0].Count==1);
    F.Pick(ERogueUpgradeType::MoveSpeed);F.Pick(ERogueUpgradeType::ProjectileCount);F.Pick(ERogueUpgradeType::AttackPower,2);F.Pick(ERogueUpgradeType::RocketCount);
    auto View=RogueOwnedItems::BuildView(F.Player,F.State);TestEqual(TEXT("Quantity cards merge into weapons instead of duplicating tiles"),View.Num(),4);
    if(View.Num()!=4)return false;
    TestTrue(TEXT("Initial frost weapon stays first"),View[0].Type==ERogueUpgradeType::ProjectileCount&&View[0].Count==2&&View[0].bWeapon);
    TestTrue(TEXT("First distinct card retains its acquisition order"),View[1].Type==ERogueUpgradeType::MoveSpeed&&!View[1].bWeapon);
    TestTrue(TEXT("Repeated power cards stack in one tile"),View[2].Type==ERogueUpgradeType::AttackPower&&View[2].Count==2);
    TestTrue(TEXT("New weapon appears at its first quantity acquisition"),View[3].Type==ERogueUpgradeType::RocketCount&&View[3].bWeapon);
    F.Pick(ERogueUpgradeType::MoveSpeed);auto Again=RogueOwnedItems::BuildView(F.Player,F.State);TestEqual(TEXT("Duplicate pick does not add a tile"),Again.Num(),4);TestTrue(TEXT("Duplicate updates count without reordering"),Again[1].Type==ERogueUpgradeType::MoveSpeed&&Again[1].Count==2);return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOwnedItemsOwnerMirror,"Rogue.UI.OwnedItems.OwnerOnlyMirrorAndStageCopy",RogueOwnedItemsTests::Flags)
bool FOwnedItemsOwnerMirror::RunTest(const FString&)
{
    RogueOwnedItemsTests::FFixture F;if(!F.Init())return false;F.Pick(ERogueUpgradeType::AttackPower,3);F.Pick(ERogueUpgradeType::Recovery);
    const auto* Field=FindFProperty<FArrayProperty>(ARoguePlayerState::StaticClass(),TEXT("UpgradePickViews"));if(!TestNotNull(TEXT("Pick views use a replicated array, not a replicated map"),Field))return false;
    ARoguePlayerState::StaticClass()->SetUpRuntimeReplicationData();
    TArray<FLifetimeProperty> Props;F.State->GetLifetimeReplicatedProps(Props);const auto* Route=Props.FindByPredicate([Field](const FLifetimeProperty& P){return P.RepIndex==Field->RepIndex;});TestTrue(TEXT("Only the owning connection receives pick history"),Route&&Route->Condition==COND_OwnerOnly);
    auto* Replica=F.W->SpawnActor<ARoguePlayerState>();FRogueOwnedItemsTestAccess::Mirror(Replica,F.State);TestEqual(TEXT("Client mirror rebuilds card counts"),Replica->GetUpgradePickCount(ERogueUpgradeType::AttackPower),3);
    auto* Other=F.W->SpawnActor<ARoguePlayerState>();TestTrue(TEXT("Another player does not share this inventory"),Other->GetUpgradePickViews().IsEmpty());
    F.State->PrepareStageTravel();F.State->CopyProperties(Other);TestEqual(TEXT("Seamless stage copy retains first-acquisition history"),Other->GetUpgradePickViews().Num(),2);TestEqual(TEXT("Seamless stage copy retains stacked count"),Other->GetUpgradePickCount(ERogueUpgradeType::AttackPower),3);
    Other->ResetUpgradeState();TestTrue(TEXT("A fresh run clears the collection mirror"),Other->GetUpgradePickViews().IsEmpty());return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOwnedItemsArt,"Rogue.UI.OwnedItems.AllCardsAndWeaponsHaveArt",RogueOwnedItemsTests::Flags)
bool FOwnedItemsArt::RunTest(const FString&)
{
    auto* Theme=LoadObject<URoguePresentationTheme>(nullptr,TEXT("/Game/GameAssets/ArtDirection/Completion/DA_ArtPresentation.DA_ArtPresentation"));if(!TestNotNull(TEXT("Presentation theme loads"),Theme))return false;
    TSet<const UMaterialInterface*> Unique;int32 Weapons=0;
    for(int32 I=0;I<StaticEnum<ERogueUpgradeType>()->NumEnums()-1;++I)
    {const auto Type=static_cast<ERogueUpgradeType>(I);auto* Icon=Theme->GetCardIcon(Type);TestNotNull(TEXT("Every card has its own icon"),Icon);Unique.Add(Icon);if(RogueOwnedItems::IsWeaponType(Type)){++Weapons;TestNotNull(TEXT("Each weapon has its separate base icon"),Theme->GetWeaponIcon(Type));TestTrue(TEXT("Weapon art differs from its quantity-card art"),Theme->GetWeaponIcon(Type)!=Icon);}}
    TestEqual(TEXT("Every reflected card identity has distinct art"),Unique.Num(),StaticEnum<ERogueUpgradeType>()->NumEnums()-1);TestEqual(TEXT("All eight current weapons have base art"),Weapons,8);return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOwnedItemsMotion,"Rogue.UI.OwnedItems.AttackHopIsBoundedAndDoesNotMoveLayout",RogueOwnedItemsTests::Flags)
bool FOwnedItemsMotion::RunTest(const FString&)
{
    auto* Class=LoadClass<URogueOwnedItemWidget>(nullptr,TEXT("/Game/UMG/HUD/WBP_OwnedItem.WBP_OwnedItem_C"));if(!Class)return false;
    auto* W=NewObject<URogueOwnedItemWidget>(GetTransientPackage(),Class);W->Initialize();FRogueOwnedItemView View;View.Type=ERogueUpgradeType::ProjectileCount;View.bWeapon=true;View.Count=2;W->SetItem(View,nullptr);
    auto* Icon=Cast<UImage>(W->GetWidgetFromName(TEXT("ItemIcon")));auto* Count=Cast<UTextBlock>(W->GetWidgetFromName(TEXT("StackText")));if(!Icon||!Count)return false;
    const auto* Layout=Cast<UCanvasPanelSlot>(Icon->Slot);const FVector2D Base=Layout->GetPosition();W->PlayAttackPulse();W->AdvanceMotion(.016f);TestTrue(TEXT("Firing actually lifts the graphic"),Icon->GetRenderTransform().Translation.Y<0);
    for(int32 I=0;I<120;++I){W->PlayAttackPulse();W->AdvanceMotion(.016f);TestTrue(TEXT("Rapid fire remains bounded"),Icon->GetRenderTransform().Translation.Y>=-W->HopHeight-.01f);}
    for(int32 I=0;I<160;++I)W->AdvanceMotion(.016f);
    TestTrue(TEXT("Graphic returns exactly to its authored position"),Icon->GetRenderTransform().Translation.IsNearlyZero(.001));TestTrue(TEXT("Layout position never hops"),Layout->GetPosition().Equals(Base));TestEqual(TEXT("Attack does not alter the quantity badge"),Count->GetText().ToString(),FString(TEXT("2")));
    FRogueOwnedItemView Actual = View;
    Actual.Count = 4;
    W->SetItem(Actual, nullptr, 2);
    TestEqual(TEXT("Confirmed gameplay count updates while the icon is flying"), W->View.Count, 4);
    TestEqual(TEXT("Visual badge waits for the flight to land"), Count->GetText().ToString(), FString(TEXT("2")));
    W->SetItem(Actual, nullptr);
    TestEqual(TEXT("Landing reveals the actual count"), Count->GetText().ToString(), FString(TEXT("4")));
    View.bWeapon=false;View.Type=ERogueUpgradeType::AttackPower;View.Count=3;W->SetItem(View,nullptr);W->PlayAttackPulse();TestEqual(TEXT("Passive upgrades do not react to weapon attacks"),FRogueOwnedItemsTestAccess::Velocity(W),0.f);TestEqual(TEXT("Stack badge reflects successful picks"),Count->GetText().ToString(),FString(TEXT("3")));return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOwnedItemsReceipts, "Rogue.UI.OwnedItems.ConfirmedReceiptsIncludePassiveAndRepeatedCardsOnce", RogueOwnedItemsTests::Flags)
bool FOwnedItemsReceipts::RunTest(const FString&)
{
    const auto* Class = LoadClass<URogueOwnedItemsWidget>(nullptr,
        TEXT("/Game/UMG/HUD/WBP_OwnedItems.WBP_OwnedItems_C"));
    if (!TestNotNull(TEXT("Actual collection WBP"), Class)) return false;
    auto* Bar = NewObject<URogueOwnedItemsWidget>(GetTransientPackage(), Class);
    Bar->Initialize();
    const FGuid First = FGuid::NewGuid();
    const FGuid Second = FGuid::NewGuid();
    Bar->ConfirmCardAcquired(ERogueUpgradeType::AttackPower, First, 1, 1);
    Bar->ConfirmCardAcquired(ERogueUpgradeType::AttackPower, First, 1, 1);
    Bar->ConfirmCardAcquired(ERogueUpgradeType::AttackPower, Second, 2, 2);
    TestEqual(TEXT("Passive and repeated cards each animate once per receipt"), FRogueOwnedItemsTestAccess::Flights(Bar), 2);
    Bar->ConfirmCardAcquired(ERogueUpgradeType::RocketCount, FGuid(), 1, 1);
    Bar->ConfirmCardAcquired(ERogueUpgradeType::RocketCount, FGuid::NewGuid(), 0, 0);
    TestEqual(TEXT("Invalid or unsuccessful receipts add no flight"), FRogueOwnedItemsTestAccess::Flights(Bar), 2);
    return true;
}
#endif
