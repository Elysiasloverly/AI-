#include "Combat/Projectiles/RogueEnemyProjectile.h"
#include "Combat/RogueCombatCollision.h"
#include "Combat/Projectiles/RogueEnemyBulletSubsystem.h"
#include "Debug/RoguePerfStats.h"
#include "HAL/IConsoleManager.h"
#include "Engine/World.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/ComponentDelegateBinding.h"

#include "Player/RogueCharacter.h"
#include "AbilitySystem/RogueAbilitySystemLibrary.h"
#include "AbilitySystem/RogueGameplayTags.h"
#include "Combat/Targeting/RogueEnemyTrackerSubsystem.h"
#include "Combat/Visuals/RogueImpactEffect.h"
#include "Enemies/RogueEnemy.h"
#include "Components/SphereComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"

namespace
{
TAutoConsoleVariable<int32> CVarBatchEnemyBullets(TEXT("Rogue.Projectiles.BatchEnemyMovement"),1,
    TEXT("Use batched simulation for compatible straight enemy bullets. 0 retains component simulation for comparison; affects newly activated bullets."));
}

ARogueEnemyProjectile::ARogueEnemyProjectile()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.03f;

	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	Collision->InitSphereRadius(22.0f);
    Collision->SetCanEverAffectNavigation(false);
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Collision->SetCollisionObjectType(RogueCombatCollision::EnemyProjectileChannel);
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
    Collision->SetCanEverAffectNavigation(false);
    // Saved child BPs can retain the old WorldDynamic object type.
    Collision->SetCollisionObjectType(RogueCombatCollision::EnemyProjectileChannel);

	// 蓝图可以在组件面板里改碰撞球半径；这里以蓝图配置为准记录基础值。
	BaseCollisionRadius = Collision != nullptr ? Collision->GetUnscaledSphereRadius() : 22.0f;
	DeactivateToPool();
}

void ARogueEnemyProjectile::ActivatePooledProjectile(AActor* InOwner, APawn* InInstigator, const FVector& SpawnLocation, const FRotator& SpawnRotation, const FVector& InDirection, float InSpeed, float InDamage, bool bInBossShot, float InLifetime, float InVisualScale, bool bInSimpleVisual, const FRogueEnemyProjectileMotion& InMotion)
{
	// 每次激活都更换批次，旧池周期的待处理条目不能再次移动新弹体。
    bBatchedMovement=false;++BatchGeneration;
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
	const float RequestedLifetime = InLifetime > 0.0f ? InLifetime : PooledLifetime;
	// 数据表与旧蓝图都可能仍保存更长的时间。服务端和客户端表现弹共用这个上限。
	constexpr float MaxLifetime = FRogueEnemyBulletRingSettings::MaxProjectileLifetime;
	RemainingLifetime = FMath::Clamp(FMath::IsFinite(RequestedLifetime) ? RequestedLifetime : MaxLifetime,
		0.1f, MaxLifetime);
	bResolvedImpact = false;
	bPoolAvailable = false;
	bVisualsSimplified = false;
	bWasActivated = true;
    PatternMotion=InMotion;
    PatternMotion.WarningSeconds=FMath::IsFinite(InMotion.WarningSeconds)?FMath::Clamp(InMotion.WarningSeconds,0.f,3.f):0.f;
    PatternMotion.ReturnAfter=FMath::IsFinite(InMotion.ReturnAfter)?FMath::Clamp(InMotion.ReturnAfter,0.f,2.f):0.f;
    PatternAge=0; bPatternLaunched=PatternMotion.WarningSeconds<=0; bPatternReturned=false;
    PatternVelocity=InDirection.GetSafeNormal()*InSpeed;
    const auto* SourceEnemy=Cast<ARogueEnemy>(InOwner);
    PatternOwnerCycle=SourceEnemy?SourceEnemy->GetSpawnCycleId():FGuid();

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
		ProjectileMovement->Velocity = bPatternLaunched ? PatternVelocity : FVector::ZeroVector;
		ProjectileMovement->UpdateComponentVelocity();
		ProjectileMovement->Activate(true);
	}

	Collision->SetSphereRadius(BaseCollisionRadius * VisualScale, false);
	SetActorHiddenInGame(false);
	SetActorTickEnabled(true);

	// 外观完全交给蓝图：在开启碰撞前给蓝图一次设置材质/粒子/缩放的机会。
	OnProjectileActivated(bBossShot, bSimpleVisual, VisualScale);
    Collision->SetCollisionObjectType(RogueCombatCollision::EnemyProjectileChannel);

    if (bPoolAvailable || IsActorBeingDestroyed()) { return; }
    if (!bPatternLaunched) { return; } // 预警期显示星阵，但不能造成接触伤害。
    if (CanBatchMovement())
    {
        if (auto* Batch=GetWorld()->GetSubsystem<URogueEnemyBulletSubsystem>())
        {
            bBatchedMovement=true;
            ProjectileMovement->SetComponentTickEnabled(false);
            SetActorTickEnabled(false);
            SetActorEnableCollision(true);
            // 世界扫掠直接返回命中；不再为每颗弹体维持物理形状和重叠集合。
            Collision->SetGenerateOverlapEvents(false);
            Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            Batch->RegisterProjectile(this);
            TArray<FHitResult> InitialHits;
            AdvanceBatchedMovement(0,InitialHits); // 出生即重叠玩家也必须立刻结算。
            return;
        }
    }
	Collision->SetGenerateOverlapEvents(true);
	SetActorEnableCollision(true);
	// 此调用可能立即命中并归池；之后不再写入激活状态、运动或外观。
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
}

