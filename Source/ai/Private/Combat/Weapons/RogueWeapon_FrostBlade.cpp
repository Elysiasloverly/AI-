#include "Combat/Weapons/RogueWeapon_FrostBlade.h"
#include "Combat/Projectiles/RogueFrostBlade.h"
#include "Player/RogueCharacter.h"
#include "Enemies/RogueEnemy.h"
#include "Engine/World.h"
float ARogueWeapon_FrostBlade::GetEffectiveDamage()const
{
    // Permanent base attack and in-run attack cards share this GAS bonus. Size
    // amplifies the combined damage once; the optional authored cap remains final.
    const float Raw=(Config.Damage+GetWeaponBonusValues().AttackPower*Config.FrostBladeCombat.AttackPowerScale)*GetEffectiveBladeScale();
    return FMath::Max(0.f,Config.MaxDamage>=0?FMath::Min(Raw,Config.MaxDamage):Raw);
}
float ARogueWeapon_FrostBlade::GetEffectiveBladeScale()const{return 1+FMath::Max(0.f,GetWeaponBonusValues().FrostBladeSize);}
float ARogueWeapon_FrostBlade::GetEffectiveWaveScale()const{return 1+FMath::Max(0.f,GetWeaponBonusValues().FrostBladeWaveSize);}
int32 ARogueWeapon_FrostBlade::GetEffectiveWaveCount()const
{
    if(GetWeaponBonusValues().FrostBladeWave<=0)return 0;
    const int32 Count=1+FMath::Max(0,FMath::RoundToInt(GetWeaponBonusValues().FrostBladeWaveCount));
    return Config.FrostBladeCombat.MaxWaveCount<0?Count:FMath::Min(Count,Config.FrostBladeCombat.MaxWaveCount);
}
float ARogueWeapon_FrostBlade::GetEffectiveRange()const
{
    const auto& C=Config.FrostBladeCombat;
    // General range increases real reach, with no blade-specific range ceiling.
    // The size card independently increases the blade and its physical slash length.
    return FMath::Max(0.f,Config.Range+GetWeaponBonusValues().AttackRange)
        +C.BladeLength*(GetEffectiveBladeScale()-1);
}
void ARogueWeapon_FrostBlade::WeaponTick(float Delta)
{
    auto* Player=GetOwnerCharacter();if(!Player||!Player->IsAuthoritySide())return;
    if(!Player->IsAlive()||GetEffectiveCount()<=0){OnOwnerDied();return;}
    if(!IsValid(Blade))
    {
        FActorSpawnParameters P;P.Owner=Player;P.Instigator=Player;P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        UClass* Class=Config.FrostBladeClass?Config.FrostBladeClass.Get():ARogueFrostBlade::StaticClass();
        Blade=GetWorld()->SpawnActor<ARogueFrostBlade>(Class,Player->GetActorLocation(),FRotator::ZeroRotator,P);
        if(!Blade)return;
    }
    FRogueFrostBladeCombatSettings Combat=Config.FrostBladeCombat;
    const float Scale=GetEffectiveBladeScale();
    const float RangeBonus=FMath::Max(0.f,GetWeaponBonusValues().AttackRange);
    // Preserve the initial close-range pose, but allow the pivot to reach all valid
    // targets as either the general range bonus or authored base range grows.
    Combat.MaxLeashDistance=FMath::Max(Combat.MaxLeashDistance+RangeBonus,
        GetEffectiveRange()-Combat.BladeLength*Scale);
    Blade->Configure(Player,Combat,Scale);
    const float Now=GetWorld()->GetTimeSeconds();if(Now<NextAttack||Blade->GetPhase()!=ERogueFrostBladePhase::Hover)return;
    TArray<ARogueEnemy*> Candidates;CollectEnemiesInRange(GetEffectiveRange(),Candidates);
    // A blocked nearest enemy must not prevent attacks on other visible enemies.
    FCollisionQueryParams Sight(SCENE_QUERY_STAT(FrostBladeTargetSight),false,Player);
    Candidates.RemoveAll([&](const ARogueEnemy* Enemy)
    {
        FHitResult Wall;
        return GetWorld()->LineTraceSingleByObjectType(Wall,Player->GetActorLocation(),Enemy->GetActorLocation(),FCollisionObjectQueryParams(ECC_WorldStatic),Sight);
    });
    auto* Target=SelectDistributedTarget(Candidates,{});if(!Target)return;
    const float Damage=GetEffectiveDamage();
    if(Blade->StartAttack(Target,this,Damage,GetEffectiveWaveCount(),Damage*Config.FrostBladeCombat.WaveDamageMultiplier,GetEffectiveWaveScale(),GetEffectiveProjectileSpeed(Config.FrostBladeCombat.WaveSpeed),Config.FrostWaveClass))
    {
        NextAttack=Now+GetEffectiveCooldown();
        // The blade confirms audio, cast art and the HUD pulse when its slash actually begins.
    }
}
void ARogueWeapon_FrostBlade::OnOwnerDied(){Super::OnOwnerDied();if(IsValid(Blade))Blade->Destroy();Blade=nullptr;NextAttack=0;}
void ARogueWeapon_FrostBlade::EndPlay(const EEndPlayReason::Type Reason){OnOwnerDied();Super::EndPlay(Reason);}
