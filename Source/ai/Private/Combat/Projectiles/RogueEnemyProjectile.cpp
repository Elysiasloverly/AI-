#include "Combat/Projectiles/RogueEnemyProjectile.h"

#include "Player/RogueCharacter.h"
#include "AbilitySystem/RogueAbilitySystemLibrary.h"
#include "AbilitySystem/RogueGameplayTags.h"
#include "Combat/Targeting/RogueEnemyTrackerSubsystem.h"
#include "Combat/Visuals/RogueImpactEffect.h"
#include "Enemies/RogueEnemy.h"
#include "Components/PointLightComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

ARogueEnemyProjectile::ARogueEnemyProjectile()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.03f;
	NormalVisualState.BodyScale = 0.58f;
	NormalVisualState.CoreScale = 0.32f;
	NormalVisualState.GlowScale = 1.12f;
	NormalVisualState.SpineScale = 0.56f;
	NormalVisualState.RingScale = 1.28f;
	NormalVisualState.TrailScale = FVector(0.10f, 0.10f, 0.28f);
	NormalVisualState.CollisionRadius = 22.0f;
	NormalVisualState.LightIntensity = 4200.0f;
	NormalVisualState.LightAttenuationRadius = 260.0f;
	NormalVisualState.LightSourceRadius = 10.0f;
	NormalVisualState.LightSoftSourceRadius = 16.0f;
	NormalVisualState.ImpactEffectScale = FVector(0.24f, 0.24f, 0.24f);

	BossVisualState = NormalVisualState;
	BossVisualState.BodyScale = 0.80f;
	BossVisualState.CoreScale = 0.46f;
	BossVisualState.GlowScale = 1.48f;
	BossVisualState.SpineScale = 0.74f;
	BossVisualState.RingScale = 1.62f;
	BossVisualState.TrailScale = FVector(0.10f, 0.10f, 0.44f);
	BossVisualState.CollisionRadius = 28.0f;
	BossVisualState.LightAttenuationRadius = 320.0f;
	BossVisualState.LightSourceRadius = 14.0f;
	BossVisualState.LightSoftSourceRadius = 22.0f;
	BossVisualState.ImpactEffectScale = FVector(0.30f, 0.30f, 0.30f);

	SimplifiedNormalVisualState = NormalVisualState;
	SimplifiedNormalVisualState.TrailScale = FVector(0.09f, 0.09f, 0.28f);

	SimplifiedBossVisualState = BossVisualState;
	SimplifiedBossVisualState.TrailScale = FVector(0.09f, 0.09f, 0.44f);
	SimplifiedBossVisualState.LightIntensity = 5600.0f;

	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	Collision->InitSphereRadius(NormalVisualState.CollisionRadius);
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Collision->SetCollisionObjectType(ECC_WorldDynamic);
	Collision->SetCollisionResponseToAllChannels(ECR_Ignore);
	Collision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Collision->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	RootComponent = Collision;

	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	BodyMesh->SetupAttachment(RootComponent);
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BodyMesh->SetCastShadow(false);
	BodyMesh->SetRelativeScale3D(FVector(NormalVisualState.BodyScale));

	CoreMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CoreMesh"));
	CoreMesh->SetupAttachment(RootComponent);
	CoreMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CoreMesh->SetCastShadow(false);
	CoreMesh->SetTranslucentSortPriority(4);
	CoreMesh->SetRelativeScale3D(FVector(NormalVisualState.CoreScale));

	GlowMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GlowMesh"));
	GlowMesh->SetupAttachment(RootComponent);
	GlowMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GlowMesh->SetCastShadow(false);
	GlowMesh->SetTranslucentSortPriority(2);
	GlowMesh->SetRelativeScale3D(FVector(NormalVisualState.GlowScale));

	SpineMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SpineMesh"));
	SpineMesh->SetupAttachment(RootComponent);
	SpineMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SpineMesh->SetCastShadow(false);
	SpineMesh->SetTranslucentSortPriority(1);
	SpineMesh->SetRelativeScale3D(FVector(NormalVisualState.SpineScale));

	RingMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RingMesh"));
	RingMesh->SetupAttachment(RootComponent);
	RingMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RingMesh->SetCastShadow(false);
	RingMesh->SetTranslucentSortPriority(1);
	RingMesh->SetRelativeScale3D(FVector(NormalVisualState.RingScale));

	TrailMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TrailMesh"));
	TrailMesh->SetupAttachment(RootComponent);
	TrailMesh->SetRelativeLocation(FVector(-26.0f, 0.0f, 0.0f));
	TrailMesh->SetRelativeRotation(FRotator(0.0f, 0.0f, 90.0f));
	TrailMesh->SetTranslucentSortPriority(0);
	TrailMesh->SetRelativeScale3D(NormalVisualState.TrailScale);
	TrailMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TrailMesh->SetCastShadow(false);

	GlowLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("GlowLight"));
	GlowLight->SetupAttachment(RootComponent);
	GlowLight->SetIntensity(NormalVisualState.LightIntensity);
	GlowLight->SetLightColor(FLinearColor(1.0f, 0.10f, 0.08f));
	GlowLight->SetAttenuationRadius(NormalVisualState.LightAttenuationRadius);
	GlowLight->SetSourceRadius(NormalVisualState.LightSourceRadius);
	GlowLight->SetSoftSourceRadius(NormalVisualState.LightSoftSourceRadius);
	GlowLight->SetUseInverseSquaredFalloff(false);
	GlowLight->SetCastShadows(false);

	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->SetUpdatedComponent(Collision);
	ProjectileMovement->InitialSpeed = 920.0f;
	ProjectileMovement->MaxSpeed = 920.0f;
	ProjectileMovement->ProjectileGravityScale = 0.0f;
	ProjectileMovement->bRotationFollowsVelocity = false;
	ProjectileMovement->bShouldBounce = false;
	ProjectileMovement->bInitialVelocityInLocalSpace = false;
	ProjectileMovement->bSweepCollision = true;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Game/GameAssets/StarterContent/Shapes/Shape_Sphere.Shape_Sphere"));
	if (SphereMesh.Succeeded())
	{
		BodyMesh->SetStaticMesh(SphereMesh.Object);
		CoreMesh->SetStaticMesh(SphereMesh.Object);
		GlowMesh->SetStaticMesh(SphereMesh.Object);
		SpineMesh->SetStaticMesh(SphereMesh.Object);
		RingMesh->SetStaticMesh(SphereMesh.Object);
	}

	static ConstructorHelpers::FObjectFinder<UStaticMesh> TrailStaticMesh(TEXT("/Game/GameAssets/StarterContent/Shapes/Shape_Cylinder.Shape_Cylinder"));
	if (TrailStaticMesh.Succeeded())
	{
		TrailMesh->SetStaticMesh(TrailStaticMesh.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BodyMaterial(TEXT("/Game/GameAssets/StarterContent/Materials/M_Metal_Copper.M_Metal_Copper"));
	if (BodyMaterial.Succeeded())
	{
		BodyMesh->SetMaterial(0, BodyMaterial.Object);
	}
	else
	{
		static ConstructorHelpers::FObjectFinder<UMaterialInterface> BodyFallbackMaterial(TEXT("/Game/GameAssets/StarterContent/Materials/M_Metal_Rust.M_Metal_Rust"));
		if (BodyFallbackMaterial.Succeeded())
		{
			BodyMesh->SetMaterial(0, BodyFallbackMaterial.Object);
		}
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> CoreMaterial(TEXT("/Game/GameAssets/Realistic_Starter_VFX_Pack_Vol2/Materials/M_Glow_D.M_Glow_D"));
	if (CoreMaterial.Succeeded())
	{
		CoreMesh->SetMaterial(0, CoreMaterial.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> GlowMaterial(TEXT("/Game/GameAssets/Realistic_Starter_VFX_Pack_Vol2/Materials/M_Glow_D.M_Glow_D"));
	if (GlowMaterial.Succeeded())
	{
		GlowMesh->SetMaterial(0, GlowMaterial.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> RingMaterial(TEXT("/Game/GameAssets/Realistic_Starter_VFX_Pack_Vol2/Materials/M_Glow_D.M_Glow_D"));
	if (RingMaterial.Succeeded())
	{
		RingMesh->SetMaterial(0, RingMaterial.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> SpineMaterial(TEXT("/Game/GameAssets/Realistic_Starter_VFX_Pack_Vol2/Materials/M_Glow_D.M_Glow_D"));
	if (SpineMaterial.Succeeded())
	{
		SpineMesh->SetMaterial(0, SpineMaterial.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> TrailMaterial(TEXT("/Game/GameAssets/Realistic_Starter_VFX_Pack_Vol2/Materials/M_Glow_D.M_Glow_D"));
	if (TrailMaterial.Succeeded())
	{
		TrailMesh->SetMaterial(0, TrailMaterial.Object);
	}

	Collision->OnComponentBeginOverlap.AddDynamic(this, &ARogueEnemyProjectile::HandleOverlap);
	ProjectileMovement->OnProjectileStop.AddDynamic(this, &ARogueEnemyProjectile::HandleProjectileStop);
}

void ARogueEnemyProjectile::BeginPlay()
{
	Super::BeginPlay();
	UnscaledTrailLocation = TrailMesh->GetRelativeLocation();
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
	PulseTime = 0.0f;
	RemainingLifetime = InLifetime > 0.0f ? InLifetime : PooledLifetime;
	bResolvedImpact = false;
	bPoolAvailable = false;
	bVisualsSimplified = false;
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

	const FRogueEnemyProjectileVisualState& ActiveState = bBossShot ? BossVisualState : NormalVisualState;
	Collision->SetSphereRadius(ActiveState.CollisionRadius * VisualScale, false);
	ApplyVisualState(ActiveState, 0.0f);
	SetActorHiddenInGame(false);
	SetActorTickEnabled(true);
	Collision->SetGenerateOverlapEvents(true);
	SetActorEnableCollision(true);
	// 此调用可能立即命中并归池；之后不再写入激活状态、运动或外观。
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
}

void ARogueEnemyProjectile::DeactivateToPool()
{
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
	PulseTime = 0.0f;
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
	Collision->SetSphereRadius(NormalVisualState.CollisionRadius, false);
	ApplyVisualState(NormalVisualState, 0.0f);
	if (GlowLight != nullptr)
	{
		GlowLight->SetVisibility(false);
		GlowLight->SetHiddenInGame(true);
		GlowLight->SetIntensity(0.0f);
	}
}

void ARogueEnemyProjectile::ApplyVisualState(const FRogueEnemyProjectileVisualState& State, float PulseAlpha)
{
	// 保留蓝图组件缩放；普通/Boss/简化表现切换更新灯光、拖尾偏移与可见性。
	TrailMesh->SetRelativeLocation(UnscaledTrailLocation * VisualScale);
	SpineMesh->SetVisibility(!bSimpleVisual);
	SpineMesh->SetHiddenInGame(bSimpleVisual);
	RingMesh->SetVisibility(!bSimpleVisual);
	RingMesh->SetHiddenInGame(bSimpleVisual);
	TrailMesh->SetVisibility(!bSimpleVisual);
	TrailMesh->SetHiddenInGame(bSimpleVisual);
	if (GlowLight != nullptr)
	{
		GlowLight->SetVisibility(!bSimpleVisual);
		GlowLight->SetHiddenInGame(bSimpleVisual);
		GlowLight->SetIntensity(bSimpleVisual ? 0.0f : (State.LightIntensity
			+ PulseAlpha * (bBossShot ? LightPulseAmplitudeBoss : LightPulseAmplitudeNormal)) * VisualScale);
		GlowLight->SetAttenuationRadius(State.LightAttenuationRadius * VisualScale);
		GlowLight->SetSourceRadius(State.LightSourceRadius * VisualScale);
		GlowLight->SetSoftSourceRadius(State.LightSoftSourceRadius * VisualScale);
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
	if (bCullCombatEffects)
	{
		if (!bVisualsSimplified)
		{
			bVisualsSimplified = true;
			const FRogueEnemyProjectileVisualState& SimplifiedState = bBossShot ? SimplifiedBossVisualState : SimplifiedNormalVisualState;
			ApplyVisualState(SimplifiedState, 0.0f);
		}
		return;
	}

	bVisualsSimplified = false;

	PulseTime += DeltaSeconds;
	const FRogueEnemyProjectileVisualState& ActiveState = bBossShot ? BossVisualState : NormalVisualState;
	const float PulseAlpha = 0.5f + 0.5f * FMath::Sin(PulseTime * (bBossShot ? PulseSpeedBoss : PulseSpeedNormal));

	ApplyVisualState(ActiveState, PulseAlpha);
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
		const FVector ImpactScale = (bBossShot ? BossVisualState.ImpactEffectScale : NormalVisualState.ImpactEffectScale) * VisualScale;
		ARogueImpactEffect::SpawnImpactEffect(World, ImpactLocation, ImpactNormal.Rotation(), ERogueImpactVisualStyle::Spark, ImpactScale, 0.16f, this);
	}

	if (PlayerCharacter != nullptr && HasAuthority() && GetNetMode() != NM_Client)
	{
		// 敌方弹体：公共伤害入口再次检查玩家存活状态。
		RogueAbilitySystem::ApplyDamageToTarget(DamageCauser, PlayerCharacter, Damage, RogueGameplayTags::Damage_Source_Projectile);
	}

	DeactivateToPool();
}
