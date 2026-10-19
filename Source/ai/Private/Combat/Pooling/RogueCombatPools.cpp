#include "Combat/Pooling/RogueCombatPools.h"

#include "Combat/Visuals/RogueImpactEffect.h"
#include "Combat/Visuals/RogueLaserBeam.h"
#include "Combat/Projectiles/RogueOrbitingBlade.h"
#include "Combat/Projectiles/RogueProjectile.h"
#include "Combat/Projectiles/RogueRocketProjectile.h"
#include "Enemies/RogueEnemy.h"
#include "Combat/Projectiles/RogueEnemyProjectile.h"
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

	/** 剔除池里已失效（被销毁 / GC）的对象。
	 *  只在"未命中"的冷路径调用，避免在每次取用的热路径上改动数组。 */
	template <typename TActor>
	void CompactPool(TArray<TObjectPtr<TActor>>& Pool)
	{
		for (int32 Index = Pool.Num() - 1; Index >= 0; --Index)
		{
			if (!IsValid(Pool[Index]))
			{
				Pool.RemoveAtSwap(Index);
			}
		}
	}

	template <typename TActor>
	TActor* AcquirePooledActor(
		UWorld* World,
		TArray<TObjectPtr<TActor>>& Pool,
		FRoguePoolRuntime& Runtime,
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

		// 从上次命中位置继续环形查找可用且精确同类的对象。
		// 游标可避免反复检查相同前缀，但最坏时间复杂度仍为 O(N)。
		const int32 Num = Pool.Num();
		if (Num > 0)
		{
			if (Runtime.Cursor < 0 || Runtime.Cursor >= Num)
			{
				Runtime.Cursor = 0;
			}

			int32 Index = Runtime.Cursor;
			for (int32 Scanned = 0; Scanned < Num; ++Scanned)
			{
				TActor* Actor = Pool[Index];
				if (IsValid(Actor) && Actor->IsAvailableInPool() && Actor->GetClass() == ActorClass)
				{
					Runtime.Cursor = (Index + 1) % Num;
					Actor->SetOwner(OwnerActor);
					Actor->SetInstigator(InstigatorPawn);
					Actor->SetActorLocationAndRotation(SpawnLocation, SpawnRotation, false, nullptr, ETeleportType::TeleportPhysics);
					return Actor;
				}

				Index = (Index + 1) % Num;
			}
		}

		// ── 未命中：先清理失效对象，再走运行时新建兜底 ──
		CompactPool(Pool);
		Runtime.Cursor = 0;

		// 记录池未命中后的生成尝试，首次及每 32 次输出诊断。
		// 未命中可能来自容量、类不匹配或失效对象；生成失败也会计入。
		++Runtime.RuntimeSpawnCount;
		if (Runtime.RuntimeSpawnCount == 1 || Runtime.RuntimeSpawnCount % 32 == 0)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[Pool] 池内没有可用的 %s，运行时新建第 %d 个（当前池大小 %d，峰值 %d）—— 建议调高该池的预热量"),
				*ActorClass->GetName(), Runtime.RuntimeSpawnCount, Pool.Num(), Runtime.PeakSize);
		}

		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Owner = OwnerActor;
		SpawnParameters.Instigator = InstigatorPawn;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		TActor* Actor = World->SpawnActor<TActor>(ActorClass, SpawnLocation, SpawnRotation, SpawnParameters);
		if (Actor != nullptr)
		{
			Pool.Add(Actor);
			Runtime.PeakSize = FMath::Max(Runtime.PeakSize, Pool.Num());
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

	// 输出各池预热后的总大小；该汇总不列出每种类的数量。
	LogPoolDiagnostics();
}

ARogueEnemy* FRogueCombatPools::AcquireEnemy(UWorld* World, TSubclassOf<ARogueEnemy> EnemyClass, AActor* OwnerActor, const FVector& SpawnLocation, const FRotator& SpawnRotation)
{
	UClass* EnemyClassToSpawn = EnemyClass ? EnemyClass.Get() : ARogueEnemy::StaticClass();
	return AcquirePooledActor(World, EnemyPool, EnemyPoolRuntime, EnemyClassToSpawn, OwnerActor, nullptr, SpawnLocation, SpawnRotation);
}

ARogueProjectile* FRogueCombatPools::AcquirePlayerProjectile(UWorld* World, TSubclassOf<ARogueProjectile> ProjectileClass, AActor* OwnerActor, APawn* InstigatorPawn, const FVector& SpawnLocation, const FRotator& SpawnRotation)
{
	UClass* ProjectileClassToSpawn = ProjectileClass ? ProjectileClass.Get() : ARogueProjectile::StaticClass();
	return AcquirePooledActor(World, PlayerProjectilePool, PlayerProjectilePoolRuntime, ProjectileClassToSpawn, OwnerActor, InstigatorPawn, SpawnLocation, SpawnRotation);
}

