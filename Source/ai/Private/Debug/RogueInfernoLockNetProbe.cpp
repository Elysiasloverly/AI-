#if WITH_DEV_AUTOMATION_TESTS
#include "RogueProbeSupport.h"
#include "AbilitySystem/RogueAbilitySystemComponent.h"
#include "Combat/Targeting/RogueEnemyTrackerSubsystem.h"
#include "Combat/Visuals/RogueInfernoLockRing.h"
#include "Combat/Visuals/RogueInfernoLockSubsystem.h"
#include "Combat/Visuals/RogueLaserBeam.h"
#include "Combat/Weapons/RogueWeapon_HellTower.h"
#include "Containers/Ticker.h"
#include "Enemies/RogueEnemy.h"
#include "Enemies/Spawning/RogueSpawnSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/RogueGameMode.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "HAL/IConsoleManager.h"
#include "Player/RogueCharacter.h"
#include "Player/RoguePlayerState.h"

// Explicit disposable listen-server/client check. Never runs without the command.
namespace RogueInfernoLockNet
{
struct FProbe
{
	double Started = FPlatformTime::Seconds();
	double Ready = 0;
	double LastStep = 0;
	double SawTwoAt = 0;
	bool bQuiet = false;
	bool bReleased = false;
	TWeakObjectPtr<ARogueWeapon_HellTower> Tower;
	TArray<TWeakObjectPtr<ARogueEnemy>> Targets;

    bool End(bool bSuccess, const TCHAR* Detail)
    {
        return RogueProbeSupport::Finish(TEXT("InfernoLockNet"), bSuccess, Detail);
    }

