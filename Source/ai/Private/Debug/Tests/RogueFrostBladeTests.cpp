#if WITH_DEV_AUTOMATION_TESTS
#include "RogueTestWorld.h"
#include "AbilitySystemComponent.h"
#include "Audio/RogueAttackAudioSettings.h"
#include "Components/CapsuleComponent.h"
#include "Progression/Upgrades/RogueUpgradeDefinitionAsset.h"
#include "Progression/Upgrades/RogueUpgradeSystem.h"
#include "Progression/Upgrades/RogueUpgradeRuleAsset.h"
#include "Progression/Shop/RogueShopSystem.h"
#include "AbilitySystem/RogueAttributeSet.h"
#include "Combat/Weapons/RogueWeapon_FrostBlade.h"
#include "Combat/Projectiles/RogueFrostBlade.h"
#include "Combat/Projectiles/RogueFrostSwordWave.h"
#include "Combat/Targeting/RogueEnemyTrackerSubsystem.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
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
#include "Progression/Upgrades/RogueUpgradeSystemRules.h"
#include "Tests/AutomationCommon.h"
#include "UObject/UnrealType.h"
namespace FrostBladeTests
{
constexpr auto Flags=EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter;
struct FFixture
{
 FTestWorldWrapper TestWorld;UWorld* World=nullptr;ARogueCharacter* Player=nullptr;ARogueWeapon_FrostBlade* Weapon=nullptr;
 FTestConsoleVariable SelfTest{TEXT("Rogue.SelfTest.WeaponUpgradeChain")},Stress{TEXT("Rogue.SelfTest.EnemyStress")};
 bool Init(bool bUnlocked=true,int32 BaseAttackRank=0)
 {
  SelfTest.Set(TEXT("0"));Stress.Set(TEXT("0"));if (!RogueTestWorld::Begin(TestWorld, World)) { return false; }auto* PC=World->SpawnActor<ARoguePlayerController>();
  FTransform T(FVector(0,0,200));Player=World->SpawnActorDeferred<ARogueCharacter>(ARogueCharacter::StaticClass(),T,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
  auto* Table=NewObject<UDataTable>(World);Table->RowStruct=FRogueWeaponTableRow::StaticStruct();FRogueWeaponTableRow R;
  R.WeaponClass=ARogueWeapon_FrostBlade::StaticClass();R.Count=0;R.CountUpgradeType=ERogueUpgradeType::FrostBladeCount;R.Range=450;R.Damage=25;R.Cooldown=1.5f;R.MinCooldown=.7f;
  Table->AddRow(TEXT("FrostBlade"),R);FindFProperty<FObjectProperty>(ARogueCharacter::StaticClass(),TEXT("WeaponDataTable"))->SetObjectPropertyValue_InContainer(Player,Table);
  Player->AutoPossessPlayer=EAutoReceiveInput::Disabled;Player->FinishSpawning(T);FRogueProgressionSnapshot Profile;
  for(const auto& N:URogueUpgradeTreeDefinition::GetDefaultDefinition()->GetNodes())if(bUnlocked&&N.Kind!=ERogueMetaNodeKind::BaseStat)Profile.NodeRanks.Add(N.NodeId,1);
  if(BaseAttackRank>0)Profile.NodeRanks.Add(TEXT("Stat.AttackPower"),BaseAttackRank);
  PC->GetPlayerState<ARoguePlayerState>()->SetProgressionSnapshotForRun(Profile);PC->Possess(Player);
  Player->SetActorTickEnabled(false);Player->GetCharacterMovement()->SetComponentTickEnabled(false);Player->SetActorEnableCollision(false);Player->GetAbilitySystemComponent()->CancelAllAbilities();
  Player->GetRogueAttributeSet()->SetWeaponBonusCaps(FRogueWeaponBonusCaps());Weapon=Player->FindWeapon<ARogueWeapon_FrostBlade>();return Weapon!=nullptr;
 }
 void Upgrade(ERogueUpgradeType T,float M=1){FRogueUpgradeOption O;O.Type=T;O.Magnitude=M;Player->ApplyUpgrade(O);}
 void Step(float S){for(float T=0;T<S;T+=.01f)TestWorld.TickTestWorld(.01f);}
 ARogueEnemy* Enemy(FVector L)
 {
  FActorSpawnParameters P;P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;auto* E=World->SpawnActor<ARogueEnemy>(ARogueEnemy::StaticClass(),L,FRotator::ZeroRotator,P);
  E->ActivatePooledEnemy(nullptr,L,FRotator::ZeroRotator);FRogueEnemyProfile R;R.Health=1000;E->InitializeEnemy(R);E->SetActorTickEnabled(false);E->GetCharacterMovement()->SetComponentTickEnabled(false);
  World->GetSubsystem<URogueEnemyTrackerSubsystem>()->RegisterEnemy(E);return E;
 }
 AActor* Wall(float X)
 {
  auto* A=World->SpawnActor<AActor>();auto* B=NewObject<UBoxComponent>(A);A->SetRootComponent(B);B->SetBoxExtent(FVector(10,500,400));B->SetCollisionObjectType(ECC_WorldStatic);B->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);B->SetCollisionResponseToAllChannels(ECR_Block);B->RegisterComponent();A->SetActorLocation(FVector(X,0,200));return A;
 }
 ARogueFrostBlade* Blade(){for(TActorIterator<ARogueFrostBlade> I(World);I;++I)return *I;return nullptr;}
 ARogueFrostSwordWave* Wave(){return World->GetSubsystem<URogueFrostWavePool>()->Acquire(nullptr);}
 float HP(ARogueEnemy* E){return E->GetAbilitySystemComponent()->GetNumericAttribute(URogueAttributeSet::GetHealthAttribute());}
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrostSingle,"Rogue.Gameplay.FrostBlade.SingleAcquisitionAndActualGASUpgrades",FrostBladeTests::Flags)
bool FFrostSingle::RunTest(const FString&)
{
 FrostBladeTests::FFixture F;if(!F.Init())return false;
 TestEqual(TEXT("Permanent unlock equips the blade before any card is chosen"),F.Weapon->GetEffectiveCount(),1);F.Upgrade(ERogueUpgradeType::FrostBladeCount);F.Upgrade(ERogueUpgradeType::FrostBladeCount);
 TestEqual(TEXT("Still exactly one blade"),F.Weapon->GetEffectiveCount(),1);TestFalse(TEXT("Acquisition removed once owned"),RogueUpgradeSystemRules::IsUpgradeAllowedForProgression(ERogueUpgradeType::FrostBladeCount,F.Player));
 TestEqual(TEXT("No wave before unlock"),F.Weapon->GetEffectiveWaveCount(),0);TestFalse(TEXT("Wave count requires actual wave"),RogueUpgradeSystemRules::IsUpgradeAllowedForProgression(ERogueUpgradeType::FrostBladeWaveCount,F.Player));
 F.Upgrade(ERogueUpgradeType::FrostBladeSize,.2f);F.Upgrade(ERogueUpgradeType::FrostBladeSize,.2f);TestTrue(TEXT("Size stacks through GAS"),FMath::IsNearlyEqual(F.Weapon->GetEffectiveBladeScale(),1.4f));
 F.Upgrade(ERogueUpgradeType::FrostBladeWave);F.Upgrade(ERogueUpgradeType::FrostBladeWaveCount,2);F.Upgrade(ERogueUpgradeType::FrostBladeWaveSize,.2f);
 TestFalse(TEXT("Wave unlock cannot be offered again even without definition asset"),RogueUpgradeSystemRules::IsUpgradeAllowedForProgression(ERogueUpgradeType::FrostBladeWave,F.Player));
 TestEqual(TEXT("Unlock plus two count cards gives three waves"),F.Weapon->GetEffectiveWaveCount(),3);TestTrue(TEXT("Wave size stacks independently"),FMath::IsNearlyEqual(F.Weapon->GetEffectiveWaveScale(),1.2f));return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrostMotion,"Rogue.Gameplay.FrostBlade.VerticalHoverHorizontalSlashReturnAndLeash",FrostBladeTests::Flags)
bool FFrostMotion::RunTest(const FString&)
{
 FrostBladeTests::FFixture F;if(!F.Init())return false;F.Upgrade(ERogueUpgradeType::FrostBladeCount);F.Weapon->WeaponTick(0);F.Step(.02f);auto* B=F.Blade();if(!TestNotNull(TEXT("Blade spawned"),B))return false;
 TestTrue(TEXT("Handle up, blade tip down"),B->GetActorForwardVector().Z<-.99f);TestTrue(TEXT("Hovers behind player"),B->GetActorLocation().X<F.Player->GetActorLocation().X);
 auto* E=F.Enemy(FVector(270,0,200));F.Weapon->WeaponTick(0);F.Step(.18f);TestEqual(TEXT("Approach enters slash"),B->GetPhase(),ERogueFrostBladePhase::Slash);TestTrue(TEXT("Blade is horizontal"),FMath::Abs(B->GetActorForwardVector().Z)<.01f);
 TestTrue(TEXT("Attack origin on player side of enemy"),B->GetActorLocation().X<E->GetActorLocation().X);F.Player->SetActorLocation(FVector(-900,0,200));F.Step(.05f);
 TestTrue(TEXT("No unlimited flight when player moves"),FVector::Dist(B->GetActorLocation(),F.Player->GetActorLocation())<=331);F.Step(.6f);TestEqual(TEXT("Returns to hover"),B->GetPhase(),ERogueFrostBladePhase::Hover);
 F.Weapon->OnOwnerDied();TestTrue(TEXT("Owner death removes blade"),!IsValid(B));return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrostSlash,"Rogue.Gameplay.FrostBlade.SlashDamagesMultipleEnemiesOnceAndRejectsWalls",FrostBladeTests::Flags)
bool FFrostSlash::RunTest(const FString&)
{
 FrostBladeTests::FFixture F;if(!F.Init())return false;F.Upgrade(ERogueUpgradeType::FrostBladeCount);auto* E=F.Enemy(FVector(270,0,200));auto* E2=F.Enemy(FVector(290,50,200));F.Weapon->WeaponTick(0);F.Step(.8f);
 TestTrue(TEXT("Slash really hits first enemy"),FMath::IsNearlyEqual(F.HP(E),975,.1f));TestTrue(TEXT("Slash hits nearby second enemy exactly once"),FMath::IsNearlyEqual(F.HP(E2),975,.1f));
 auto* W=F.Wall(150);TestFalse(TEXT("Cannot start attack through wall"),F.Blade()->StartAttack(E,F.Weapon,25,0,10,1,1000,nullptr));W->Destroy();return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrostWavePenetration,"Rogue.Gameplay.FrostBlade.WavePenetratesEnemiesHitsOnceAndStopsAtWall",FrostBladeTests::Flags)
bool FFrostWavePenetration::RunTest(const FString&)
{
 FrostBladeTests::FFixture F;if(!F.Init())return false;auto* A=F.Enemy(FVector(200,0,200));auto* B=F.Enemy(FVector(350,0,200));auto* C=F.Enemy(FVector(800,0,200));F.Wall(600);auto* W=F.Wave();
 W->Launch(F.Player,FVector(0,0,200),FVector::ForwardVector,1000,10,1,30,120,30,30);F.Step(.45f);
 TestFalse(TEXT("Enemies do not consume wave"),W->IsAvailableInPool());TestTrue(TEXT("First enemy hit exactly once"),FMath::IsNearlyEqual(F.HP(A),990,.1f));TestTrue(TEXT("Second enemy hit too"),FMath::IsNearlyEqual(F.HP(B),990,.1f));F.Step(.4f);
 TestTrue(TEXT("Map stops wave"),W->IsAvailableInPool());TestTrue(TEXT("Enemy behind wall untouched"),FMath::IsNearlyEqual(F.HP(C),1000,.1f));return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrostWaveReuse,"Rogue.Gameplay.FrostBlade.WaveThirtySecondsSizeAndPoolReuse",FrostBladeTests::Flags)
bool FFrostWaveReuse::RunTest(const FString&)
{
 FrostBladeTests::FFixture F;if(!F.Init())return false;TestEqual(TEXT("Default lifetime matches other bullets"),FRogueFrostBladeCombatSettings().WaveLifetime,30.f);
 auto* E=F.Enemy(FVector(180,180,200));auto* W=F.Wave();W->Launch(F.Player,FVector(0,0,200),FVector::ForwardVector,1000,10,1,30,100,20,25);F.Step(.3f);TestEqual(TEXT("Outside base width"),F.HP(E),1000.f);
 W->Deactivate();auto* Reused=F.Wave();TestEqual(TEXT("Reuses inactive actor"),Reused,W);W->Launch(F.Player,FVector(0,0,200),FVector::ForwardVector,1000,10,2,30,100,20,25);F.Step(.3f);TestTrue(TEXT("Size increases real damage width"),FMath::IsNearlyEqual(F.HP(E),990,.1f));
 W->Deactivate();W->Launch(F.Player,FVector(0,0,200),FVector::ForwardVector,1000,10,2,.08f,100,20,25);F.Step(.1f);TestTrue(TEXT("Lifetime returns wave to pool"),W->IsAvailableInPool());
 W->Launch(F.Player,FVector(0,0,200),FVector::ForwardVector,1000,10,2,30,100,20,25);F.Step(.3f);TestTrue(TEXT("Pool reuse clears previous hit history"),FMath::IsNearlyEqual(F.HP(E),980,.1f));W->Deactivate();
 auto* Floor=F.World->SpawnActor<AActor>();auto* Box=NewObject<UBoxComponent>(Floor);Floor->SetRootComponent(Box);Box->SetBoxExtent(FVector(5000,5000,10));Box->SetCollisionObjectType(ECC_WorldStatic);Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);Box->SetCollisionResponseToAllChannels(ECR_Block);Box->RegisterComponent();
 W->Launch(F.Player,FVector(0,0,200),FVector::ForwardVector,1000,0,10,30,100,20,25);F.Step(.1f);TestFalse(TEXT("Planar size growth does not accidentally collide with ground"),W->IsAvailableInPool());return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrostBiggerSlash,"Rogue.Gameplay.FrostBlade.SizeUpgradeReachesEnemyOutsideBaseRange",FrostBladeTests::Flags)
