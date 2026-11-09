#include "Combat/Weapons/RogueWeapon_Talisman.h"
#include "Combat/Projectiles/RogueReturningTalisman.h"
#include "Player/RogueCharacter.h"
#include "Engine/World.h"

int32 ARogueWeapon_Talisman::GetEffectiveTargetCount() const
{
    return FMath::Max(1,Config.TalismanCombat.BaseTargetCount)+FMath::Max(0,FMath::RoundToInt(GetWeaponBonusValues().TalismanPierce));
}
int32 ARogueWeapon_Talisman::GetEffectiveHitsPerTarget() const
{
    return FMath::Max(1,Config.TalismanCombat.BaseHitsPerTarget)+FMath::Max(0,FMath::RoundToInt(GetWeaponBonusValues().TalismanRepeat));
}
void ARogueWeapon_Talisman::WeaponTick(float DeltaSeconds)
{
    ARogueCharacter* Player=GetOwnerCharacter();
    if (!Player || !Player->IsAuthoritySide()) { return; }
    if (!Player->IsAlive()) { OnOwnerDied(); return; }
    Talismans.RemoveAll([](const auto& Actor){return !IsValid(Actor);});
    const int32 Count=GetEffectiveCount();
    while (Talismans.Num()>Count) { if (ARogueReturningTalisman* Actor=Talismans.Pop()) { Actor->Destroy(); } }
    while (Talismans.Num()<Count)
    {
        FActorSpawnParameters Params;
        Params.Owner=Player; Params.Instigator=Player;
        Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        UClass* Class=Config.TalismanClass ? Config.TalismanClass.Get() : ARogueReturningTalisman::StaticClass();
        auto* Actor=GetWorld()->SpawnActor<ARogueReturningTalisman>(Class,Player->GetActorLocation(),FRotator::ZeroRotator,Params);
        if (!Actor) { break; }
        Talismans.Add(Actor);
    }
    bool bLaunched=false;FVector LaunchLocation=Player->GetActorLocation()+FVector(0,0,35),LaunchDirection=Player->GetActorForwardVector();
    for (int32 I=0;I<Talismans.Num();++I)
    {
        Talismans[I]->Configure(Player,I,Talismans.Num(),GetEffectiveDamage(),GetEffectiveRange(),
            GetEffectiveCooldown(),GetEffectiveTargetCount(),GetEffectiveHitsPerTarget(),Config.TalismanCombat,this);
        if(Talismans[I]->TryLaunch())
        {
            if(!bLaunched){LaunchLocation=Talismans[I]->GetActorLocation();LaunchDirection=Talismans[I]->GetActorForwardVector();}
            bLaunched=true;
        }
    }
    if (bLaunched)
    {
        FRogueWeaponCosmeticFire Audio; Audio.Start=LaunchLocation;Audio.Direction=LaunchDirection; Audio.bPlayAttackSound=true; Audio.bPlayFireAccent=true;
        BroadcastCosmeticFire(Audio);
    }
}
void ARogueWeapon_Talisman::OnOwnerDied()
{
    Super::OnOwnerDied();
    for (ARogueReturningTalisman* Actor:Talismans) { if (IsValid(Actor)) { Actor->Destroy(); } }
    Talismans.Reset();
}
void ARogueWeapon_Talisman::EndPlay(const EEndPlayReason::Type Reason)
{
    OnOwnerDied(); Super::EndPlay(Reason);
}
