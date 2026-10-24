#include "Progression/Meta/RogueProgressionSubsystem.h"

#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Progression/Meta/RogueProgressionSaveGame.h"
#include "Progression/Meta/RogueUpgradeTreeDefinition.h"

DEFINE_LOG_CATEGORY_STATIC(LogRogueProgression, Log, All);

URogueProgressionSubsystem* URogueProgressionSubsystem::GetForPlayer(APlayerController* PlayerController)
{
	ULocalPlayer* LocalPlayer = PlayerController ? PlayerController->GetLocalPlayer() : nullptr;
	return LocalPlayer ? LocalPlayer->GetSubsystem<URogueProgressionSubsystem>() : nullptr;
}

void URogueProgressionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	// Split-screen local players are independent. Remote players have their own machine's slot.
	const int32 LocalPlayerIndex = GetLocalPlayer() ? FMath::Max(0, GetLocalPlayer()->GetLocalPlayerIndex()) : 0;
	if (!ResolveSaveLocation(LocalPlayerIndex, SaveSlot, SaveUserIndex, SaveError))
	{
		Profile = NewObject<URogueProgressionSaveGame>(this);
		bSaveWritable = false;
		UE_LOG(LogRogueProgression, Warning, TEXT("Rejected unsafe progression slot override; profile is read-only."));
		return;
	}
	UE_LOG(LogRogueProgression, Log, TEXT("Local progression slot: %s (user %d), path: %s"), *SaveSlot, SaveUserIndex,
		*FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("SaveGames") / (SaveSlot + TEXT(".sav"))));
	Profile = LoadProfileFromSlot(SaveSlot, SaveUserIndex, GetDefinition(), bSaveWritable, SaveError);
}

FRogueProgressionSnapshot URogueProgressionSubsystem::GetSnapshot() const
{
	return Profile ? GetDefinition()->SanitizeSnapshot(Profile->Progression) : FRogueProgressionSnapshot();
}

int32 URogueProgressionSubsystem::GetIceBricks() const
{
	return Profile ? Profile->IceBricks : 0;
}

const URogueUpgradeTreeDefinition* URogueProgressionSubsystem::GetDefinition() const
{
	return URogueUpgradeTreeDefinition::GetDefaultDefinition();
}

bool URogueProgressionSubsystem::TryPurchaseNode(FName NodeId, FText& OutFailure)
{
	if (!bSaveWritable || !Profile)
	{
		OutFailure = SaveError.IsEmpty() ? FText::FromString(TEXT("当前存档无法写入。")) : SaveError;
		return false;
	}
	URogueProgressionSaveGame* Candidate = DuplicateObject<URogueProgressionSaveGame>(Profile, this);
	if (!Candidate->TryPurchase(GetDefinition(), NodeId, OutFailure)) { return false; }
	if (!CommitCandidate(Candidate))
	{
		OutFailure = SaveError;
		return false;
	}
	return true;
}

bool URogueProgressionSubsystem::ApplyBossReward(const FGuid& RewardId)
{
	if (!bSaveWritable || !Profile || !RewardId.IsValid()) { return false; }
	if (Profile->AppliedBossRewards.Contains(RewardId)) { return true; }
	URogueProgressionSaveGame* Candidate = DuplicateObject<URogueProgressionSaveGame>(Profile, this);
	return Candidate->AddBossReward(RewardId) && CommitCandidate(Candidate);
}

bool URogueProgressionSubsystem::CommitCandidate(URogueProgressionSaveGame* Candidate)
{
	const double Started = FPlatformTime::Seconds();
	const bool bSaved = Candidate && SaveProfileToSlot(Candidate, SaveSlot, SaveUserIndex);
	LastSaveMilliseconds = float((FPlatformTime::Seconds() - Started) * 1000.0);
	if (!bSaved)
	{
		SaveError = FText::FromString(TEXT("存档写入失败，本次冰砖及解锁变更未提交。请检查磁盘空间和存档目录权限。"));
		UE_LOG(LogRogueProgression, Warning, TEXT("Progression save failed: %s"), *SaveSlot);
		return false;
	}
	Profile = Candidate;
	SaveError = FText::GetEmpty();
	OnProgressionChanged.Broadcast();
	return true;
}

bool URogueProgressionSubsystem::ResolveSaveLocation(int32 LocalPlayerIndex, FString& OutSlot, int32& OutUserIndex, FText& OutError)
{
	OutError = FText::GetEmpty();
	OutUserIndex = 0;
	OutSlot = FString::Printf(TEXT("Baka_UpgradeTree_Player%d"), LocalPlayerIndex);
	FString OverrideSlot;
	if (FParse::Value(FCommandLine::Get(), TEXT("RogueProgressionSlot="), OverrideSlot))
	{
		bool bSafeSlot = !OverrideSlot.IsEmpty() && OverrideSlot.Len() <= 96;
		for (const TCHAR Character : OverrideSlot)
		{
			bSafeSlot &= (Character >= 'a' && Character <= 'z') || (Character >= 'A' && Character <= 'Z')
				|| (Character >= '0' && Character <= '9') || Character == '_' || Character == '-';
		}
		if (bSafeSlot) { OutSlot = OverrideSlot; }
		else
		{
			OutError = FText::FromString(TEXT("测试存档槽名无效，仅允许英文字母、数字、下划线和短横线。"));
			OutSlot.Reset();
			return false;
		}
	}
	int32 OverrideUserIndex = 0;
	if (FParse::Value(FCommandLine::Get(), TEXT("RogueProgressionUserIndex="), OverrideUserIndex))
	{
		OutUserIndex = FMath::Max(0, OverrideUserIndex);
	}
	return true;
}

