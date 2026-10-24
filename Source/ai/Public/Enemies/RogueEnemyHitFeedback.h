#pragma once

#include "CoreMinimal.h"
#include "RogueEnemyHitFeedback.generated.h"

/** One coherent presentation snapshot per replication update, not a reliable event for each hit. */
USTRUCT()
struct FRogueEnemyHitFeedback
{
	GENERATED_BODY()
	UPROPERTY() uint32 Generation = 0;
	UPROPERTY() uint32 Sequence = 0;
	UPROPERTY() float TotalDamage = 0.0f;
	UPROPERTY() float LatestDamage = 0.0f;
	UPROPERTY() float ServerTime = -1000.0f;
	UPROPERTY() bool bActive = false;

	bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
	{
		Ar << Generation << Sequence << TotalDamage << LatestDamage << ServerTime;
		uint8 Active = bActive ? 1 : 0;
		Ar.SerializeBits(&Active, 1);
		if (Ar.IsLoading()) { bActive = Active != 0; }
		bOutSuccess = !Ar.IsError() && FMath::IsFinite(TotalDamage) && FMath::IsFinite(LatestDamage) && FMath::IsFinite(ServerTime);
		return bOutSuccess;
	}
	bool operator==(const FRogueEnemyHitFeedback& Other) const
	{
		return Generation == Other.Generation && Sequence == Other.Sequence && TotalDamage == Other.TotalDamage
			&& LatestDamage == Other.LatestDamage && ServerTime == Other.ServerTime && bActive == Other.bActive;
	}
};

template<> struct TStructOpsTypeTraits<FRogueEnemyHitFeedback> : public TStructOpsTypeTraitsBase2<FRogueEnemyHitFeedback>
{
	enum { WithNetSerializer = true, WithIdenticalViaEquality = true };
};