ARogueEnemyProjectile* FRogueCombatPools::AcquireEnemyProjectile(UWorld* World, TSubclassOf<ARogueEnemyProjectile> ProjectileClass, AActor* OwnerActor, APawn* InstigatorPawn, const FVector& SpawnLocation, const FRotator& SpawnRotation)
{
	UClass* ProjectileClassToSpawn = ProjectileClass ? ProjectileClass.Get() : ARogueEnemyProjectile::StaticClass();
	return AcquirePooledActor(World, EnemyProjectilePool, EnemyProjectilePoolRuntime, ProjectileClassToSpawn, OwnerActor, InstigatorPawn, SpawnLocation, SpawnRotation);
}

ARogueRocketProjectile* FRogueCombatPools::AcquireRocketProjectile(UWorld* World, TSubclassOf<ARogueRocketProjectile> ProjectileClass, AActor* OwnerActor, APawn* InstigatorPawn, const FVector& SpawnLocation, const FRotator& SpawnRotation)
{
	UClass* ProjectileClassToSpawn = ProjectileClass ? ProjectileClass.Get() : ARogueRocketProjectile::StaticClass();
	return AcquirePooledActor(World, RocketProjectilePool, RocketProjectilePoolRuntime, ProjectileClassToSpawn, OwnerActor, InstigatorPawn, SpawnLocation, SpawnRotation);
}

ARogueExperiencePickup* FRogueCombatPools::AcquireExperiencePickup(UWorld* World, TSubclassOf<ARogueExperiencePickup> PickupClass, AActor* OwnerActor, const FVector& SpawnLocation)
{
	UClass* PickupClassToSpawn = PickupClass ? PickupClass.Get() : ARogueExperiencePickup::StaticClass();
	return AcquirePooledActor(World, ExperiencePickupPool, ExperiencePickupPoolRuntime, PickupClassToSpawn, OwnerActor, nullptr, SpawnLocation, FRotator::ZeroRotator);
}

ARogueImpactEffect* FRogueCombatPools::AcquireImpactEffect(UWorld* World, AActor* OwnerActor, const FVector& SpawnLocation, const FRotator& SpawnRotation)
{
	return AcquirePooledActor(World, ImpactEffectPool, ImpactEffectPoolRuntime, ARogueImpactEffect::StaticClass(), OwnerActor, nullptr, SpawnLocation, SpawnRotation);
}

ARogueLaserBeam* FRogueCombatPools::AcquireLaserBeam(UWorld* World, TSubclassOf<ARogueLaserBeam> BeamClass, AActor* OwnerActor, const FVector& SpawnLocation, const FRotator& SpawnRotation)
{
	UClass* BeamClassToSpawn = BeamClass ? BeamClass.Get() : ARogueLaserBeam::StaticClass();
	return AcquirePooledActor(World, LaserBeamPool, LaserBeamPoolRuntime, BeamClassToSpawn, OwnerActor, nullptr, SpawnLocation, SpawnRotation);
}

ARogueOrbitingBlade* FRogueCombatPools::AcquireOrbitingBlade(UWorld* World, TSubclassOf<ARogueOrbitingBlade> BladeClass, AActor* OwnerActor, const FVector& SpawnLocation, const FRotator& SpawnRotation)
{
	UClass* BladeClassToSpawn = BladeClass ? BladeClass.Get() : ARogueOrbitingBlade::StaticClass();
	return AcquirePooledActor(World, OrbitingBladePool, OrbitingBladePoolRuntime, BladeClassToSpawn, OwnerActor, nullptr, SpawnLocation, SpawnRotation);
}

void FRogueCombatPools::LogPoolDiagnostics()
{
	// 把当前大小并入峰值后再打印，这样"预热了多少"和"后来涨到多少"能一起看到
	auto Report = [](const TCHAR* Name, int32 Size, FRoguePoolRuntime& Runtime)
	{
		Runtime.PeakSize = FMath::Max(Runtime.PeakSize, Size);
		UE_LOG(LogTemp, Log, TEXT("[Pool] %s：大小 %d，峰值 %d，运行时新建 %d%s"),
			Name, Size, Runtime.PeakSize, Runtime.RuntimeSpawnCount,
			Runtime.RuntimeSpawnCount > 0 ? TEXT("  ← 该调高预热量") : TEXT(""));
	};

	UE_LOG(LogTemp, Log, TEXT("===== 对象池诊断 ====="));
	Report(TEXT("敌人"),         EnemyPool.Num(),              EnemyPoolRuntime);
	Report(TEXT("玩家子弹"),     PlayerProjectilePool.Num(),   PlayerProjectilePoolRuntime);
	Report(TEXT("敌方弹体"),     EnemyProjectilePool.Num(),    EnemyProjectilePoolRuntime);
	Report(TEXT("火箭/迫击炮"),  RocketProjectilePool.Num(),   RocketProjectilePoolRuntime);
	Report(TEXT("经验球"),       ExperiencePickupPool.Num(),   ExperiencePickupPoolRuntime);
	Report(TEXT("命中特效"),     ImpactEffectPool.Num(),       ImpactEffectPoolRuntime);
	Report(TEXT("激光 Beam"),    LaserBeamPool.Num(),          LaserBeamPoolRuntime);
	Report(TEXT("环绕刀片"),     OrbitingBladePool.Num(),      OrbitingBladePoolRuntime);
	UE_LOG(LogTemp, Log, TEXT("======================"));
}
