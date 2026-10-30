#include "Enemies/RogueEnemy.h"
#include "Combat/Projectiles/RogueEnemySkyfallProjectile.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "Player/RogueCharacter.h"

bool ARogueEnemy::StartSkyfallAttack(ARogueCharacter* Target)
{
    if (!HasAuthority() || bDead || bPoolAvailable || !IsValid(Target) || !Target->IsAlive()) return false;
    const auto S=CurrentArchetype.Ranged.Skyfall.Sanitized();
    const FVector Feet=Target->GetActorLocation()-FVector(0,0,Target->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
    FHitResult Hit;
    FCollisionQueryParams Q(SCENE_QUERY_STAT(RogueSkyfallLockGround),false,this); Q.AddIgnoredActor(Target);
    // 从玩家脚边向下找地面，避免从天顶射线先选中树冠或屋顶。
    if (!GetWorld()->LineTraceSingleByObjectType(Hit,Feet+FVector(0,0,120),Feet-FVector(0,0,4000),
        FCollisionObjectQueryParams(ECC_WorldStatic),Q) || Hit.ImpactNormal.Z<.5f) return false;
    const FVector Ground=Hit.ImpactPoint;
    const FVector TowardSource=(GetActorLocation()-Ground).GetSafeNormal2D();
    const FVector Launch=Ground+TowardSource*S.LaunchOffset+FVector(0,0,S.SpawnHeight);
    const TSubclassOf<ARogueEnemySkyfallProjectile> Class=S.ProjectileClass?S.ProjectileClass.Get():ARogueEnemySkyfallProjectile::StaticClass();
    FActorSpawnParameters P; P.Owner=this; P.Instigator=this;
    P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Shot=GetWorld()->SpawnActor<ARogueEnemySkyfallProjectile>(Class,Launch,FRotator::ZeroRotator,P);
    if (!Shot) return false;
    Shot->InitializeAttack(this,Launch,Ground,S,GetEnemyProjectileClass(),ContactDamage);
    NotifyAttackAudio(GetActorLocation());
    return true;
}
