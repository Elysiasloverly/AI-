#if WITH_DEV_AUTOMATION_TESTS
#include "AbilitySystemComponent.h"
#include "AbilitySystem/RogueAttributeSet.h"
#include "Combat/Projectiles/RogueProjectile.h"
#include "Combat/Weapons/RogueWeapon_Projectile.h"
#include "Combat/Weapons/RogueWeapon_Rocket.h"
#include "Combat/Weapons/RogueWeapon_HellTower.h"
#include "Combat/Targeting/RogueEnemyTrackerSubsystem.h"
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
#include "Progression/Meta/RogueProgressionSaveGame.h"
#include "Progression/Meta/RogueUpgradeTreeDefinition.h"
#include "Progression/Upgrades/RogueUpgradeDefinitionAsset.h"
#include "Progression/Upgrades/RogueUpgradeRuleAsset.h"
#include "Progression/Upgrades/RogueUpgradeSystem.h"
#include "Progression/Shop/RogueShopSystem.h"
#include "Tests/AutomationCommon.h"
#include "UObject/UnrealType.h"

namespace RogueExclusiveCardsTests
{
	constexpr EAutomationTestFlags Flags=EAutomationTestFlags::EditorContext|EAutomationTestFlags::ProductFilter;
	const ERogueUpgradeType Types[]={ERogueUpgradeType::ProjectileBurst,ERogueUpgradeType::RocketBlastRadius,ERogueUpgradeType::HellTowerDamageRamp};
	struct FFixture
	{
		FTestConsoleVariable SelfTest{TEXT("Rogue.SelfTest.WeaponUpgradeChain")};
		FTestConsoleVariable Stress{TEXT("Rogue.SelfTest.EnemyStress")};
		FTestWorldWrapper TestWorld; UWorld* World=nullptr; TArray<ARogueCharacter*> Players;
		bool Initialize(int32 Count=1)
		{
			SelfTest.Set(TEXT("0"));Stress.Set(TEXT("0"));
			if(!TestWorld.CreateTestWorld(EWorldType::Game))return false;
			World=TestWorld.GetTestWorld();World->GetWorldSettings()->DefaultGameMode=ARogueGameMode::StaticClass();
			if(!TestWorld.BeginPlayInTestWorld())return false;
			World->GetAuthGameMode()->SetActorTickEnabled(false);
			for(int32 I=0;I<Count;++I)
			{
				auto* PC=World->SpawnActor<ARoguePlayerController>();
				const FTransform Transform(FVector(0,I*2000,200));
				auto* Player=World->SpawnActorDeferred<ARogueCharacter>(ARogueCharacter::StaticClass(),Transform,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
				auto* Table=NewObject<UDataTable>(World);Table->RowStruct=FRogueWeaponTableRow::StaticStruct();
				FRogueWeaponTableRow Row;Row.Count=1;Row.ProjectileClass=ARogueProjectile::StaticClass();Row.ProjectileSpeed=0;Row.Range=2000;Row.ProjectileBurstInterval=.1f;
				Row.WeaponClass=ARogueWeapon_Projectile::StaticClass();Table->AddRow(TEXT("Projectile"),Row);
				Row.WeaponClass=ARogueWeapon_Rocket::StaticClass();Table->AddRow(TEXT("Rocket"),Row);
				Row.WeaponClass=ARogueWeapon_HellTower::StaticClass();Table->AddRow(TEXT("HellTower"),Row);
				FindFProperty<FObjectProperty>(ARogueCharacter::StaticClass(),TEXT("WeaponDataTable"))->SetObjectPropertyValue_InContainer(Player,Table);
				Player->AutoPossessPlayer=EAutoReceiveInput::Disabled;Player->FinishSpawning(Transform);
				FRogueProgressionSnapshot Profile;
				for(const auto& Node:URogueUpgradeTreeDefinition::GetDefaultDefinition()->GetNodes())
				{
					const bool bNewCard=Node.UpgradeType==Types[0] || Node.UpgradeType==Types[1] || Node.UpgradeType==Types[2];
					if(Node.Kind!=ERogueMetaNodeKind::BaseStat && (I==0 || !bNewCard)) Profile.NodeRanks.Add(Node.NodeId,1);
				}
				PC->GetPlayerState<ARoguePlayerState>()->SetProgressionSnapshotForRun(Profile);
				PC->Possess(Player);Player->SetActorTickEnabled(false);Player->GetCharacterMovement()->SetComponentTickEnabled(false);Player->SetActorEnableCollision(false);
				Player->GetAbilitySystemComponent()->CancelAllAbilities();Player->GetRogueAttributeSet()->SetWeaponBonusCaps(FRogueWeaponBonusCaps());
				Players.Add(Player);
			}
			return true;
		}
		void Upgrade(ERogueUpgradeType Type,float Magnitude,int32 I=0)
		{FRogueUpgradeOption U;U.Type=Type;U.Magnitude=Magnitude;Players[I]->ApplyUpgrade(U);}
		ARogueEnemy* Enemy(FVector Location)
		{
			FActorSpawnParameters Params;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			auto* E=World->SpawnActor<ARogueEnemy>(ARogueEnemy::StaticClass(),Location,FRotator::ZeroRotator,Params);
			E->ActivatePooledEnemy(nullptr,Location,FRotator::ZeroRotator);FRogueEnemyProfile Profile;Profile.Health=1000;E->InitializeEnemy(Profile);
			E->SetActorTickEnabled(false);E->GetCharacterMovement()->SetComponentTickEnabled(false);E->SetActorEnableCollision(false);
			World->GetSubsystem<URogueEnemyTrackerSubsystem>()->RegisterEnemy(E);return E;
		}
		void Step(float Seconds){for(float T=0;T<Seconds;T+=.01f)TestWorld.TickTestWorld(.01f);}
		int32 Bullets() const {int32 N=0;for(TActorIterator<ARogueProjectile> It(World);It;++It)if(!It->IsAvailableInPool())++N;return N;}
		void ClearBullets(){for(TActorIterator<ARogueProjectile> It(World);It;++It)It->DeactivateToPool();}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueBurstRealFireTest,"Rogue.Gameplay.ExclusiveCards.BurstGASCountsAndDelayedFullVolleys",RogueExclusiveCardsTests::Flags)
bool FRogueBurstRealFireTest::RunTest(const FString& Parameters)
{
	using namespace RogueExclusiveCardsTests;FFixture F;if(!F.Initialize())return false;
	auto* Weapon=F.Players[0]->FindWeapon<ARogueWeapon_Projectile>();auto* Enemy=F.Enemy(FVector(700,0,200));
	for(int32 Picks=0;Picks<=3;++Picks)
	{
		if(Picks>0)F.Upgrade(ERogueUpgradeType::ProjectileBurst,1);
		F.ClearBullets();TestEqual(TEXT("Each selected card adds one extra volley"),Weapon->GetEffectiveBurstCount(),1+Picks);
		Weapon->FireAtTargets({Enemy});TestEqual(TEXT("First volley fires immediately"),F.Bullets(),1);
		F.Step(.04f);TestEqual(TEXT("Extra shots are not an instant stacked volley"),F.Bullets(),1);
		F.Step(.5f);TestEqual(TEXT("The complete delayed burst fires"),F.Bullets(),1+Picks);
	}
	F.Upgrade(ERogueUpgradeType::ProjectileCount,1);F.ClearBullets();Weapon->FireAtTargets({Enemy});F.Step(.5f);
	TestEqual(TEXT("Quantity and four-round burst combine into eight real projectiles"),F.Bullets(),8);
	TestTrue(TEXT("Burst duration is included before normal cooldown"),Weapon->GetAbilityCooldown()>=Weapon->GetEffectiveCooldown()+.29f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueBurstLifecycleTest,"Rogue.Gameplay.ExclusiveCards.BurstRetargetsAndStopsOnDeathOrDestroy",RogueExclusiveCardsTests::Flags)
bool FRogueBurstLifecycleTest::RunTest(const FString& Parameters)
{
	using namespace RogueExclusiveCardsTests;FFixture F;if(!F.Initialize())return false;
	auto* Weapon=F.Players[0]->FindWeapon<ARogueWeapon_Projectile>();auto* Target=F.Enemy(FVector(700,0,200));F.Upgrade(ERogueUpgradeType::ProjectileBurst,2);
	Weapon->FireAtTargets({Target});TestFalse(TEXT("Cannot start overlapping bursts"),Weapon->CanFireNow());
	Target->DeactivateToPool();F.Enemy(FVector(800,0,200));F.Step(.35f);
	TestEqual(TEXT("A pooled target is replaced by a live nearby target"),F.Bullets(),3);
	F.ClearBullets();TArray<ARogueEnemy*> Targets;Weapon->CollectFireTargets(Targets);Weapon->FireAtTargets(Targets);Weapon->OnOwnerDied();F.Step(.4f);
	TestEqual(TEXT("Death cancels delayed rounds"),F.Bullets(),1);
	F.ClearBullets();Weapon->FireAtTargets(Targets);Weapon->Destroy();F.Step(.4f);
	TestEqual(TEXT("Destroy cancels delayed rounds"),F.Bullets(),1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueBlastAndRampTest,"Rogue.Gameplay.ExclusiveCards.RocketRadiusAndActualTowerRampDamage",RogueExclusiveCardsTests::Flags)
bool FRogueBlastAndRampTest::RunTest(const FString& Parameters)
{
	using namespace RogueExclusiveCardsTests;FFixture F;if(!F.Initialize(2))return false;
	auto* Rocket=F.Players[0]->FindWeapon<ARogueWeapon_Rocket>();const float Radius=Rocket->GetExplosionRadius();
	F.Upgrade(ERogueUpgradeType::RocketBlastRadius,90);F.Upgrade(ERogueUpgradeType::RocketBlastRadius,90);
	TestEqual(TEXT("Two radius cards add 180 cm"),Rocket->GetExplosionRadius(),Radius+180);
	TestEqual(TEXT("Another player's radius is unchanged"),F.Players[1]->FindWeapon<ARogueWeapon_Rocket>()->GetExplosionRadius(),Radius);
	F.Upgrade(ERogueUpgradeType::HellTowerCount,1);auto* Tower=F.Players[0]->FindWeapon<ARogueWeapon_HellTower>();
	const float Base=Tower->GetEffectiveBaseDamage(),Ramp=Tower->GetEffectiveDamageRampPerTick();
	F.Upgrade(ERogueUpgradeType::HellTowerDamageRamp,.1f);F.Upgrade(ERogueUpgradeType::HellTowerDamageRamp,.1f);
	TestTrue(TEXT("Each card adds exactly 0.1 ramp"),FMath::IsNearlyEqual(Tower->GetEffectiveDamageRampPerTick(),Ramp+.2f));
	TestEqual(TEXT("Base tick damage is unchanged"),Tower->GetEffectiveBaseDamage(),Base);
	auto* E=F.Enemy(FVector(700,0,200));const float Before=E->GetRogueAttributeSet()->GetHealth();
	Tower->WeaponTick(.1f);Tower->WeaponTick(.1f);
	TestTrue(TEXT("Actual first two GAS ticks use base, then base plus upgraded ramp"),FMath::IsNearlyEqual(Before-E->GetRogueAttributeSet()->GetHealth(),Base*2+Ramp+.2f,.01f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueExclusivePoolsTest,"Rogue.Gameplay.ExclusiveCards.PermanentTailsAndPerPlayerUpgradeShopPools",RogueExclusiveCardsTests::Flags)
bool FRogueExclusivePoolsTest::RunTest(const FString& Parameters)
{
	using namespace RogueExclusiveCardsTests;FFixture F;if(!F.Initialize(2))return false;
	auto* Tree=URogueUpgradeTreeDefinition::GetDefaultDefinition();auto* Definitions=LoadObject<URogueUpgradeDefinitionAsset>(nullptr,TEXT("/Game/Data/Upgrades/DA_UpgradeDefinitions.DA_UpgradeDefinitions"));
	auto* Rules=LoadObject<URogueUpgradeRuleAsset>(nullptr,TEXT("/Game/Data/Upgrades/DA_UpgradeRules.DA_UpgradeRules"));
	FRogueUpgradeSystem System;System.SetDefinitionAsset(Definitions);System.SetRuleAsset(Rules);
	for(int32 I=0;I<2;++I)
	{
		F.Upgrade(ERogueUpgradeType::RocketCount,1,I);F.Upgrade(ERogueUpgradeType::HellTowerCount,1,I);
		TArray<FRogueUpgradeOption> Options;System.BuildRandomUpgradeOptions(F.Players[I],100,Options);
		FRogueShopSystem Shop;Shop.Configure(100,1,0,30);Shop.OpenShop(F.Players[I],System);
		for(auto Type:Types)
		{
			TestEqual(TEXT("Upgrade pool uses this player's permanent unlock"),Options.ContainsByPredicate([Type](const auto& O){return O.Type==Type;}),I==0);
			TestEqual(TEXT("Shop pool uses this player's permanent unlock"),Shop.GetOffers().ContainsByPredicate([Type](const auto& O){return O.GetUpgrade().Type==Type;}),I==0);
		}
	}
	const TCHAR* Ids[]={TEXT("Card.ProjectileBurst"),TEXT("Card.RocketBlastRadius"),TEXT("Card.HellTowerDamageRamp")};
	const FName Parents[]={TEXT("Card.ProjectileCount"),TEXT("Card.RocketCount"),TEXT("Card.HellTowerCount")};
	for(int32 I=0;I<3;++I)
	{
		const auto* Node=Tree->FindNode(Ids[I]);if(!TestNotNull(TEXT("New node exists in the saved DA_UpgradeTree"),Node))return false;
		TestEqual(TEXT("Each permanent card costs five bricks"),Node->Cost,5);TestFalse(TEXT("Card is not initially unlocked"),Node->bInitiallyUnlocked);
		TestEqual(TEXT("Card attaches to one prior tail"),Node->Prerequisites.Num(),1);
		if(Node->Prerequisites.Num()==1)TestEqual(TEXT("Prerequisite is the original last node in this weapon column"),Node->Prerequisites[0],Parents[I]);
		auto* Profile=NewObject<URogueProgressionSaveGame>();Profile->IceBricks=5;FText Failure;
		for(const auto& Existing:Tree->GetNodes())
		{
			if(Existing.NodeId!=Parents[I] && Existing.NodeId!=Node->NodeId && Existing.Kind!=ERogueMetaNodeKind::BaseStat)
				Profile->Progression.NodeRanks.Add(Existing.NodeId,1);
		}
		if(!Tree->FindNode(Parents[I])->bInitiallyUnlocked)
		{
			TestFalse(TEXT("Cannot skip the original tail when buying the new card"),Profile->TryPurchase(Tree,Node->NodeId,Failure));
			TestEqual(TEXT("Rejected skip does not spend bricks"),Profile->IceBricks,5);
		}
		Profile->Progression.NodeRanks.Add(Parents[I],1);
		TestTrue(TEXT("The new card can be purchased after its tail is unlocked"),Profile->TryPurchase(Tree,Node->NodeId,Failure));
		TestEqual(TEXT("The permanent unlock spends five bricks"),Profile->IceBricks,0);
		FRogueUpgradeDefinitionRow Row;if(!TestTrue(TEXT("Card has a real table row"),Definitions->FindDefinition(Types[I],Row)))return false;
		TestTrue(TEXT("Matching table title is used"),Row.Title==Node->Title.ToString());
		TestTrue(TEXT("No card with missing permanent rank is available"),!Tree->IsUpgradeAvailable({},Types[I],true));
	}
	return true;
}
#endif
