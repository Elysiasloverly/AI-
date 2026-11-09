#include "Enemies/RogueEnemy.h"
#include "Player/RogueCharacter.h"
#include "Combat/Pooling/RogueCombatPoolSubsystem.h"
#include "Combat/Projectiles/RogueEnemyProjectile.h"
#include "GameFramework/CharacterMovementComponent.h"

void ARogueEnemy::InitializeFlight()
{
    FlyingVolleysRemaining=0; FlyingVolleyTimer=0; FlyingTarget.Reset(); FlightAge=0;
    if (!RogueEnemyArchetypes::IsFlyingType(EnemyType)) { return; }
    auto* Movement=GetCharacterMovement();
    Movement->StopMovementImmediately(); Movement->GravityScale=0; Movement->SetMovementMode(MOVE_Flying);
    if (HasAuthority())
    {
        FVector Position=GetActorLocation(); Position.Z=FlightGroundZ+CurrentArchetype.Flying.Sanitized().HoverHeight;
        SetActorLocation(Position,false,nullptr,ETeleportType::TeleportPhysics);
    }
}

void ARogueEnemy::TickFlight(float DeltaSeconds)
{
    if (!RogueEnemyArchetypes::IsFlyingType(EnemyType)) { return; }
    FlightAge+=DeltaSeconds;
    const auto Settings=CurrentArchetype.Flying.Sanitized();
    // 服务端保持出池处的飞行平面；客户端直接使用角色移动复制，避免两端漂浮相位争抢位置。
    FVector Position=GetActorLocation();
    Position.Z=FlightGroundZ+Settings.HoverHeight+FMath::Sin(FlightAge*2.5f)*Settings.BobAmplitude;
    SetActorLocation(Position,true);
}

void ARogueEnemy::SpawnFlyingVolley(const TArray<FRogueEnemyPatternShot>& Shots,float Speed,float Damage,float Lifetime,TSubclassOf<ARogueEnemyProjectile> Class)
{
    auto* Pools=GetWorld()?GetWorld()->GetSubsystem<URogueCombatPoolSubsystem>():nullptr;
    if (!Pools || !Class || Shots.Num()>32) { return; }
    for (const auto& Shot:Shots)
    {
        if (auto* Projectile=Pools->AcquireEnemyProjectile(Class,this,this,Shot.Location,Shot.Direction.Rotation()))
            Projectile->ActivatePooledProjectile(this,this,Shot.Location,Shot.Direction.Rotation(),Shot.Direction,Speed,Damage,false,Lifetime,0.65f,true,Shot.Motion);
    }
}

void ARogueEnemy::Multicast_FlyingVolley_Implementation(const TArray<FRogueEnemyPatternShot>& Shots,float Speed,float Lifetime,TSubclassOf<ARogueEnemyProjectile> Class)
{
    if (!HasAuthority()) { SpawnFlyingVolley(Shots,Speed,0,Lifetime,Class); }
}

void ARogueEnemy::EmitFlyingVolley(const TArray<FRogueEnemyPatternShot>& Shots,float Lifetime)
{
    const auto& Ranged=CurrentArchetype.Ranged;
    SpawnFlyingVolley(Shots,Ranged.ProjectileSpeed,ContactDamage*Ranged.ProjectileDamageMultiplier,Lifetime,GetEnemyProjectileClass());
    Multicast_FlyingVolley(Shots,Ranged.ProjectileSpeed,Lifetime,GetEnemyProjectileClass());
}

void ARogueEnemy::StartFlyingAttack(ARogueCharacter* Target)
{
    if (FlyingVolleysRemaining>0 || !IsValid(Target) || !Target->IsAlive()) { return; }
    const auto S=CurrentArchetype.Flying.Sanitized();
    FlyingAim=(Target->GetActorLocation()-GetActorLocation()).GetSafeNormal2D();
    if (FlyingAim.IsNearlyZero()) { FlyingAim=FVector::ForwardVector; }
    FlyingTarget=Target;
    if (EnemyType==ERogueEnemyType::SkyMoth)
    {
        FlyingVolleysRemaining=S.VolleyCount; FlyingVolleyIndex=0; FlyingVolleyTimer=0;
        TickFlyingAttack(0);
    }
    else
    {
        TArray<FRogueEnemyPatternShot> Shots;
        const FVector Center=Target->GetActorLocation();
        FVector Gap=Target->GetVelocity().GetSafeNormal2D(); if (Gap.IsNearlyZero()) { Gap=FlyingAim; }
        for (int32 I=0;I<S.ShotCount;++I)
        {
            FRogueEnemyPatternShot Shot;
            if (EnemyType==ERogueEnemyType::WindReaver)
            {
                Shot.Direction=FlyingAim.RotateAngleAxis((I-(S.ShotCount-1)*0.5f)*S.AngleStep,FVector::UpVector);
                FVector Origin=GetActorLocation(); Origin.Z=Center.Z; Shot.Location=Origin+FVector(Shot.Direction)*60;
                Shot.Motion.ReturnAfter=S.ReturnAfter;
            }
            else
            {
                const float Yaw=Gap.Rotation().Yaw+S.GapDegrees*0.5f+(360-S.GapDegrees)*I/(S.ShotCount-1);
                const FVector Radial=FRotator(0,Yaw,0).Vector();
                Shot.Location=Center+Radial*S.TrapRadius; Shot.Direction=-Radial;
                Shot.Motion.WarningSeconds=S.WarningSeconds;
            }
            Shots.Add(Shot);
        }
        EmitFlyingVolley(Shots,0.0f);
    }
    NotifyAttackFeedback(GetActorLocation());
}

void ARogueEnemy::TickFlyingAttack(float DeltaSeconds)
{
    if (FlyingVolleysRemaining<=0) { return; }
    const ARogueCharacter* Target=FlyingTarget.Get();
    if (!Target || !Target->IsAlive()) { FlyingVolleysRemaining=0; FlyingTarget.Reset(); return; }
    FlyingVolleyTimer-=DeltaSeconds;
    if (FlyingVolleyTimer>0) { return; }
    const auto S=CurrentArchetype.Flying.Sanitized();
    const float Sweep=(FlyingVolleyIndex-(S.VolleyCount-1)*0.5f)*S.AngleStep;
    TArray<FRogueEnemyPatternShot> Shots;
    for (float Sign:{-1.f,1.f})
    {
        FRogueEnemyPatternShot Shot;
        // 两条弹流错开半个角度步长，扫射中会穿过瞄准线，避免所有齐射都留下固定中心盲区。
        const float Angle=Sign>0 ? Sweep : -Sweep+S.AngleStep*0.5f;
        Shot.Direction=FlyingAim.RotateAngleAxis(Angle,FVector::UpVector);
        FVector Origin=GetActorLocation(); Origin.Z=Target->GetActorLocation().Z;
        Shot.Location=Origin+FVector(Shot.Direction)*60; Shots.Add(Shot);
    }
    EmitFlyingVolley(Shots,0.0f); ++FlyingVolleyIndex; --FlyingVolleysRemaining;
    FlyingVolleyTimer=FMath::Max(0.08f,S.VolleyInterval/AttackSpeedMultiplier);
}
