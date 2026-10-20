#include "Framework/RogueGameMode.h"
#include "Debug/RogueSelfTest.h"
#include "AbilitySystem/RogueGameplayTags.h"
#include "AbilitySystem/RogueAbilitySystemLibrary.h"
#include "Shared/RogueTypes.h"
#include "AbilitySystem/RogueAttributeSet.h"

#include "World/RogueArena.h"
#include "Player/RogueCharacter.h"
#include "Player/RoguePlayerState.h"
#include "Player/RoguePlayerController.h"
#include "EngineUtils.h"
#include "Enemies/RogueEnemy.h"
#include "World/RogueExperiencePickup.h"
#include "World/RogueShopTerminal.h"
#include "UI/RogueHUD.h"
#include "Framework/RogueGameBalanceAsset.h"
#include "Progression/Upgrades/RogueUpgradeDefinitionAsset.h"
#include "Progression/Upgrades/RogueUpgradeRuleAsset.h"
#include "Framework/RogueGameModeRules.h"
#include "Framework/RogueGameState.h"
#include "Player/RoguePlayerQuery.h"
#include "Progression/Upgrades/RogueUpgradeSystem.h"
#include "Combat/Projectiles/RogueMortarProjectile.h"
#include "Combat/Weapons/RogueWeaponBase.h"
#include "Combat/Projectiles/RogueEnemyProjectile.h"
#include "Engine/DataTable.h"
#include "Combat/Pooling/RogueCombatPoolSubsystem.h"
#include "Combat/Targeting/RogueEnemyTrackerSubsystem.h"
#include "Enemies/Spawning/RogueSpawnSubsystem.h"
#include "World/RogueSceneSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/PackageName.h"
#include "UObject/SoftObjectPath.h"

bool ARogueGameMode::EvaluateTeamWipe()
{
	if (RunState.bGameOver || GetWorld() == nullptr)
	{
		return RunState.bGameOver;
	}
	int32 ParticipantCount = 0;
	TArray<ARogueCharacter*, TInlineAllocator<4>> Participants;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* Controller = It->Get();
		if (!IsValid(Controller) || Controller->IsActorBeingDestroyed())
		{
			continue;
		}
		const ARoguePlayerState* State = Controller->GetPlayerState<ARoguePlayerState>();
		if (State == nullptr || State->IsOnlyASpectator())
		{
			continue;
		}
		ARogueCharacter* Character = RoguePlayerQuery::GetCharacterOf(Controller);
		// 正在登录/重新拥有 Pawn 的玩家不能被当成已濒死而误判团灭。
		if (Character == nullptr || Character->IsAlive())
		{
			return false;
		}
		++ParticipantCount;
		Participants.Add(Character);
	}
	if (ParticipantCount == 0)
	{
		return false;
	}
	RunState.bGameOver = true;
	RunState.bBossAlive = false;
	for (ARogueCharacter* Character : Participants)
	{
		Character->FinalizeForGameOver();
		if (ARoguePlayerState* State = Character->GetPlayerState<ARoguePlayerState>())
		{
			State->ResetUpgradeState();
			State->GetShopRuntime().CloseShop();
			State->ApplyShopSnapshot(false, TArray<FRogueShopOffer>(), 0, 0.0f);
			State->ForceNetUpdate();
		}
	}
	PushRunStateToGameState();
	if (ARogueGameState* State = GetGameState<ARogueGameState>())
	{
		State->ForceNetUpdate();
	}
	UE_LOG(LogTemp, Log, TEXT("[RogueLife] 全员濒死：本局结束，已取消 %d 名玩家的复活"), ParticipantCount);
	return true;
}

ARogueCharacter* ARogueGameMode::FindNearestAlivePlayer(const FVector& Location) const
{
	TArray<ARogueCharacter*, TInlineAllocator<4>> AlivePlayers;
	CollectAlivePlayers(AlivePlayers);

	ARogueCharacter* Nearest = nullptr;
	float NearestDistanceSquared = TNumericLimits<float>::Max();
	for (ARogueCharacter* Candidate : AlivePlayers)
	{
		const float DistanceSquared = FVector::DistSquared2D(Location, Candidate->GetActorLocation());
		if (DistanceSquared < NearestDistanceSquared)
		{
			NearestDistanceSquared = DistanceSquared;
			Nearest = Candidate;
		}
	}

	return Nearest;
}

