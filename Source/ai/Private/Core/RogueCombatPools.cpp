#include "Core/RogueCombatPools.h"

#include "Combat/RogueImpactEffect.h"
#include "Combat/RogueLaserBeam.h"
#include "Combat/RogueOrbitingBlade.h"
#include "Combat/RogueProjectile.h"
#include "Combat/RogueRocketProjectile.h"
#include "Enemies/RogueEnemy.h"
#include "Enemies/RogueEnemyProjectile.h"
#include "World/RogueExperiencePickup.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"

namespace
{
	template <typename TActor>
	TActor* AcquirePooledActor(
		UWorld* World,
		TArray<TObjectPtr<TActor>>& Pool,
		UClass* ActorClass,
		AActor* OwnerActor,
		APawn* InstigatorPawn,
		const FVector& SpawnLocation,
		const FRotator& SpawnRotation)
	{
		if (World == nullptr || ActorClass == nullptr)
		{
			return nullptr;
		}

		for (int32 Index = Pool.Num() - 1; Index >= 0; --Index)
		{
			TActor* Actor = Pool[Index];
			if (!IsValid(Actor))
			{
				Pool.RemoveAtSwap(Index);
				continue;
			}

			if (Actor->IsAvailableInPool() && Actor->IsA(ActorClass))
			{
				Actor->SetOwner(OwnerActor);
				Actor->SetInstigator(InstigatorPawn);
				Actor->SetActorLocationAndRotation(SpawnLocation, SpawnRotation, false, nullptr, ETeleportType::TeleportPhysics);
				return Actor;
			}
		}

		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Owner = OwnerActor;
		SpawnParameters.Instigator = InstigatorPawn;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		TActor* Actor = World->SpawnActor<TActor>(ActorClass, SpawnLocation, SpawnRotation, SpawnParameters);
		if (Actor != nullptr)
		{
			Pool.Add(Actor);
		}

		return Actor;
	}

	template <typename TActor>
	void PrewarmActorPool(
		UWorld* World,
		TArray<TObjectPtr<TActor>>& Pool,
		int32 TargetSize,
		UClass* ActorClass,
		AActor* OwnerActor,
		const FVector& PoolLocation)
	{
		if (World == nullptr || ActorClass == nullptr || TargetSize <= Pool.Num())
		{
			return;
		}

		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Owner = OwnerActor;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		for (int32 Index = Pool.Num(); Index < TargetSize; ++Index)
		{
			TActor* Actor = World->SpawnActor<TActor>(ActorClass, PoolLocation, FRotator::ZeroRotator, SpawnParameters);
			if (Actor != nullptr)
			{
				Pool.Add(Actor);
			}
		}
	}
}

void FRogueCombatPools::Prewarm(
	UWorld* World,
	AActor* OwnerActor,
	const FRoguePoolSettings& Settings,
	TSubclassOf<ARogueEnemy> EnemyClass,
	TSubclassOf<ARogueExperiencePickup> ExperiencePickupClass)
{
	const FVector PoolLocation(0.0f, 0.0f, -20000.0f);
	UClass* EnemyToSpawn = EnemyClass ? EnemyClass.Get() : ARogueEnemy::StaticClass();
	UClass* PickupClassToSpawn = ExperiencePickupClass ? ExperiencePickupClass.Get() : ARogueExperiencePickup::StaticClass();

	PrewarmActorPool(World, EnemyPool, Settings.InitialEnemyPoolSize, EnemyToSpawn, OwnerActor, PoolLocation);
	PrewarmActorPool(World, PlayerProjectilePool, Settings.InitialPlayerProjectilePoolSize, ARogueProjectile::StaticClass(), OwnerActor, PoolLocation);
	PrewarmActorPool(World, EnemyProjectilePool, Settings.InitialEnemyProjectilePoolSize, ARogueEnemyProjectile::StaticClass(), OwnerActor, PoolLocation);
	PrewarmActorPool(World, RocketProjectilePool, Settings.InitialRocketProjectilePoolSize, ARogueRocketProjectile::StaticClass(), OwnerActor, PoolLocation);
	PrewarmActorPool(World, ExperiencePickupPool, Settings.InitialExperiencePickupPoolSize, PickupClassToSpawn, OwnerActor, PoolLocation);
	PrewarmActorPool(World, ImpactEffectPool, Settings.InitialImpactEffectPoolSize, ARogueImpactEffect::StaticClass(), OwnerActor, PoolLocation);
	PrewarmActorPool(World, LaserBeamPool, Settings.InitialLaserBeamPoolSize, ARogueLaserBeam::StaticClass(), OwnerActor, PoolLocation);
	PrewarmActorPool(World, OrbitingBladePool, Settings.InitialOrbitingBladePoolSize, ARogueOrbitingBlade::StaticClass(), OwnerActor, PoolLocation);
}

