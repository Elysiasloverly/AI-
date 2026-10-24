#include "Progression/Meta/RogueProgressionSubsystem.h"
#include "Progression/Meta/RogueProgressionSaveGame.h"
#include "Progression/Meta/RogueUpgradeTreeDefinition.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif

namespace RogueProgressionStorage
{
	bool SafeSlot(const FString& Slot)
	{
		if (Slot.IsEmpty() || Slot.Len() > 96) { return false; }
		for (TCHAR C : Slot)
		{
			if (!((C >= 'a' && C <= 'z') || (C >= 'A' && C <= 'Z') || (C >= '0' && C <= '9') || C == '_' || C == '-')) { return false; }
		}
		return true;
	}

	FString Digest(const URogueProgressionSaveGame* Profile)
	{
		FString Canonical = FString::Printf(TEXT("%d|%d|"), Profile->Version, Profile->IceBricks);
		TArray<FName> Keys;
		Profile->Progression.NodeRanks.GetKeys(Keys);
		Keys.Sort(FNameLexicalLess());
		for (FName Key : Keys)
		{
			const FString Name = Key.ToString();
			Canonical += FString::Printf(TEXT("%d:%s:%d|"), Name.Len(), *Name, Profile->Progression.NodeRanks[Key]);
		}
		for (const FGuid& Receipt : Profile->AppliedBossRewards) { Canonical += Receipt.ToString(EGuidFormats::Digits); }
		FTCHARToUTF8 Bytes(*Canonical);
		FSHAHash Hash;
		FSHA1::HashBuffer(Bytes.Get(), Bytes.Length(), Hash.Hash);
		return Hash.ToString();
	}

	bool Valid(const URogueProgressionSaveGame* Profile)
	{
		return Profile && Profile->Version >= 0 && Profile->Version <= URogueProgressionSaveGame::CurrentVersion
			&& Profile->IceBricks >= 0 && (Profile->IntegrityDigest.IsEmpty() || Profile->IntegrityDigest == Digest(Profile));
	}

#if PLATFORM_WINDOWS
	FString Filename(const FString& Slot)
	{
		// UE 5.5 desktop GenericSaveGameSystem also ignores UserIndex and uses this path.
		return FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("SaveGames") / (Slot + TEXT(".sav")));
	}

	bool AtomicWrite(const FString& Target, const TArray<uint8>& Bytes)
	{
		const FString Temporary = Target + TEXT(".tmp");
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(Target), true);
		TUniquePtr<FArchive> Writer(IFileManager::Get().CreateFileWriter(*Temporary));
		if (!Writer) { return false; }
		Writer->Serialize(const_cast<uint8*>(Bytes.GetData()), Bytes.Num());
		Writer->Flush();
		const bool bWritten = !Writer->IsError() && Writer->Close();
		Writer.Reset();
		TArray<uint8> Verified;
		const bool bVerified = bWritten && FFileHelper::LoadFileToArray(Verified, *Temporary) && Verified == Bytes;
		// Same-directory replacement is atomic on Windows. Unlike IFileManager::Move,
		// MoveFileEx does not delete the existing destination before replacing it.
		const bool bCommitted = bVerified && !!::MoveFileExW(*Temporary, *Target, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
		if (!bCommitted) { IFileManager::Get().Delete(*Temporary, false, false, true); }
		return bCommitted;
	}
#endif
}

URogueProgressionSaveGame* URogueProgressionSubsystem::LoadProfileFromSlot(const FString& SlotName, int32 UserIndex,
	const URogueUpgradeTreeDefinition* Definition, bool& bOutWritable, FText& OutError)
{
	using namespace RogueProgressionStorage;
	bOutWritable = SafeSlot(SlotName) && UserIndex >= 0;
	OutError = FText::GetEmpty();
	URogueProgressionSaveGame* Loaded = nullptr;
	if (bOutWritable && UGameplayStatics::DoesSaveGameExist(SlotName, UserIndex))
	{
		Loaded = Cast<URogueProgressionSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, UserIndex));
		// Never downgrade a future schema by silently restoring an older backup.
		if (Loaded && Loaded->Version > URogueProgressionSaveGame::CurrentVersion) { bOutWritable = false; }
		else if (!Valid(Loaded)) { Loaded = nullptr; }
	}
	if (bOutWritable && !Loaded && UGameplayStatics::DoesSaveGameExist(SlotName + TEXT("_backup"), UserIndex))
	{
		Loaded = Cast<URogueProgressionSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName + TEXT("_backup"), UserIndex));
		if (!Valid(Loaded)) { Loaded = nullptr; bOutWritable = false; }
		else { OutError = FText::FromString(TEXT("已从上一份有效备份恢复升级树；损坏的原文件尚未覆盖。")); }
	}
	if (bOutWritable && !Loaded && UGameplayStatics::DoesSaveGameExist(SlotName, UserIndex)) { bOutWritable = false; }
	if (!bOutWritable)
	{
		OutError = FText::FromString(TEXT("升级树存档无法读取，或来自更新的版本。原存档已保留，暂时禁止购买和奖励写入。"));
		return NewObject<URogueProgressionSaveGame>();
	}
	if (!Loaded) { Loaded = NewObject<URogueProgressionSaveGame>(); }
	Loaded->UpgradeLegacyUnlocks(Definition);
	if (Definition) { Loaded->Progression = Definition->SanitizeSnapshot(Loaded->Progression); }
	Loaded->AppliedBossRewards.RemoveAll([](const FGuid& Id) { return !Id.IsValid(); });
	// Digest belongs to the serialized data. A migrated/sanitized candidate gets a new one on commit.
	Loaded->IntegrityDigest.Reset();
	return Loaded;
}

bool URogueProgressionSubsystem::SaveProfileToSlot(const URogueProgressionSaveGame* ProfileToSave, const FString& SlotName, int32 UserIndex)
{
	using namespace RogueProgressionStorage;
	if (!ProfileToSave || !SafeSlot(SlotName) || UserIndex < 0 || ProfileToSave->IceBricks < 0
		|| ProfileToSave->Version != URogueProgressionSaveGame::CurrentVersion) { return false; }
	URogueProgressionSaveGame* Existing = Cast<URogueProgressionSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, UserIndex));
	if (Existing && Existing->Version > URogueProgressionSaveGame::CurrentVersion) { return false; }
	URogueProgressionSaveGame* Copy = DuplicateObject<URogueProgressionSaveGame>(ProfileToSave, GetTransientPackage());
	Copy->IntegrityDigest = Digest(Copy);
	TArray<uint8> Bytes;
	if (!UGameplayStatics::SaveGameToMemory(Copy, Bytes)) { return false; }
	if (!Valid(Cast<URogueProgressionSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes)))) { return false; }
#if PLATFORM_WINDOWS
	if (Valid(Existing))
	{
		TArray<uint8> OldBytes;
		if (!UGameplayStatics::LoadDataFromSlot(OldBytes, SlotName, UserIndex)
			|| !AtomicWrite(Filename(SlotName + TEXT("_backup")), OldBytes)) { return false; }
	}
	return AtomicWrite(Filename(SlotName), Bytes);
#else
	// Non-desktop save providers need their platform transaction implementation.
	// Keep a recoverable backup here; do not claim their main-slot write is atomic.
	if (Valid(Existing) && !UGameplayStatics::SaveGameToSlot(Existing, SlotName + TEXT("_backup"), UserIndex)) { return false; }
	return UGameplayStatics::SaveDataToSlot(Bytes, SlotName, UserIndex);
#endif
}