ARogueCharacter* ARogueGameMode::GetSpawnReferenceCharacter() const
{
	// 随机取一个活着的玩家当刷怪参照点（为什么不固定用玩家 0，见头文件里的说明）。
	TArray<ARogueCharacter*, TInlineAllocator<4>> AlivePlayers;
	CollectAlivePlayers(AlivePlayers);

	if (AlivePlayers.Num() == 0)
	{
		// 全员阵亡（或还没生成角色）：返回 nullptr。
		// SpawnEnemy 会因 !IsValid(Character) 直接拒绝 —— 这正是想要的"团灭就别刷了"。
		return nullptr;
	}

	return AlivePlayers[FMath::RandRange(0, AlivePlayers.Num() - 1)];
}

void ARogueGameMode::NotifyPlayerDied(ARogueCharacter* DeadCharacter)
{
	if (!IsValid(DeadCharacter))
	{
		return;
	}
	if (ARoguePlayerState* State = DeadCharacter->GetPlayerState<ARoguePlayerState>())
	{
		State->ResetUpgradeState();
		State->GetShopRuntime().CloseShop();
		State->ApplyShopSnapshot(false, TArray<FRogueShopOffer>(), 0, 0.0f);
		State->ForceNetUpdate();
	}
	if (RunState.bGameOver)
	{
		DeadCharacter->FinalizeForGameOver();
		return;
	}
	EvaluateTeamWipe();
}

bool ARogueGameMode::SetPause(APlayerController* PC, FCanUnpause CanUnpauseDelegate)
{
	// 此入口返回 false；日志中的控制器名称可用于定位暂停请求来源。
	UE_LOG(LogTemp, Warning, TEXT("[Rogue] 收到暂停请求（PC=%s）：本作不支持暂停，已拒绝。若你看到这条日志，请在蓝图里搜索 Set Game Paused / SetPause 节点并删除"),
		PC != nullptr ? *PC->GetName() : TEXT("未知（可能来自控制台或关卡蓝图）"));
	return false;
}

void ARogueGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	// Pawn 已禁用 Player0 自动接管；登录结束时保留归属检查，处理外部解除控制。
	RepairPlayerPawnOwnership();
	if (RunState.bGameOver)
	{
		if (ARogueCharacter* Character = RoguePlayerQuery::GetCharacterOf(NewPlayer))
		{
			Character->FinalizeForGameOver();
		}
	}
}

void ARogueGameMode::RepairPlayerPawnOwnership()
{
	UWorld* World = GetWorld();
	if (World == nullptr)
	{
		return;
	}

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PlayerController = It->Get();
		if (PlayerController == nullptr || PlayerController->GetPawn() != nullptr)
		{
			// 已经有 Pawn 的不用管（正常情况）
			continue;
		}

		APlayerState* OwningPlayerState = PlayerController->PlayerState;

		// 优先认领控制器记录的失控 Pawn；有效且未被控制即可，不在这里再次验证 PlayerState。
		if (const ARoguePlayerController* RoguePlayerController = Cast<ARoguePlayerController>(PlayerController))
		{
			APawn* ReclaimCandidate = RoguePlayerController->GetLastUnpossessedPawn();
			if (IsValid(ReclaimCandidate) && ReclaimCandidate->GetController() == nullptr)
			{
				PlayerController->Possess(ReclaimCandidate);
				UE_LOG(LogTemp, Warning, TEXT("[Rogue] 归属校正：%s 认领回原角色 %s"),
					*PlayerController->GetName(), *ReclaimCandidate->GetName());
				continue;
			}
		}

		if (OwningPlayerState == nullptr)
		{
			continue;
		}

		// 找"本命角色"：同一份 PlayerState、且当前没有控制器
		for (TActorIterator<ARogueCharacter> CharacterIt(World); CharacterIt; ++CharacterIt)
		{
			ARogueCharacter* Candidate = *CharacterIt;
			if (Candidate == nullptr || Candidate->GetController() != nullptr)
			{
				continue;
			}

			if (Candidate->GetPlayerState() != OwningPlayerState)
			{
				continue;
			}

			PlayerController->Possess(Candidate);
			UE_LOG(LogTemp, Warning, TEXT("[Rogue] 归属校正：%s 重新接管 %s"),
				*PlayerController->GetName(), *Candidate->GetName());
			break;
		}

		// 未找到可认领 Pawn 时保持等待；这里不生成替代角色，以免遗留旧 Pawn 并拆散归属。
		if (PlayerController->GetPawn() == nullptr)
		{
			UE_LOG(LogTemp, Warning, TEXT("[Rogue] 归属校正：%s 目前没有 Pawn 且无可认领的原角色（按设计不做重生，避免制造孤儿角色）"),
				*PlayerController->GetName());
		}
	}
}

