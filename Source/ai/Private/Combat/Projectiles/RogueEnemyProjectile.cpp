#include "Combat/Projectiles/RogueEnemyProjectile.h"
#include "Combat/RogueCombatCollision.h"

#include "Player/RogueCharacter.h"
#include "AbilitySystem/RogueAbilitySystemLibrary.h"
#include "AbilitySystem/RogueGameplayTags.h"
#include "Combat/Targeting/RogueEnemyTrackerSubsystem.h"
#include "Combat/Visuals/RogueImpactEffect.h"
#include "Enemies/RogueEnemy.h"
#include "Components/SphereComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"

ARogueEnemyProjectile::ARogueEnemyProjectile()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.03f;

	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	Collision->InitSphereRadius(22.0f);
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Collision->SetCollisionObjectType(ECC_WorldDynamic);
	Collision->SetCollisionResponseToAllChannels(ECR_Ignore);
	Collision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Collision->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	RootComponent = Collision;

	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->SetUpdatedComponent(Collision);
	ProjectileMovement->InitialSpeed = 920.0f;
	ProjectileMovement->MaxSpeed = 920.0f;
	ProjectileMovement->ProjectileGravityScale = 0.0f;
	ProjectileMovement->bRotationFollowsVelocity = false;
	ProjectileMovement->bShouldBounce = false;
	ProjectileMovement->bInitialVelocityInLocalSpace = false;
	ProjectileMovement->bSweepCollision = true;

	Collision->OnComponentBeginOverlap.AddDynamic(this, &ARogueEnemyProjectile::HandleOverlap);
	ProjectileMovement->OnProjectileStop.AddDynamic(this, &ARogueEnemyProjectile::HandleProjectileStop);
}

void ARogueEnemyProjectile::BeginPlay()
{
	Super::BeginPlay();

	// 外观组件完全由蓝图决定；这里只保证除命中球外的组件不参与物理/阻挡。
	RogueCombatCollision::DisableNonHitCollision(this, Collision);

	// 蓝图可以在组件面板里改碰撞球半径；这里以蓝图配置为准记录基础值。
	BaseCollisionRadius = Collision != nullptr ? Collision->GetUnscaledSphereRadius() : 22.0f;
	DeactivateToPool();
}

void ARogueEnemyProjectile::ActivatePooledProjectile(AActor* InOwner, APawn* InInstigator, const FVector& SpawnLocation, const FRotator& SpawnRotation, const FVector& InDirection, float InSpeed, float InDamage, bool bInBossShot, float InLifetime, float InVisualScale, bool bInSimpleVisual)
{
	// 初始化期间保持无碰撞；最后启用球体查询时才允许触发即时重叠。
	SetActorEnableCollision(false);
	Collision->SetGenerateOverlapEvents(false);
	Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	if (ProjectileMovement != nullptr)
	{
		ProjectileMovement->OnProjectileStop.RemoveDynamic(this, &ARogueEnemyProjectile::HandleProjectileStop);
		ProjectileMovement->StopMovementImmediately();
		ProjectileMovement->Deactivate();
	}

	// 客户端本地创建的表现 Actor 也可能是 Authority，不能只用 Role 决定伤害权限。
	Damage = HasAuthority() && GetNetMode() != NM_Client ? InDamage : 0.0f;
	bBossShot = bInBossShot;
	bSimpleVisual = bInSimpleVisual;
	VisualScale = FMath::Max(InVisualScale, KINDA_SMALL_NUMBER);
	RemainingLifetime = InLifetime > 0.0f ? InLifetime : PooledLifetime;
	bResolvedImpact = false;
	bPoolAvailable = false;
	bVisualsSimplified = false;
	bWasActivated = true;

	SetOwner(InOwner);
	SetInstigator(InInstigator);
	SetActorLocationAndRotation(SpawnLocation, SpawnRotation, false, nullptr, ETeleportType::TeleportPhysics);
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
		ProjectileMovement->OnProjectileStop.AddDynamic(this, &ARogueEnemyProjectile::HandleProjectileStop);
		ProjectileMovement->SetUpdatedComponent(Collision);
		ProjectileMovement->SetComponentTickEnabled(true);
		ProjectileMovement->StopMovementImmediately();
		ProjectileMovement->InitialSpeed = InSpeed;
		ProjectileMovement->MaxSpeed = InSpeed;
		ProjectileMovement->Velocity = InDirection.GetSafeNormal() * InSpeed;
		ProjectileMovement->UpdateComponentVelocity();
		ProjectileMovement->Activate(true);
	}

	Collision->SetSphereRadius(BaseCollisionRadius * VisualScale, false);
	SetActorHiddenInGame(false);
	SetActorTickEnabled(true);

	// 外观完全交给蓝图：在开启碰撞前给蓝图一次设置材质/粒子/缩放的机会。
	OnProjectileActivated(bBossShot, bSimpleVisual, VisualScale);

	Collision->SetGenerateOverlapEvents(true);
	SetActorEnableCollision(true);
	// 此调用可能立即命中并归池；之后不再写入激活状态、运动或外观。
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
}

