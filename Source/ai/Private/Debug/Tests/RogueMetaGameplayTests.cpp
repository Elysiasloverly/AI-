#if WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystem/RogueAttributeSet.h"
#include "AbilitySystem/RogueAbilitySystemLibrary.h"
#include "AbilitySystem/RogueGameplayTags.h"
#include "Combat/Weapons/RogueWeaponBase.h"
#include "Combat/Weapons/RogueWeapon_Projectile.h"
#include "Combat/Weapons/RogueWeapon_Scythe.h"
#include "Combat/Weapons/RogueWeapon_Rocket.h"
#include "Combat/Weapons/RogueWeapon_Laser.h"
#include "Combat/Weapons/RogueWeapon_HellTower.h"
#include "Combat/Weapons/RogueWeapon_Mortar.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "Framework/RogueGameMode.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/WorldSettings.h"
#include "Misc/AutomationTest.h"
#include "Player/RogueCharacter.h"
#include "Player/RoguePlayerBalanceAsset.h"
#include "Player/RoguePlayerController.h"
#include "Player/RoguePlayerState.h"
#include "Progression/Meta/RogueProgressionTypes.h"
#include "Progression/Meta/RogueUpgradeTreeDefinition.h"
#include "Progression/Upgrades/RogueUpgradeSystemRules.h"
#include "Progression/Upgrades/RogueUpgradeSystem.h"
#include "Progression/Shop/RogueShopSystem.h"
#include "Tests/AutomationCommon.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

