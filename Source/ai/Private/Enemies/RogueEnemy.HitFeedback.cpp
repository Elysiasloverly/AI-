#include "Enemies/RogueEnemy.h"
#include "Combat/Visuals/RogueHitFlashComponent.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"

void ARogueEnemy::OnEnemyDamaged_Implementation(float ActualDamage)
{
	if (HitFlashComponent)
	{
		HitFlashComponent->FlashMaterial = HitFlashMaterial;
		HitFlashComponent->PlayHitFlash();
	}
}

void ARogueEnemy::RecordHitFeedback(float ActualDamage)
{
	if (!HasAuthority() || bPoolAvailable || bDead || ActualDamage <= 0.0f || !FMath::IsFinite(ActualDamage)) { return; }
	++HitFeedback.Sequence;
	HitFeedback.TotalDamage += ActualDamage;
	HitFeedback.LatestDamage = ActualDamage;
	HitFeedback.ServerTime = GetWorld()->GetTimeSeconds();
	// Existing replication budget coalesces rapid hits; do not force a packet/RPC for every pellet.
	if (GetNetMode() != NM_DedicatedServer) { OnEnemyDamaged(ActualDamage); }
}

void ARogueEnemy::OnRep_HitFeedback()
{
	ConsumeReplicatedHitFeedback();
}

void ARogueEnemy::ConsumeReplicatedHitFeedback()
{
	const bool bNewGeneration = ObservedHitGeneration != HitFeedback.Generation;
	if (bNewGeneration || !HitFeedback.bActive)
	{
		if (HitFlashComponent) { HitFlashComponent->ClearHitFlash(); }
	}
	if (!HitFeedback.bActive || HitFeedback.Sequence == 0)
	{
		ObservedHitGeneration = HitFeedback.Generation;
		ObservedHitSequence = HitFeedback.Sequence;
		ObservedHitTotalDamage = HitFeedback.TotalDamage;
		return;
	}
	// Actor activation and this snapshot may be notified in either order. Consume after activation.
	if (!bReplicatedActive || bPoolAvailable || bDead) { return; }
	if (!bNewGeneration && HitFeedback.Sequence == ObservedHitSequence) { return; }
	const float Damage = bNewGeneration ? HitFeedback.LatestDamage : FMath::Max(0.0f, HitFeedback.TotalDamage - ObservedHitTotalDamage);
	ObservedHitGeneration = HitFeedback.Generation;
	ObservedHitSequence = HitFeedback.Sequence;
	ObservedHitTotalDamage = HitFeedback.TotalDamage;
	const UWorld* World = GetWorld();
	const AGameStateBase* State = World ? World->GetGameState() : nullptr;
	const float Now = State ? State->GetServerWorldTimeSeconds() : (World ? World->GetTimeSeconds() : 0.0f);
	// Do not flash old damage when an already-injured enemy first becomes relevant.
	if (Now - HitFeedback.ServerTime > 0.5f || Now - HitFeedback.ServerTime < -1.0f) { return; }
	if (Damage > 0.0f && GetNetMode() != NM_DedicatedServer) { OnEnemyDamaged(Damage); }
}