void ARogueEnemyProjectile::DeactivateToPool()
{
    bBatchedMovement=false;++BatchGeneration;
	const bool bShouldNotify = bWasActivated;
	bWasActivated = false;
    PatternMotion=FRogueEnemyProjectileMotion(); PatternAge=0; PatternVelocity=FVector::ZeroVector;
    bPatternLaunched=true; bPatternReturned=false; PatternOwnerCycle.Invalidate();

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
    CSV_SCOPED_TIMING_STAT(RoguePerf, EnemyBulletState);
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

    PatternAge+=DeltaSeconds;
    if (PatternMotion.WarningSeconds>0 || PatternMotion.ReturnAfter>0)
    {
        // 生命周期ID只在权威端比较；客户端的本地ID不是服务器池周期ID。
        const auto* SourceEnemy=Cast<ARogueEnemy>(GetOwner());
        if (HasAuthority() && GetNetMode()!=NM_Client && (!SourceEnemy || SourceEnemy->IsDead() || SourceEnemy->GetSpawnCycleId()!=PatternOwnerCycle))
        { DeactivateToPool(); return; }
        if (!bPatternLaunched && PatternAge>=PatternMotion.WarningSeconds)
        {
            bPatternLaunched=true; ProjectileMovement->SetUpdatedComponent(Collision); ProjectileMovement->Activate(true); ProjectileMovement->Velocity=PatternVelocity; ProjectileMovement->UpdateComponentVelocity();
            Collision->SetGenerateOverlapEvents(true); SetActorEnableCollision(true); Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
            if (bPoolAvailable) { return; }
        }
        if (!bPatternReturned && PatternMotion.ReturnAfter>0 && PatternAge>=PatternMotion.WarningSeconds+PatternMotion.ReturnAfter)
        {
            bPatternReturned=true; ProjectileMovement->Velocity=-PatternVelocity;
            ProjectileMovement->UpdateComponentVelocity(); SetActorRotation((-PatternVelocity).Rotation());
        }
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

	// 所有敌方弹体都穿过友方敌人；表现简化标记不应决定碰撞规则。
	// 否则普通弹体在怪堆中出生或飞入怪堆时，会被友方 Pawn 提前消耗。
	if (Cast<ARogueEnemy>(OtherActor) != nullptr)
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


bool ARogueEnemyProjectile::CanBatchMovement() const
{
    // Component delegate graphs may rely on the original overlap/stop callbacks; preserve their path.
    for(const UClass* Class=GetClass();Class;Class=Class->GetSuperClass())
    {
        if(const auto* Blueprint=Cast<UBlueprintGeneratedClass>(Class))
            for(const auto& Binding:Blueprint->DynamicBindingObjects)
                if(const auto* Components=Cast<UComponentDelegateBinding>(Binding.Get());Components && !Components->ComponentDelegateBindings.IsEmpty())return false;
    }
    return CVarBatchEnemyBullets.GetValueOnGameThread()!=0 && bAllowBatchedMovement
        && PatternMotion.WarningSeconds<=0 && PatternMotion.ReturnAfter<=0
        && ProjectileMovement && ProjectileMovement->GetClass()==UProjectileMovementComponent::StaticClass()
        && FMath::IsNearlyZero(ProjectileMovement->ProjectileGravityScale)
        && GetRootComponent()==Collision && ProjectileMovement->bSimulationEnabled && ProjectileMovement->UpdatedComponent==Collision
        && !ProjectileMovement->bIsHomingProjectile && !ProjectileMovement->bShouldBounce
        && !ProjectileMovement->bConstrainToPlane && ProjectileMovement->bSweepCollision
        && !GetClass()->IsFunctionImplementedInScript(TEXT("ReceiveTick"));
}

void ARogueEnemyProjectile::AdvanceBatchedMovement(float DeltaSeconds,TArray<FHitResult>& Hits)
{
    const uint64 Generation=BatchGeneration;
    if(DeltaSeconds>0)Tick(DeltaSeconds);
    if(!bBatchedMovement || bPoolAvailable || IsActorBeingDestroyed() || BatchGeneration!=Generation)return;
    const FVector Start=GetActorLocation();
    const FVector Velocity=ProjectileMovement->Velocity;
    const FVector End=Start+Velocity*DeltaSeconds;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(RogueEnemyBulletBatch),false,this);
    FCollisionResponseParams Response;
    Collision->InitSweepCollisionParams(Params,Response);
    Params.bFindInitialOverlaps=true;
    Hits.Reset();
    {
        CSV_SCOPED_TIMING_STAT(RoguePerf, EnemyBulletSweep);
        GetWorld()->SweepMultiByChannel(Hits,Start,End,Collision->GetComponentQuat(),
            Collision->GetCollisionObjectType(),Collision->GetCollisionShape(),Params,Response);
    }
    // Sweep results are in travel order; process the first valid touch or blocking wall only.
    FHitResult Impact;bool bHit=false;
    for(const FHitResult& Hit:Hits)
    {
        AActor* Other=Hit.GetActor();
        if(Other && (Other==GetOwner() || Other==GetInstigator() || Cast<ARogueEnemy>(Other)))continue;
        if(const auto* Player=Cast<ARogueCharacter>(Other);Player && !Player->IsAlive())continue;
        if(!Hit.bBlockingHit)
        {
            const auto* OtherComponent=Hit.GetComponent();
            if(!IsValid(Other) || !OtherComponent || !OtherComponent->GetGenerateOverlapEvents())continue;
        }
        Impact=Hit;bHit=true;break;
    }
    {
        CSV_SCOPED_TIMING_STAT(RoguePerf, EnemyBulletTransform);
        const FVector Destination=bHit?FMath::Lerp(Start,End,double(Impact.Time)):End;
        if(ProjectileMovement->bRotationFollowsVelocity && !Velocity.IsNearlyZero())
        {
            FRotator Rotation=Velocity.Rotation();
            if(ProjectileMovement->bRotationRemainsVertical){Rotation.Pitch=0;Rotation.Roll=0;}
            SetActorLocationAndRotation(Destination,Rotation,false,nullptr,ETeleportType::None);
        }
        else SetActorLocation(Destination,false,nullptr,ETeleportType::None);
        Collision->ComponentVelocity=Velocity;
    }
    if(bHit && bBatchedMovement && BatchGeneration==Generation)
    {
        const FVector Normal=Impact.ImpactNormal.IsNearlyZero()?-Velocity.GetSafeNormal():FVector(Impact.ImpactNormal);
        ResolveImpact(Impact.GetActor(),Impact.ImpactPoint,Normal);
    }
}

void ARogueEnemyProjectile::EndPlay(const EEndPlayReason::Type Reason)
{
    bBatchedMovement=false;++BatchGeneration;
    Super::EndPlay(Reason);
}
