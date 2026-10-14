// 对象池子系统 —— 实现
#include "Combat/Pooling/RogueCombatPoolSubsystem.h"

#include "Enemies/RogueEnemy.h"
#include "Combat/Projectiles/RogueProjectile.h"
#include "Combat/Projectiles/RogueEnemyProjectile.h"
#include "Combat/Projectiles/RogueRocketProjectile.h"
#include "World/RogueExperiencePickup.h"
#include "Combat/Visuals/RogueImpactEffect.h"
#include "Combat/Visuals/RogueLaserBeam.h"
#include "Combat/Projectiles/RogueOrbitingBlade.h"

void URogueCombatPoolSubsystem::Prewarm(
	AActor* OwnerActor,
	const FRoguePoolSettings& Settings,
	TSubclassOf<ARogueEnemy> EnemyClass,
	TSubclassOf<ARogueExperiencePickup> ExperiencePickupClass,
	const FRogueCombatPoolPrewarmClasses& AdditionalClasses)
{
	CombatPools.Prewarm(GetWorld(), OwnerActor, Settings, EnemyClass, ExperiencePickupClass, AdditionalClasses);
}

ARogueEnemy* URogueCombatPoolSubsystem::AcquireEnemy(TSubclassOf<ARogueEnemy> EnemyClass, AActor* OwnerActor, const FVector& SpawnLocation, const FRotator& SpawnRotation)
{
	return CombatPools.AcquireEnemy(GetWorld(), EnemyClass, OwnerActor, SpawnLocation, SpawnRotation);
}

ARogueProjectile* URogueCombatPoolSubsystem::AcquirePlayerProjectile(TSubclassOf<ARogueProjectile> ProjectileClass, AActor* OwnerActor, APawn* InstigatorPawn, const FVector& SpawnLocation, const FRotator& SpawnRotation)
{
	return CombatPools.AcquirePlayerProjectile(GetWorld(), ProjectileClass, OwnerActor, InstigatorPawn, SpawnLocation, SpawnRotation);
}

ARogueEnemyProjectile* URogueCombatPoolSubsystem::AcquireEnemyProjectile(TSubclassOf<ARogueEnemyProjectile> ProjectileClass, AActor* OwnerActor, APawn* InstigatorPawn, const FVector& SpawnLocation, const FRotator& SpawnRotation)
{
	return CombatPools.AcquireEnemyProjectile(GetWorld(), ProjectileClass, OwnerActor, InstigatorPawn, SpawnLocation, SpawnRotation);
}

ARogueRocketProjectile* URogueCombatPoolSubsystem::AcquireRocketProjectile(TSubclassOf<ARogueRocketProjectile> ProjectileClass, AActor* OwnerActor, APawn* InstigatorPawn, const FVector& SpawnLocation, const FRotator& SpawnRotation)
{
	return CombatPools.AcquireRocketProjectile(GetWorld(), ProjectileClass, OwnerActor, InstigatorPawn, SpawnLocation, SpawnRotation);
}

ARogueExperiencePickup* URogueCombatPoolSubsystem::AcquireExperiencePickup(TSubclassOf<ARogueExperiencePickup> PickupClass, AActor* OwnerActor, const FVector& SpawnLocation)
{
	return CombatPools.AcquireExperiencePickup(GetWorld(), PickupClass, OwnerActor, SpawnLocation);
}

ARogueImpactEffect* URogueCombatPoolSubsystem::AcquireImpactEffect(const FVector& SpawnLocation, const FRotator& SpawnRotation, AActor* OwnerActor)
{
	return CombatPools.AcquireImpactEffect(GetWorld(), OwnerActor, SpawnLocation, SpawnRotation);
}

ARogueLaserBeam* URogueCombatPoolSubsystem::AcquireLaserBeam(TSubclassOf<ARogueLaserBeam> BeamClass, AActor* OwnerActor, const FVector& SpawnLocation, const FRotator& SpawnRotation)
{
	return CombatPools.AcquireLaserBeam(GetWorld(), BeamClass, OwnerActor, SpawnLocation, SpawnRotation);
}

ARogueOrbitingBlade* URogueCombatPoolSubsystem::AcquireOrbitingBlade(TSubclassOf<ARogueOrbitingBlade> BladeClass, AActor* OwnerActor, const FVector& SpawnLocation, const FRotator& SpawnRotation)
{
	return CombatPools.AcquireOrbitingBlade(GetWorld(), BladeClass, OwnerActor, SpawnLocation, SpawnRotation);
}

void URogueCombatPoolSubsystem::LogPoolDiagnostics()
{
	CombatPools.LogPoolDiagnostics();
}