	bool Tick(float Delta)
	{
		const double Now = FPlatformTime::Seconds();
		if (Now - Started > 60)
		{
			return End(false, TEXT("timeout"));
		}
		UWorld* W = RogueProbeSupport::FindGameWorld();
        if (!W)
		{
			return true;
		}
		auto* Rings = W->GetSubsystem<URogueInfernoLockSubsystem>();
		if (!Rings)
		{
			return true;
		}
		if (W->GetNetMode() == NM_Client)
		{
			auto* PC = W->GetFirstPlayerController();
			if (auto* Pawn = PC ? Cast<ARogueCharacter>(PC->GetPawn()) : nullptr)
			{
				if (auto* ASC = Pawn->GetRogueAbilitySystemComponent())
				{
					ASC->CancelAllAbilities();
				}
			}
			const int32 Count = Rings->GetActiveRingCount();
			if (Count == 2 && !SawTwoAt)
			{
				SawTwoAt = Now;
				UE_LOG(LogTemp, Display, TEXT("[InfernoLockNet] CLIENT saw two distinct authoritative target circles"));
			}
			if (SawTwoAt && Count > 2)
			{
				return End(false, TEXT("duplicate target circles"));
			}
			if (SawTwoAt && Now - SawTwoAt > 2 && Count == 0)
			{
				return End(true, TEXT("CLIENT received both target locks, then both releases"));
			}
			return true;
		}
		auto* Mode = W->GetAuthGameMode<ARogueGameMode>();
		if (!Mode)
		{
			return true;
		}
		Mode->SetActorTickEnabled(false);
		if (auto* Spawns = W->GetSubsystem<URogueSpawnSubsystem>())
		{
			Spawns->SetSpawningSuspended(true);
		}
		if (!bQuiet)
		{
			for (TActorIterator<ARogueEnemy> It(W); It; ++It)
			{
				It->DeactivateToPool();
			}
			bQuiet = true;
		}
		if (!Ready)
		{
			TArray<ARogueCharacter*> Players;
			ARogueCharacter* Remote = nullptr;
			for (auto It = W->GetPlayerControllerIterator(); It; ++It)
			{
				auto* PC = It->Get();
				auto* P = PC ? Cast<ARogueCharacter>(PC->GetPawn()) : nullptr;
				auto* PS = P ? P->GetPlayerState<ARoguePlayerState>() : nullptr;
				if (PS && PS->IsBaseAttributesInitialized())
				{
					Players.Add(P);
					if (!PC->IsLocalController())
					{
						Remote = P;
					}
				}
			}
			if (Players.Num() < 2 || !Remote)
			{
				return true;
			}
			for (auto* P : Players)
			{
				P->SetActorTickEnabled(false);
				if (auto* ASC = P->GetRogueAbilitySystemComponent())
				{
					ASC->CancelAllAbilities();
				}
				for (const auto& Weapon : P->GetWeapons())
				{
					if (Weapon)
					{
						Weapon->OnOwnerDied();
						Weapon->SetActorTickEnabled(false);
						Weapon->GetConfig().Count = 0;
					}
				}
			}
			// Use the joining player's weapon: its own client skips remote beam replay,
			// but must still receive lock metadata and draw its target circles.
			Tower = Remote->FindWeapon<ARogueWeapon_HellTower>();
			if (!Tower.IsValid())
			{
				return End(false, TEXT("missing registered weapon"));
			}
			Tower->GetConfig().Count = 2;
			Tower->GetConfig().Range = 3000;
			Tower->GetConfig().MaxCount = 2;
			Tower->GetConfig().HellTowerMinDamage = 0;
			Tower->GetConfig().BaseDamage = 0;
			Tower->GetConfig().BaseDamageBonusScale = 0;
			Tower->GetConfig().DamageRampPerTick = 0;
			Tower->GetConfig().DamageRampBonusScale = 0;
			for (int32 I = 0; I < 2; ++I)
			{
				auto* E = W->SpawnActor<ARogueEnemy>();
				E->ActivatePooledEnemy(nullptr, Remote->GetActorLocation() + FVector(450, I * 260 - 130, 0),
									   FRotator::ZeroRotator);
				FRogueEnemyProfile Profile;
				Profile.Health = 1000000;
				Profile.Speed = Profile.Damage = 0;
				E->InitializeEnemy(Profile);
				E->SetActorTickEnabled(false);
				E->GetCharacterMovement()->SetComponentTickEnabled(false);
				E->ForceNetUpdate();
				W->GetSubsystem<URogueEnemyTrackerSubsystem>()->RegisterEnemy(E);
				Targets.Add(E);
			}
			Ready = Now;
			LastStep = Now;
			UE_LOG(LogTemp, Display, TEXT("[InfernoLockNet] SERVER two targets activated"));
		}
		if (!bReleased && Now - Ready < 7)
		{
			// Core ticker callbacks receive the engine frame delta, not the callback
			// interval. Account for elapsed time between our sampled callbacks.
			Tower->WeaponTick(FMath::Min(.1f, float(Now - LastStep)));
			LastStep = Now;
			if (auto* P = Cast<ARogueCharacter>(Tower->GetOwner())) { P->FlushWeaponCosmetics(); }
			if (Now - Ready > 1)
			{
				if (Rings->GetActiveRingCount() != 2)
				{
					UE_LOG(LogTemp, Error, TEXT("[InfernoLockNet] count=%d weaponCount=%d beam=%s"),
						   Rings->GetActiveRingCount(), Tower->GetEffectiveCount(),
						   *GetNameSafe(Tower->GetConfig().BeamClass));
					UE_LOG(LogTemp, Error, TEXT("[InfernoLockNet] ringClass=%s"),
						   *GetNameSafe(Tower->GetConfig().BeamClass.GetDefaultObject()->InfernoLockRingClass.Get()));
					for (auto E : Targets)
					{
						UE_LOG(LogTemp, Error, TEXT("[InfernoLockNet] target=%s health=%.1f dead=%d pool=%d loc=%s"),
							   *GetNameSafe(E.Get()), E->GetCurrentHealth(), E->IsDead(), E->IsAvailableInPool(),
							   *E->GetActorLocation().ToString());
					}
					auto* Owner = Cast<ARogueCharacter>(Tower->GetOwner());
					TArray<ARogueEnemy*> Candidates;
					W->GetSubsystem<URogueEnemyTrackerSubsystem>()->CollectEnemiesInRange(Owner->GetActorLocation(), Tower->GetEffectiveRange(), Candidates, 0, true);
					UE_LOG(LogTemp, Error, TEXT("[InfernoLockNet] candidates=%d ownerAlive=%d range=%.1f step=%.3f time=%.3f paused=%d"), Candidates.Num(), Owner->IsAlive(), Tower->GetEffectiveRange(), Tower->GetConfig().UpdateInterval, W->GetTimeSeconds(), W->IsPaused());
					for(TActorIterator<ARogueInfernoLockRing> It(W);It;++It) { UE_LOG(LogTemp, Error, TEXT("[InfernoLockNet] actor material=%s available=%d"), *GetNameSafe(It->RingMaterial), It->IsAvailable()); }
					Rings->Tick(.05f);
					UE_LOG(LogTemp, Error, TEXT("[InfernoLockNet] after forced tick count=%d"), Rings->GetActiveRingCount());
					return End(false, TEXT("server did not draw exactly two circles"));
				}
				for (auto E : Targets)
				{
					if (Rings->GetLockProgress(E.Get()) <= 0)
					{
						return End(false, TEXT("lock did not shrink"));
					}
				}
			}
		}
		else if (!bReleased)
		{
			Tower->OnOwnerDied();
			if (auto* P = Cast<ARogueCharacter>(Tower->GetOwner())) { P->FlushWeaponCosmetics(); }
			bReleased = true;
			if (Rings->GetActiveRingCount() != 0)
			{
				return End(false, TEXT("server release left circles"));
			}
			UE_LOG(LogTemp, Display, TEXT("[InfernoLockNet] SERVER released both locks"));
		}
		if (Now - Ready > 10)
		{
			return End(true, TEXT("HOST owner-client weapon locks and releases are correct"));
		}
		return true;
	}
};
} // namespace RogueInfernoLockNet
static FAutoConsoleCommand InfernoLockNetProbe(
	TEXT("Rogue.Test.InfernoLockNet"), TEXT("Isolated two-process target-circle check; exits when complete."),
	FConsoleCommandDelegate::CreateLambda([] {
		auto Probe = MakeShared<RogueInfernoLockNet::FProbe>();
		FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateLambda([Probe](float Delta) { return Probe->Tick(Delta); }), .02f);
	}));
#endif
