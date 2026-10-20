#if WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystem/RogueAbilitySystemLibrary.h"
#include "AbilitySystem/RogueAttributeSet.h"
#include "AbilitySystem/RogueGameplayTags.h"
#include "Combat/Pooling/RogueCombatPoolSubsystem.h"
#include "Combat/Projectiles/RogueEnemyProjectile.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Enemies/RogueEnemy.h"
#include "Framework/RogueGameMode.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameFramework/WorldSettings.h"
#include "Misc/AutomationTest.h"
#include "Player/RogueCharacter.h"
#include "Player/RoguePlayerController.h"
#include "Tests/AutomationCommon.h"

namespace RogueShockPillarTests
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter;

	struct FFixture
	{
		FTestConsoleVariable WeaponSelfTest{TEXT("Rogue.SelfTest.WeaponUpgradeChain")};
		FTestConsoleVariable EnemyStress{TEXT("Rogue.SelfTest.EnemyStress")};
		FTestConsoleVariable DisableEnemyAttacks{TEXT("Rogue.SelfTest.DisableEnemyAttackAbilities")};
		FTestWorldWrapper TestWorld;
		UWorld* World = nullptr;
		ARogueCharacter* Players[2] = {};
		ARogueEnemy* Pillar = nullptr;

		bool Initialize(FAutomationTestBase& Test)
		{
			WeaponSelfTest.Set(TEXT("0"));
			EnemyStress.Set(TEXT("0"));
			DisableEnemyAttacks.Set(TEXT("0"));
			if (!TestWorld.CreateTestWorld(EWorldType::Game)) { return false; }
			World = TestWorld.GetTestWorld();
			World->GetWorldSettings()->DefaultGameMode = ARogueGameMode::StaticClass();
			if (!TestWorld.BeginPlayInTestWorld()) { return false; }
			ARogueGameMode* GameMode = World->GetAuthGameMode<ARogueGameMode>();
			if (!Test.TestNotNull(TEXT("The authoritative GameMode exists"), GameMode)) { return false; }
			GameMode->SetActorTickEnabled(false);

			for (int32 Index = 0; Index < UE_ARRAY_COUNT(Players); ++Index)
			{
				ARoguePlayerController* Controller = World->SpawnActor<ARoguePlayerController>();
				const FTransform Transform(FVector(1000.0f + Index * 1000.0f, 0.0f, 120.0f));
				Players[Index] = World->SpawnActorDeferred<ARogueCharacter>(ARogueCharacter::StaticClass(),
					Transform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
				if (!Test.TestNotNull(TEXT("The controller exists"), Controller)
					|| !Test.TestNotNull(TEXT("The player exists"), Players[Index])) { return false; }
				Players[Index]->AutoPossessPlayer = EAutoReceiveInput::Disabled;
				Players[Index]->FinishSpawning(Transform);
				Controller->Possess(Players[Index]);
				Players[Index]->SetActorTickEnabled(false);
				Players[Index]->GetCharacterMovement()->SetComponentTickEnabled(false);
				Players[Index]->SetCurrentArmor(0.0f);
				Players[Index]->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
				Players[Index]->GetCapsuleComponent()->SetGenerateOverlapEvents(true);
				Players[Index]->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
				if (!Test.TestTrue(TEXT("The possessed player starts alive"), Players[Index]->IsAlive())) { return false; }
			}

			// Exercise the actual Blueprint and its serialized DataTable reference, including
			// any Blueprint behavior hook that could accidentally keep the old attack alive.
			UClass* PillarClass = LoadClass<ARogueEnemy>(nullptr,
				TEXT("/Game/BP/Enemy/BP_EnemyShockPillar.BP_EnemyShockPillar_C"));
			if (!Test.TestNotNull(TEXT("The project ShockPillar Blueprint loads"), PillarClass)) { return false; }
			FActorSpawnParameters Spawn;
			Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Pillar = World->SpawnActor<ARogueEnemy>(PillarClass, FVector(0.0f, 0.0f, 120.0f), FRotator::ZeroRotator, Spawn);
			if (!Test.TestNotNull(TEXT("The real ShockPillar exists"), Pillar)) { return false; }
			FRogueEnemyProfile Profile;
			Profile.EnemyType = ERogueEnemyType::ShockPillar;
			Profile.Health = 10000.0f;
			Profile.Speed = 0.0f;
			Profile.Damage = 20.0f;
			Profile.ExperienceReward = 0;
			Pillar->ActivatePooledEnemy(nullptr, Pillar->GetActorLocation(), FRotator::ZeroRotator);
			Pillar->InitializeEnemy(Profile);
			Pillar->SetActorTickEnabled(false);
			Pillar->GetCharacterMovement()->SetComponentTickEnabled(false);
			return true;
		}

		TArray<ARogueEnemyProjectile*> ActiveProjectiles() const
		{
			TArray<ARogueEnemyProjectile*> Result;
			for (TActorIterator<ARogueEnemyProjectile> It(World); It; ++It)
			{
				if (!It->IsAvailableInPool()) { Result.Add(*It); }
			}
			return Result;
		}

		ARogueEnemyProjectile* AcquireProjectile(const FVector& Location) const
		{
			URogueCombatPoolSubsystem* Pool = World->GetSubsystem<URogueCombatPoolSubsystem>();
			return Pool ? Pool->AcquireEnemyProjectile(ARogueEnemyProjectile::StaticClass(), Pillar, Pillar,
				Location, FRotator::ZeroRotator) : nullptr;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueShockPillarAISalvoTest,
	"Rogue.Gameplay.ShockPillar.RealBlueprintRadialSalvoAndCooldown", RogueShockPillarTests::Flags)

bool FRogueShockPillarAISalvoTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	RogueShockPillarTests::FFixture Fixture;
	if (!Fixture.Initialize(*this)) { return false; }
	TestTrue(TEXT("The pillar receives the ranged GAS slot"), Fixture.Pillar->RangedAttackAbilityHandle.IsValid());
	TestFalse(TEXT("The pillar receives no contact attack"), Fixture.Pillar->ContactAttackAbilityHandle.IsValid());
	TestEqual(TEXT("No active enemy bullets precede the attack"), Fixture.ActiveProjectiles().Num(), 0);

	Fixture.Pillar->SetRangedAttackCooldown(0.02f);
	Fixture.Pillar->Tick(0.03f);
	TArray<ARogueEnemyProjectile*> Projectiles = Fixture.ActiveProjectiles();
	if (!TestEqual(TEXT("One AI/GAS attack emits exactly 48 bullets"), Projectiles.Num(), 48)) { return false; }
	TestTrue(TEXT("Successful attack starts the configured cooldown"),
		FMath::IsNearlyEqual(Fixture.Pillar->GetRangedAttackCooldownRemaining(),
			Fixture.Pillar->GetRangedArchetype().NextShotCooldown));

	TArray<float> Yaws;
	const FVector PillarLocation = Fixture.Pillar->GetActorLocation();
	const float ExpectedHeight = PillarLocation.Z - Fixture.Pillar->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 48.0f;
	for (ARogueEnemyProjectile* Projectile : Projectiles)
	{
		UProjectileMovementComponent* Movement = Projectile->FindComponentByClass<UProjectileMovementComponent>();
		if (!TestNotNull(TEXT("Each bullet has real projectile movement"), Movement)) { return false; }
		const FVector Direction = Movement->Velocity.GetSafeNormal();
		TestTrue(TEXT("Bullets travel horizontally at 720 cm/s"),
			FMath::IsNearlyZero(Movement->Velocity.Z) && FMath::IsNearlyEqual(static_cast<float>(Movement->Velocity.Size()), 720.0f, 0.01f));
		TestTrue(TEXT("Every bullet spawns 48 cm above the column foot"),
			FMath::IsNearlyEqual(static_cast<float>(Projectile->GetActorLocation().Z), ExpectedHeight, 0.01f));
		TestTrue(TEXT("Every bullet points outward from the column"),
			(Projectile->GetActorLocation() - PillarLocation).GetSafeNormal2D().Equals(Direction, 0.001f));
		Yaws.Add(FRotator::ClampAxis(Direction.Rotation().Yaw));
	}
	Yaws.Sort();
	for (int32 Index = 0; Index < Yaws.Num(); ++Index)
	{
		const float Next = Index + 1 < Yaws.Num() ? Yaws[Index + 1] : Yaws[0] + 360.0f;
		TestTrue(TEXT("The complete ring has uniform 7.5-degree gaps, including its seam"),
			FMath::IsNearlyEqual(Next - Yaws[Index], 7.5f, 0.01f));
	}
	Fixture.Pillar->Tick(0.2f);
	TestEqual(TEXT("AI ticks during cooldown do not emit another ring"), Fixture.ActiveProjectiles().Num(), 48);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueShockPillarProjectileReuseTest,
	"Rogue.Gameplay.ShockPillar.PooledBulletSizeVisualAndLifetimeReset", RogueShockPillarTests::Flags)

bool FRogueShockPillarProjectileReuseTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	RogueShockPillarTests::FFixture Fixture;
	if (!Fixture.Initialize(*this)) { return false; }
	const FVector Location(7000.0f, 0.0f, 120.0f);
	ARogueEnemyProjectile* Projectile = Fixture.AcquireProjectile(Location);
	if (!TestNotNull(TEXT("A real projectile is acquired from the combat pool"), Projectile)) { return false; }
	USphereComponent* Collision = Projectile->FindComponentByClass<USphereComponent>();
	if (!TestNotNull(TEXT("The collision sphere exists"), Collision)) { return false; }

	Projectile->ActivatePooledProjectile(Fixture.Pillar, Fixture.Pillar, Location, FRotator::ZeroRotator,
		FVector::ForwardVector, 720.0f, 7.0f, false, 1.0f, 0.4f, true);
	TestTrue(TEXT("Ring collision scales to 8.8 cm"), FMath::IsNearlyEqual(Collision->GetScaledSphereRadius(), 8.8f, 0.01f));
	Projectile->Tick(0.7f);
	TestFalse(TEXT("A ring bullet survives before its own deadline"), Projectile->IsAvailableInPool());
	TestTrue(TEXT("Visual ticks preserve the small collision size"), FMath::IsNearlyEqual(Collision->GetScaledSphereRadius(), 8.8f, 0.01f));
	Projectile->Tick(0.31f);
	TestTrue(TEXT("A ring bullet returns to the pool at its configured lifetime"), Projectile->IsAvailableInPool());

	// Reuse the same pooled actor with the legacy call shape used by other enemies.
	Projectile->ActivatePooledProjectile(Fixture.Pillar, Fixture.Pillar, Location, FRotator::ZeroRotator,
		FVector::ForwardVector, 920.0f, 7.0f, false);
	TestTrue(TEXT("Ordinary reuse restores the original 22 cm collision"), FMath::IsNearlyEqual(Collision->GetScaledSphereRadius(), 22.0f));
	Projectile->Tick(1.1f);
	TestFalse(TEXT("Ordinary reuse does not retain the short ring lifetime"), Projectile->IsAvailableInPool());
	Projectile->Tick(3.0f);
	TestTrue(TEXT("Ordinary reuse expires at its original four-second lifetime"), Projectile->IsAvailableInPool());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRogueShockPillarProjectileHitTest,
	"Rogue.Gameplay.ShockPillar.AliveImpactAndQueuedGhostOverlap", RogueShockPillarTests::Flags)

bool FRogueShockPillarProjectileHitTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	RogueShockPillarTests::FFixture Fixture;
	if (!Fixture.Initialize(*this)) { return false; }
	ARogueCharacter* Target = Fixture.Players[0];
	ARogueEnemyProjectile* Projectile = Fixture.AcquireProjectile(Target->GetActorLocation());
	if (!TestNotNull(TEXT("The pooled projectile exists"), Projectile)) { return false; }
	const URogueAttributeSet* Attributes = Target->GetRogueAttributeSet();
	if (!TestNotNull(TEXT("The target has its real GAS AttributeSet"), Attributes)) { return false; }
	TestTrue(TEXT("The fixture removes armor in the authoritative attribute set"), FMath::IsNearlyZero(Attributes->GetArmor()));
	// Character Tick is disabled in this fixture, so its display mirrors deliberately
	// do not refresh. Damage assertions must read the authoritative GAS values.
	const float BeforeHealth = Attributes->GetHealth();
	Projectile->ActivatePooledProjectile(Fixture.Pillar, Fixture.Pillar, Target->GetActorLocation(), FRotator::ZeroRotator,
		FVector::ForwardVector, 720.0f, 7.0f, false, 5.0f, 0.4f, true);
	TestTrue(TEXT("A live player's immediate overlap applies one configured hit"),
		FMath::IsNearlyEqual(Attributes->GetHealth(), BeforeHealth - 7.0f, 0.01f));
	TestTrue(TEXT("An immediate impact returns the bullet to the pool"), Projectile->IsAvailableInPool());
	TestTrue(TEXT("Activation cannot reshow a bullet already consumed by overlap"), Projectile->IsHidden());
	TestFalse(TEXT("Activation cannot restore collision after immediate impact"), Projectile->GetActorEnableCollision());

	if (!TestTrue(TEXT("The real GAS lethal damage is accepted"), RogueAbilitySystem::ApplyDamageToTarget(
		Fixture.Players[1], Target, 1000000.0f, RogueGameplayTags::Damage_Source_Projectile))) { return false; }
	Fixture.TestWorld.TickTestWorld(0.02f);
	Fixture.TestWorld.TickTestWorld(0.02f);
	if (!TestTrue(TEXT("A teammate keeps the target in the revivable downed phase"), Target->IsDowned())) { return false; }
	const float DownedHealth = Attributes->GetHealth();
	TestTrue(TEXT("Lethal damage leaves the real GAS health at zero"), FMath::IsNearlyZero(DownedHealth));
	Projectile->ActivatePooledProjectile(Fixture.Pillar, Fixture.Pillar, Target->GetActorLocation(), FRotator::ZeroRotator,
		FVector::ForwardVector, 720.0f, 7.0f, false, 5.0f, 0.4f, true);
	USphereComponent* Collision = Projectile->FindComponentByClass<USphereComponent>();
	if (!TestNotNull(TEXT("The projectile collision exists"), Collision)) { return false; }
	// A deferred overlap may have been queued before the Pawn disabled collision.
	// Broadcast the component's real delegate to exercise that callback race.
	Collision->OnComponentBeginOverlap.Broadcast(Collision, Target, Target->GetCapsuleComponent(), 0, false, FHitResult());
	TestFalse(TEXT("An already queued ghost overlap does not consume the bullet"), Projectile->IsAvailableInPool());
	TestTrue(TEXT("A ghost takes no additional damage"), FMath::IsNearlyEqual(Attributes->GetHealth(), DownedHealth));
	Collision->OnComponentBeginOverlap.Broadcast(Collision, Fixture.Pillar, Fixture.Pillar->GetCapsuleComponent(), 0, false, FHitResult());
	TestFalse(TEXT("Ring bullets also survive overlap with the firing column"), Projectile->IsAvailableInPool());
	return true;
}

#endif