namespace RogueMetaGameplayTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;

	void AddUnlock(FRogueProgressionSnapshot& Profile, ERogueMetaNodeKind Kind, ERogueUpgradeType Type)
	{
		for (const FRogueUpgradeTreeNode& Node : URogueUpgradeTreeDefinition::GetDefaultDefinition()->GetNodes())
		{
			if (Node.Kind == Kind && Node.UpgradeType == Type)
			{
				Profile.NodeRanks.Add(Node.NodeId, 1);
			}
		}
	}

	struct FFixture
	{
		FTestConsoleVariable WeaponSelfTest{TEXT("Rogue.SelfTest.WeaponUpgradeChain")};
		FTestConsoleVariable EnemyStress{TEXT("Rogue.SelfTest.EnemyStress")};
		TStrongObjectPtr<URoguePlayerBalanceAsset> Balance{NewObject<URoguePlayerBalanceAsset>()};
		FTestWorldWrapper TestWorld;
		UWorld* World = nullptr;
		TArray<ARoguePlayerController*> Controllers;
		TArray<ARogueCharacter*> Players;

		bool Initialize(FAutomationTestBase& Test, const TArray<FRogueProgressionSnapshot>& Profiles)
		{
			WeaponSelfTest.Set(TEXT("0"));
			EnemyStress.Set(TEXT("0"));
			if (!TestWorld.CreateTestWorld(EWorldType::Game)) { return false; }
			World = TestWorld.GetTestWorld();
			World->GetWorldSettings()->DefaultGameMode = ARogueGameMode::StaticClass();
			if (!TestWorld.BeginPlayInTestWorld()) { return false; }
			ARogueGameMode* Mode = World->GetAuthGameMode<ARogueGameMode>();
			if (!Test.TestNotNull(TEXT("An authoritative game mode exists"), Mode)) { return false; }
			Mode->SetActorTickEnabled(false);

			// 故意模拟旧表：所有武器基础数量 9。角色开局规则必须覆盖数量，而不改动原表。
			UDataTable* WeaponTable = NewObject<UDataTable>(World);
			WeaponTable->RowStruct = FRogueWeaponTableRow::StaticStruct();
			const TSubclassOf<ARogueWeaponBase> WeaponClasses[] = {
				ARogueWeapon_Projectile::StaticClass(), ARogueWeapon_Scythe::StaticClass(),
				ARogueWeapon_Rocket::StaticClass(), ARogueWeapon_Laser::StaticClass(),
				ARogueWeapon_HellTower::StaticClass(), ARogueWeapon_Mortar::StaticClass()
			};
			const ERogueUpgradeType QuantityTypes[] = {
				ERogueUpgradeType::ProjectileCount, ERogueUpgradeType::ScytheCount,
				ERogueUpgradeType::RocketCount, ERogueUpgradeType::LaserCount,
				ERogueUpgradeType::HellTowerCount, ERogueUpgradeType::MortarCount
			};
			for (int32 Index = 0; Index < UE_ARRAY_COUNT(QuantityTypes); ++Index)
			{
				FRogueWeaponTableRow Row;
				Row.WeaponClass = WeaponClasses[Index];
				Row.CountUpgradeType = QuantityTypes[Index];
				Row.Count = 9;
				WeaponTable->AddRow(FName(*FString::Printf(TEXT("Weapon%d"), Index)), Row);
			}
			FObjectProperty* TableProperty = FindFProperty<FObjectProperty>(ARogueCharacter::StaticClass(), TEXT("WeaponDataTable"));
			FProperty* BalanceProperty = FindFProperty<FProperty>(ARogueCharacter::StaticClass(), TEXT("PlayerBalanceAsset"));
			if (!Test.TestNotNull(TEXT("The table property exists"), TableProperty)
				|| !Test.TestNotNull(TEXT("The balance property exists"), BalanceProperty)) { return false; }
			for (int32 Index = 0; Index < Profiles.Num(); ++Index)
			{
				ARoguePlayerController* Controller = World->SpawnActor<ARoguePlayerController>();
				if (!Test.TestNotNull(TEXT("The controller exists"), Controller)) { return false; }
				ARoguePlayerState* State = Controller->GetPlayerState<ARoguePlayerState>();
				if (!Test.TestNotNull(TEXT("The per-player state exists"), State)
					|| !Test.TestTrue(TEXT("The profile is accepted before possession"), State->SetProgressionSnapshotForRun(Profiles[Index]))) { return false; }
				const FTransform Transform(FVector(Index * 500.0f, 0.0f, 120.0f));
				ARogueCharacter* Player = World->SpawnActorDeferred<ARogueCharacter>(ARogueCharacter::StaticClass(), Transform,
					nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
				if (!Test.TestNotNull(TEXT("The character exists"), Player)) { return false; }
				TableProperty->SetObjectPropertyValue_InContainer(Player, WeaponTable);
				// SoftObjectProperty 的值直接使用配置类型写入，避免依赖外部已保存资产。
				TSoftObjectPtr<URoguePlayerBalanceAsset>* Asset = BalanceProperty->ContainerPtrToValuePtr<TSoftObjectPtr<URoguePlayerBalanceAsset>>(Player);
				*Asset = Balance.Get();
				Player->FinishSpawning(Transform);
				Controller->Possess(Player);
				Player->GetCharacterMovement()->SetComponentTickEnabled(false);
				Player->SetActorTickEnabled(false);
				Player->SetActorEnableCollision(false);
				Controllers.Add(Controller);
				Players.Add(Player);
				if (!Test.TestTrue(TEXT("The real GAS baseline is initialized"), State->IsBaseAttributesInitialized())) { return false; }
			}
			return true;
		}

		TArray<ERogueUpgradeType> Pool(int32 Index) const
		{
			TArray<ERogueUpgradeType> Result;
			RogueUpgradeSystemRules::BuildUpgradePool(Result, Players[Index], nullptr);
			return Result;
		}

		void Apply(int32 Index, ERogueUpgradeType Type, float Magnitude = 1.0f) const
		{
			FRogueUpgradeOption Upgrade;
			Upgrade.Type = Type;
			Upgrade.Magnitude = Magnitude;
			Players[Index]->ApplyUpgrade(Upgrade);
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueMetaFreshRunTest,
	"Rogue.Progression.Gameplay.FreshRunOneWeaponAndNoArmor", RogueMetaGameplayTests::Flags)

bool FRogueMetaFreshRunTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RogueMetaGameplayTests;
	// Arrange: 旧资产的装备和护甲默认值不能越过新档案的规则。
	FFixture Fixture;
	Fixture.Balance->BaseStats.MaxArmor = 80.0f;
	Fixture.Balance->InitialWeaponBonuses.ProjectileCount = 8;
	Fixture.Balance->InitialWeaponBonuses.ScytheCount = 8;
	Fixture.Balance->InitialWeaponBonuses.LaserCount = 8;
	Fixture.Balance->InitialWeaponBonuses.ScytheSize = 0.8f;
	if (!Fixture.Initialize(*this, {FRogueProgressionSnapshot()})) { return false; }

	// Act / Assert: 读取真实武器实例、GAS 和双方卡池共用的过滤入口。
	ARogueCharacter* Player = Fixture.Players[0];
	TestEqual(TEXT("One talisman starts equipped"), Player->GetEffectiveProjectileCount(), 1);
	TestEqual(TEXT("The scythe starts unequipped"), Player->GetEffectiveScytheCount(), 0);
	TestEqual(TEXT("The rocket starts unequipped"), Player->GetEffectiveRocketLauncherCount(), 0);
	TestEqual(TEXT("The laser starts unequipped"), Player->GetEffectiveLaserCannonCount(), 0);
	TestEqual(TEXT("The tower starts unequipped"), Player->GetEffectiveHellTowerCount(), 0);
	TestEqual(TEXT("The mortar starts unequipped"), Player->GetEffectiveMortarCount(), 0);
	TestEqual(TEXT("Locked armor has no capacity"), Player->GetRogueAttributeSet()->GetMaxArmor(), 0.0f);
	TestEqual(TEXT("Locked armor starts empty"), Player->GetRogueAttributeSet()->GetArmor(), 0.0f);
	TestEqual(TEXT("Old initial dedicated bonuses are absent"), Player->GetWeaponBonusSnapshot().ScytheSize, 0.0f);
	const TArray<ERogueUpgradeType> Pool = Fixture.Pool(0);
	TestTrue(TEXT("Talisman upgrades are free from the start"), Pool.Contains(ERogueUpgradeType::ProjectileCount));
	TestTrue(TEXT("General health upgrades remain available"), Pool.Contains(ERogueUpgradeType::MaxHealth));
	TestFalse(TEXT("Locked weapon acquisition is excluded"), Pool.Contains(ERogueUpgradeType::ScytheCount));
	TestFalse(TEXT("Locked armor capacity cards are excluded"), Pool.Contains(ERogueUpgradeType::ArmorCapacity));
	TestFalse(TEXT("Locked armor recovery cards are excluded"), Pool.Contains(ERogueUpgradeType::Armor));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueMetaPerPlayerPoolTest,
	"Rogue.Progression.Gameplay.IndependentProfilesAndDedicatedCards", RogueMetaGameplayTests::Flags)

bool FRogueMetaPerPlayerPoolTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RogueMetaGameplayTests;
	// Arrange: 两个玩家使用不同的武器许可；专属卡沿各自分支逐层购买，玩家之间的许可相互独立。
	FRogueProgressionSnapshot LaserProfile;
	AddUnlock(LaserProfile, ERogueMetaNodeKind::Weapon, ERogueUpgradeType::LaserCount);
	AddUnlock(LaserProfile, ERogueMetaNodeKind::UpgradeCard, ERogueUpgradeType::LaserCount);
	AddUnlock(LaserProfile, ERogueMetaNodeKind::UpgradeCard, ERogueUpgradeType::LaserRefraction);
	FRogueProgressionSnapshot ScytheProfile;
	AddUnlock(ScytheProfile, ERogueMetaNodeKind::Weapon, ERogueUpgradeType::ScytheCount);
	AddUnlock(ScytheProfile, ERogueMetaNodeKind::UpgradeCard, ERogueUpgradeType::ScytheCount);
	AddUnlock(ScytheProfile, ERogueMetaNodeKind::UpgradeCard, ERogueUpgradeType::ScytheSize);
	FFixture Fixture;
	if (!Fixture.Initialize(*this, {LaserProfile, ScytheProfile})) { return false; }
	const TArray<ERogueUpgradeType> BeforeLaser = Fixture.Pool(0);
	const TArray<ERogueUpgradeType> BeforeScythe = Fixture.Pool(1);
	TestTrue(TEXT("The laser owner may acquire one laser in this run"), BeforeLaser.Contains(ERogueUpgradeType::LaserCount));
	TestFalse(TEXT("The laser owner does not borrow the other player's scythe license"), BeforeLaser.Contains(ERogueUpgradeType::ScytheCount));
	TestFalse(TEXT("Permanent refraction still requires a current-run laser"), BeforeLaser.Contains(ERogueUpgradeType::LaserRefraction));
	TestTrue(TEXT("The scythe owner may acquire one scythe"), BeforeScythe.Contains(ERogueUpgradeType::ScytheCount));
	TestFalse(TEXT("Permanent size still requires a current-run scythe"), BeforeScythe.Contains(ERogueUpgradeType::ScytheSize));
	TestEqual(TEXT("Permanent weapon unlock does not equip a starting laser"), Fixture.Players[0]->GetEffectiveLaserCannonCount(), 0);

	// Act: 使用局内真实 GE 路径获取武器，无需等下一帧。
	Fixture.Apply(0, ERogueUpgradeType::LaserCount);
	Fixture.Apply(1, ERogueUpgradeType::ScytheCount);

	// Assert: 首次获取卡和重复数量卡不同；专属卡还需要自己的永久许可。
	const TArray<ERogueUpgradeType> AfterLaser = Fixture.Pool(0);
	const TArray<ERogueUpgradeType> AfterScythe = Fixture.Pool(1);
	TestTrue(TEXT("Owned and permanently unlocked refraction becomes available"), AfterLaser.Contains(ERogueUpgradeType::LaserRefraction));
	TestTrue(TEXT("Sequential refraction profile already purchased the middle quantity card"), AfterLaser.Contains(ERogueUpgradeType::LaserCount));
	TestTrue(TEXT("The independently purchased size card becomes available"), AfterScythe.Contains(ERogueUpgradeType::ScytheSize));
	TestFalse(TEXT("Size does not unlock lifesteal for free"), AfterScythe.Contains(ERogueUpgradeType::ScytheLifesteal));
	TestTrue(TEXT("Sequential size profile already purchased the middle quantity card"), AfterScythe.Contains(ERogueUpgradeType::ScytheCount));
	TestFalse(TEXT("One player's unlocks never change the other's pool"), AfterScythe.Contains(ERogueUpgradeType::LaserCount));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueMetaBaselineIdempotenceTest,
	"Rogue.Progression.Gameplay.PermanentBaselineDoesNotStackOnRepossess", RogueMetaGameplayTests::Flags)

bool FRogueMetaBaselineIdempotenceTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RogueMetaGameplayTests;
	// Arrange: 所有基础属性两级，护甲的付费前置显式存在。
	const URogueUpgradeTreeDefinition* Tree = URogueUpgradeTreeDefinition::GetDefaultDefinition();
	FRogueProgressionSnapshot Profile;
	for (const FRogueUpgradeTreeNode& Node : Tree->GetNodes())
	{
		if (Node.Kind == ERogueMetaNodeKind::ArmorUnlock || Node.Kind == ERogueMetaNodeKind::BaseStat)
		{
			Profile.NodeRanks.Add(Node.NodeId, Node.Kind == ERogueMetaNodeKind::BaseStat ? 2 : 1);
		}
	}
	FFixture Fixture;
	if (!Fixture.Initialize(*this, {Profile, FRogueProgressionSnapshot()})) { return false; }
	ARogueCharacter* Player = Fixture.Players[0];
	URogueAttributeSet* Attributes = Player->GetRogueAttributeSet();
	const FRoguePlayerBaseStatConfig& Base = Fixture.Balance->BaseStats;
	const FRogueInitialWeaponBonusConfig& Initial = Fixture.Balance->InitialWeaponBonuses;
	TestEqual(TEXT("Permanent health is part of the baseline"), Attributes->GetMaxHealth(), Base.MaxHealth + Tree->GetStatBonus(Profile, ERogueMetaStat::MaxHealth));
	TestEqual(TEXT("Paid armor unlock enables configured armor plus its ranks"), Attributes->GetMaxArmor(), Base.MaxArmor + Tree->GetStatBonus(Profile, ERogueMetaStat::ArmorCapacity));
	TestEqual(TEXT("Permanent movement applies once"), Attributes->GetMoveSpeed(), Base.MoveSpeed + Tree->GetStatBonus(Profile, ERogueMetaStat::MoveSpeed));
	TestEqual(TEXT("Permanent pickup range applies once"), Attributes->GetPickupRadius(), Base.PickupRadius + Tree->GetStatBonus(Profile, ERogueMetaStat::PickupRadius));
	TestEqual(TEXT("Permanent recovery applies once"), Attributes->GetHealthRegenPerSecond(), Base.HealthRegenPerSecond + Tree->GetStatBonus(Profile, ERogueMetaStat::HealthRegen));
	TestEqual(TEXT("Permanent attack speed applies once"), Attributes->GetAttackSpeedBonus(), Initial.AttackSpeed + Tree->GetStatBonus(Profile, ERogueMetaStat::AttackSpeed));
	TestEqual(TEXT("Permanent experience applies once"), Attributes->GetExperienceMultiplier(), Base.ExperienceMultiplier + Tree->GetStatBonus(Profile, ERogueMetaStat::ExperienceGain));
	TestEqual(TEXT("Other player's fresh profile does not receive armor"), Fixture.Players[1]->GetRogueAttributeSet()->GetMaxArmor(), 0.0f);
	const float PermanentAttack = Initial.AttackPower + Tree->GetStatBonus(Profile, ERogueMetaStat::AttackPower);
	TestEqual(TEXT("Permanent attack applies once"), Attributes->GetAttackPowerBonus(), PermanentAttack);

	// Act: 局内 GE 叠加到永久基值，再反复通知与重新接管。
	Fixture.Apply(0, ERogueUpgradeType::AttackPower, 3.0f);
	Player->SetCurrentHealth(37.0f);
	Player->NotifyProgressionReady();
	Fixture.Controllers[0]->UnPossess();
	Fixture.Controllers[0]->Possess(Player);
	Player->NotifyProgressionReady();

	// Assert: 不补满当前生命，也不重放永久成长或清除局内 GE。
	TestEqual(TEXT("The run effect and permanent baseline are distinct"), Attributes->GetAttackPowerBonus(), PermanentAttack + 3.0f);
	TestEqual(TEXT("Repeated readiness and possession preserve current health"), Attributes->GetHealth(), 37.0f);
	TestEqual(TEXT("The next player still has an independent attack baseline"), Fixture.Players[1]->GetRogueAttributeSet()->GetAttackPowerBonus(), Initial.AttackPower);
	return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueMetaStaleShopOfferTest,
	"Rogue.Progression.Gameplay.StaleAcquisitionOfferDoesNotCharge", RogueMetaGameplayTests::Flags)

bool FRogueMetaStaleShopOfferTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RogueMetaGameplayTests;
	// Arrange: 玩家只解锁极限火花的首次获取，没有重复数量卡的许可。
	FRogueProgressionSnapshot Profile;
	AddUnlock(Profile, ERogueMetaNodeKind::Weapon, ERogueUpgradeType::LaserCount);
	FFixture Fixture;
	if (!Fixture.Initialize(*this, {Profile})) { return false; }
	ARogueCharacter* Player = Fixture.Players[0];
	Player->AddMoney(100);
	FRogueUpgradeSystem Upgrades;
	FRogueShopSystem Shop;
	Shop.Configure(100, 2, 1, 60.0f);
	if (!TestTrue(TEXT("The real shop opens with the current player's filtered pool"), Shop.OpenShop(Player, Upgrades))) { return false; }
	const int32 OfferIndex = Shop.GetOffers().IndexOfByPredicate([](const FRogueShopOffer& Offer)
	{
		return Offer.GetUpgrade().Type == ERogueUpgradeType::LaserCount;
	});
	if (!TestTrue(TEXT("An initial acquisition offer exists before gaining the laser"), OfferIndex != INDEX_NONE)) { return false; }
	TestFalse(TEXT("Shop does not offer another locked weapon"), Shop.GetOffers().ContainsByPredicate([](const FRogueShopOffer& Offer)
	{
		return Offer.GetUpgrade().Type == ERogueUpgradeType::ScytheCount;
	}));

	// Act: 在商店报价仍保留时，通过局内升级获取极限火花。
	Fixture.Apply(0, ERogueUpgradeType::LaserCount);
	const int32 PreviousMoney = Player->GetMoney();
	FRogueUpgradeOption Bought;
	const bool bPurchased = Shop.TryBuyOffer(OfferIndex, Player, Bought);

	// Assert: 这份旧报价如今属于未解锁的重复数量卡，必须在扣费前拒绝。
	TestFalse(TEXT("A stale acquisition offer cannot bypass the dedicated-card unlock"), bPurchased);
	TestEqual(TEXT("A rejected stale offer never charges money"), Player->GetMoney(), PreviousMoney);
	TestFalse(TEXT("The rejected offer is not marked purchased"), Shop.GetOffers()[OfferIndex].IsPurchased());
	return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueMetaPendingProfileTest,
	"Rogue.Progression.Gameplay.PendingProfileCannotBeTargetedOrDowned", RogueMetaGameplayTests::Flags)

bool FRogueMetaPendingProfileTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace RogueMetaGameplayTests;
	// Arrange: 模拟 PlayerState 已到达而所属玩家档案仍在传输的窗口。
	FTestWorldWrapper TestWorld;
	if (!TestWorld.CreateTestWorld(EWorldType::Game)) { return false; }
	UWorld* World = TestWorld.GetTestWorld();
	World->GetWorldSettings()->DefaultGameMode = ARogueGameMode::StaticClass();
	if (!TestWorld.BeginPlayInTestWorld()) { return false; }
	World->GetAuthGameMode<ARogueGameMode>()->SetActorTickEnabled(false);
	ARoguePlayerController* Controller = World->SpawnActor<ARoguePlayerController>();
	if (!TestNotNull(TEXT("A controller exists for the pending player"), Controller)) { return false; }
	ARoguePlayerState* State = Controller->GetPlayerState<ARoguePlayerState>();
	if (!TestNotNull(TEXT("A PlayerState exists before the profile arrives"), State)) { return false; }
	FBoolProperty* ReadyProperty = FindFProperty<FBoolProperty>(ARoguePlayerState::StaticClass(), TEXT("bProgressionReady"));
	FObjectProperty* PawnStateProperty = FindFProperty<FObjectProperty>(APawn::StaticClass(), TEXT("PlayerState"));
	if (!TestNotNull(TEXT("The replicated ready flag can arrange a delayed handshake"), ReadyProperty)
		|| !TestNotNull(TEXT("The Pawn's reflected PlayerState exists"), PawnStateProperty)) { return false; }
	// 普通无本地玩家的自动测试世界会自动给空档案；这里只恢复真实联机中的未就绪状态。
	ReadyProperty->SetPropertyValue_InContainer(State, false);
	const FTransform Transform(FVector(0.0f, 0.0f, 120.0f));
	ARogueCharacter* Player = World->SpawnActorDeferred<ARogueCharacter>(ARogueCharacter::StaticClass(), Transform,
		nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!TestNotNull(TEXT("The pending character exists"), Player)) { return false; }
	PawnStateProperty->SetObjectPropertyValue_InContainer(Player, State);
	Player->FinishSpawning(Transform);
	Player->GetCharacterMovement()->SetComponentTickEnabled(false);
	Player->SetActorTickEnabled(false);
	TestFalse(TEXT("Pending attributes are not a valid target"), Player->IsAlive());
	TestFalse(TEXT("Waiting for a profile does not mean downed"), Player->IsDowned());
	TestFalse(TEXT("Pending collision is suspended until the baseline exists"), Player->GetActorEnableCollision());

	// Act / Assert: 共享伤害入口与直接生命周期请求都不能把握手窗口误判成濒死。
	TestFalse(TEXT("Enemy damage rejects a player without a baseline"),
		RogueAbilitySystem::ApplyDamageToTarget(Controller, Player, 100.0f, RogueGameplayTags::Damage_Source_Projectile));
	Player->Die();
	TestFalse(TEXT("A pending player cannot start a revive timer"), Player->IsDowned());

	// 档案到达后通过同一公开通知恢复；不需要死亡/复活来解开碰撞。
	if (!TestTrue(TEXT("The delayed profile is accepted"), State->SetProgressionSnapshotForRun(FRogueProgressionSnapshot()))) { return false; }
	Player->NotifyProgressionReady();
	Controller->Possess(Player);
	TestTrue(TEXT("Ready attributes make the character targetable"), Player->IsAlive());
	TestTrue(TEXT("Readiness restores authored actor collision"), Player->GetActorEnableCollision());
	TestFalse(TEXT("The ready player remains alive rather than reviving"), Player->IsDowned());
	return true;
}

#endif