bool FFrostBiggerSlash::RunTest(const FString&)
{
 FrostBladeTests::FFixture F;if(!F.Init())return false;F.Upgrade(ERogueUpgradeType::FrostBladeCount);auto* E=F.Enemy(FVector(520,0,200));F.Weapon->WeaponTick(0);F.Step(.8f);
 TestEqual(TEXT("Enemy outside original range untouched"),F.HP(E),1000.f);F.Upgrade(ERogueUpgradeType::FrostBladeSize,.4f);F.Weapon->WeaponTick(0);F.Step(.8f);
 TestTrue(TEXT("Larger blade increases real damage reach"),FMath::IsNearlyEqual(F.HP(E),1000-F.Weapon->GetEffectiveDamage(),.1f));return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrostProgression,"Rogue.Gameplay.FrostBlade.FiveIceBrickNodesCannotSkipAndProfilesStaySeparate",FrostBladeTests::Flags)
bool FFrostProgression::RunTest(const FString&)
{
 const auto* T=URogueUpgradeTreeDefinition::GetDefaultDefinition();FRogueProgressionSnapshot A,B;
 const FName Ids[]={TEXT("Weapon.FrostBlade"),TEXT("Card.FrostBladeSize"),TEXT("Card.FrostBladeWave"),TEXT("Card.FrostBladeWaveCount"),TEXT("Card.FrostBladeWaveSize")};
 for(int32 I=0;I<5;++I)
 {
  const auto* N=T->FindNode(Ids[I]);if(!TestNotNull(TEXT("Authored node exists"),N))return false;TestEqual(TEXT("Every node costs five bricks"),N->Cost,5);
  TestFalse(TEXT("Four bricks cannot buy node"),T->GetPurchaseFailure(A,4,Ids[I]).IsEmpty());
  if(I<4)TestFalse(TEXT("Cannot skip to next node"),T->GetPurchaseFailure(A,100,Ids[I+1]).IsEmpty());
  TestTrue(TEXT("Five bricks and completed prerequisites allow next node"),T->GetPurchaseFailure(A,5,Ids[I]).IsEmpty());A.NodeRanks.Add(Ids[I],1);
 }
 TestTrue(TEXT("Own profile has weapon permission"),T->IsWeaponUnlocked(A,ERogueUpgradeType::FrostBladeCount));TestFalse(TEXT("Another profile stays locked"),T->IsWeaponUnlocked(B,ERogueUpgradeType::FrostBladeCount));
 B.NodeRanks.Add(Ids[4],1);TestEqual(TEXT("Orphaned card removed from loaded profile"),T->GetNodeRank(T->SanitizeSnapshot(B),Ids[4]),0);return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrostCycle,"Rogue.Gameplay.FrostBlade.WaveCanHitReusedEnemyGenerationAndCancelsPooledTarget",FrostBladeTests::Flags)