void ARogueEnemyProjectile::DeactivateToPool()
{
	const bool bShouldNotify = bWasActivated;
	bWasActivated = false;

	bPoolAvailable = true;
	bResolvedImpact = true;
	SetActorEnableCollision(false);
	Collision->SetGenerateOverlapEvents(false);
	Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	if (ProjectileMovement != nullptr)
	{
		ProjectileMovement->OnProjectileStop.RemoveDynamic(this, &ARogueEnemyProjectile::HandleProjectileStop);
		ProjectileMovement->SetUpdatedComponent(Collision);
		ProjectileMovement->StopMovementImmediately();
		ProjectileMovement->Deactivate();
		ProjectileMovement->SetComponentTickEnabled(false);
		ProjectileMovement->Velocity = FVector::ZeroVector;
	}

	bResolvedImpact = false;
	bBossShot = false;
	bSimpleVisual = false;
	VisualScale = 1.0f;
	Damage = 0.0f;
	RemainingLifetime = 0.0f;
	bVisualsSimplified = false;
	SetActorTickEnabled(false);
	SetActorEnableCollision(false);
	SetActorHiddenInGame(true);
	SetOwner(nullptr);
	SetInstigator(nullptr);
	Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Collision->SetGenerateOverlapEvents(false);
	Collision->ClearMoveIgnoreActors();
	Collision->SetSphereRadius(BaseCollisionRadius, false);

	if (bShouldNotify)
	{
		OnProjectileDeactivated();
	}
}

void ARogueEnemyProjectile::Tick(float DeltaSeconds)
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

	URogueEnemyTrackerSubsystem* Tracker = GetWorld() ? GetWorld()->GetSubsystem<URogueEnemyTrackerSubsystem>() : nullptr;
	const bool bCullCombatEffects = Tracker != nullptr && Tracker->ShouldCullCombatEffects();
	if (bCullCombatEffects != bVisualsSimplified)
	{
		bVisualsSimplified = bCullCombatEffects;
		OnProjectileCulled(bVisualsSimplified);
	}
}

void ARogueEnemyProjectile::HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (bPoolAvailable || bResolvedImpact || IsActorBeingDestroyed() || !IsValid(OtherActor) || OtherActor == GetOwner())
	{
		return;
	}

	FVector ImpactLocation = GetActorLocation();
	if (!SweepResult.ImpactPoint.IsNearlyZero())
	{
		ImpactLocation = FVector(SweepResult.ImpactPoint);
	}

	const FVector ImpactNormal = SweepResult.ImpactNormal.IsNearlyZero() ? -GetVelocity().GetSafeNormal() : FVector(SweepResult.ImpactNormal);
	ResolveImpact(OtherActor, ImpactLocation, ImpactNormal);
}

void ARogueEnemyProjectile::HandleProjectileStop(const FHitResult& ImpactResult)
{
	if (bPoolAvailable || bResolvedImpact || IsActorBeingDestroyed())
	{
		return;
	}

	FVector ImpactLocation = GetActorLocation();
	if (!ImpactResult.ImpactPoint.IsNearlyZero())
	{
		ImpactLocation = FVector(ImpactResult.ImpactPoint);
	}

	const FVector ImpactNormal = ImpactResult.ImpactNormal.IsNearlyZero() ? -GetVelocity().GetSafeNormal() : FVector(ImpactResult.ImpactNormal);
	ResolveImpact(ImpactResult.GetActor(), ImpactLocation, ImpactNormal);
}

void ARogueEnemyProjectile::ResolveImpact(AActor* OtherActor, const FVector& ImpactLocation, const FVector& ImpactNormal)
{
	if (bPoolAvailable || bResolvedImpact || IsActorBeingDestroyed())
	{
		return;
	}

	// 密集环形弹可以穿过其它敌人，避免出生圈与友方 Pawn 重叠时立即消耗。
	if (bSimpleVisual && Cast<ARogueEnemy>(OtherActor) != nullptr)
	{
		return;
	}

	// 死亡前排队的重叠事件也要忽略幽灵，不能消耗弹体或生成命中特效。
	ARogueCharacter* PlayerCharacter = Cast<ARogueCharacter>(OtherActor);
	if (PlayerCharacter != nullptr && !PlayerCharacter->IsAlive())
	{
		return;
	}

	bResolvedImpact = true;
	UWorld* World = GetWorld();
	AActor* DamageCauser = IsValid(GetOwner()) ? GetOwner() : this;
	if (World != nullptr)
	{
		const FVector ImpactScale = ImpactEffectScale * VisualScale;
		ARogueImpactEffect::SpawnImpactEffect(World, ImpactLocation, ImpactNormal.Rotation(), ERogueImpactVisualStyle::Spark, ImpactScale, 0.16f, this);
	}

	if (PlayerCharacter != nullptr && HasAuthority() && GetNetMode() != NM_Client)
	{
		// 敌方弹体：公共伤害入口再次检查玩家存活状态。
		RogueAbilitySystem::ApplyDamageToTarget(DamageCauser, PlayerCharacter, Damage, RogueGameplayTags::Damage_Source_Projectile);
	}

	DeactivateToPool();
}
