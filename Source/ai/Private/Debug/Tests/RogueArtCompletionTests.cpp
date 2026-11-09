#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Components/HorizontalBox.h"
#include "UI/Widgets/RogueOwnedItemsWidget.h"
#include "Tests/AutomationCommon.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Framework/RogueGameMode.h"
#include "Framework/RogueGameState.h"
#include "Player/RogueCharacter.h"
#include "Player/RoguePlayerController.h"
#include "Player/RoguePlayerState.h"
#include "Player/RoguePlayerMotionComponent.h"
#include "AbilitySystemComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "UI/RoguePresentationTheme.h"
#include "UI/Widgets/RogueUpgradeCardWidgetBase.h"
#include "Components/Button.h"
#include "Widgets/SWidget.h"
#include "UI/Widgets/RogueUpgradeTreeNodeWidgetBase.h"
#include "UI/Widgets/RogueBattlePresentationWidget.h"
#include "UI/Widgets/RogueVictoryScreenWidgetBase.h"
#include "World/RogueSceneDetail.h"
#include "UObject/UnrealType.h"

struct FRogueArtCompletionTestAccess
{
    static void Reward(URogueBattlePresentationWidget* W, int32 Amount){W->ShowReward(Amount);}
    static float CueAge(const URogueBattlePresentationWidget* W){return W->Age;}
    static void Step(URogueBattlePresentationWidget* W,float DT){W->NativeTick(FGeometry(),DT);}
    static void Step(URogueVictoryScreenWidgetBase* W,float DT){W->NativeTick(FGeometry(),DT);}
    static void Step(URoguePlayerMotionComponent* M,float DT){M->TickComponent(DT,LEVELTICK_All,nullptr);}
};
namespace RogueArtCompletionTests
{
    constexpr auto Flags=EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter;
    struct FFixture
    {
        FTestWorldWrapper Test;UWorld* World=nullptr;
        bool Init()
        {if(!Test.CreateTestWorld(EWorldType::Game))return false;World=Test.GetTestWorld();World->GetWorldSettings()->DefaultGameMode=ARogueGameMode::StaticClass();if(!Test.BeginPlayInTestWorld())return false;World->GetAuthGameMode()->SetActorTickEnabled(false);return true;}
    };
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArtPlayerPose,"Rogue.Presentation.Completion.PlayerPosePreservesCollisionAndAuthoredScale",RogueArtCompletionTests::Flags)
bool FArtPlayerPose::RunTest(const FString&)
{
    RogueArtCompletionTests::FFixture F;if(!F.Init())return false;
    auto* Class=LoadClass<ARogueCharacter>(nullptr,TEXT("/Game/BP/Player/BP_RogueCharacter.BP_RogueCharacter_C"));
    auto* PC=F.World->SpawnActor<ARoguePlayerController>();
    auto* Player=F.World->SpawnActorDeferred<ARogueCharacter>(Class,FTransform(FVector(0,0,300)),nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    if(!TestNotNull(TEXT("Actual player blueprint spawns"),Player))return false;
    Player->AutoPossessPlayer=EAutoReceiveInput::Disabled;Player->FinishSpawning(FTransform(FVector(0,0,300)));
    PC->GetPlayerState<ARoguePlayerState>()->SetProgressionSnapshotForRun({});PC->Possess(Player);
    Player->SetActorTickEnabled(false);Player->GetCharacterMovement()->SetComponentTickEnabled(false);Player->GetAbilitySystemComponent()->CancelAllAbilities();
    auto* Motion=Player->FindComponentByClass<URoguePlayerMotionComponent>();if(!TestNotNull(TEXT("Player inherits the visual motion component"),Motion))return false;
    TestNotNull(TEXT("Reopened player Blueprint preserves dash launch settings"), Motion->DashLaunch.EffectClass.Get());
    TestNotNull(TEXT("Reopened player Blueprint preserves dash braking settings"), Motion->DashBrake.EffectClass.Get());
    TestNotNull(TEXT("Reopened player Blueprint preserves landing settings"), Motion->LandingFlakes.EffectClass.Get());
    TMap<UStaticMeshComponent*,FTransform> Before;for(auto* M:TInlineComponentArray<UStaticMeshComponent*>(Player))Before.Add(M,M->GetRelativeTransform());
    const FTransform Actor=Player->GetActorTransform();const float Radius=Player->GetCapsuleComponent()->GetScaledCapsuleRadius();
    Motion->PlayCastCue();FRogueArtCompletionTestAccess::Step(Motion,.07f);
    int32 Changed=0;for(const auto& Pair:Before){Changed+=!Pair.Key->GetRelativeTransform().Equals(Pair.Value,.001f);TestTrue(TEXT("Authored static mesh scale is preserved"),Pair.Key->GetRelativeScale3D().Equals(Pair.Value.GetScale3D()));}
    TestTrue(TEXT("A real casting cue poses the visible model"),Changed>0);
    TestTrue(TEXT("Visual motion cannot move the player actor"),Player->GetActorTransform().Equals(Actor));TestEqual(TEXT("Capsule radius is unchanged"),Player->GetCapsuleComponent()->GetScaledCapsuleRadius(),Radius);
    Motion->ResetPose();for(const auto& Pair:Before)TestTrue(TEXT("Reset restores every authored mesh pose"),Pair.Key->GetRelativeTransform().Equals(Pair.Value,.001f));
    Motion->bEnableMotion=false;Motion->PlayCastCue();FRogueArtCompletionTestAccess::Step(Motion,.05f);
    for(const auto& Pair:Before)TestTrue(TEXT("Disabled motion leaves authored poses untouched"),Pair.Key->GetRelativeTransform().Equals(Pair.Value,.001f));return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArtRapidPlayerCast,
    "Rogue.Presentation.Completion.PlayerRapidFireDoesNotInterruptCasting", RogueArtCompletionTests::Flags)
bool FArtRapidPlayerCast::RunTest(const FString&)
{
    RogueArtCompletionTests::FFixture Fixture;
    if (!Fixture.Init()) return false;
    auto* Class = LoadClass<ARogueCharacter>(nullptr, TEXT("/Game/BP/Player/BP_RogueCharacter.BP_RogueCharacter_C"));
    auto* Controller = Fixture.World->SpawnActor<ARoguePlayerController>();
    auto* Player = Fixture.World->SpawnActorDeferred<ARogueCharacter>(Class, FTransform(FVector(0, 0, 300)),
        nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    if (!TestNotNull(TEXT("Real player Blueprint spawns for rapid-fire regression"), Player)) return false;
    Player->AutoPossessPlayer = EAutoReceiveInput::Disabled;
    Player->FinishSpawning(FTransform(FVector(0, 0, 300)));
    Controller->GetPlayerState<ARoguePlayerState>()->SetProgressionSnapshotForRun({});
    Controller->Possess(Player);
    Player->GetCharacterMovement()->SetComponentTickEnabled(false);
    Player->GetAbilitySystemComponent()->CancelAllAbilities();
    auto* Motion = Player->FindComponentByClass<URoguePlayerMotionComponent>();
    UStaticMeshComponent* Body = nullptr;
    for (auto* Mesh : TInlineComponentArray<UStaticMeshComponent*>(Player))
        if (Mesh->GetStaticMesh() && Mesh->GetStaticMesh()->GetName().Contains(TEXT("Blue_Bow_Fairy"))) Body = Mesh;
    if (!TestNotNull(TEXT("Authored Baka body is present"), Body) || !TestNotNull(TEXT("Motion component exists"), Motion)) return false;
    Motion->ResetPose();
    const FTransform Rest = Body->GetRelativeTransform();
    const FTransform Actor = Player->GetActorTransform();
    TArray<FTransform> SamplesAt30Hz;
    for (int32 Rate : {30, 60, 120})
    {
        TArray<FTransform> SingleCast;
        const float Delta = 1.f / Rate;
        Motion->ResetPose();
        Motion->PlayCastCue();
        for (int32 Frame = 0; Frame < Rate * 3 / 5; ++Frame)
        {
            FRogueArtCompletionTestAccess::Step(Motion, Delta);
            SingleCast.Add(Body->GetRelativeTransform());
        }
        TestTrue(TEXT("Single cast completely settles before accepting another"),
            Body->GetRelativeTransform().Equals(Rest, .001f));
        Motion->ResetPose();
        FTransform Previous = Rest;
        for (int32 Frame = 0; Frame < SingleCast.Num(); ++Frame)
        {
            // 模拟多武器及持续光束每帧触发；观察实际模型，而非检查内部计时器。
            Motion->PlayCastCue();
            FRogueArtCompletionTestAccess::Step(Motion, Delta);
            const FTransform Current = Body->GetRelativeTransform();
            TestTrue(TEXT("Dense attacks cannot truncate or restart the current gesture"),
                Current.Equals(SingleCast[Frame], .001f));
            TestTrue(TEXT("Casting stays below 1 cm of whole-body displacement"),
                Current.GetLocation().Equals(Rest.GetLocation(), .85f));
            TestTrue(TEXT("Casting stays below 1.3 degrees of whole-body tilt"),
                FMath::RadiansToDegrees(Current.GetRotation().AngularDistance(Rest.GetRotation())) <= 1.3f);
            TestTrue(TEXT("No abrupt reversal between rendered poses at 30 FPS or higher"),
                FMath::RadiansToDegrees(Current.GetRotation().AngularDistance(Previous.GetRotation())) < .5f);
            if ((Frame + 1) % (Rate / 10) == 0)
            {
                const int32 Index = (Frame + 1) / (Rate / 10) - 1;
                if (Rate == 30) SamplesAt30Hz.Add(Current);
                else TestTrue(TEXT("Sampled cast poses agree across 30, 60 and 120 FPS"),
                    Current.Equals(SamplesAt30Hz[Index], .001f));
            }
            Previous = Current;
        }
    }
    // 动作结束后下一次正常攻击仍能触发；独立关闭施法不影响其余动作组件。
    FRogueArtCompletionTestAccess::Step(Motion, .1f);
    FRogueArtCompletionTestAccess::Step(Motion, .1f);
    Motion->PlayCastCue();
    FRogueArtCompletionTestAccess::Step(Motion, .1f);
    TestFalse(TEXT("A later attack starts a fresh subtle cast"), Body->GetRelativeTransform().Equals(Rest, .001f));
    Motion->ResetPose();
    Motion->bEnableCastMotion = false;
    Motion->PlayCastCue();
    FRogueArtCompletionTestAccess::Step(Motion, .1f);
    TestTrue(TEXT("Independent cast switch leaves body in its authored pose"), Body->GetRelativeTransform().Equals(Rest, .001f));
    TestTrue(TEXT("Cast presentation cannot move the player actor"), Player->GetActorTransform().Equals(Actor));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArtTypedIcons,"Rogue.Presentation.Completion.TypedIconsCoverAllCardsAndTree",RogueArtCompletionTests::Flags)
bool FArtTypedIcons::RunTest(const FString&)
{
    auto* Theme=LoadObject<URoguePresentationTheme>(nullptr,TEXT("/Game/GameAssets/ArtDirection/Completion/DA_ArtPresentation.DA_ArtPresentation"));if(!TestNotNull(TEXT("Editable icon theme exists"),Theme))return false;
    for(int32 I=0;I<StaticEnum<ERogueUpgradeType>()->NumEnums()-1;++I)TestNotNull(TEXT("Every upgrade has a typed pictorial icon"),Theme->GetCardIcon(static_cast<ERogueUpgradeType>(I)));
    auto* Class=LoadClass<URogueUpgradeCardWidgetBase>(nullptr,TEXT("/Game/UMG/Menus/Upgrade/WBP_UpgradeCard.WBP_UpgradeCard_C"));
    auto* Card=NewObject<URogueUpgradeCardWidgetBase>(GetTransientPackage(),Class);Card->Initialize();FRogueUpgradeCardViewData Data;Data.IconType=ERogueUpgradeType::RocketBlastRadius;Card->UpdateUpgradeCardView(Data);
    const TSharedRef<SWidget> CardSlate = Card->TakeWidget();
    auto* Select = Cast<UButton>(Card->GetWidgetFromName(TEXT("Button_Select")));
    if (!TestNotNull(TEXT("The authored card keeps its selection button"), Select)) { return false; }
    TestFalse(TEXT("Clicking an upgrade keeps keyboard focus on the viewport"), Select->TakeWidget()->SupportsKeyboardFocus());
    TestFalse(TEXT("The card wrapper cannot capture keyboard focus either"), CardSlate->SupportsKeyboardFocus());
    auto* Image=Cast<UImage>(Card->GetWidgetFromName(TEXT("ArtIcon")));if(!TestNotNull(TEXT("Illustration is saved in the designer widget tree"),Image))return false;
    TestTrue(TEXT("Exclusive fireball upgrade selects the fireball icon"),Image->GetBrush().GetResourceObject()==Theme->GetCardIcon(Data.IconType));
    TestEqual(TEXT("Illustration never intercepts selection"),Image->GetVisibility(),ESlateVisibility::HitTestInvisible);
    auto* TreeClass=LoadClass<URogueUpgradeTreeNodeWidgetBase>(nullptr,TEXT("/Game/UMG/Menus/Progression/WBP_UpgradeTreeNode.WBP_UpgradeTreeNode_C"));auto* Node=NewObject<URogueUpgradeTreeNodeWidgetBase>(GetTransientPackage(),TreeClass);Node->Initialize();FRogueUpgradeTreeNodeViewData View;View.IconType=ERogueUpgradeType::MaxHealth;Node->SetViewData(View);
    auto* NodeImage=Cast<UImage>(Node->GetWidgetFromName(TEXT("ArtIcon")));TestTrue(TEXT("Tree shares the same typed art theme"),NodeImage&&NodeImage->GetBrush().GetResourceObject()==Theme->GetCardIcon(View.IconType));return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArtBossCue,"Rogue.Presentation.Completion.BossCueDeduplicatesGenerationAndHonorsRunOver",RogueArtCompletionTests::Flags)
bool FArtBossCue::RunTest(const FString&)
{
    RogueArtCompletionTests::FFixture F;if(!F.Init())return false;auto* State=F.World->GetGameState<ARogueGameState>();if(!State)return false;State->SetRunStateSnapshot(1,0,0,false,false);
    auto* Class=LoadClass<URogueBattlePresentationWidget>(nullptr,TEXT("/Game/UMG/HUD/WBP_BattlePresentation.WBP_BattlePresentation_C"));auto* W=NewObject<URogueBattlePresentationWidget>(F.World,Class);W->Initialize();W->RefreshPresentation();
    auto* Property=FindFProperty<FStructProperty>(ARogueGameState::StaticClass(),TEXT("BossHealthView"));auto& Boss=*Property->ContainerPtrToValuePtr<FRogueBossHealthView>(State);Boss.bVisible=true;Boss.SpawnId=FGuid::NewGuid();Boss.CurrentHealth=Boss.MaxHealth=500;
    W->RefreshPresentation();auto* Title=Cast<UTextBlock>(W->GetWidgetFromName(TEXT("CueTitle")));if(!TestNotNull(TEXT("Actual boss announcement WBP binds its title"),Title))return false;TestTrue(TEXT("Boss state starts the announcement"),Title->GetText().ToString().Contains(TEXT("异变显现")));
    FRogueArtCompletionTestAccess::Step(W,.6f);W->RefreshPresentation();TestTrue(TEXT("Repeated replication does not restart the announcement"),FRogueArtCompletionTestAccess::CueAge(W)>.5f);
    FRogueArtCompletionTestAccess::Step(W,3.6f);
    TestEqual(TEXT("Finished announcement stops painting its panel"),W->GetWidgetFromName(TEXT("CuePanel"))->GetVisibility(),ESlateVisibility::Collapsed);
    TestEqual(TEXT("Entry curtain is fully removed after its fade"),W->GetWidgetFromName(TEXT("EntryVeil"))->GetVisibility(),ESlateVisibility::Collapsed);
    Boss.SpawnId=FGuid::NewGuid();W->RefreshPresentation();TestEqual(TEXT("A newly spawned boss can announce again"),FRogueArtCompletionTestAccess::CueAge(W),0.f);
    Boss.bVisible=false;W->RefreshPresentation();TestEqual(TEXT("Leaving the live boss state plays the short closing cue"),Title->GetText().ToString(),FString(TEXT("符阵退治")));

    State->SetRunStateSnapshot(2, 0, 10.f, false, false);
    W->RefreshPresentation();
    TestEqual(TEXT("The next wave shows its actual number"), Title->GetText().ToString(), FString(TEXT("第 2 波")));
    FRogueArtCompletionTestAccess::Step(W, .4f);
    W->RefreshPresentation();
    TestTrue(TEXT("Repeated wave snapshots do not restart the banner"), FRogueArtCompletionTestAccess::CueAge(W) > .3f);

    Boss.bVisible = true;
    Boss.SpawnId = FGuid::NewGuid();
    W->RefreshPresentation();
    TestTrue(TEXT("A later Boss update keeps the current wave number on screen"),
        Title->GetText().ToString().StartsWith(TEXT("第 2 波 · ")) && Title->GetText().ToString().Contains(TEXT("异变显现")));
    FRogueArtCompletionTestAccess::Step(W, 3.6f);
    Boss.bVisible = false;
    FRogueBattleStageView Stage;
    Stage.Phase = ERogueBattleStage::Countdown;
    State->SetBattleStage(Stage);
    State->SetRunStateSnapshot(3, 0, 20.f, false, false);
    W->RefreshPresentation();
    TestEqual(TEXT("The travel countdown does not show a wave banner"),
        W->GetWidgetFromName(TEXT("CuePanel"))->GetVisibility(), ESlateVisibility::Collapsed);
    Stage.Phase = ERogueBattleStage::Night;
    State->SetBattleStage(Stage);
    W->RefreshPresentation();
    TestEqual(TEXT("Combat resumes with the next wave number"), Title->GetText().ToString(), FString(TEXT("第 3 波")));
    State->SetRunStateSnapshot(3, 0, 20.f, true, false);
    W->RefreshPresentation();
    TestTrue(TEXT("Game over cancels pending battle announcements"), FRogueArtCompletionTestAccess::CueAge(W) > 10.f);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArtScenicBudget,"Rogue.Presentation.Completion.ScenicClassesHaveNoTickCollisionOrNavigation",RogueArtCompletionTests::Flags)
bool FArtScenicBudget::RunTest(const FString&)
{
    for(const TCHAR* Theme:{TEXT("Forest"),TEXT("SakuraShrine"),TEXT("MistyIceLake"),TEXT("MoonBamboo"),TEXT("ScarletClock")})for(const TCHAR* Light:{TEXT("Day"),TEXT("Night")})
    {
        const FString Name=FString::Printf(TEXT("BP_Detail_%s_%s"),Theme,Light);const FString Path=FString::Printf(TEXT("/Game/BP/Environment/ThemeDetails/%s.%s_C"),*Name,*Name);auto* Class=LoadClass<ARogueSceneDetail>(nullptr,*Path);if(!TestNotNull(*Path,Class))return false;auto* A=Class->GetDefaultObject<ARogueSceneDetail>();
        TestFalse(TEXT("Decorations add no actor Tick"),A->PrimaryActorTick.bCanEverTick);TestFalse(TEXT("Decorations add no replication"),A->GetIsReplicated());TestFalse(TEXT("Decorations add no actor collision"),A->GetActorEnableCollision());
        for(auto* C:{A->Props.Get(),A->GroundAccents.Get()}){TestNotNull(TEXT("Saved scenic component has its mesh"),C->GetStaticMesh().Get());TestEqual(TEXT("Scenery cannot block players or enemy bullets"),C->GetCollisionEnabled(),ECollisionEnabled::NoCollision);TestFalse(TEXT("Scenery cannot carve navigation"),C->CanEverAffectNavigation());}
    }
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FArtResultReveal,"Rogue.Presentation.Completion.VictoryEntranceDoesNotRestartOnViewRefresh",RogueArtCompletionTests::Flags)
bool FArtResultReveal::RunTest(const FString&)
{
    auto* Class=LoadClass<URogueVictoryScreenWidgetBase>(nullptr,TEXT("/Game/UMG/Menus/InGame/WBP_VictoryScreen.WBP_VictoryScreen_C"));auto* W=NewObject<URogueVictoryScreenWidgetBase>(GetTransientPackage(),Class);W->Initialize();FRogueRunResult R;R.Outcome=ERogueRunOutcome::Victory;R.Wave=100;
    W->UpdateVictoryView(R);TestEqual(TEXT("Victory begins transparent"),W->GetRenderOpacity(),0.f);FRogueArtCompletionTestAccess::Step(W,.2f);const float Mid=W->GetRenderOpacity();W->UpdateVictoryView(R);TestTrue(TEXT("Refresh keeps the current entrance progress"),Mid>0&&FMath::IsNearlyEqual(Mid,W->GetRenderOpacity(),.0001f));FRogueArtCompletionTestAccess::Step(W,.25f);TestEqual(TEXT("Result settles fully visible"),W->GetRenderOpacity(),1.f);return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueJourneyPresentationBindings, "Rogue.Presentation.Completion.JourneyArtAndNonBlockingFeedback", RogueArtCompletionTests::Flags)
bool FRogueJourneyPresentationBindings::RunTest(const FString&)
{
    RogueArtCompletionTests::FFixture Fixture;
    if (!Fixture.Init()) return false;
    auto* State = Fixture.World->GetGameState<ARogueGameState>();
    if (!State) return false;
    const auto* Class = LoadClass<URogueBattlePresentationWidget>(nullptr,
        TEXT("/Game/UMG/HUD/WBP_BattlePresentation.WBP_BattlePresentation_C"));
    if (!TestNotNull(TEXT("Battle WBP loads"), Class)) return false;
    auto* View = NewObject<URogueBattlePresentationWidget>(Fixture.World, Class);
    View->Initialize();
    auto* Dusk = Cast<UImage>(View->GetWidgetFromName(TEXT("DuskArt")));
    auto* Dawn = Cast<UImage>(View->GetWidgetFromName(TEXT("DawnArt")));
    auto* Reward = Cast<UTextBlock>(View->GetWidgetFromName(TEXT("RewardAmount")));
    auto* Panel = View->GetWidgetFromName(TEXT("RewardPanel"));
    if (!TestNotNull(TEXT("Dusk image"), Dusk) || !TestNotNull(TEXT("Dawn image"), Dawn) ||
        !TestNotNull(TEXT("Receipt amount"), Reward) || !TestNotNull(TEXT("Receipt panel"), Panel)) return false;
    TestTrue(TEXT("Dawn and dusk have distinct textures"), Dusk->GetBrush().GetResourceObject() &&
        Dawn->GetBrush().GetResourceObject() && Dusk->GetBrush().GetResourceObject() != Dawn->GetBrush().GetResourceObject());
    FRogueBattleStageView Stage;
    Stage.Phase = ERogueBattleStage::Countdown;
    Stage.CountdownDeadline = State->GetServerWorldTimeSeconds() + 20.0;
    State->SetBattleStage(Stage);
    View->RefreshPresentation();
    TestEqual(TEXT("Countdown remains unobscured until the final second"), Dusk->GetVisibility(), ESlateVisibility::Collapsed);
    Stage.CountdownDeadline = State->GetServerWorldTimeSeconds() + .3;
    State->SetBattleStage(Stage);
    View->RefreshPresentation();
    TestEqual(TEXT("Transition art never captures input"), Dusk->GetVisibility(), ESlateVisibility::HitTestInvisible);
    Stage.bNextStageIsNight = false;
    State->SetBattleStage(Stage);
    View->RefreshPresentation();
    TestEqual(TEXT("Day transition uses the dawn illustration"), Dawn->GetVisibility(), ESlateVisibility::HitTestInvisible);
    TestEqual(TEXT("Unused chapter art is hidden"), Dusk->GetVisibility(), ESlateVisibility::Collapsed);
    FRogueArtCompletionTestAccess::Reward(View, 2);
    FRogueArtCompletionTestAccess::Reward(View, 3);
    FRogueArtCompletionTestAccess::Step(View, .3f);
    TestTrue(TEXT("Overlapping positive receipts accumulate"), Reward->GetText().ToString().Contains(TEXT("+5")));
    TestEqual(TEXT("Receipt never captures input"), Panel->GetVisibility(), ESlateVisibility::HitTestInvisible);
    FRogueArtCompletionTestAccess::Step(View, 2.4f);
    TestEqual(TEXT("Receipt collapses when the animation ends"), Panel->GetVisibility(), ESlateVisibility::Collapsed);

    for (const TCHAR* Screen : {TEXT("WBP_VictoryScreen"), TEXT("WBP_DeathScreen")})
    {
        const FString Path = FString::Printf(TEXT("/Game/UMG/Menus/InGame/%s.%s_C"), Screen, Screen);
        const auto* ScreenClass = LoadClass<UUserWidget>(nullptr, *Path);
        if (!TestNotNull(*Path, ScreenClass)) continue;
        auto* Widget = NewObject<UUserWidget>(Fixture.World, ScreenClass);
        Widget->Initialize();
        auto* Icons = Cast<UHorizontalBox>(Widget->GetWidgetFromName(TEXT("PersonalRunLoadout")));
        TestTrue(TEXT("Settlement binds a caption and a slot for every weapon"),
            Widget->GetWidgetFromName(TEXT("PersonalRunReward")) && Icons && Icons->GetChildrenCount() == 8);
    }
    const auto* InventoryClass = LoadClass<URogueOwnedItemsWidget>(nullptr,
        TEXT("/Game/UMG/HUD/WBP_OwnedItems.WBP_OwnedItems_C"));
    if (!TestNotNull(TEXT("Inventory WBP loads"), InventoryClass)) return false;
    auto* Inventory = NewObject<URogueOwnedItemsWidget>(Fixture.World, InventoryClass);
    Inventory->Initialize();
    TestNotNull(TEXT("Acquisition image is editable in the WBP"), Inventory->GetWidgetFromName(TEXT("AcquisitionIcon")));
    return true;
}

#endif
