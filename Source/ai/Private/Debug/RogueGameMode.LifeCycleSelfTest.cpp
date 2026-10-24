#include "Framework/RogueGameMode.h"

#include "AbilitySystem/RogueAbilitySystemComponent.h"
#include "AbilitySystem/RogueAbilitySystemLibrary.h"
#include "AbilitySystem/RogueAttributeSet.h"
#include "AbilitySystem/RogueGameplayTags.h"
#include "Combat/Projectiles/RogueEnemyProjectile.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Enemies/RogueEnemy.h"
#include "Framework/RogueGameState.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Player/RogueCharacter.h"
#include "Player/RoguePlayerQuery.h"
#include "Player/RoguePlayerState.h"

namespace
{
	TAutoConsoleVariable<int32> CVarPlayerLifeCycleSelfTest(
		TEXT("Rogue.SelfTest.PlayerLifeCycle"), 0,
		TEXT("Real multiplayer lifecycle test: wait for a remote player, GAS death, ten-second revival, team wipe, no revival after twelve seconds."),
		ECVF_Default);

	bool HasGhostCollision(const ARogueCharacter* Character)
	{
		if (Character->GetActorEnableCollision()
			|| Character->GetCharacterMovement()->MovementMode != MOVE_None)
		{
			return false;
		}
		TInlineComponentArray<UPrimitiveComponent*> Primitives;
		Character->GetComponents(Primitives);
		for (const UPrimitiveComponent* Primitive : Primitives)
		{
			if (Primitive->GetCollisionEnabled() != ECollisionEnabled::NoCollision)
			{
				return false;
			}
		}
		return true;
	}

	bool HasCorrectOwnership(const ARogueCharacter* Character, const APlayerController* Controller)
	{
		if (!IsValid(Character) || !IsValid(Controller)
			|| Controller->GetPawn() != Character || Character->GetController() != Controller)
		{
			return false;
		}
		const ARoguePlayerState* State = Character->GetPlayerState<ARoguePlayerState>();
		const URogueAbilitySystemComponent* ASC = Character->GetRogueAbilitySystemComponent();
		return State != nullptr && State == Controller->PlayerState && State->GetPawn() == Character
			&& ASC != nullptr && ASC->GetOwnerActor() == State && ASC->GetAvatarActor() == Character;
	}

	void LogLifeCyclePlayer(const TCHAR* Stage, const ARogueCharacter* Character, double Now)
	{
		const ARoguePlayerState* State = Character->GetPlayerState<ARoguePlayerState>();
		const URogueAttributeSet* Attributes = Character->GetRogueAttributeSet();
		const URogueAbilitySystemComponent* ASC = Character->GetRogueAbilitySystemComponent();
		const APlayerController* Controller = Cast<APlayerController>(Character->GetController());
		UE_LOG(LogTemp, Log,
			TEXT("[LifeCycleSelfTest] %s Time=%.3f Player=%s Pawn=%s PC=%s PS=%s ASC.Owner=%s ASC.Avatar=%s Local=%d RemoteConnection=%d Phase=%d Health=%.1f/%.1f ActorCollision=%d Pending=%d Remaining=%.3f"),
			Stage, Now, State != nullptr ? *State->GetPlayerName() : TEXT("<null>"),
			*GetNameSafe(Character), *GetNameSafe(Controller), *GetNameSafe(State),
			*GetNameSafe(ASC != nullptr ? ASC->GetOwnerActor() : nullptr),
			*GetNameSafe(ASC != nullptr ? ASC->GetAvatarActor() : nullptr),
			Character->IsLocallyControlled() ? 1 : 0,
			Controller != nullptr && Controller->GetNetConnection() != nullptr ? 1 : 0,
			static_cast<int32>(Character->GetLifePhase()),
			Attributes != nullptr ? Attributes->GetHealth() : -1.0f,
			Attributes != nullptr ? Attributes->GetMaxHealth() : -1.0f,
			Character->GetActorEnableCollision() ? 1 : 0,
			Character->IsRevivePending() ? 1 : 0, Character->GetReviveSecondsRemaining());
	}
}