bool FFrostCycle::RunTest(const FString&)
{
 FrostBladeTests::FFixture F;if(!F.Init())return false;auto* E=F.Enemy(FVector(130,0,200));auto* W=F.Wave();W->Launch(F.Player,FVector(0,0,200),FVector::ForwardVector,1000,10,1,30,120,30,30);F.Step(.2f);
 TestTrue(TEXT("First spawn damaged once"),FMath::IsNearlyEqual(F.HP(E),990,.1f));E->DeactivateToPool();E->ActivatePooledEnemy(nullptr,FVector(420,0,200),FRotator::ZeroRotator);FRogueEnemyProfile R;R.Health=1000;E->InitializeEnemy(R);F.Step(.3f);
 TestTrue(TEXT("Same actor with fresh spawn ID can be hit"),FMath::IsNearlyEqual(F.HP(E),990,.1f));F.World->GetSubsystem<URogueEnemyTrackerSubsystem>()->RegisterEnemy(E);
 F.Upgrade(ERogueUpgradeType::FrostBladeCount);F.Weapon->WeaponTick(0);E->DeactivateToPool();F.Step(.2f);auto* Blade=F.Blade();if(!TestNotNull(TEXT("Blade alive"),Blade))return false;
 TestEqual(TEXT("Pooled target cancels approach"),Blade->GetPhase(),ERogueFrostBladePhase::Return);return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrostArtPose,"Rogue.Presentation.FrostArt.GiantBladeClearsFeetAndTrailHasRelease",FrostBladeTests::Flags)
