#include "Combat/Projectiles/RogueRocketProjectile.h"

#include "Combat/Visuals/RogueImpactEffect.h"
#include "Enemies/RogueEnemy.h"
#include "AbilitySystem/RogueAbilitySystemLibrary.h"
#include "AbilitySystem/RogueGameplayTags.h"
#include "Combat/Targeting/RogueEnemyTrackerSubsystem.h"
#include "Components/SphereComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystem.h"

ARogueRocketProjectile::ARogueRocketProjectile()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.03f;

	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	Collision->InitSphereRadius(CollisionRadius);
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Collision->SetCollisionObjectType(ECC_WorldDynamic);
	Collision->SetCollisionResponseToAllChannels(ECR_Ignore);
	Collision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Collision->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	RootComponent = Collision;











	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->SetUpdatedComponent(Collision);
	ProjectileMovement->InitialSpeed = 900.0f;
	ProjectileMovement->MaxSpeed = 900.0f;
	ProjectileMovement->ProjectileGravityScale = 0.0f;
	ProjectileMovement->bRotationFollowsVelocity = true;
	ProjectileMovement->bShouldBounce = false;
	ProjectileMovement->bSweepCollision = true;












	Collision->OnComponentBeginOverlap.AddDynamic(this, &ARogueRocketProjectile::HandleOverlap);
	ProjectileMovement->OnProjectileStop.AddDynamic(this, &ARogueRocketProjectile::HandleProjectileStop);
	
}

void ARogueRocketProjectile::BeginPlay()
{
	Super::BeginPlay();
	DeactivateToPool();
}

void ARogueRocketProjectile::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bPoolAvailable)
	{
		return;
	}

	RemainingLifetime -= DeltaSeconds;
	if (RemainingLifetime <= 0.0f)
	{
		DeactivateToPool();
		return;
	}





}

void ARogueRocketProjectile::InitializeRocket(const FVector& InDirection, float InSpeed, float InDamage, float InExplosionRadius)
{
	Damage = InDamage;
	ExplosionRadius = InExplosionRadius;

	if (ProjectileMovement != nullptr)
	{
		ProjectileMovement->SetUpdatedComponent(Collision);
		ProjectileMovement->SetComponentTickEnabled(true);
		ProjectileMovement->InitialSpeed = InSpeed;
		ProjectileMovement->MaxSpeed = InSpeed;
		ProjectileMovement->Velocity = InDirection.GetSafeNormal() * InSpeed;
		ProjectileMovement->UpdateComponentVelocity();
		ProjectileMovement->Activate(true);
	}
}

void ARogueRocketProjectile::ActivatePooledRocket(AActor* InOwner, APawn* InInstigator, const FVector& SpawnLocation, const FRotator& SpawnRotation, const FVector& InDirection, float InSpeed, float InDamage, float InExplosionRadius)
{
	SetOwner(InOwner);
	SetInstigator(InInstigator);
	SetActorLocationAndRotation(SpawnLocation, SpawnRotation, false, nullptr, ETeleportType::TeleportPhysics);
	SetActorHiddenInGame(false);
	SetActorTickEnabled(true);
	SetActorEnableCollision(true);

	bPoolAvailable = false;
	bExploded = false;
	RemainingLifetime = PooledLifetime;
	Damage = InDamage;
	ExplosionRadius = InExplosionRadius;

	if (Collision != nullptr)
	{
		Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Collision->ClearMoveIgnoreActors();
		if (InOwner != nullptr)
		{
			Collision->IgnoreActorWhenMoving(InOwner, true);
		}

		if (InInstigator != nullptr)
		{
			Collision->IgnoreActorWhenMoving(InInstigator, true);
		}
	}


	if (ProjectileMovement != nullptr)
	{
		ProjectileMovement->OnProjectileStop.RemoveDynamic(this, &ARogueRocketProjectile::HandleProjectileStop);
		ProjectileMovement->OnProjectileStop.AddDynamic(this, &ARogueRocketProjectile::HandleProjectileStop);
		ProjectileMovement->ProjectileGravityScale = 0.0f;
	}


	InitializeRocket(InDirection, InSpeed, InDamage, InExplosionRadius);
}

