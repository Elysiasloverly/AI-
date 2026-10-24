#include "Progression/Meta/RogueProgressionTypes.h"

bool FRogueProgressionSnapshot::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	constexpr int32 MaximumEntries = 128;
	bOutSuccess = false;
	if (Ar.IsSaving() && NodeRanks.Num() > MaximumEntries)
	{
		Ar.SetError();
		return false;
	}
	uint8 Count = Ar.IsSaving() ? static_cast<uint8>(NodeRanks.Num()) : 0;
	Ar.SerializeBits(&Count, 8);
	if (Ar.IsError() || Count > MaximumEntries)
	{
		Ar.SetError();
		if (Ar.IsLoading()) { NodeRanks.Reset(); }
		return false;
	}
	TArray<FName> Keys;
	if (Ar.IsSaving())
	{
		NodeRanks.GetKeys(Keys);
		Keys.Sort([](const FName A, const FName B) { return A.LexicalLess(B); });
	}
	else
	{
		NodeRanks.Reset();
	}
	for (int32 Index = 0; Index < Count; ++Index)
	{
		FName Name = Ar.IsSaving() ? Keys[Index] : NAME_None;
		int32 RankValue = Ar.IsSaving() ? NodeRanks.FindChecked(Name) : 0;
		if (Ar.IsSaving() && (RankValue < 0 || RankValue > 5 || Name.IsNone()))
		{
			Ar.SetError();
			return false;
		}
		Ar << Name;
		uint8 Rank = static_cast<uint8>(RankValue);
		Ar.SerializeBits(&Rank, 3);
		if (Ar.IsError() || Rank > 5 || Name.IsNone() || (Ar.IsLoading() && NodeRanks.Contains(Name)))
		{
			Ar.SetError();
			if (Ar.IsLoading()) { NodeRanks.Reset(); }
			return false;
		}
		if (Ar.IsLoading()) { NodeRanks.Add(Name, Rank); }
	}
	bOutSuccess = !Ar.IsError();
	return bOutSuccess;
}
