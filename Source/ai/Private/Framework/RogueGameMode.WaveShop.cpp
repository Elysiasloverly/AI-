#include "Framework/RogueGameMode.h"

#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Player/RogueCharacter.h"
#include "Player/RoguePlayerState.h"
#include "World/RogueShopTerminal.h"

void ARogueGameMode::QueueWaveShopDrop()
{
	bWaveShopDropPending = true;
	NextShopDropAttempt = 0.0;
	TickWaveShop();
}

ARogueCharacter* ARogueGameMode::FindWaveShopAnchor() const
{
	ARogueCharacter* Fallback = nullptr;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* Controller = It->Get();
		if (!IsValid(Controller) || Controller->IsActorBeingDestroyed()) { continue; }
		const APlayerState* State = Controller->PlayerState;
		if (State && State->IsOnlyASpectator()) { continue; }
		ARogueCharacter* Character = Cast<ARogueCharacter>(Controller->GetPawn());
		if (!IsValid(Character) || Character->IsActorBeingDestroyed()) { continue; }
		// 监听服的本地控制器就是房主，加入者不会因为排在列表前面而抢到落点。
		if (Controller->IsLocalController()) { return Character; }
		if (!Fallback && Character->IsAlive()) { Fallback = Character; }
	}
	return Fallback;
}

void ARogueGameMode::TickWaveShop()
{
	ARogueShopTerminal* Terminal = LevelShopTerminal.Get();
	if (!Terminal) { return; }
	if (Terminal->IsReady() && (bWaveShopDropPending || bWaveShopWasOpened))
	{
		for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			APlayerController* Controller = It->Get();
			ARoguePlayerState* State = IsValid(Controller) ? Controller->GetPlayerState<ARoguePlayerState>() : nullptr;
			if (State && State->GetShopRuntime().IsOpen()) { return; }
		}
		Terminal->BeginDeparture();
		bWaveShopWasOpened = false;
	}

	const double Now = GetWorld()->GetTimeSeconds();
	if (!bWaveShopDropPending || !Terminal->IsWaitingInSky() || Now < NextShopDropAttempt) { return; }
	NextShopDropAttempt = Now + .25;
	if (const ARogueCharacter* Anchor = FindWaveShopAnchor(); Anchor && Terminal->TryDropNear(*Anchor))
	{
		bWaveShopDropPending = false;
	}
}