bool FFrostArtPose::RunTest(const FString&)
{
 FrostBladeTests::FFixture F;if(!F.Init())return false;
 F.Weapon->GetConfig().FrostBladeClass=LoadClass<ARogueFrostBlade>(nullptr,TEXT("/Game/BP/Projectiles/FrostBlade/BP_RogueFrostBlade.BP_RogueFrostBlade_C"));
 F.Upgrade(ERogueUpgradeType::FrostBladeCount);F.Upgrade(ERogueUpgradeType::FrostBladeSize,2);F.Weapon->WeaponTick(0);F.Step(.03f);auto* B=F.Blade();if(!B)return false;
 const FVector Tip=B->Mesh->GetComponentTransform().TransformPosition(FVector(190,0,0));const float Feet=F.Player->GetActorLocation().Z-F.Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
 TestTrue(TEXT("Three-times blade tip stays above feet"),Tip.Z>Feet+10);TestTrue(TEXT("Real V2 model assigned"),B->Mesh->GetStaticMesh()!=nullptr);
 F.Enemy(FVector(300,0,200));F.Weapon->WeaponTick(0);F.Step(.24f);TestTrue(TEXT("Real swing shows trail"),B->SlashTrail->IsVisible());
 const auto& Data=B->SlashTrail->GetCustomPrimitiveData().Data;TestTrue(TEXT("Trail only reveals the angle actually swept"),Data.Num()>2&&Data[2]>0&&Data[2]<=1);
 F.Step(.22f);TestTrue(TEXT("Returning blade leaves a short frozen afterimage"),B->Afterimage->IsVisible());TestFalse(TEXT("Main trail ends with slash"),B->SlashTrail->IsVisible());F.Step(.2f);TestFalse(TEXT("Afterimage releases without lingering"),B->Afterimage->IsVisible());return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrostAudioTiming,"Rogue.Presentation.FrostArt.AudioOnlyAtConfirmedSlashAndAbortedApproachIsSilent",FrostBladeTests::Flags)
