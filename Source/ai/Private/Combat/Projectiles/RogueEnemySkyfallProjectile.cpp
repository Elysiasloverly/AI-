#include "Combat/Projectiles/RogueEnemySkyfallProjectile.h"

#include "AbilitySystem/RogueAbilitySystemLibrary.h"
#include "AbilitySystem/RogueGameplayTags.h"
#include "Combat/Pooling/RogueCombatPoolSubsystem.h"
#include "Combat/Projectiles/RogueEnemyProjectile.h"
#include "Combat/Visuals/RogueImpactEffect.h"
#include "Components/DecalComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Enemies/RogueEnemy.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "Framework/RogueGameState.h"
#include "Net/UnrealNetwork.h"
#include "Player/RogueCharacter.h"

ARogueEnemySkyfallProjectile::ARogueEnemySkyfallProjectile()
{
    PrimaryActorTick.bCanEverTick=true;
    bReplicates=true;
    bAlwaysRelevant=true;
    SetReplicateMovement(false);
    SetNetUpdateFrequency(30);
    RootComponent=CreateDefaultSubobject<USceneComponent>(TEXT("SkyfallRoot"));
    SetActorEnableCollision(false);
}

void ARogueEnemySkyfallProjectile::BeginPlay()
{
    Super::BeginPlay();
    // 模型与材质由蓝图组件配置；装饰不参加碰撞，地面接触由服务端球形扫掠检测。
    for (auto* Mesh:TInlineComponentArray<UStaticMeshComponent*>(this))
    {
        Mesh->SetSimulatePhysics(false); Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Mesh->SetGenerateOverlapEvents(false);
    }
    UpdateVisuals();
}

void ARogueEnemySkyfallProjectile::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ARogueEnemySkyfallProjectile,State);
}

float ARogueEnemySkyfallProjectile::ServerTime() const
{
    const auto* World=GetWorld();
    const auto* GS=World?World->GetGameState():nullptr;
    return GS?GS->GetServerWorldTimeSeconds():(World?World->GetTimeSeconds():0.f);
}

float ARogueEnemySkyfallProjectile::GetAttackAge() const
{
    return FMath::Max(0.f,ServerTime()-State.StartTime);
}

bool ARogueEnemySkyfallProjectile::IsWarningVisible() const
{
    if (!State.bActive || State.bImpacted || IsActorBeingDestroyed()) return false;
    for (auto* Decal:TInlineComponentArray<UDecalComponent*>(this))
        if (Decal->ComponentHasTag(TEXT("Rogue.SkyfallWarning")) && Decal->IsVisible()) return true;
    return false;
}

void ARogueEnemySkyfallProjectile::InitializeAttack(ARogueEnemy* Source,const FVector& Launch,const FVector& Ground,
    const FRogueEnemySkyfallSettings& Settings,TSubclassOf<ARogueEnemyProjectile> BurstClass,float Damage)
{
    if (!HasAuthority() || GetNetMode()==NM_Client || State.bActive || !IsValid(Source) || Source->IsDead()) return;
    SetOwner(Source); SetInstigator(Source); SourceCycle=Source->GetSpawnCycleId();
    AttackDamage=FMath::IsFinite(Damage)?FMath::Max(0.f,Damage):0.f;
    State.bActive=true; State.Launch=Launch; State.Ground=Ground; State.StartTime=ServerTime();
    State.StartYaw=Source->GetActorRotation().Yaw; State.Settings=Settings.Sanitized(); State.BurstClass=BurstClass;
    SetActorLocation(Launch); UpdateVisuals(); SetLifeSpan(30.f); ForceNetUpdate();
    UE_LOG(LogTemp,Log,TEXT("[Skyfall] Summoned Ground=%s Warning=%.2f Fall=%.2f"),*Ground.ToCompactString(),State.Settings.WarningSeconds,State.Settings.FallSeconds);
}

void ARogueEnemySkyfallProjectile::UpdateVisuals()
{
    const bool bShow=State.bActive&&!State.bImpacted;
    SetActorHiddenInGame(!bShow);
    for (auto* Decal:TInlineComponentArray<UDecalComponent*>(this))
    {
        if (!Decal->ComponentHasTag(TEXT("Rogue.SkyfallWarning"))) continue;
        // 红圈脱离下落根节点，留在锁定落点；不设置淡出时间，直到收到权威触地或销毁。
        Decal->SetUsingAbsoluteLocation(true); Decal->SetUsingAbsoluteRotation(true);
        Decal->SetUsingAbsoluteScale(true); Decal->SetWorldScale3D(FVector::OneVector);
        Decal->SetWorldLocation(FVector(State.Ground)+FVector(0,0,8)); Decal->SetWorldRotation(FRotator(-90,0,0));
        Decal->DecalSize=FVector(200,State.Settings.ImpactRadius,State.Settings.ImpactRadius);
        Decal->MarkRenderStateDirty(); Decal->SetVisibility(bShow);
    }
}

void ARogueEnemySkyfallProjectile::OnRep_State()
{
    UpdateVisuals();
    if (!State.bImpacted || bPlayedImpact) return;
    bPlayedImpact=true;
    EmitBurst(false);
    UE_LOG(LogTemp,Log,TEXT("[Skyfall] ClientImpact Burst=%d WarningVisible=%d"),State.Settings.Burst.ProjectileCount,IsWarningVisible());
    if (GetNetMode()!=NM_DedicatedServer)
        ARogueImpactEffect::SpawnImpactEffect(GetWorld(),State.Impact,FRotator::ZeroRotator,
            ERogueImpactVisualStyle::Spark,FVector(1.4f),.25f,this);
    OnSkyfallImpact(State.Impact);
}