bool ARogueGameMode::RunPlayerLifeCycleSelfTest()
{
	UWorld* World = GetWorld();
	if (CVarPlayerLifeCycleSelfTest.GetValueOnGameThread() == 0 || World == nullptr || !HasAuthority())
	{
		return false;
	}
	if (LifeCycleSelfTestPhase == 7 || LifeCycleSelfTestPhase == 99)
	{
		return true;
	}
	const double Now = World->GetTimeSeconds();
	auto Fail = [this, Now](const TCHAR* Reason)
	{
		UE_LOG(LogTemp, Error, TEXT("[LifeCycleSelfTest] FAIL Phase=%d Time=%.3f Duration=%.3f Reason=%s"),
			LifeCycleSelfTestPhase, Now, Now - LifeCycleSelfTestStartTime, Reason);
		for (const TWeakObjectPtr<ARogueCharacter>& Player : LifeCycleSelfTestPlayers)
		{
			if (Player.IsValid()) { LogLifeCyclePlayer(TEXT("FAIL.Snapshot"), Player.Get(), Now); }
		}
		LifeCycleSelfTestPhase = 99;
	};
	if (LifeCycleSelfTestStartTime < 0.0)
	{
		LifeCycleSelfTestStartTime = Now;
		UE_LOG(LogTemp, Log, TEXT("[LifeCycleSelfTest] START Time=%.3f NetMode=%d; requires two initialized players and a real remote connection; normal spawning is suspended"),
			Now, static_cast<int32>(World->GetNetMode()));
	}

	// 只清理干扰对象；不改玩家血量、生命状态、复活截止或 TimerManager。
	for (TActorIterator<ARogueEnemy> It(World); It; ++It)
	{
		if (!It->IsAvailableInPool()) { It->DeactivateToPool(); }
	}
	for (TActorIterator<ARogueEnemyProjectile> It(World); It; ++It)
	{
		if (!It->IsAvailableInPool()) { It->DeactivateToPool(); }
	}

	if (LifeCycleSelfTestPhase == 0)
	{
		if (IsGameOver())
		{
			Fail(TEXT("Run ended before the two-player fixture became ready"));
			return true;
		}
		TArray<ARogueCharacter*, TInlineAllocator<4>> ReadyPlayers;
		int32 ParticipantCount = 0;
		int32 RemoteCount = 0;
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			APlayerController* Controller = It->Get();
			const ARoguePlayerState* State = IsValid(Controller) ? Controller->GetPlayerState<ARoguePlayerState>() : nullptr;
			if (State == nullptr || State->IsOnlyASpectator()) { continue; }
			++ParticipantCount;
			ARogueCharacter* Character = RoguePlayerQuery::GetCharacterOf(Controller);
			const URogueAttributeSet* Attributes = Character != nullptr ? Character->GetRogueAttributeSet() : nullptr;
			if (!HasCorrectOwnership(Character, Controller) || !State->IsBaseAttributesInitialized()
				|| Attributes == nullptr || Attributes->GetMaxHealth() <= 0.0f || Attributes->GetHealth() <= 0.0f
				|| !Character->IsAlive())
			{
				continue;
			}
			// A 必须是有真实网络连接的客户端玩家，让等待界面经历真实复制。
			if (Controller->GetNetConnection() != nullptr)
			{
				++RemoteCount;
				ReadyPlayers.Insert(Character, 0);
			}
			else { ReadyPlayers.Add(Character); }
		}
		if (ParticipantCount < 2 || ReadyPlayers.Num() != ParticipantCount || RemoteCount == 0
			|| World->GetNetMode() == NM_Standalone)
		{
			if (Now - LifeCycleSelfTestStartTime > 90.0)
			{
				Fail(TEXT("Timed out waiting for two correctly owned, initialized network players"));
			}
			return true;
		}
		for (ARogueCharacter* Player : ReadyPlayers)
		{
			LifeCycleSelfTestPlayers.Add(Player);
			LifeCycleSelfTestControllers.Add(Cast<APlayerController>(Player->GetController()));
			LogLifeCyclePlayer(TEXT("READY"), Player, Now);
		}
		ARogueCharacter* First = ReadyPlayers[0];
		bLifeCycleSelfTestActorCollision = First->GetActorEnableCollision();
		TInlineComponentArray<UPrimitiveComponent*> Primitives;
		First->GetComponents(Primitives);
		for (UPrimitiveComponent* Primitive : Primitives)
		{
			LifeCycleSelfTestCollisionSnapshot.Add(Primitive, static_cast<uint8>(Primitive->GetCollisionEnabled()));
		}
		LifeCycleSelfTestPhase = 1;
		LifeCycleSelfTestPhaseTime = Now;
		UE_LOG(LogTemp, Log, TEXT("[LifeCycleSelfTest] PASS NetworkFixture Players=%d RemoteConnections=%d Time=%.3f; three seconds for initial client presentation"),
			ReadyPlayers.Num(), RemoteCount, Now);
		if (CVarPlayerLifeCycleSelfTest.GetValueOnGameThread() == 2)
		{
			const ARogueGameState* State = World->GetGameState<ARogueGameState>();
			if (IsGameOver() || State == nullptr || State->IsGameOver())
			{
				Fail(TEXT("Restart retained the previous GameOver state"));
			}
			else
			{
				UE_LOG(LogTemp, Log, TEXT("[LifeCycleSelfTest] PASS Restart Players=%d RemoteConnections=%d AllAlive=1 GameOver=0"), ReadyPlayers.Num(), RemoteCount);
				LifeCycleSelfTestPhase = 7;
			}
		}
		return true;
	}

	for (int32 Index = 0; Index < LifeCycleSelfTestPlayers.Num(); ++Index)
	{
		if (!HasCorrectOwnership(LifeCycleSelfTestPlayers[Index].Get(), LifeCycleSelfTestControllers[Index].Get()))
		{
			Fail(TEXT("A participant lost the original PC/Pawn/PlayerState/ASC ownership"));
			return true;
		}
	}
	ARogueCharacter* First = LifeCycleSelfTestPlayers[0].Get();
	auto KillThroughGAS = [this](ARogueCharacter* Player)
	{
		const URogueAttributeSet* Attributes = Player->GetRogueAttributeSet();
		const float LethalDamage = FMath::Max(1000000.0f, (Attributes->GetMaxHealth() + Attributes->GetArmor()) * 1000.0f);
		return RogueAbilitySystem::ApplyDamageToTarget(this, Player, LethalDamage, RogueGameplayTags::Damage_Source_Contact)
			&& Attributes->GetHealth() <= 0.0f;
	};

	if (LifeCycleSelfTestPhase == 1)
	{
		if (IsGameOver() || !First->IsAlive())
		{
			Fail(TEXT("An unrelated death interrupted preparation"));
		}
		else if (Now - LifeCycleSelfTestPhaseTime >= 3.0)
		{
			if (!KillThroughGAS(First)) { Fail(TEXT("Initial lethal GAS damage was rejected or did not reduce health to zero")); }
			else
			{
				LifeCycleSelfTestPhase = 2;
				LifeCycleSelfTestPhaseTime = Now;
				LogLifeCyclePlayer(TEXT("GAS.Lethal.A"), First, Now);
			}
		}
		return true;
	}

	if (LifeCycleSelfTestPhase == 2)
	{
		if (First->IsDowned())
		{
			const float Remaining = First->GetReviveSecondsRemaining();
			if (IsGameOver() || !First->IsRevivePending() || !HasGhostCollision(First)
				|| Remaining < 9.0f || Remaining > 10.01f)
			{
				Fail(TEXT("Downed did not start a ten-second ghost state while teammates survived"));
				return true;
			}
			if (RogueAbilitySystem::ApplyDamageToTarget(this, First, 1.0f, RogueGameplayTags::Damage_Source_Contact))
			{
				Fail(TEXT("A downed ghost accepted damage"));
				return true;
			}
			TArray<ARogueCharacter*, TInlineAllocator<4>> Alive;
			RoguePlayerQuery::CollectAlivePlayers(World, Alive);
			if (Alive.Contains(First) || Alive.Num() != LifeCycleSelfTestPlayers.Num() - 1)
			{
				Fail(TEXT("Alive target queries included the ghost or lost its surviving teammates"));
				return true;
			}
			LifeCycleSelfTestDeathTime = Now + Remaining - 10.0;
			LifeCycleSelfTestPhase = 3;
			LogLifeCyclePlayer(TEXT("PASS.Ghost"), First, Now);
			return true;
		}
		if (IsGameOver() || First->IsEliminated() || Now - LifeCycleSelfTestPhaseTime > 2.0)
		{
			Fail(TEXT("Lethal GAS damage did not enter Downed with a surviving teammate"));
		}
		return true;
	}

	if (LifeCycleSelfTestPhase == 3)
	{
		const double Elapsed = Now - LifeCycleSelfTestDeathTime;
		if (IsGameOver() || First->IsEliminated())
		{
			Fail(TEXT("The run ended during individual revival"));
			return true;
		}
		for (int32 Index = 1; Index < LifeCycleSelfTestPlayers.Num(); ++Index)
		{
			if (!LifeCycleSelfTestPlayers[Index]->IsAlive())
			{
				Fail(TEXT("The survivor died during the ten-second wait"));
				return true;
			}
		}
		if (First->IsAlive())
		{
			bool bRestored = First->GetActorEnableCollision() == bLifeCycleSelfTestActorCollision
				&& (First->GetCharacterMovement()->MovementMode == MOVE_Walking
					|| First->GetCharacterMovement()->MovementMode == MOVE_Falling); // Spawn may be above the map floor.
			for (const TPair<TWeakObjectPtr<UPrimitiveComponent>, uint8>& Snapshot : LifeCycleSelfTestCollisionSnapshot)
			{
				bRestored = bRestored && Snapshot.Key.IsValid()
					&& static_cast<uint8>(Snapshot.Key->GetCollisionEnabled()) == Snapshot.Value;
			}
			if (Elapsed < 9.99 || Elapsed > 12.0 || !bRestored || First->IsRevivePending()
				|| !FMath::IsNearlyZero(First->GetReviveSecondsRemaining()))
			{
				UE_LOG(LogTemp, Error, TEXT("[LifeCycleSelfTest] Revival diagnostics: elapsed=%.6f restored=%d movement=%d pending=%d remaining=%.6f"),
					Elapsed, bRestored, int32(First->GetCharacterMovement()->MovementMode), First->IsRevivePending(), First->GetReviveSecondsRemaining());
				for (const auto& Snapshot : LifeCycleSelfTestCollisionSnapshot)
				{
					if (Snapshot.Key.IsValid() && uint8(Snapshot.Key->GetCollisionEnabled()) != Snapshot.Value)
					{
						UE_LOG(LogTemp, Error, TEXT("[LifeCycleSelfTest] Collision mismatch %s expected=%d actual=%d"), *Snapshot.Key->GetName(), Snapshot.Value, int32(Snapshot.Key->GetCollisionEnabled()));
					}
				}
				Fail(TEXT("Revival missed the ten-second deadline or failed to restore collision/movement/countdown"));
				return true;
			}
			LifeCycleSelfTestPhase = 4;
			LifeCycleSelfTestPhaseTime = Now;
			UE_LOG(LogTemp, Log, TEXT("[LifeCycleSelfTest] PASS TenSecondRevival Time=%.3f ActualWait=%.3f ComponentsRestored=%d SamePawn=1 SameASC=1"),
				Now, Elapsed, LifeCycleSelfTestCollisionSnapshot.Num());
			LogLifeCyclePlayer(TEXT("PASS.Revived.A"), First, Now);
		}
		else if (!First->IsDowned() || !HasGhostCollision(First) || Elapsed > 12.0)
		{
			Fail(TEXT("Ghost collision changed during the wait or revival did not happen"));
		}
		return true;
	}

	if (LifeCycleSelfTestPhase == 4)
	{
		// 留出复制/UI 收起的时间，也让正常的 1.5 秒复活无敌自然结束。
		if (Now - LifeCycleSelfTestPhaseTime < 3.0) { return true; }
		for (const TWeakObjectPtr<ARogueCharacter>& Player : LifeCycleSelfTestPlayers)
		{
			if (!Player->IsAlive() || !KillThroughGAS(Player.Get()))
			{
				Fail(TEXT("Team-wipe lethal GAS damage was rejected or a participant was already downed"));
				return true;
			}
			LogLifeCyclePlayer(TEXT("GAS.Lethal.Team"), Player.Get(), Now);
		}
		LifeCycleSelfTestPhase = 5;
		LifeCycleSelfTestPhaseTime = Now;
		return true;
	}

	if (LifeCycleSelfTestPhase == 5 || LifeCycleSelfTestPhase == 6)
	{
		const ARogueGameState* State = World->GetGameState<ARogueGameState>();
		bool bFinal = IsGameOver() && State != nullptr && State->IsGameOver();
		for (const TWeakObjectPtr<ARogueCharacter>& Player : LifeCycleSelfTestPlayers)
		{
			bFinal = bFinal && Player->IsEliminated() && !Player->IsRevivePending()
				&& FMath::IsNearlyZero(Player->GetReviveSecondsRemaining()) && HasGhostCollision(Player.Get());
		}
		if (LifeCycleSelfTestPhase == 5)
		{
			if (!bFinal)
			{
				if (Now - LifeCycleSelfTestPhaseTime > 2.0) { Fail(TEXT("Team wipe did not finalize all players and publish game over")); }
				return true;
			}
			LifeCycleSelfTestPhase = 6;
			LifeCycleSelfTestPhaseTime = Now;
			UE_LOG(LogTemp, Log, TEXT("[LifeCycleSelfTest] PASS TeamWipe Time=%.3f Players=%d; observing the cancelled revival window for 12.5 seconds"),
				Now, LifeCycleSelfTestPlayers.Num());
			for (const TWeakObjectPtr<ARogueCharacter>& Player : LifeCycleSelfTestPlayers)
			{
				LogLifeCyclePlayer(TEXT("PASS.Eliminated"), Player.Get(), Now);
			}
			return true;
		}
		if (!bFinal)
		{
			Fail(TEXT("A player revived or a countdown/collision reappeared after team wipe"));
			return true;
		}
		if (Now - LifeCycleSelfTestPhaseTime >= 12.5)
		{
			LifeCycleSelfTestPhase = 7;
			UE_LOG(LogTemp, Log, TEXT("[LifeCycleSelfTest] PASS Final Time=%.3f Duration=%.3f PostWipeWait=%.3f Players=%d GameOver=1 AllEliminated=1 AllCountdowns=0"),
				Now, Now - LifeCycleSelfTestStartTime, Now - LifeCycleSelfTestPhaseTime, LifeCycleSelfTestPlayers.Num());
			CVarPlayerLifeCycleSelfTest->SetWithCurrentPriority(2);
			UE_LOG(LogTemp, Log, TEXT("[LifeCycleSelfTest] Restarting through the real server restart request"));
			HandleRestartRequest(First->GetPlayerState<ARoguePlayerState>());
		}
	}
	return true;
}
