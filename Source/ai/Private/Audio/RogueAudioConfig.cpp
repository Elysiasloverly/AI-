#include "Audio/RogueAudioConfig.h"
#include "Combat/Weapons/RogueWeaponBase.h"
#include "Enemies/RogueEnemy.h"

const FRogueMusicTrack* URogueAudioConfig::FindTrack(ERogueMusicScene Scene) const
{
    switch (Scene)
    {
    case ERogueMusicScene::Menu: return &Menu;
    case ERogueMusicScene::Day: return &Day;
    case ERogueMusicScene::Night: return &Night;
    case ERogueMusicScene::Boss: return &Boss;
    case ERogueMusicScene::Results: return &Results;
    default: return nullptr;
    }
}

const FRogueAttackAudioSettings* URogueAudioConfig::FindWeaponAudio(const UClass* Class) const
{
    // 先匹配具体子类，再向上查父类；空槽代表有意留空，不偷偷借用别种武器。
    for (const UClass* Candidate = Class; Candidate; Candidate = Candidate->GetSuperClass())
        for (const auto& Entry : Weapons)
            if (Entry.WeaponClass.Get() == Candidate) return &Entry.AttackAudio;
    return nullptr;
}

const FRogueAttackAudioSettings* URogueAudioConfig::FindEnemyAudio(const UClass* Class) const
{
    for (const UClass* Candidate = Class; Candidate; Candidate = Candidate->GetSuperClass())
        for (const auto& Entry : Enemies)
            if (Entry.EnemyClass.Get() == Candidate && (!Entry.AttackAudio.bEnabled || Entry.AttackAudio.HasSound())) return &Entry.AttackAudio;
    return nullptr;
}
