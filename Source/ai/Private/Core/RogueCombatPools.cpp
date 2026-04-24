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
	void AddUniqueActorClass(TArray<UClass*>& OutClasses, UClass* ActorClass)
	{
		if (ActorClass != nullptr && ActorClass->IsChildOf(TActor::StaticClass()))
		{
			OutClasses.AddUnique(ActorClass);
		}
	}

	template <typename TActor>
	void AddUniqueActorClasses(TArray<UClass*>& OutClasses, const TArray<TSubclassOf<TActor>>& ActorClasses)
	{
		for (const TSubclassOf<TActor>& ActorClass : ActorClasses)
		{
			AddUniqueActorClass<TActor>(OutClasses, ActorClass.Get());
		}
	}

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

			if (Actor->IsAvailableInPool() && Actor->GetClass() == ActorClass)
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
		if (World == nullptr || ActorClass == nullptr || TargetSize <= 0)
		{
			return;
		}

		int32 ExistingClassCount = 0;
		for (int32 Index = Pool.Num() - 1; Index >= 0; --Index)
		{
			TActor* Actor = Pool[Index];
			if (!IsValid(Actor))
			{
				Pool.RemoveAtSwap(Index);
				continue;
			}

			if (Actor->GetClass() == ActorClass)
			{
				++ExistingClassCount;
			}
		}

		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Owner = OwnerActor;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		for (int32 Index = ExistingClassCount; Index < TargetSize; ++Index)
		{
			TActor* Actor = World->SpawnActor<TActor>(ActorClass, PoolLocation, FRotator::ZeroRotator, SpawnParameters);
			if (Actor != nullptr)
			{
				Pool.Add(Actor);
			}
		}
	}

	template <typename TActor>
	void PrewarmActorPoolForClasses(
		UWorld* World,
		TArray<TObjectPtr<TActor>>& Pool,
		int32 TotalTargetSize,
		const TArray<UClass*>& ActorClasses,
		UClass* FallbackClass,
		AActor* OwnerActor,
		const FVector& PoolLocation)
	{
		TArray<UClass*> UniqueClasses;
		AddUniqueActorClass<TActor>(UniqueClasses, FallbackClass);
		for (UClass* ActorClass : ActorClasses)
		{
			AddUniqueActorClass<TActor>(UniqueClasses, ActorClass);
		}

		if (UniqueClasses.Num() == 0 || TotalTargetSize <= 0)
		{
			return;
		}

		const int32 TargetSizePerClass = FMath::Max(1, FMath::CeilToInt(static_cast<float>(TotalTargetSize) / static_cast<float>(UniqueClasses.Num())));
		for (UClass* ActorClass : UniqueClasses)
		{
			PrewarmActorPool(World, Pool, TargetSizePerClass, ActorClass, OwnerActor, PoolLocation);
		}
	}
}

void FRogueCombatPools::Prewarm(
	UWorld* World,
	AActor* OwnerActor,
	const FRoguePoolSettings& Settings,
	TSubclassOf<ARogueEnemy> EnemyClass,
	TSubclassOf<ARogueExperiencePickup> ExperiencePickupClass,
	const FRogueCombatPoolPrewarmClasses& AdditionalClasses)
{
	const FVector PoolLocation(0.0f, 0.0f, -20000.0f);

	TArray<UClass*> EnemyClasses;
	AddUniqueActorClasses(EnemyClasses, AdditionalClasses.EnemyClasses);
	AddUniqueActorClass<ARogueEnemy>(EnemyClasses, EnemyClass.Get());

	TArray<UClass*> PlayerProjectileClasses;
	AddUniqueActorClasses(PlayerProjectileClasses, AdditionalClasses.PlayerProjectileClasses);

	TArray<UClass*> EnemyProjectileClasses;
	AddUniqueActorClasses(EnemyProjectileClasses, AdditionalClasses.EnemyProjectileClasses);

	TArray<UClass*> RocketProjectileClasses;
	AddUniqueActorClasses(RocketProjectileClasses, AdditionalClasses.RocketProjectileClasses);

	TArray<UClass*> ExperiencePickupClasses;
	AddUniqueActorClasses(ExperiencePickupClasses, AdditionalClasses.ExperiencePickupClasses);
	AddUniqueActorClass<ARogueExperiencePickup>(ExperiencePickupClasses, ExperiencePickupClass.Get());

	TArray<UClass*> LaserBeamClasses;
	AddUniqueActorClasses(LaserBeamClasses, AdditionalClasses.LaserBeamClasses);

	TArray<UClass*> OrbitingBladeClasses;
	AddUniqueActorClasses(OrbitingBladeClasses, AdditionalClasses.OrbitingBladeClasses);

	PrewarmActorPoolForClasses(World, EnemyPool, Settings.InitialEnemyPoolSize, EnemyClasses, ARogueEnemy::StaticClass(), OwnerActor, PoolLocation);
	PrewarmActorPoolForClasses(World, PlayerProjectilePool, Settings.InitialPlayerProjectilePoolSize, PlayerProjectileClasses, ARogueProjectile::StaticClass(), OwnerActor, PoolLocation);
	PrewarmActorPoolForClasses(World, EnemyProjectilePool, Settings.InitialEnemyProjectilePoolSize, EnemyProjectileClasses, ARogueEnemyProjectile::StaticClass(), OwnerActor, PoolLocation);
	PrewarmActorPoolForClasses(World, RocketProjectilePool, Settings.InitialRocketProjectilePoolSize, RocketProjectileClasses, ARogueRocketProjectile::StaticClass(), OwnerActor, PoolLocation);
	PrewarmActorPoolForClasses(World, ExperiencePickupPool, Settings.InitialExperiencePickupPoolSize, ExperiencePickupClasses, ARogueExperiencePickup::StaticClass(), OwnerActor, PoolLocation);
	PrewarmActorPool(World, ImpactEffectPool, Settings.InitialImpactEffectPoolSize, ARogueImpactEffect::StaticClass(), OwnerActor, PoolLocation);
	PrewarmActorPoolForClasses(World, LaserBeamPool, Settings.InitialLaserBeamPoolSize, LaserBeamClasses, ARogueLaserBeam::StaticClass(), OwnerActor, PoolLocation);
	PrewarmActorPoolForClasses(World, OrbitingBladePool, Settings.InitialOrbitingBladePoolSize, OrbitingBladeClasses, ARogueOrbitingBlade::StaticClass(), OwnerActor, PoolLocation);
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