bool FFrostAudioTiming::RunTest(const FString&)
{
 FrostBladeTests::FFixture F;if(!F.Init())return false;int32 Calls=0;
 const auto Hook=RogueAttackAudio::OnPrepared().AddLambda([&](const UObject*,const AActor* Source,const FRogueAttackAudioRequest&){if(Source==F.Weapon)++Calls;});
 F.Upgrade(ERogueUpgradeType::FrostBladeCount);auto* E=F.Enemy(FVector(270,0,200));F.Weapon->WeaponTick(0);F.Step(.08f);TestEqual(TEXT("Approach has not fired audio or attack pulse"),Calls,0);
 F.Step(.12f);TestEqual(TEXT("Slash confirms exactly one centralized sound"),Calls,1);F.Step(.7f);F.Weapon->OnOwnerDied();
 F.Weapon->WeaponTick(0);E->DeactivateToPool();F.Step(.7f);TestEqual(TEXT("Canceled approach creates no false sound"),Calls,1);RogueAttackAudio::OnPrepared().Remove(Hook);return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrostShopSync,"Rogue.Gameplay.FrostBlade.RealShopHonorsSingleBladeAndWavePrerequisites",FrostBladeTests::Flags)
bool FFrostShopSync::RunTest(const FString&)
{
 FrostBladeTests::FFixture F;if(!F.Init())return false;FRogueUpgradeSystem Up;auto* Definitions=NewObject<URogueUpgradeDefinitionAsset>(F.World);
 Definitions->UpgradeDefinitionTable=LoadObject<UDataTable>(nullptr,TEXT("/Game/Data/Upgrades/DT_UpgradeDefinitions.DT_UpgradeDefinitions"));Up.SetDefinitionAsset(Definitions);Up.SetRuleAsset(NewObject<URogueUpgradeRuleAsset>(F.World));F.Player->AddMoney(5000);
 FRogueShopSystem Shop;Shop.Configure(64,1,0,5);Shop.OpenShop(F.Player,Up);
 auto Find=[&](ERogueUpgradeType Type){return Shop.GetOffers().IndexOfByPredicate([&](const auto& O){return O.GetUpgrade().Type==Type;});};
 auto Buy=[&](ERogueUpgradeType Type){const int32 I=Find(Type);if(I==INDEX_NONE)return false;const auto Offer=Shop.GetOffers()[I];FRogueUpgradeOption Chosen;if(!Shop.TryBuyOffer(I,Offer.QuoteId,Offer.GetCost(),F.Player,Up,Chosen))return false;F.Player->ApplyUpgrade(Chosen);return true;};
 TestEqual(TEXT("Automatic starting blade has no acquisition offer"),Find(ERogueUpgradeType::FrostBladeCount),INDEX_NONE);
 TestEqual(TEXT("An unlocked profile starts with exactly one blade"),F.Weapon->GetEffectiveCount(),1);
 TestTrue(TEXT("Size and wave unlock can now appear"),Find(ERogueUpgradeType::FrostBladeSize)!=INDEX_NONE&&Find(ERogueUpgradeType::FrostBladeWave)!=INDEX_NONE);TestEqual(TEXT("Wave count not offered prematurely"),Find(ERogueUpgradeType::FrostBladeWaveCount),INDEX_NONE);
 TestTrue(TEXT("Actual quote enables sword wave"),Buy(ERogueUpgradeType::FrostBladeWave));Shop.TryRefresh(F.Player,Up);TestEqual(TEXT("Wave unlock is also one-time"),Find(ERogueUpgradeType::FrostBladeWave),INDEX_NONE);TestTrue(TEXT("Both later wave cards now enter shop"),Find(ERogueUpgradeType::FrostBladeWaveCount)!=INDEX_NONE&&Find(ERogueUpgradeType::FrostBladeWaveSize)!=INDEX_NONE);return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrostWallArt,"Rogue.Presentation.FrostArt.WallStopPlaysOnceAndPoolReuseRestoresArt",FrostBladeTests::Flags)
