#include "Combat/Projectiles/RogueProjectile.h"
#include "Combat/RogueCombatCollision.h"

#include "Combat/Visuals/RogueImpactEffect.h"
#include "Enemies/RogueEnemy.h"
#include "AbilitySystem/RogueAbilitySystemLibrary.h"
#include "AbilitySystem/RogueGameplayTags.h"
#include "Components/SphereComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"

ARogueProjectile::ARogueProjectile()
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
	Collision->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Ignore);
	RootComponent = Collision;

	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->SetUpdatedComponent(Collision);
	ProjectileMovement->InitialSpeed = Speed;
	ProjectileMovement->MaxSpeed = Speed;
	ProjectileMovement->ProjectileGravityScale = 0.0f;
	ProjectileMovement->bRotationFollowsVelocity = false;
	ProjectileMovement->bShouldBounce = false;
	ProjectileMovement->bInitialVelocityInLocalSpace = false;
	ProjectileMovement->bSweepCollision = true;

	Collision->OnComponentBeginOverlap.AddDynamic(this, &ARogueProjectile::HandleOverlap);
	ProjectileMovement->OnProjectileStop.AddDynamic(this, &ARogueProjectile::HandleProjectileStop);
}

void ARogueProjectile::BeginPlay()
{
	Super::BeginPlay();

	// 只允许命中球参与查询；蓝图追加的网格/特效组件不参与物理和碰撞。
	RogueCombatCollision::DisableNonHitCollision(this, Collision);
	DeactivateToPool();
}

void ARogueProjectile::ActivatePooledProjectile(AActor* InOwner, APawn* InInstigator, const FVector& SpawnLocation, const FRotator& SpawnRotation, const FVector& InDirection, float InSpeed, float InDamage)
{
	TravelDirection = InDirection.GetSafeNormal();
	Speed = InSpeed;
	Damage = InDamage;
	SetOwner(InOwner);
	SetInstigator(InInstigator);
	SetActorLocationAndRotation(SpawnLocation, SpawnRotation, false, nullptr, ETeleportType::TeleportPhysics);
	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);
	SetActorTickEnabled(true);
	bPoolAvailable = false;
	bResolvedImpact = false;
	RemainingLifetime = PooledLifetime;
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Collision->SetGenerateOverlapEvents(true);
	Collision->ClearMoveIgnoreActors();

	if (AActor* OwnerActor = GetOwner())
	{
		Collision->IgnoreActorWhenMoving(OwnerActor, true);
	}

	if (APawn* InstigatorPawn = GetInstigator())
	{
		Collision->IgnoreActorWhenMoving(InstigatorPawn, true);
	}

	if (ProjectileMovement != nullptr)
	{
		ProjectileMovement->OnProjectileStop.RemoveDynamic(this, &ARogueProjectile::HandleProjectileStop);
		ProjectileMovement->OnProjectileStop.AddDynamic(this, &ARogueProjectile::HandleProjectileStop);
		ProjectileMovement->SetUpdatedComponent(Collision);
		ProjectileMovement->SetComponentTickEnabled(true);
		ProjectileMovement->StopMovementImmediately();
		ProjectileMovement->InitialSpeed = Speed;
		ProjectileMovement->MaxSpeed = Speed;
		ProjectileMovement->Velocity = TravelDirection * Speed;
		ProjectileMovement->UpdateComponentVelocity();
		ProjectileMovement->Activate(true);
	}
}

void ARogueProjectile::DeactivateToPool()
{
	bPoolAvailable = true;
	bResolvedImpact = true;

	if (ProjectileMovement != nullptr)
	{
		ProjectileMovement->OnProjectileStop.RemoveDynamic(this, &ARogueProjectile::HandleProjectileStop);
		ProjectileMovement->SetUpdatedComponent(Collision);
		ProjectileMovement->StopMovementImmediately();
		ProjectileMovement->Deactivate();
		ProjectileMovement->Velocity = FVector::ZeroVector;
	}

	bResolvedImpact = false;
	RemainingLifetime = 0.0f;
	SetActorTickEnabled(false);
	SetActorEnableCollision(false);
	SetActorHiddenInGame(true);
	SetOwner(nullptr);
	SetInstigator(nullptr);
	Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Collision->SetGenerateOverlapEvents(false);
	Collision->ClearMoveIgnoreActors();
}

void ARogueProjectile::Tick(float DeltaSeconds)
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

	// 模型缩放由蓝图组件决定；飞行、低负载/高负载切换与对象池复用均不重写它。
}

void ARogueProjectile::HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (bPoolAvailable || bResolvedImpact || IsActorBeingDestroyed() || !IsValid(OtherActor) || OtherActor == GetOwner())
	{
		return;
	}

	ARogueEnemy* Enemy = Cast<ARogueEnemy>(OtherActor);
	if (!IsValid(Enemy))
	{
		return;
	}

	bResolvedImpact = true;
	UWorld* World = GetWorld();
	AActor* DamageCauser = IsValid(GetOwner()) ? GetOwner() : this;
	FVector ImpactLocation = Enemy->GetActorLocation() + FVector(0.0f, 0.0f, 40.0f);
	if (!SweepResult.ImpactPoint.IsNearlyZero())
	{
		ImpactLocation = FVector(SweepResult.ImpactPoint);
	}
	if (World != nullptr)
	{
		ARogueImpactEffect::SpawnImpactEffect(World, ImpactLocation, SweepResult.ImpactNormal.Rotation(), ERogueImpactVisualStyle::Spark, VisualConfig.ImpactEffectScale, VisualConfig.ImpactEffectLifetime, this);
	}
	// 走统一的 GAS 伤害入口（内部会构建 Damage GE Spec 并施加到目标的 ASC）
	RogueAbilitySystem::ApplyDamageToTarget(DamageCauser, Enemy, Damage, RogueGameplayTags::Damage_Source_Projectile);
	DeactivateToPool();
}

void ARogueProjectile::HandleProjectileStop(const FHitResult& ImpactResult)
{
	if (bPoolAvailable || bResolvedImpact || IsActorBeingDestroyed())
	{
		return;
	}

	bResolvedImpact = true;
	UWorld* World = GetWorld();
	FVector ImpactLocation = GetActorLocation();
	if (!ImpactResult.ImpactPoint.IsNearlyZero())
	{
		ImpactLocation = FVector(ImpactResult.ImpactPoint);
	}
	if (World != nullptr)
	{
		ARogueImpactEffect::SpawnImpactEffect(World, ImpactLocation, ImpactResult.ImpactNormal.Rotation(), ERogueImpactVisualStyle::Spark, VisualConfig.StopImpactEffectScale, VisualConfig.StopImpactEffectLifetime, this);
	}
	DeactivateToPool();
}
