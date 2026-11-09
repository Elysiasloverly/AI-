#include "RogueBossAttackSettingsCustomization.h"

#include "AbilitySystem/Abilities/RogueBossAttackAbilities.h"
#include "DetailWidgetRow.h"
#include "Enemies/Data/RogueBossAttackSettings.h"
#include "Enemies/Data/RogueBossSkillTypes.h"
#include "IDetailChildrenBuilder.h"
#include "IPropertyUtilities.h"
#include "PropertyHandle.h"

namespace
{
#define BOSS_SETTING(Member) GET_MEMBER_NAME_CHECKED(FRogueBossAttackSettings, Member)

TConstArrayView<FName> SettingsUsedBy(const UClass* Ability)
{
    static const FName Octagram[] = { BOSS_SETTING(DamageScale), BOSS_SETTING(Speed), BOSS_SETTING(RotationSpeed), BOSS_SETTING(BulletClass) };
    static const FName RollingSuns[] = { BOSS_SETTING(DamageScale), BOSS_SETTING(Radius), BOSS_SETTING(Speed), BOSS_SETTING(SpreadDegrees), BOSS_SETTING(EffectClass) };
    static const FName JudgmentRay[] = { BOSS_SETTING(DamageScale), BOSS_SETTING(BeamHalfWidth), BOSS_SETTING(BeamRange), BOSS_SETTING(EffectClass) };
    static const FName Support[] = { BOSS_SETTING(Radius), BOSS_SETTING(EffectClass) };
    static const FName ScarletRain[] = {
        BOSS_SETTING(DamageScale), BOSS_SETTING(BombsPerPulse), BOSS_SETTING(WarningSeconds), BOSS_SETTING(Radius),
        BOSS_SETTING(SafeRadius), BOSS_SETTING(ImpactSpacing), BOSS_SETTING(FireSeconds), BOSS_SETTING(BurnDamageScale),
        BOSS_SETTING(BurnInterval), BOSS_SETTING(FallHeight), BOSS_SETTING(EffectClass)
    };
    static const FName FlameCrescents[] = {
        BOSS_SETTING(DamageScale), BOSS_SETTING(Speed), BOSS_SETTING(SpreadDegrees), BOSS_SETTING(WaveHalfWidth),
        BOSS_SETTING(WaveHeightAboveGround), BOSS_SETTING(WaveHalfHeight), BOSS_SETTING(WaveGrowthDistance), BOSS_SETTING(WaveMaxScale), BOSS_SETTING(EffectClass)
    };
    static const FName EndlessRing[] = { BOSS_SETTING(DamageScale), BOSS_SETTING(RingCount), BOSS_SETTING(Speed), BOSS_SETTING(BulletClass) };
    static const FName FrostPursuit[] = { BOSS_SETTING(DamageScale), BOSS_SETTING(WarningSeconds), BOSS_SETTING(Radius), BOSS_SETTING(EffectClass) };

    // Exact native classes have a known set of inputs. Custom abilities keep access to all fields.
    if (Ability == URogueBossOctagramAbility::StaticClass()) return Octagram;
    if (Ability == URogueBossRollingSunsAbility::StaticClass()) return RollingSuns;
    if (Ability == URogueBossJudgmentRayAbility::StaticClass()) return JudgmentRay;
    if (Ability == URogueBossLastBloomAbility::StaticClass() || Ability == URogueBossDiamondReversalAbility::StaticClass()) return Support;
    if (Ability == URogueBossScarletRainAbility::StaticClass()) return ScarletRain;
    if (Ability == URogueBossFlameCrescentsAbility::StaticClass()) return FlameCrescents;
    if (Ability == URogueBossEndlessRingAbility::StaticClass()) return EndlessRing;
    if (Ability == URogueBossFrostPursuitAbility::StaticClass()) return FrostPursuit;
    return {};
}

#undef BOSS_SETTING
}

TSharedRef<IPropertyTypeCustomization> FRogueBossAttackSettingsCustomization::MakeInstance()
{
    return MakeShared<FRogueBossAttackSettingsCustomization>();
}

void FRogueBossAttackSettingsCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> Struct, FDetailWidgetRow& Header,
    IPropertyTypeCustomizationUtils& Utils)
{
    Header.NameContent()[Struct->CreatePropertyNameWidget()];
}

void FRogueBossAttackSettingsCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> Struct, IDetailChildrenBuilder& Children,
    IPropertyTypeCustomizationUtils& Utils)
{
    const TSharedPtr<IPropertyHandle> Row = Struct->GetParentHandle();
    const FName AbilityProperty = GET_MEMBER_NAME_CHECKED(FRogueBossSkillRow, AbilityClass);
    const TSharedPtr<IPropertyHandle> Ability = Row ? Row->GetChildHandle(AbilityProperty, false) : nullptr;
    UObject* AbilityValue = nullptr;
    if (Ability && Ability->GetValue(AbilityValue) == FPropertyAccess::Success)
    {
        const TWeakPtr<IPropertyUtilities> Utilities = Utils.GetPropertyUtilities();
        Ability->SetOnPropertyValueChanged(FSimpleDelegate::CreateLambda([Utilities]
        {
            if (const auto Pinned = Utilities.Pin()) Pinned->RequestForceRefresh();
        }));

        const TConstArrayView<FName> UsedSettings = SettingsUsedBy(Cast<UClass>(AbilityValue));
        if (!UsedSettings.IsEmpty())
        {
            for (const FName Name : UsedSettings)
                if (const auto Handle = Struct->GetChildHandle(Name, false)) Children.AddProperty(Handle.ToSharedRef());
            return;
        }
    }

    uint32 Count = 0;
    Struct->GetNumChildren(Count);
    for (uint32 Index = 0; Index < Count; ++Index)
        if (const auto Handle = Struct->GetChildHandle(Index)) Children.AddProperty(Handle.ToSharedRef());
}
