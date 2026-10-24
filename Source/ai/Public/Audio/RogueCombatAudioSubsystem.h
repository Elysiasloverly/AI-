#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Audio/RogueAttackAudioSettings.h"
#include "RogueCombatAudioSubsystem.generated.h"

/** 仅本地播放；服务器确认事件由角色/敌人的表现通道传入。 */
UCLASS()
class AI_API URogueCombatAudioSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    bool PlayWeaponAttack(AActor* Source, const FVector& Location, FRogueAttackAudioState& State);
    bool PlayEnemyAttack(AActor* Source, const FVector& Location, FRogueAttackAudioState& State);
    const FRogueAttackAudioSettings& GetEnemyAttackSettings(const UClass* Class) const;
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    bool PlayAttack(AActor* Source, const FVector& Location, const FRogueAttackAudioSettings& Settings,
        FRogueAttackAudioState& State, bool bEnemy);
private:
    const class URogueAudioConfig* ResolveConfig() const;
    UPROPERTY(Transient) TObjectPtr<USoundAttenuation> DefaultAttenuation;
    UPROPERTY(Transient) TObjectPtr<USoundConcurrency> WeaponConcurrency;
    UPROPERTY(Transient) TObjectPtr<USoundConcurrency> EnemyConcurrency;
    FRandomStream AudioRandom;
};
