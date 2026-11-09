#if WITH_DEV_AUTOMATION_TESTS
#include "AbilitySystemComponent.h"
#include "AbilitySystem/RogueAttributeSet.h"
#include "Combat/Projectiles/RogueProjectile.h"
#include "Combat/Projectiles/RogueRocketProjectile.h"
#include "Combat/Projectiles/RogueIceFlameField.h"
#include "Combat/Pooling/RogueCombatPoolSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
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
			E->ActivatePooledEnemy(nullptr,Location,FRotator::ZeroRotator);FRogueEnemyProfile Profile;Profile.Health=1000;Profile.Speed=260;E->InitializeEnemy(Profile);
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueIceFlameStatusTest,"Rogue.Gameplay.ExclusiveCards.IceFlameDamageSlowExitAndPoolReset",RogueExclusiveCardsTests::Flags)
bool FRogueIceFlameStatusTest::RunTest(const FString& Parameters)
{
    using namespace RogueExclusiveCardsTests;FFixture F;if(!F.Initialize(2))return false;
    auto* E=F.Enemy(FVector(700,0,200));auto* Outside=F.Enemy(FVector(1000,0,200));auto* High=F.Enemy(FVector(700,0,900));
    const float Base=E->GetMoveSpeed();TestTrue(TEXT("Slow test uses a genuinely moving baseline"),Base>0);FRogueIceFlameCombatSettings S;S.Lifetime=1.2f;S.DamageInterval=.2f;
    auto* Pool=F.World->GetSubsystem<URogueCombatPoolSubsystem>();
    auto* Field=Pool->SpawnIceFlameField(ARogueIceFlameField::StaticClass(),F.Players[0],FVector(700,0,100),FVector::UpVector,100,10,S);
    if(!TestNotNull(TEXT("Server creates the persistent field"),Field))return false;
    F.Step(.51f);
    TestTrue(TEXT("Damage is low and proportional to elapsed exposure"),E->GetCurrentHealth()<997 && E->GetCurrentHealth()>993);
    TestTrue(TEXT("Ground enemy is slowed by 25 percent"),FMath::IsNearlyEqual(E->GetMoveSpeed(),Base*.75f));
    TestEqual(TEXT("Outside enemies receive no residual damage"),Outside->GetCurrentHealth(),1000.f);
    TestEqual(TEXT("Enemies above the flame height receive no damage"),High->GetCurrentHealth(),1000.f);
    const float Health=E->GetCurrentHealth();E->SetActorLocation(FVector(1300,0,200));F.Step(.11f);
    TestEqual(TEXT("Leaving the field restores the original speed"),E->GetMoveSpeed(),Base);
    F.Step(.25f);TestEqual(TEXT("Leaving the field stops its damage"),E->GetCurrentHealth(),Health);
    E->SetActorLocation(FVector(700,0,200));F.World->GetSubsystem<URogueEnemyTrackerSubsystem>()->UnregisterEnemy(E);F.World->GetSubsystem<URogueEnemyTrackerSubsystem>()->RegisterEnemy(E);
    F.Step(.11f);
    TestEqual(TEXT("Nearby repeated impacts refresh the same player's field"),Pool->SpawnIceFlameField(ARogueIceFlameField::StaticClass(),F.Players[0],FVector(705,0,100),FVector::UpVector,100,10,S),Field);
    S.SlowFraction=.5f;auto* Stronger=Pool->SpawnIceFlameField(ARogueIceFlameField::StaticClass(),F.Players[1],FVector(700,0,100),FVector::UpVector,100,0,S);
    F.Step(.11f);TestTrue(TEXT("Overlapping slows use the strongest, without multiplication"),FMath::IsNearlyEqual(E->GetMoveSpeed(),Base*.5f));
    Stronger->Deactivate();TestTrue(TEXT("Removing the stronger source retains the weaker slow"),FMath::IsNearlyEqual(E->GetMoveSpeed(),Base*.75f));
    E->DeactivateToPool();E->ActivatePooledEnemy(nullptr,FVector(700,0,200),FRotator::ZeroRotator);FRogueEnemyProfile Profile;Profile.Speed=300;Profile.Health=1000;E->InitializeEnemy(Profile);E->SetActorTickEnabled(false);E->GetCharacterMovement()->SetComponentTickEnabled(false);
    TestEqual(TEXT("Enemy reuse clears all previous-spawn movement effects"),E->GetMoveSpeed(),300.f);
    Field->Deactivate();TestEqual(TEXT("An old spawn's cleanup never modifies the new baseline"),E->GetMoveSpeed(),300.f);
    S.Lifetime=.2f;S.SlowFraction=.25f;Field->Activate(F.Players[0],FVector(700,0,100),FVector::UpVector,100,0,S);F.Step(.3f);
    TestTrue(TEXT("Field expires and becomes reusable"),Field->IsAvailableInPool());TestEqual(TEXT("Expiry restores movement"),E->GetMoveSpeed(),300.f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueIceFlameImpactTest,"Rogue.Gameplay.ExclusiveCards.IceMeteorCreatesGroundedBlueFlameAndResetsOnReuse",RogueExclusiveCardsTests::Flags)
bool FRogueIceFlameImpactTest::RunTest(const FString& Parameters)
{
    using namespace RogueExclusiveCardsTests;FFixture F;if(!F.Initialize())return false;
    auto* Weapon=F.Players[0]->FindWeapon<ARogueWeapon_Rocket>();auto& Config=Weapon->GetConfig();
    Config.RocketClass=LoadClass<ARogueRocketProjectile>(nullptr,TEXT("/Game/BP/Projectiles/BP_RogueRocketProjectile.BP_RogueRocketProjectile_C"));
    Config.Count=1;Config.Damage=100;Config.RocketSpeed=1900;Config.bEnableSpeedRandomization=false;Config.bEnableTrajectoryDeviation=false;Config.IceFlame.Lifetime=1.2f;Config.IceFlame.DamageInterval=.2f;
    auto* E=F.Enemy(FVector(700,0,200));const float Base=E->GetMoveSpeed();
    auto* Floor=F.World->SpawnActor<AActor>();auto* Box=NewObject<UBoxComponent>(Floor);Floor->SetRootComponent(Box);Box->SetBoxExtent(FVector(2000,2000,20));
    Box->SetCollisionObjectType(ECC_WorldStatic);Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);Box->SetCollisionResponseToAllChannels(ECR_Block);Box->RegisterComponent();Floor->SetActorLocation(FVector(700,0,162));
    Weapon->FireAtTargets({E});F.Step(.85f);
    ARogueIceFlameField* Field=nullptr;for(TActorIterator<ARogueIceFlameField> It(F.World);It;++It)if(!It->IsAvailableInPool())Field=*It;
    if(!TestNotNull(TEXT("Actual meteor impact leaves its configured Blueprint field"),Field))return false;
    TestTrue(TEXT("Field is projected onto ground, not left at capsule height"),FMath::IsNearlyEqual(Field->GetActorLocation().Z,182.,.1));
    TestTrue(TEXT("Field radius follows the upgraded explosion radius"),FMath::IsNearlyEqual(Field->GetRadius(),Weapon->GetExplosionRadius()*Config.IceFlame.RadiusScale));
    const float AfterImpact=E->GetCurrentHealth();F.Step(.5f);
    TestTrue(TEXT("Residual flame damages after the meteor is back in its pool"),E->GetCurrentHealth()<AfterImpact);
    TestTrue(TEXT("Actual impact slow affects movement"),E->GetMoveSpeed()<Base);
    TestFalse(TEXT("Ground art cannot block players or enemies"),Field->GetActorEnableCollision());
    F.Step(1.3f);TestTrue(TEXT("Residual flame expires"),Field->IsAvailableInPool());
    auto* Rocket=F.World->GetSubsystem<URogueCombatPoolSubsystem>()->AcquireRocketProjectile(Config.RocketClass,F.Players[0],F.Players[0],FVector(700,0,600),FRotator(-90,0,0));
    Rocket->ActivatePooledRocket(F.Players[0],F.Players[0],FVector(700,0,600),FRotator(-90,0,0),-FVector::UpVector,1000,100,280);
    F.Step(.7f);TestTrue(TEXT("Reuse without explicit meteor configuration cannot leak a ground field to other rocket-based weapons"),Field->IsAvailableInPool());
    return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueIceMeteorFlightTest,"Rogue.Gameplay.ExclusiveCards.IceMeteorSkyfallScaleAndCosmeticReuse",RogueExclusiveCardsTests::Flags)