void ARogueRocketProjectile::DeactivateToPool()
{
	SetActorHiddenInGame(true);
	SetActorTickEnabled(false);
	SetActorEnableCollision(false);

	if (Collision != nullptr)
	{
		Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Collision->ClearMoveIgnoreActors();
	}

	bPoolAvailable = true;
	RemainingLifetime = 0.0f;

	if (ProjectileMovement != nullptr)
	{
		ProjectileMovement->OnProjectileStop.RemoveDynamic(this, &ARogueRocketProjectile::HandleProjectileStop);
		ProjectileMovement->SetUpdatedComponent(Collision);
		ProjectileMovement->StopMovementImmediately();
		ProjectileMovement->Deactivate();
		ProjectileMovement->Velocity = FVector::ZeroVector;
		ProjectileMovement->ProjectileGravityScale = 0.0f;
	}

	bExploded = false;
	SetOwner(nullptr);
	SetInstigator(nullptr);
}

void ARogueRocketProjectile::HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (bPoolAvailable)
	{
		return;
	}

	if (!IsValid(OtherActor) || OtherActor == GetOwner())
	{
		return;
	}

	Explode();
}

void ARogueRocketProjectile::HandleProjectileStop(const FHitResult& ImpactResult)
{
	if (bPoolAvailable)
	{
		return;
	}

	Explode();
}

void ARogueRocketProjectile::Explode()
{
	if (bExploded || bPoolAvailable || IsActorBeingDestroyed())
	{
		return;
	}

	bExploded = true;
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		DeactivateToPool();
		return;
	}

	const FVector ExplosionLocation = GetActorLocation();
	const FVector ExplosionScale(ExplosionRadius / 220.0f, ExplosionRadius / 220.0f, 1.0f);
	AActor* DamageCauser = IsValid(GetOwner()) ? GetOwner() : this;
	AController* InstigatorController = GetInstigatorController();

	if (Collision != nullptr)
	{
		Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	SetActorEnableCollision(false);
	if (ProjectileMovement != nullptr)
	{
		ProjectileMovement->OnProjectileStop.RemoveDynamic(this, &ARogueRocketProjectile::HandleProjectileStop);
		ProjectileMovement->StopMovementImmediately();
		ProjectileMovement->Deactivate();
	}

	ARogueImpactEffect::SpawnImpactEffect(World, ExplosionLocation, FRotator::ZeroRotator, ERogueImpactVisualStyle::Explosion, ExplosionScale, ImpactEffectLifetime, this);

	URogueEnemyTrackerSubsystem* Tracker = World->GetSubsystem<URogueEnemyTrackerSubsystem>();

	TArray<ARogueEnemy*> AffectedEnemies;
	TArray<float> PendingDamages;
	if (!IsValid(Tracker))
	{
		DeactivateToPool();
		return;
	}

	TArray<ARogueEnemy*> NearbyEnemies;
	Tracker->CollectEnemiesInRange(ExplosionLocation, ExplosionRadius, NearbyEnemies, 0, false);
	for (ARogueEnemy* Enemy : NearbyEnemies)
	{
		if (!IsValid(Enemy) || Enemy->IsDead())
		{
			continue;
		}

		const float Distance = FVector::Dist2D(ExplosionLocation, Enemy->GetActorLocation());
		if (Distance > ExplosionRadius)
		{
			continue;
		}

		const float DistanceAlpha = 1.0f - FMath::Clamp(Distance / ExplosionRadius, 0.0f, 1.0f);
		const float FinalDamage = FMath::Lerp(Damage * 0.45f, Damage, DistanceAlpha);
		AffectedEnemies.Add(Enemy);
		PendingDamages.Add(FinalDamage);
	}

	for (int32 Index = 0; Index < AffectedEnemies.Num(); ++Index)
	{
		ARogueEnemy* Enemy = AffectedEnemies[Index];
		if (!IsValid(Enemy) || Enemy->IsDead())
		{
			continue;
		}

		// 爆炸是范围伤害，Tag 用 Damage.Source.Area
		RogueAbilitySystem::ApplyDamageToTarget(DamageCauser, Enemy, PendingDamages[Index], RogueGameplayTags::Damage_Source_Area);
	}

	DeactivateToPool();
}