void ARogueEnemySkyfallProjectile::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!State.bActive || State.bImpacted) return;
    const bool bServer=HasAuthority()&&GetNetMode()!=NM_Client;
    if (bServer)
    {
        const auto* Source=Cast<ARogueEnemy>(GetOwner());
        const auto* GS=GetWorld()->GetGameState<ARogueGameState>();
        if (!IsValid(Source) || Source->IsDead() || Source->GetSpawnCycleId()!=SourceCycle || (GS&&GS->IsGameOver()))
        { Destroy(); return; }
    }
    const float Alpha=FMath::Max(0.f,(GetAttackAge()-State.Settings.WarningSeconds)/State.Settings.FallSeconds);
    const FVector End=FVector(State.Ground)+FVector(0,0,State.Settings.CollisionRadius);
    // 客户端到达预计落点后等待权威触地；不能仅凭本机计时撤掉预警或生成伤害。
    const FVector Next=FMath::Lerp(FVector(State.Launch),End,bServer?Alpha:FMath::Min(Alpha,1.f));
    if (bServer && Alpha>0)
    {
        FHitResult Hit;
        FCollisionQueryParams Q(SCENE_QUERY_STAT(RogueSkyfallGround),false,this); Q.AddIgnoredActor(GetOwner());
        const FCollisionObjectQueryParams Objects(ECC_WorldStatic);
        if (GetWorld()->SweepSingleByObjectType(Hit,GetActorLocation(),Next,FQuat::Identity,Objects,
            FCollisionShape::MakeSphere(State.Settings.CollisionRadius),Q) && Hit.ImpactNormal.Z>=.5f)
        {
            SetActorLocation(Hit.Location);
            ImpactGround(Hit.ImpactPoint,Hit.ImpactNormal); return;
        }
    }
    SetActorLocation(Next);
    if (Alpha>0) SetActorRotation((End-FVector(State.Launch)).Rotation());
}

void ARogueEnemySkyfallProjectile::ImpactGround(const FVector& Ground,const FVector& Normal)
{
    if (!HasAuthority() || GetNetMode()==NM_Client || State.bImpacted || !State.bActive) return;
    // 先锁状态，再调用GAS；死亡、全灭或回调重入都不能重复爆发。
    State.bImpacted=true; State.Impact=Ground; UpdateVisuals();
    TArray<FOverlapResult> Hits;
    FCollisionQueryParams Q(SCENE_QUERY_STAT(RogueSkyfallDamage),false,this);
    GetWorld()->OverlapMultiByObjectType(Hits,Ground,FQuat::Identity,FCollisionObjectQueryParams(ECC_Pawn),
        FCollisionShape::MakeSphere(State.Settings.ImpactRadius),Q);
    TSet<ARogueCharacter*> Damaged;
    for (const auto& Hit:Hits)
    {
        auto* Player=Cast<ARogueCharacter>(Hit.GetActor());
        if (!Player || !Player->IsAlive() || Damaged.Contains(Player)
            || FVector::DistSquared2D(Player->GetActorLocation(),Ground)>FMath::Square(State.Settings.ImpactRadius)) continue;
        Damaged.Add(Player);
        RogueAbilitySystem::ApplyDamageToTarget(this,Player,AttackDamage*State.Settings.ImpactDamageMultiplier,
            RogueGameplayTags::Damage_Source_Projectile);
    }
    EmitBurst(true); bPlayedImpact=true;
    if (GetNetMode()!=NM_DedicatedServer)
        ARogueImpactEffect::SpawnImpactEffect(GetWorld(),Ground,Normal.Rotation(),ERogueImpactVisualStyle::Spark,FVector(1.4f),.25f,this);
    OnSkyfallImpact(Ground); ForceNetUpdate();
    UE_LOG(LogTemp,Log,TEXT("[Skyfall] ServerImpact Burst=%d PlayersDamaged=%d Ground=%s"),State.Settings.Burst.ProjectileCount,Damaged.Num(),*Ground.ToCompactString());
    // 给触地快照留出复制时间；隐藏Actor保留片刻，客户端随后收到销毁。
    SetLifeSpan(1.f);
}

void ARogueEnemySkyfallProjectile::EmitBurst(bool bDamageEnabled)
{
    auto* Pools=GetWorld()->GetSubsystem<URogueCombatPoolSubsystem>();
    if (!Pools || !State.BurstClass) return;
    const auto S=State.Settings.Burst.Sanitized();
    const FVector Center=FVector(State.Impact)+FVector(0,0,S.HeightAboveGround);
    const float SpawnRadius=State.Settings.CollisionRadius+24;
    // 子弹拥有施法者而非短命落弹Actor，避免落弹销毁后伤害归属悬空。
    AActor* Source=IsValid(GetOwner())?GetOwner():nullptr;
    for (int32 I=0;I<S.ProjectileCount;++I)
    {
        const FVector Direction=FRotator(0,State.StartYaw+360.f*I/S.ProjectileCount,0).Vector();
        const FVector Position=Center+Direction*SpawnRadius;
        if (auto* Bullet=Pools->AcquireEnemyProjectile(State.BurstClass,Source,GetInstigator(),Position,Direction.Rotation()))
            Bullet->ActivatePooledProjectile(Source,GetInstigator(),Position,Direction.Rotation(),Direction,S.ProjectileSpeed,
                bDamageEnabled?AttackDamage*S.DamageMultiplier:0.f,false,S.ProjectileLifetime,S.ProjectileScale,true);
    }
}