bool FRogueIceMeteorFlightTest::RunTest(const FString& Parameters)
{
    using namespace RogueExclusiveCardsTests;
    FFixture F; if (!F.Initialize(2)) { return false; }
    auto* Weapon = F.Players[0]->FindWeapon<ARogueWeapon_Rocket>();
    auto& Config = Weapon->GetConfig();
    Config.RocketClass = LoadClass<ARogueRocketProjectile>(nullptr,TEXT("/Game/BP/Projectiles/BP_RogueRocketProjectile.BP_RogueRocketProjectile_C"));
    if (!TestNotNull(TEXT("Saved ice meteor Blueprint loads"),Config.RocketClass.Get())) { return false; }
    Config.Count=1; Config.RocketFall.SummonHeight=1100; Config.RocketFall.HorizontalOffset=650;
    Config.bEnableSpeedRandomization=false; Config.bEnableTrajectoryDeviation=true; Config.TrajectoryDeviationMaxAngle=40;
    auto* Target=F.Enemy(FVector(700,100,200));
    auto Active=[&]() -> ARogueRocketProjectile* {
        for(TActorIterator<ARogueRocketProjectile> It(F.World);It;++It) if(!It->IsAvailableInPool()) return *It;
        return nullptr;
    };
    Weapon->FireAtTargets({Target});
    auto* Meteor=Active(); if(!TestNotNull(TEXT("One real meteor summoned"),Meteor)) return false;
    auto* Movement=Meteor->FindComponentByClass<UProjectileMovementComponent>();
    const FVector Delta=Meteor->GetActorLocation()-Target->GetActorLocation();
    TestTrue(TEXT("Height follows DT setting above the target"),FMath::IsNearlyEqual(Delta.Z,1100.,.1));
    TestTrue(TEXT("Planar spawn offset follows DT even with random azimuth"),FMath::IsNearlyEqual(Delta.Size2D(),650.,.1));
    TestTrue(TEXT("Velocity points diagonally down at the target"),Movement->Velocity.Z<0 && FVector::DotProduct(Movement->Velocity.GetSafeNormal(),-Delta.GetSafeNormal())>.9999);
    TMap<FName,FVector> Authored;
    TInlineComponentArray<UStaticMeshComponent*> Meshes; Meteor->GetComponents(Meshes);
    for(auto* Mesh:Meshes) Authored.Add(Mesh->GetFName(),Mesh->GetComponentScale());
    TestTrue(TEXT("Saved Blueprint has a visible fireball"),!Authored.IsEmpty());
    Meteor->DeactivateToPool();
    F.Upgrade(ERogueUpgradeType::RocketBlastRadius,90);
    const float Scale=Weapon->GetExplosionRadius()/Config.ExplosionRadius;
    Weapon->FireAtTargets({Target}); Meteor=Active(); if(!Meteor)return false;
    auto CheckScale=[&](float Expected){
        Meshes.Reset();Meteor->GetComponents(Meshes);
        for(auto* Mesh:Meshes)if(const auto* Original=Authored.Find(Mesh->GetFName()))
            TestTrue(TEXT("Authored mesh proportions scale once with actual blast radius"),Mesh->GetComponentScale().Equals(*Original*Expected,.001));
    };
    CheckScale(Scale);Meteor->SetExplosionVisualScale(Scale);CheckScale(Scale);
    TestTrue(TEXT("Visual growth does not scale the collision root"),Meteor->GetActorScale3D().Equals(FVector::OneVector));
    TestEqual(TEXT("Another player has no inherited upgrade"),F.Players[1]->FindWeapon<ARogueWeapon_Rocket>()->GetExplosionRadius(),Config.ExplosionRadius);
    FRogueWeaponCosmeticFire Fire;Fire.Start=Meteor->GetActorLocation();Fire.Direction=Movement->Velocity.GetSafeNormal();
    Fire.Speed=0;Fire.Param=Weapon->GetExplosionRadius();Fire.ProjectileVisualScale=Scale;
    Meteor->DeactivateToPool();CheckScale(1);
    // The client path must use the server payload, even when its local upgrade snapshot differs.
    Weapon->PlayCosmeticFire(Fire);Meteor=Active();if(!Meteor)return false;CheckScale(Scale);
    Meteor->DeactivateToPool();CheckScale(1);
    Weapon->GetConfig().RocketFall.HorizontalOffset=1;Target->SetActorLocation(F.Players[0]->GetActorLocation());
    Weapon->FireAtTargets({Target});Meteor=Active();if(!Meteor)return false;
    Movement=Meteor->FindComponentByClass<UProjectileMovementComponent>();
    TestTrue(TEXT("A target at the player still receives a finite downward trajectory"),!Movement->Velocity.ContainsNaN() && Movement->Velocity.Z<0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueIceMeteorAreaTest,"Rogue.Gameplay.ExclusiveCards.IceMeteorGroundImpactAndExpandedDamage",RogueExclusiveCardsTests::Flags)
bool FRogueIceMeteorAreaTest::RunTest(const FString& Parameters)
{
    using namespace RogueExclusiveCardsTests;
    FFixture F;if(!F.Initialize())return false;
    auto* Weapon=F.Players[0]->FindWeapon<ARogueWeapon_Rocket>();auto& Config=Weapon->GetConfig();
    Config.Count=1;Config.Damage=100;Config.RocketSpeed=1900;Config.bEnableSpeedRandomization=false;Config.bEnableTrajectoryDeviation=false;
    auto* Target=F.Enemy(FVector(700,0,200));auto* Edge=F.Enemy(FVector(1030,0,200));auto* Outside=F.Enemy(FVector(1200,0,200));
    auto* Floor=F.World->SpawnActor<AActor>();auto* Box=NewObject<UBoxComponent>(Floor);
    Floor->SetRootComponent(Box);Box->SetBoxExtent(FVector(1800,1800,20));Box->SetCollisionObjectType(ECC_WorldStatic);
    Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);Box->SetCollisionResponseToAllChannels(ECR_Block);Box->RegisterComponent();Floor->SetActorLocation(FVector(700,0,162));
    Weapon->FireAtTargets({Target});F.Step(2);
    TestTrue(TEXT("Descending meteor hits the floor and damages the center enemy through GAS"),Target->GetRogueAttributeSet()->GetHealth()<1000);
    TestEqual(TEXT("Enemy outside the original radius is unharmed"),Edge->GetRogueAttributeSet()->GetHealth(),1000.f);
    F.Upgrade(ERogueUpgradeType::RocketBlastRadius,90);
    Weapon->FireAtTargets({Target});F.Step(2);
    TestTrue(TEXT("Radius card expands actual damage as well as the model"),Edge->GetRogueAttributeSet()->GetHealth()<1000);
    TestEqual(TEXT("Enemy outside upgraded radius is unharmed"),Outside->GetRogueAttributeSet()->GetHealth(),1000.f);
    int32 Active=0;for(TActorIterator<ARogueRocketProjectile> It(F.World);It;++It)if(!It->IsAvailableInPool())++Active;
    TestEqual(TEXT("Ground impacts return both meteors to the pool"),Active,0);
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
	TestTrue(TEXT("Actual first two GAS ticks use base, then base plus upgraded ramp"),FMath::IsNearlyEqual(Before-E->GetRogueAttributeSet()->GetHealth(),(Base*2+Ramp+.2f)*Tower->GetEffectiveCount(),.01f));
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
			const bool bAvailable = Type == ERogueUpgradeType::ProjectileBurst || I == 0;
			TestEqual(TEXT("Upgrade pool uses the default and personal permanent unlocks"),Options.ContainsByPredicate([Type](const auto& O){return O.Type==Type;}),bAvailable);
			TestEqual(TEXT("Shop pool uses the default and personal permanent unlocks"),Shop.GetOffers().ContainsByPredicate([Type](const auto& O){return O.GetUpgrade().Type==Type;}),bAvailable);
		}
	}
	const TCHAR* Ids[]={TEXT("Card.ProjectileBurst"),TEXT("Card.RocketBlastRadius"),TEXT("Card.HellTowerDamageRamp")};
	TArray<FRogueUpgradeOption> TitleOptions;System.BuildRandomUpgradeOptions(F.Players[0],100,TitleOptions);
	const FName Parents[]={TEXT("Card.ProjectileCount"),TEXT("Card.RocketCount"),TEXT("Card.HellTowerCount")};
	for(int32 I=0;I<3;++I)
	{
		const auto* Node=Tree->FindNode(Ids[I]);if(!TestNotNull(TEXT("New node exists in the saved DA_UpgradeTree"),Node))return false;
		const bool bInitiallyUnlocked = Types[I] == ERogueUpgradeType::ProjectileBurst;
		TestEqual(TEXT("Only the starter burst card is free"), Node->Cost, bInitiallyUnlocked ? 0 : 5);
		TestEqual(TEXT("Only the starter burst card is initially unlocked"), Node->bInitiallyUnlocked, bInitiallyUnlocked);
		TestEqual(TEXT("Card attaches to one prior tail"),Node->Prerequisites.Num(),1);
		if(Node->Prerequisites.Num()==1)TestEqual(TEXT("Prerequisite is the original last node in this weapon column"),Node->Prerequisites[0],Parents[I]);
		auto* Profile = NewObject<URogueProgressionSaveGame>();
		FText Failure;
		if (bInitiallyUnlocked)
		{
			const auto* Native = GetDefault<URogueUpgradeTreeDefinition>();
			TestEqual(TEXT("A fresh profile starts with the burst card"), Tree->GetNodeRank({}, Node->NodeId), 1);
			TestEqual(TEXT("Restoring the default tree keeps the burst card open"), Native->GetNodeRank({}, Node->NodeId), 1);
			TestFalse(TEXT("A free starting unlock cannot be purchased again"), Profile->TryPurchase(Tree, Node->NodeId, Failure));
			TestEqual(TEXT("Opening the starter card requires zero bricks"), Profile->IceBricks, 0);
		}
		else
		{
			Profile->IceBricks = 5;
			for (const auto& Existing : Tree->GetNodes())
			{
				if (Existing.NodeId != Parents[I] && Existing.NodeId != Node->NodeId && Existing.Kind != ERogueMetaNodeKind::BaseStat)
					Profile->Progression.NodeRanks.Add(Existing.NodeId, 1);
			}
			TestFalse(TEXT("Cannot skip the original tail when buying the new card"), Profile->TryPurchase(Tree, Node->NodeId, Failure));
			TestEqual(TEXT("Rejected skip does not spend bricks"), Profile->IceBricks, 5);
			Profile->Progression.NodeRanks.Add(Parents[I], 1);
			TestTrue(TEXT("The new card can be purchased after its tail is unlocked"), Profile->TryPurchase(Tree, Node->NodeId, Failure));
			TestEqual(TEXT("The permanent unlock spends five bricks"), Profile->IceBricks, 0);
		}
		FRogueUpgradeDefinitionRow Row;if(!TestTrue(TEXT("Card has a real table row"),Definitions->FindDefinition(Types[I],Row)))return false;
		// 永久节点可自定义解锁标题；局内升级卡应读取自己的数据表文案。
		TestFalse(TEXT("The upgrade data table provides a card title"),Row.Title.IsEmpty());
		const auto* Generated=TitleOptions.FindByPredicate([&](const auto& Option){return Option.Type==Types[I];});
		if(TestNotNull(TEXT("The unlocked card is generated through the public pool entry"),Generated))
			TestEqual(TEXT("The generated upgrade uses its data table title"),Generated->Title,Row.Title);
		TestEqual(TEXT("The starter card is available without a purchased rank"),
			Tree->IsUpgradeAvailable({}, Types[I], true), bInitiallyUnlocked);
	}
	return true;
}
#endif
