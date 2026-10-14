#include "Player/RoguePlayerQuery.h"

#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Player/RogueCharacter.h"

namespace RoguePlayerQuery
{
	ARogueCharacter* GetCharacterOf(const APlayerController* PlayerController)
	{
		if (PlayerController == nullptr)
		{
			return nullptr;
		}

		ARogueCharacter* Character = Cast<ARogueCharacter>(PlayerController->GetPawn());
		return IsValid(Character) ? Character : nullptr;
	}

	void CollectAlivePlayers(const UWorld* World, TArray<ARogueCharacter*, TInlineAllocator<4>>& OutPlayers)
	{
		OutPlayers.Reset();
		if (World == nullptr)
		{
			return;
		}

		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			ARogueCharacter* Character = GetCharacterOf(It->Get());
			if (Character != nullptr && Character->IsAlive())
			{
				OutPlayers.Add(Character);
			}
		}
	}

	ARogueCharacter* FindNearestAlivePlayer(const UWorld* World, const FVector& Location)
	{
		TArray<ARogueCharacter*, TInlineAllocator<4>> AlivePlayers;
		CollectAlivePlayers(World, AlivePlayers);

		ARogueCharacter* Nearest = nullptr;
		float NearestDistanceSquared = TNumericLimits<float>::Max();
		for (ARogueCharacter* Candidate : AlivePlayers)
		{
			const float DistanceSquared = FVector::DistSquared2D(Location, Candidate->GetActorLocation());
			if (DistanceSquared < NearestDistanceSquared)
			{
				NearestDistanceSquared = DistanceSquared;
				Nearest = Candidate;
			}
		}

		return Nearest;
	}

	bool IsAnyAlivePlayerWithin(const UWorld* World, const FVector& Location, float MinDistance)
	{
		TArray<ARogueCharacter*, TInlineAllocator<4>> AlivePlayers;
		CollectAlivePlayers(World, AlivePlayers);

		const float MinDistanceSquared = FMath::Square(MinDistance);
		for (const ARogueCharacter* Candidate : AlivePlayers)
		{
			if (FVector::DistSquared2D(Location, Candidate->GetActorLocation()) < MinDistanceSquared)
			{
				return true;
			}
		}

		return false;
	}
}
