#include "Combat/Visuals/RogueInfernoLockSubsystem.h"
#include "Combat/Visuals/RogueInfernoLockRing.h"
#include "Combat/Weapons/RogueWeaponBase.h"
#include "Enemies/RogueEnemy.h"
#include "Engine/World.h"
#include "Player/RogueCharacter.h"

TStatId URogueInfernoLockSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(RogueInfernoLock, STATGROUP_Tickables);
}
void URogueInfernoLockSubsystem::Touch(AActor* Source, ARogueEnemy* Target, uint32 Generation, float Age,
									   TSubclassOf<ARogueInfernoLockRing> Class, float Lease)
{
	if (!GetWorld() || GetWorld()->GetNetMode() == NM_DedicatedServer || !IsValid(Source) || !IsValid(Target) ||
		!Class || !FMath::IsFinite(Age))
		return;
	if (Age < 0)
	{
		Release(Source, Target, Generation);
		return;
	}
	if (Target->GetPresentationGeneration() > Generation)
		return;
	FEntry* Entry = Entries.Find(Target);
	if (Entry && Entry->Generation > Generation)
		return;
	if (Entry && (Entry->Generation != Generation || Entry->RingClass != Class))
	{
		Return(Entry->Ring.Get());
		Entries.Remove(Target);
		Entry = nullptr;
	}
	if (!Entry)
	{
		Entry = &Entries.Add(Target);
		Entry->Generation = Generation;
		Entry->RingClass = Class;
		Entry->UpdatedAt = GetWorld()->GetTimeSeconds();
	}
	const double Now = GetWorld()->GetTimeSeconds();
	if (const FSource* Old = Entry->Sources.Find(Source))
		Age = FMath::Max(Age, Old->Age + float(Now - Old->Seen));
	Entry->Sources.Add(Source, FSource{Age, Now, FMath::Max(.1f, Lease)});
}
void URogueInfernoLockSubsystem::Release(AActor* Source, ARogueEnemy* Target, uint32 Generation)
{
	if (auto* Entry = Entries.Find(Target); Entry && Entry->Generation == Generation)
	{
		Entry->Sources.Remove(Source);
		if (Entry->Sources.IsEmpty())
		{
			Return(Entry->Ring.Get());
			Entries.Remove(Target);
		}
	}
}
void URogueInfernoLockSubsystem::RemoveSource(AActor* Source)
{
	for (auto It = Entries.CreateIterator(); It; ++It)
	{
		It.Value().Sources.Remove(Source);
		if (It.Value().Sources.IsEmpty())
		{
			Return(It.Value().Ring.Get());
			It.RemoveCurrent();
		}
	}
}
ARogueInfernoLockRing* URogueInfernoLockSubsystem::Acquire(TSubclassOf<ARogueInfernoLockRing> Class)
{
	Pool.RemoveAll([](const auto& A) { return !IsValid(A); });
	for (ARogueInfernoLockRing* Ring : Pool)
		if (Ring->IsAvailable() && Ring->GetClass() == Class)
			return Ring;
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	auto* Ring = GetWorld()->SpawnActor<ARogueInfernoLockRing>(Class, FVector::ZeroVector, FRotator::ZeroRotator, P);
	if (Ring)
		Pool.Add(Ring);
	return Ring;
}
void URogueInfernoLockSubsystem::Return(ARogueInfernoLockRing* Ring)
{
	if (!Ring)
		return;
	Ring->DeactivateRing();
	int32 Idle = 0;
	for (ARogueInfernoLockRing* R : Pool)
		if (IsValid(R) && R->IsAvailable())
			++Idle;
	if (Idle > 24)
	{
		Pool.RemoveSingle(Ring);
		Ring->Destroy();
	}
}
void URogueInfernoLockSubsystem::Tick(float DeltaTime)
{
	Accumulator += FMath::Max(0.f, DeltaTime);
	if (Accumulator < .033f)
		return;
	Accumulator = 0;
	const double Now = GetWorld()->GetTimeSeconds();
	for (auto It = Entries.CreateIterator(); It; ++It)
	{
		auto* Target = It.Key().Get();
		auto& Entry = It.Value();
		if (!Target || Target->IsActorBeingDestroyed() || Target->GetPresentationGeneration() > Entry.Generation)
		{
			Return(Entry.Ring.Get());
			It.RemoveCurrent();
			continue;
		}
		Entry.Age += FMath::Max(0.f, float(Now - Entry.UpdatedAt));
		Entry.UpdatedAt = Now;
		for (auto S = Entry.Sources.CreateIterator(); S; ++S)
		{
			auto* Source = S.Key().Get();
			const auto* Player = Source ? Cast<ARogueCharacter>(Source->GetOwner()) : nullptr;
			if (!Source || (Player && !Player->IsAlive()) || Now - S.Value().Seen > S.Value().Lease)
			{
				S.RemoveCurrent();
				continue;
			}
			Entry.Age = FMath::Max(Entry.Age, S.Value().Age + float(Now - S.Value().Seen));
		}
		if (Entry.Sources.IsEmpty())
		{
			Return(Entry.Ring.Get());
			It.RemoveCurrent();
			continue;
		}
		// Cross-actor notifications can arrive before the new generation's activation.
		if (Target->GetPresentationGeneration() < Entry.Generation)
			continue;
		if (Target->IsDead() || Target->IsAvailableInPool())
		{
			Return(Entry.Ring.Get());
			It.RemoveCurrent();
			continue;
		}
		if (!Entry.Ring.IsValid())
			Entry.Ring = Acquire(Entry.RingClass);
		if (auto* Ring = Entry.Ring.Get())
		{
			if (Ring->IsAvailable())
				Ring->ActivateRing(Target, Entry.Age);
			else
				Ring->UpdateRing(Target, Entry.Age);
		}
	}
}
int32 URogueInfernoLockSubsystem::GetActiveRingCount() const
{
	int32 Count = 0;
	for (ARogueInfernoLockRing* Ring : Pool)
		if (IsValid(Ring) && !Ring->IsAvailable())
			++Count;
	return Count;
}
float URogueInfernoLockSubsystem::GetLockProgress(ARogueEnemy* Target) const
{
	const auto* E = Entries.Find(Target);
	if (!E || !E->Ring.IsValid() || E->Ring->IsAvailable())
		return -1;
	return FMath::Clamp(E->Age / FMath::Max(.1f, E->Ring->ShrinkSeconds), 0.f, 1.f);
}
