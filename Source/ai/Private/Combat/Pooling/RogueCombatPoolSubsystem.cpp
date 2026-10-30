// 对象池子系统 —— 实现
#include "Combat/Pooling/RogueCombatPoolSubsystem.h"

#include "Enemies/RogueEnemy.h"
#include "Combat/Projectiles/RogueProjectile.h"
#include "Combat/Projectiles/RogueEnemyProjectile.h"
#include "Combat/Projectiles/RogueRocketProjectile.h"
#include "World/RogueExperiencePickup.h"
#include "Combat/Visuals/RogueImpactEffect.h"
#include "Combat/Visuals/RogueCombatAccentEffect.h"
#include "Combat/Targeting/RogueEnemyTrackerSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
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

ARogueCombatAccentEffect* URogueCombatPoolSubsystem::PlayCombatAccent(const FRogueCombatAccentSettings& S,const FVector& Location,const FVector& Direction,bool bImpact)
{
    UWorld* W=GetWorld();
    if(!W || W->GetNetMode()==NM_DedicatedServer || !S.bEnabled || !S.EffectClass || Location.ContainsNaN())return nullptr;
    bool HasCamera=false,NearCamera=false;
    for(auto It=W->GetPlayerControllerIterator();It;++It)
        if(auto* PC=It->Get();PC && PC->IsLocalController() && PC->PlayerCameraManager)
        { HasCamera=true;NearCamera|=FVector::DistSquared(PC->PlayerCameraManager->GetCameraLocation(),Location)<FMath::Square(5000.f); }
    if(HasCamera && !NearCamera)return nullptr;
    AccentPool.RemoveAll([](const auto& A){return !IsValid(A);});
    const auto* Tracker=W->GetSubsystem<URogueEnemyTrackerSubsystem>();
    const int32 Limit=Tracker && Tracker->ShouldCullCombatEffects()?16:48;
    int32 Active=0;ARogueCombatAccentEffect* Available=nullptr;
    for(ARogueCombatAccentEffect* A:AccentPool)
    {
        if(!A->IsAvailableInPool())++Active;
        else if(A->GetClass()==S.EffectClass)Available=A;
    }
    if(Active>=Limit)return nullptr;
    if(!Available)
    {
        if(AccentPool.Num()>=64)return nullptr;
        FActorSpawnParameters Params;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        Available=W->SpawnActor<ARogueCombatAccentEffect>(S.EffectClass,Location,FRotator::ZeroRotator,Params);
        if(!Available)return nullptr;AccentPool.Add(Available);
    }
    Available->Activate(S,Location,Direction,bImpact);return Available;
}