#if WITH_EDITOR
bool URogueProgressionSubsystem::ResetProgressionForDebug(FText& OutError)
{
	OutError = FText::GetEmpty();
	if (!Profile || !bSaveWritable)
	{
		OutError = SaveError.IsEmpty() ? FText::FromString(TEXT("玩家存档尚未就绪或不可写。")) : SaveError;
		return false;
	}
	URogueProgressionSaveGame* Candidate = DuplicateObject<URogueProgressionSaveGame>(Profile, this);
	Candidate->Progression.NodeRanks.Reset();
	if (!CommitCandidate(Candidate)) { OutError = SaveError; return false; }
	return true;
}

bool URogueProgressionSubsystem::ResetProgressionInSlotForDebug(const FString& Slot, int32 UserIndex,
	const URogueUpgradeTreeDefinition* Definition, FText& OutError)
{
	OutError = FText::GetEmpty();
	if (Slot.IsEmpty() || Definition == nullptr)
	{
		OutError = FText::FromString(TEXT("存档槽或升级树配置无效。"));
		return false;
	}
	bool bWritable = false;
	URogueProgressionSaveGame* Candidate = LoadProfileFromSlot(Slot, UserIndex, Definition, bWritable, OutError);
	if (!Candidate || !bWritable) return false;
	Candidate->Progression.NodeRanks.Reset();
	if (!SaveProfileToSlot(Candidate, Slot, UserIndex))
	{
		OutError = FText::FromString(TEXT("解锁进度重置失败，原存档未提交变更。"));
		return false;
	}
	return true;
}

bool URogueProgressionSubsystem::SetIceBricksForDebug(int32 NewCount, FText& OutError)
{
	OutError = FText::GetEmpty();
	if (NewCount < 0) { OutError = FText::FromString(TEXT("冰砖数量不能为负数。")); return false; }
	if (!Profile || !bSaveWritable)
	{
		OutError = SaveError.IsEmpty() ? FText::FromString(TEXT("玩家存档尚未就绪或不可写。")) : SaveError;
		return false;
	}
	URogueProgressionSaveGame* Candidate = DuplicateObject<URogueProgressionSaveGame>(Profile, this);
	Candidate->IceBricks = NewCount;
	if (!CommitCandidate(Candidate)) { OutError = SaveError; return false; }
	return true;
}

bool URogueProgressionSubsystem::SetIceBricksInSlotForDebug(const FString& Slot, int32 UserIndex,
	const URogueUpgradeTreeDefinition* Definition, int32 NewCount, FText& OutError)
{
	OutError = FText::GetEmpty();
	if (NewCount < 0 || Slot.IsEmpty() || Definition == nullptr)
	{
		OutError = FText::FromString(TEXT("目标数量、存档槽或升级树配置无效。"));
		return false;
	}
	bool bWritable = false;
	URogueProgressionSaveGame* Candidate = LoadProfileFromSlot(Slot, UserIndex, Definition, bWritable, OutError);
	if (!Candidate || !bWritable) return false;
	Candidate->IceBricks = NewCount;
	if (!SaveProfileToSlot(Candidate, Slot, UserIndex))
	{
		OutError = FText::FromString(TEXT("调试冰砖写入失败，原存档未提交变更。"));
		return false;
	}
	return true;
}
#endif

void URogueProgressionSubsystem::EnqueueBossReward(const FGuid& RewardId)
{
	if (!RewardId.IsValid() || (Profile && Profile->AppliedBossRewards.Contains(RewardId))) { return; }
	PendingBossRewards.Add(RewardId);
	FlushPendingBossRewards();
	if (!PendingBossRewards.IsEmpty() && !RewardRetryHandle.IsValid())
	{
		// Core ticker survives world travel; one serial batch commit covers all pending receipts.
		RewardRetryHandle = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateUObject(this, &URogueProgressionSubsystem::FlushPendingBossRewards), 1.0f);
	}
	OnProgressionChanged.Broadcast();
}

bool URogueProgressionSubsystem::FlushPendingBossRewards(float DeltaSeconds)
{
	if (PendingBossRewards.IsEmpty()) { FTSTicker::GetCoreTicker().RemoveTicker(RewardRetryHandle); RewardRetryHandle.Reset(); return false; }
	if (!bSaveWritable || !Profile) { return true; }
	URogueProgressionSaveGame* Candidate = DuplicateObject<URogueProgressionSaveGame>(Profile, this);
	for (const FGuid& RewardId : PendingBossRewards)
	{
		if (!Candidate->AddBossReward(RewardId)) { return true; }
	}
	if (CommitCandidate(Candidate))
	{
		PendingBossRewards.Reset();
		OnProgressionChanged.Broadcast();
		FTSTicker::GetCoreTicker().RemoveTicker(RewardRetryHandle);
		RewardRetryHandle.Reset();
		return false;
	}
	return true;
}

void URogueProgressionSubsystem::Deinitialize()
{
	FTSTicker::GetCoreTicker().RemoveTicker(RewardRetryHandle);
	RewardRetryHandle.Reset();
	FlushPendingBossRewards();
	Super::Deinitialize();
}