ARogueEnemy* FRogueCombatPools::AcquireEnemy(UWorld* World, TSubclassOf<ARogueEnemy> EnemyClass, AActor* OwnerActor, const FVector& SpawnLocation, const FRotator& SpawnRotation)
{
	UClass* EnemyClassToSpawn = EnemyClass ? EnemyClass.Get() : ARogueEnemy::StaticClass();
	return AcquirePooledActor(World, EnemyPool, EnemyClassToSpawn, OwnerActor, nullptr, SpawnLocation, SpawnRotation);
}

ARogueProjectile* FRogueCombatPools::AcquirePlayerProjectile(UWorld* World, TSubclassOf<ARogueProjectile> ProjectileClass, AActor* OwnerActor, APawn* InstigatorPawn, const FVector& SpawnLocation, const FRotator& SpawnRotation)
{
	UClass* ProjectileClassToSpawn = ProjectileClass ? ProjectileClass.Get() : ARogueProjectile::StaticClass();
	return AcquirePooledActor(World, PlayerProjectilePool, ProjectileClassToSpawn, OwnerActor, InstigatorPawn, SpawnLocation, SpawnRotation);
}

ARogueEnemyProjectile* FRogueCombatPools::AcquireEnemyProjectile(UWorld* World, TSubclassOf<ARogueEnemyProjectile> ProjectileClass, AActor* OwnerActor, APawn* InstigatorPawn, const FVector& SpawnLocation, const FRotator& SpawnRotation)
{
	UClass* ProjectileClassToSpawn = ProjectileClass ? ProjectileClass.Get() : ARogueEnemyProjectile::StaticClass();
	return AcquirePooledActor(World, EnemyProjectilePool, ProjectileClassToSpawn, OwnerActor, InstigatorPawn, SpawnLocation, SpawnRotation);
}

ARogueRocketProjectile* FRogueCombatPools::AcquireRocketProjectile(UWorld* World, TSubclassOf<ARogueRocketProjectile> ProjectileClass, AActor* OwnerActor, APawn* InstigatorPawn, const FVector& SpawnLocation, const FRotator& SpawnRotation)
{
	UClass* ProjectileClassToSpawn = ProjectileClass ? ProjectileClass.Get() : ARogueRocketProjectile::StaticClass();
	return AcquirePooledActor(World, RocketProjectilePool, ProjectileClassToSpawn, OwnerActor, InstigatorPawn, SpawnLocation, SpawnRotation);
}

ARogueExperiencePickup* FRogueCombatPools::AcquireExperiencePickup(UWorld* World, TSubclassOf<ARogueExperiencePickup> PickupClass, AActor* OwnerActor, const FVector& SpawnLocation)
{
	UClass* PickupClassToSpawn = PickupClass ? PickupClass.Get() : ARogueExperiencePickup::StaticClass();
	return AcquirePooledActor(World, ExperiencePickupPool, PickupClassToSpawn, OwnerActor, nullptr, SpawnLocation, FRotator::ZeroRotator);
}

ARogueImpactEffect* FRogueCombatPools::AcquireImpactEffect(UWorld* World, AActor* OwnerActor, const FVector& SpawnLocation, const FRotator& SpawnRotation)
{
	return AcquirePooledActor(World, ImpactEffectPool, ARogueImpactEffect::StaticClass(), OwnerActor, nullptr, SpawnLocation, SpawnRotation);
}

ARogueLaserBeam* FRogueCombatPools::AcquireLaserBeam(UWorld* World, TSubclassOf<ARogueLaserBeam> BeamClass, AActor* OwnerActor, const FVector& SpawnLocation, const FRotator& SpawnRotation)
{
	UClass* BeamClassToSpawn = BeamClass ? BeamClass.Get() : ARogueLaserBeam::StaticClass();
	return AcquirePooledActor(World, LaserBeamPool, BeamClassToSpawn, OwnerActor, nullptr, SpawnLocation, SpawnRotation);
}

ARogueOrbitingBlade* FRogueCombatPools::AcquireOrbitingBlade(UWorld* World, TSubclassOf<ARogueOrbitingBlade> BladeClass, AActor* OwnerActor, const FVector& SpawnLocation, const FRotator& SpawnRotation)
{
	UClass* BladeClassToSpawn = BladeClass ? BladeClass.Get() : ARogueOrbitingBlade::StaticClass();
	return AcquirePooledActor(World, OrbitingBladePool, BladeClassToSpawn, OwnerActor, nullptr, SpawnLocation, SpawnRotation);
}