void ARogueGameMode::HandleRestartRequest(ARoguePlayerState* Requester)
{
	// 只有整局结束（全员阵亡）才允许重开。
	//
	// 为什么服务端必须挡这一下：客户端那个按钮是蓝图界面上摆着的，
	// 只靠 UI 约束等于没有约束 —— 一个已阵亡的玩家在队友还在打的时候点它，
	// 会把整局连人一起重开。
	if (!RunState.bGameOver)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Rogue] 拒绝重开请求：本局尚未结束（请求者=%s）"),
			Requester != nullptr ? *Requester->GetPlayerName() : TEXT("未知"));
		return;
	}

	// 一次性闸门：OpenLevel/ServerTravel 是很重的操作，
	// 结算界面上的按钮如果被连点（或被改过的客户端反复调 RPC），
	// 服务端会不停切图/卡在加载。第一次请求之后就不再受理。
	if (bRestartRequested)
	{
		return;
	}
	bRestartRequested = true;

	// 联机必须由服务端 ServerTravel，带所有客户端进入新局。
	// 使用当前世界的完整包路径，并剥离 PIE 的短名前缀；项目中有同名地图。
	const FString MapShortName = UGameplayStatics::GetCurrentLevelName(this, true);
	const FString MapFolder = FPackageName::GetLongPackagePath(GetWorld()->GetOutermost()->GetName());
	const FString LevelName = MapFolder / MapShortName;

	if (LevelName.IsEmpty())
	{
		bRestartRequested = false;
		UE_LOG(LogTemp, Warning, TEXT("[Rogue] 重开失败：拿不到当前关卡名"));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[Rogue] 重开关卡：%s"), *LevelName);

	// PIE（含“作为监听服务器运行”）里 ServerTravel 会先拆掉当前 PIE 世界，
	// 再依赖编辑器“重复上次 PIE 命令”把画面拉回来；某些情况下编辑器不会自动重开，
	// 表现就是点了按钮后直接停在编辑器里。PIE 统一走 OpenLevel，让编辑器自己重载关卡；
	// 只有真正打包后的联机服务器才使用 ServerTravel 带客户端一起走。
	const bool bIsPIE = GetWorld()->IsPlayInEditor();
	UE_LOG(LogTemp, Log, TEXT("[Rogue] 重开方式：%s"), bIsPIE ? TEXT("PIE/OpenLevel") : (GetNetMode() == NM_Standalone ? TEXT("Standalone/OpenLevel") : TEXT("ServerTravel")));
	if (GetNetMode() == NM_Standalone || bIsPIE)
	{
		UGameplayStatics::OpenLevel(this, FName(*LevelName));
	}
	else if (!GetWorld()->ServerTravel(LevelName + (GetNetMode() == NM_ListenServer ? TEXT("?listen") : TEXT(""))))
	{
		bRestartRequested = false;
		UE_LOG(LogTemp, Error, TEXT("[RogueLife] 联机重开失败：%s"), *LevelName);
	}
}