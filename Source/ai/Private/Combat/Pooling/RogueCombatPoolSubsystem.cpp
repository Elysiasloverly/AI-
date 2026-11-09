// 对象池子系统 —— 实现
#include "Combat/Pooling/RogueCombatPoolSubsystem.h"
#include "Combat/Projectiles/RogueIceFlameField.h"

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
    const bool bCrowded = Tracker && Tracker->ShouldCullCombatEffects();
    const int32 Limit = bCrowded ? 16 : 48;
    int32 Active=0;ARogueCombatAccentEffect* Available=nullptr;ARogueCombatAccentEffect* OtherAvailable=nullptr;
    for(ARogueCombatAccentEffect* A:AccentPool)
    {
        if(!A->IsAvailableInPool())++Active;
        else if(A->GetClass()==S.EffectClass)Available=A;
        else OtherAvailable=A;
    }
    if(Active>=Limit)
    {
        // Reserve a visible confirmation for a heavy blast without raising the
        // budget. Recycle one minor effect; never evict another active blast.
        if(S.EffectClass->GetDefaultObject<ARogueCombatAccentEffect>()->MotionProfile!=ERogueAccentMotionProfile::Burst)return nullptr;
        ARogueCombatAccentEffect* Victim=nullptr;
        for(ARogueCombatAccentEffect* A:AccentPool)
            if(!A->IsAvailableInPool()&&A->MotionProfile!=ERogueAccentMotionProfile::Burst)
            {Victim=A;if(A->MotionProfile==ERogueAccentMotionProfile::Cast)break;}
        if(!Victim)return nullptr;
        Victim->Deactivate();OtherAvailable=Victim;
    }
    if(!Available)
    {
        // Different cosmetic classes share this bounded pool. Retire one idle
        // mismatched entry instead of permanently starving a new effect style.
        if(AccentPool.Num()>=64&&OtherAvailable)
        {AccentPool.RemoveSingle(OtherAvailable);OtherAvailable->Destroy();}
        if(AccentPool.Num()>=64)return nullptr;
        FActorSpawnParameters Params;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        Available=W->SpawnActor<ARogueCombatAccentEffect>(S.EffectClass,Location,FRotator::ZeroRotator,Params);
        if(!Available)return nullptr;AccentPool.Add(Available);
    }
    if (bCrowded && Available->MotionProfile != ERogueAccentMotionProfile::Burst)
    {
        // Small sparks can overlap across a crowd. The projectile and danger visuals use separate paths.
        FRogueCombatAccentSettings Compact = S;
        Compact.Scale *= bImpact ? .67f : .82f;
        Compact.Duration *= bImpact ? .72f : .84f;
        Compact.ShardCount = FMath::Min(Compact.ShardCount, bImpact ? 2 : 3);
        if (bImpact) Compact.bShowRing = false;
        Available->Activate(Compact, Location, Direction, bImpact);
    }
    else
    {
        Available->Activate(S, Location, Direction, bImpact);
    }
    return Available;
}

ARogueIceFlameField* URogueCombatPoolSubsystem::SpawnIceFlameField(TSubclassOf<ARogueIceFlameField> Class,AActor* Source,const FVector& Location,const FVector& Normal,float Radius,float DPS,const FRogueIceFlameCombatSettings& Settings)
{
    UWorld* W=GetWorld();if(!W || W->GetNetMode()==NM_Client || !Class || !IsValid(Source) || !Settings.bEnabled || Radius<=0)return nullptr;
    ARogueIceFlameField* Available=nullptr;
    for(ARogueIceFlameField* Field:IceFlamePool)
    {
        if(!IsValid(Field)||Field->GetClass()!=Class)continue;
        if(Field->IsAvailableInPool()){if(!Available)Available=Field;continue;}
        if(Field->GetOwner()==Source && FMath::Abs(Field->GetActorLocation().Z-Location.Z)<30 &&
            FVector::DistSquared2D(Field->GetActorLocation(),Location)<FMath::Square(FMath::Min(Radius,Field->GetRadius())*.3f))
        {Field->Activate(Source,Field->GetActorLocation(),Normal,Radius,DPS,Settings);return Field;}
    }
    if(!Available)
    {
        FActorSpawnParameters Spawn;Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        Available=W->SpawnActor<ARogueIceFlameField>(Class,Location,FRotator::ZeroRotator,Spawn);
        if(!Available)return nullptr;IceFlamePool.Add(Available);
    }
    Available->Activate(Source,Location,Normal,Radius,DPS,Settings);return Available;
}
