#include "Framework/RogueBattleStage.h"
#include "Framework/RogueGameBalanceAsset.h"
#include "Misc/PackageName.h"

bool FRogueBattleStageSettings::ContainsMap(const FString& Package,bool bNight) const
{
    if(Package==(bNight?NightMap:DayMap).ToSoftObjectPath().GetLongPackageName())return true;
    return (bNight?NightMaps:DayMaps).ContainsByPredicate([&](const auto& Map){return Map.ToSoftObjectPath().GetLongPackageName()==Package;});
}
FString FRogueBattleStageSettings::ChooseMap(bool bNight,const FString& Exclude) const
{
    TArray<FString> Valid;
    for(const auto& Map:(bNight?NightMaps:DayMaps))
    {
        const FString Package=Map.ToSoftObjectPath().GetLongPackageName();
        if(!Package.IsEmpty()&&Package!=Exclude&&FPackageName::DoesPackageExist(Package))Valid.AddUnique(Package);
    }
    if(!Valid.IsEmpty())return Valid[FMath::RandHelper(Valid.Num())];
    const FString Fallback=(bNight?NightMap:DayMap).ToSoftObjectPath().GetLongPackageName();
    return Fallback!=Exclude&&FPackageName::DoesPackageExist(Fallback)?Fallback:FString();
}
FString RogueBattleMaps::ChooseInitialDay(const FString& Fallback)
{
    if(const auto* Balance=LoadObject<URogueGameBalanceAsset>(nullptr,TEXT("/Game/Data/Core/DA_GameBalance.DA_GameBalance")))
    {
        const FString Choice=Balance->BattleStage.ChooseMap(false);
        if(!Choice.IsEmpty())return Choice;
    }
    return Fallback;
}

void URogueRunTravelSubsystem::Prepare(const FString& Destination, const FRogueRunState& State)
{
    PendingDestination = Destination;
    PendingState = State;
}

bool URogueRunTravelSubsystem::Consume(const FString& Destination, FRogueRunState& OutState)
{
    const bool bMatches = !PendingDestination.IsEmpty() && PendingDestination == Destination;
    if (bMatches) { OutState = PendingState; }
    Reset();
    return bMatches;
}

void URogueRunTravelSubsystem::Reset()
{
    PendingDestination.Reset();
    PendingState = FRogueRunState();
}