bool FFrostWallArt::RunTest(const FString&)
{
 FrostBladeTests::FFixture F;if(!F.Init())return false;auto* Cl=LoadClass<ARogueFrostSwordWave>(nullptr,TEXT("/Game/BP/Projectiles/FrostBlade/BP_RogueFrostSwordWave.BP_RogueFrostSwordWave_C"));
 auto* W=F.World->GetSubsystem<URogueFrostWavePool>()->Acquire(Cl);auto* Wall=F.Wall(300);W->Launch(F.Player,FVector(0,0,200),FVector::ForwardVector,1000,1,1,30,240,55,30);F.Step(.28f);
 TestTrue(TEXT("Wall stops actual BP wave"),W->IsAvailableInPool());auto Count=[&](){int32 N=0;for(TActorIterator<ARogueCombatAccentEffect> I(F.World);I;++I)if(!I->IsAvailableInPool())++N;return N;};
 const int32 First=Count();TestTrue(TEXT("Wall creates authored ice shards"),First>0);W->ProcessEvent(W->FindFunctionChecked(TEXT("OnRep_State")),nullptr);TestEqual(TEXT("Repeated replicated state does not duplicate wall shards"),Count(),First);
 Wall->Destroy();auto* Again=F.World->GetSubsystem<URogueFrostWavePool>()->Acquire(Cl);TestEqual(TEXT("Same art actor is reused"),Again,W);Again->Launch(F.Player,FVector(0,0,200),FVector::ForwardVector,1000,1,1.4f,30,240,55,30);F.Step(.1f);
 TestTrue(TEXT("Mesh scale stays relative to authored blueprint"),Again->Mesh->GetRelativeScale3D().Equals(FVector(1.4),.001));TestFalse(TEXT("Reused wave is visible"),Again->IsHidden());return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrostTravelSync,"Rogue.Gameplay.FrostBlade.AllFiveGASValuesSurviveFreshPawnStageRestore",FrostBladeTests::Flags)
