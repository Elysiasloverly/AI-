#include "Engine/PackageMapClient.h"
#include "GameFramework/Actor.h"
#include "Shared/RogueTypes.h"

bool FRogueBeamLockCosmetic::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	uint8 Present = Target != nullptr;
	Ar.SerializeBits(&Present, 1);
	if (!Present)
	{
		if (Ar.IsLoading())
		{
			Target = nullptr;
			Generation = 0;
			LockedSeconds = 0;
		}
		bOutSuccess = !Ar.IsError();
		return true;
	}
	if (!Map)
	{
		bOutSuccess = false;
		return false;
	}
	UObject* Object = Target.Get();
	const bool Mapped = Map->SerializeObject(Ar, AActor::StaticClass(), Object);
	if (Ar.IsLoading())
		Target = Cast<AActor>(Object);
	Ar.SerializeIntPacked(Generation);
	uint16 Age =
		LockedSeconds < 0 ? MAX_uint16 : uint16(FMath::Clamp(FMath::RoundToInt(LockedSeconds * 100), 0, 65534));
	Ar << Age;
	if (Ar.IsLoading())
		LockedSeconds = Age == MAX_uint16 ? -1.f : float(Age) * .01f;
	bOutSuccess = !Ar.IsError();
	return Mapped;
}
