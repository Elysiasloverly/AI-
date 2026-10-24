#include "Framework/RogueBattleStage.h"

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