bool FFrostTravelSync::RunTest(const FString&)
{
 FrostBladeTests::FFixture F;if(!F.Init())return false;F.Upgrade(ERogueUpgradeType::FrostBladeCount);F.Upgrade(ERogueUpgradeType::FrostBladeSize,.4);F.Upgrade(ERogueUpgradeType::FrostBladeWave);F.Upgrade(ERogueUpgradeType::FrostBladeWaveCount,2);F.Upgrade(ERogueUpgradeType::FrostBladeWaveSize,.6);
 auto* OldState=F.Player->GetPlayerState<ARoguePlayerState>();OldState->PrepareStageTravel();auto* PC=F.World->SpawnActor<ARoguePlayerController>();const FTransform T(FVector(0,2000,200));auto* New=F.World->SpawnActorDeferred<ARogueCharacter>(ARogueCharacter::StaticClass(),T,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
 auto* Table=NewObject<UDataTable>(F.World);Table->RowStruct=FRogueWeaponTableRow::StaticStruct();Table->AddRow(TEXT("FrostBlade"),F.Weapon->GetConfig());FindFProperty<FObjectProperty>(ARogueCharacter::StaticClass(),TEXT("WeaponDataTable"))->SetObjectPropertyValue_InContainer(New,Table);New->AutoPossessPlayer=EAutoReceiveInput::Disabled;New->FinishSpawning(T);
 OldState->CopyProperties(PC->GetPlayerState<ARoguePlayerState>());PC->GetPlayerState<ARoguePlayerState>()->SetProgressionSnapshotForRun(OldState->GetProgressionSnapshot());PC->Possess(New);New->SetActorTickEnabled(false);New->GetCharacterMovement()->SetComponentTickEnabled(false);PC->GetPlayerState<ARoguePlayerState>()->RestoreStageTravelGrowth(New);
 auto* W=New->FindWeapon<ARogueWeapon_FrostBlade>();if(!TestNotNull(TEXT("Fresh stage weapon recreated from table"),W))return false;UE_LOG(LogTemp,Display,TEXT("[FrostTravel] blades=%d bladeScale=%.9f waves=%d waveScale=%.9f"),W->GetEffectiveCount(),W->GetEffectiveBladeScale(),W->GetEffectiveWaveCount(),W->GetEffectiveWaveScale());TestEqual(TEXT("Single blade retained"),W->GetEffectiveCount(),1);TestTrue(TEXT("Blade size retained"),FMath::IsNearlyEqual(W->GetEffectiveBladeScale(),1.4f,1.e-4f));TestEqual(TEXT("Wave unlock and count retained"),W->GetEffectiveWaveCount(),3);TestTrue(TEXT("Wave size retained"),FMath::IsNearlyEqual(W->GetEffectiveWaveScale(),1.6f,1.e-4f));return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrostUnlimitedRange,"Rogue.Gameplay.FrostBlade.GeneralRangeReallyReachesFarEnemyWithoutCeiling",FrostBladeTests::Flags)
bool FFrostUnlimitedRange::RunTest(const FString&)
{
 FrostBladeTests::FFixture F;if(!F.Init())return false;F.Upgrade(ERogueUpgradeType::FrostBladeCount);auto* E=F.Enemy(FVector(950,0,200));F.Weapon->WeaponTick(0);F.Step(.8f);
 TestEqual(TEXT("Initially out of reach"),F.HP(E),1000.f);F.Upgrade(ERogueUpgradeType::AttackRange,600);TestTrue(TEXT("Bonus expands acquisition range"),FMath::IsNearlyEqual(F.Weapon->GetEffectiveRange(),1050.f));
 F.Weapon->WeaponTick(0);F.Step(.8f);TestTrue(TEXT("Blade physically reaches beyond old 520 cm ceiling"),FMath::IsNearlyEqual(F.HP(E),975.f,.1f));
 TestTrue(TEXT("The blade itself is not enlarged by the general range card"),FMath::IsNearlyEqual(F.Weapon->GetEffectiveBladeScale(),1.f));return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrostRangeNoCap,"Rogue.Gameplay.FrostBlade.RangeHasNoWeaponCeilingAndAuthoredRangeAlsoWorks",FrostBladeTests::Flags)
bool FFrostRangeNoCap::RunTest(const FString&)
{
 FrostBladeTests::FFixture F;if(!F.Init())return false;F.Upgrade(ERogueUpgradeType::FrostBladeCount);F.Weapon->GetConfig().MaxRange=500;
 for(int32 I=0;I<5;++I)F.Upgrade(ERogueUpgradeType::AttackRange,10000);
 TestTrue(TEXT("Range continues growing despite the legacy optional cap"),FMath::IsNearlyEqual(F.Weapon->GetEffectiveRange(),50450.f,.1f));
 F.Weapon->GetConfig().Range=20000;auto* E=F.Enemy(FVector(65000,0,200));F.Weapon->WeaponTick(0);F.Step(.8f);
 TestTrue(TEXT("An authored high base range also increases real travel reach"),FMath::IsNearlyEqual(F.HP(E),975.f,.1f));return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrostAutomaticStart,"Rogue.Gameplay.FrostBlade.AutomaticUnlockRespectsEachProfileAndRepeatedInitialization",FrostBladeTests::Flags)
bool FFrostAutomaticStart::RunTest(const FString&)
{
 {
  FrostBladeTests::FFixture F;if(!F.Init(false))return false;
  TestEqual(TEXT("An unrelated fresh profile receives no blade"),F.Weapon->GetEffectiveCount(),0);
  TestFalse(TEXT("A locked profile cannot draw acquisition"),RogueUpgradeSystemRules::IsUpgradeAllowedForProgression(ERogueUpgradeType::FrostBladeCount,F.Player));
 }
 {
  FrostBladeTests::FFixture F;if(!F.Init())return false;
  TestEqual(TEXT("Unlocked profile equips exactly one blade"),F.Weapon->GetEffectiveCount(),1);
  for(int32 I=0;I<5;++I)F.Player->NotifyProgressionReady();
  TestEqual(TEXT("Repeated initialization does not grant extra copies"),F.Weapon->GetEffectiveCount(),1);
  FRogueUpgradeSystem Up;Up.SetRuleAsset(NewObject<URogueUpgradeRuleAsset>(F.World));
  for(int32 I=0;I<30;++I)
  {
   TArray<FRogueUpgradeOption> Offers;Up.BuildRandomUpgradeOptions(F.Player,64,Offers);
   TestFalse(TEXT("Level-up offers never contain the removed acquisition card"),Offers.ContainsByPredicate([](const auto& O){return O.Type==ERogueUpgradeType::FrostBladeCount;}));
  }
  F.Weapon->WeaponTick(0);F.Step(.02f);TestNotNull(TEXT("Automatic loadout creates the real hovering blade"),F.Blade());
 }
 return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrostAreaPulse,"Rogue.Gameplay.FrostBlade.AreaSectorHitsCrowdOnceAndRejectsBackHeightAndWalls",FrostBladeTests::Flags)
bool FFrostAreaPulse::RunTest(const FString&)
{
 FrostBladeTests::FFixture F;if(!F.Init())return false;F.Weapon->WeaponTick(0);F.Step(.02f);auto* B=F.Blade();if(!B)return false;
 auto* Main=F.Enemy(FVector(270,0,200));auto* Side=F.Enemy(FVector(260,100,200));auto* Front=F.Enemy(FVector(285,-60,200));
 auto* Behind=F.Enemy(FVector(80,0,200));auto* Outside=F.Enemy(FVector(600,0,200));auto* Above=F.Enemy(FVector(270,0,600));auto* Blocked=F.Enemy(FVector(340,0,200));F.Wall(310);
 TestTrue(TEXT("Start an actual area swing"),B->StartAttack(Main,F.Weapon,25,0,0,1,1000,nullptr));F.Step(.24f);
 TestEqual(TEXT("No early damage before the middle of the wind-up swing"),F.HP(Main),1000.f);F.Step(.55f);
 for(auto* E:{Main,Side,Front})TestTrue(TEXT("Every enemy in the open sector takes one full hit"),FMath::IsNearlyEqual(F.HP(E),975,.1f));
 for(auto* E:{Behind,Outside,Above,Blocked})TestEqual(TEXT("Rear, out-of-range, different-height and wall-blocked enemies are excluded"),F.HP(E),1000.f);
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrostSizeAreaDamage,"Rogue.Gameplay.FrostBlade.LargerBladeEnlargesAreaAndIncreasesActualDamage",FrostBladeTests::Flags)
bool FFrostSizeAreaDamage::RunTest(const FString&)
{
 FrostBladeTests::FFixture F;if(!F.Init())return false;F.Weapon->WeaponTick(0);F.Step(.02f);auto* Main=F.Enemy(FVector(270,0,200));auto* Far=F.Enemy(FVector(460,0,200));
 auto* B=F.Blade();if(!B)return false;B->StartAttack(Main,F.Weapon,F.Weapon->GetEffectiveDamage(),0,0,1,1000,nullptr);F.Step(.8f);
 TestEqual(TEXT("Base area does not reach the farther enemy"),F.HP(Far),1000.f);TestTrue(TEXT("Base area deals configured damage"),FMath::IsNearlyEqual(F.HP(Main),975,.1f));
 F.Upgrade(ERogueUpgradeType::FrostBladeSize,.6f);F.Weapon->WeaponTick(0);F.Step(.8f);
 TestTrue(TEXT("1.6x blade deals 1.6x damage, not unchanged damage"),FMath::IsNearlyEqual(F.Weapon->GetEffectiveDamage(),40,.01f));
 TestTrue(TEXT("Far enemy enters the larger area and takes forty damage"),FMath::IsNearlyEqual(F.HP(Far),960,.1f));
 TestTrue(TEXT("Nearby enemy takes one larger hit too"),FMath::IsNearlyEqual(F.HP(Main),935,.1f));return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrostBaseAttackSize,"Rogue.Gameplay.FrostBlade.PermanentBaseAttackAndGASAttackCardsScaleWithSizeExactlyOnce",FrostBladeTests::Flags)
bool FFrostBaseAttackSize::RunTest(const FString&)
{
 FrostBladeTests::FFixture F;if(!F.Init(true,2))return false;
 const float AttackBefore=F.Player->GetWeaponBonusSnapshot().AttackPower;
 TestTrue(TEXT("Permanent base attack tree ranks are present"),AttackBefore>0);
 const float Base=F.Weapon->GetConfig().Damage,Factor=F.Weapon->GetConfig().FrostBladeCombat.AttackPowerScale;
 TestTrue(TEXT("Permanent base attack contributes to initial damage"),FMath::IsNearlyEqual(F.Weapon->GetEffectiveDamage(),Base+AttackBefore*Factor,.001f));
 F.Upgrade(ERogueUpgradeType::AttackPower,5);const float Attack=F.Player->GetWeaponBonusSnapshot().AttackPower;
 TestTrue(TEXT("In-run attack card is added to the same base attack channel"),FMath::IsNearlyEqual(Attack,AttackBefore+5,.001f));
 F.Upgrade(ERogueUpgradeType::FrostBladeSize,.2f);F.Upgrade(ERogueUpgradeType::FrostBladeSize,.2f);
 const float Expected=(Base+Attack*Factor)*1.4f;
 TestTrue(TEXT("Two size cards add forty percent, and amplify combined damage once"),FMath::IsNearlyEqual(F.Weapon->GetEffectiveDamage(),Expected,.001f));
 auto* E=F.Enemy(FVector(270,0,200));F.Weapon->WeaponTick(0);F.Step(.8f);
 TestTrue(TEXT("The real GAS hit uses the combined permanent, card and size damage"),FMath::IsNearlyEqual(F.HP(E),1000-Expected,.1f));
 F.Weapon->GetConfig().MaxDamage=10;TestEqual(TEXT("An explicitly authored final damage cap still applies"),F.Weapon->GetEffectiveDamage(),10.f);return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFrostLongFrameArea,"Rogue.Gameplay.FrostBlade.LongFrameStillDeliversOneWholeSectorPulse",FrostBladeTests::Flags)
bool FFrostLongFrameArea::RunTest(const FString&)
{
 FrostBladeTests::FFixture F;if(!F.Init())return false;F.Weapon->WeaponTick(0);auto* B=F.Blade();if(!B)return false;
 auto* Main=F.Enemy(FVector(270,0,200));auto* Side=F.Enemy(FVector(260,100,200));B->StartAttack(Main,F.Weapon,25,0,0,1,1000,nullptr);
 F.TestWorld.TickTestWorld(.6f);F.TestWorld.TickTestWorld(.6f);F.Step(.1f);
 TestTrue(TEXT("A frame crossing the entire slash cannot miss the area hit"),FMath::IsNearlyEqual(F.HP(Main),975,.1f));
 TestTrue(TEXT("Crowd damage stays one hit despite the long frame"),FMath::IsNearlyEqual(F.HP(Side),975,.1f));return true;
}
#endif
