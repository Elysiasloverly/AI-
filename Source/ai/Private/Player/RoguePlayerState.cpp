#include "Player/RoguePlayerState.h"

#include "AbilitySystem/RogueAbilitySystemComponent.h"
#include "AbilitySystem/RogueAbilitySet.h"
#include "AbilitySystem/GameplayEffects/RogueStateEffects.h"
#include "AbilitySystem/RogueAttributeSet.h"
#include "AbilitySystem/Cues/RogueGameplayCue_Impact.h"
#include "Framework/RogueGameMode.h"
#include "GameFramework/GameStateBase.h"
#include "Player/RogueCharacter.h"
#include "Net/UnrealNetwork.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "Progression/Meta/RogueProgressionSubsystem.h"
#include "Progression/Meta/RogueUpgradeTreeDefinition.h"
#include "Progression/Upgrades/RogueUpgradeSystemRules.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogRoguePlayerState, Log, All);

namespace
{
	/** 升级选项复制回调为空实现 —— 目前客户端只做"读"（HUD 每帧轮询）。
	 *  保留 ReplicatedUsing 是为了将来要做"弹卡动画/音效"时有个确定的挂点。 */
}

void ARoguePlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// 只复制给拥有者：别的玩家不需要看到你的待选卡牌。
	// COND_OwnerOnly 既省带宽又避免"看到别人的选项"这种逻辑污染。
	DOREPLIFETIME_CONDITION(ARoguePlayerState, ProgressionSnapshot, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(ARoguePlayerState, bProgressionReady, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(ARoguePlayerState, bProgressionFailed, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(ARoguePlayerState, BossIceBricksEarnedThisRun, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(ARoguePlayerState, PendingUpgrades, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(ARoguePlayerState, bAwaitingUpgradeChoice, COND_OwnerOnly);

	// 商店镜像同样只发给拥有者（别人不需要看到你的商店界面状态）
	DOREPLIFETIME_CONDITION(ARoguePlayerState, bShopOpen, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(ARoguePlayerState, ShopOffers, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(ARoguePlayerState, ShopRefreshCost, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(ARoguePlayerState, ShopAutoRefreshDeadline, COND_OwnerOnly);

	// 复活状态和截止时间统一复制在 Character.LifeState，PlayerState 查询仅转发。

	// 注：武器加成快照也不在这里，在 ARogueCharacter 上（原因见头文件注释）。
}

float ARoguePlayerState::GetReviveSecondsRemaining() const
{
	const ARogueCharacter* Character = GetPawn<ARogueCharacter>();
	return Character != nullptr ? Character->GetReviveSecondsRemaining() : 0.0f;
}

void ARoguePlayerState::ApplyShopSnapshot(bool bInOpen, const TArray<FRogueShopOffer>& InOffers, int32 InRefreshCost, float InAutoRefreshServerTimeDeadline)
{
	if (!HasAuthority())
	{
		return;
	}

	// 比较开关、价格、截止时刻及商品的价格/已购/类型，避免每次调用都复制整个商品数组。
	// 这是当前字段子集的应用层检查，不是完整结构体相等判定；
	// 对复制属性赋相同值也不等于网络一定发送，仍由引擎变化检测与调度决定。
	const bool bChanged =
		bShopOpen != bInOpen
		|| ShopRefreshCost != InRefreshCost
		|| !FMath::IsNearlyEqual(ShopAutoRefreshDeadline, InAutoRefreshServerTimeDeadline, 0.05f)
		|| ShopOffers.Num() != InOffers.Num();

	bool bOffersDiffer = bChanged;
	if (!bOffersDiffer)
	{
		for (int32 Index = 0; Index < ShopOffers.Num(); ++Index)
		{
			if (ShopOffers[Index].GetCost() != InOffers[Index].GetCost()
				|| ShopOffers[Index].IsPurchased() != InOffers[Index].IsPurchased()
				|| ShopOffers[Index].GetUpgrade().Type != InOffers[Index].GetUpgrade().Type)
			{
				bOffersDiffer = true;
				break;
			}
		}
	}

	if (!bOffersDiffer)
	{
		return;
	}

	bShopOpen = bInOpen;
	ShopOffers = InOffers;
	ShopRefreshCost = InRefreshCost;
	ShopAutoRefreshDeadline = InAutoRefreshServerTimeDeadline;

	// 请求提前进入网络更新调度，减少等待常规更新周期；
	// ForceNetUpdate 不保证本调用立即发包、送达或执行客户端 UI 更新。
	ForceNetUpdate();
}

float ARoguePlayerState::GetShopSecondsUntilRefresh() const
{
	if (!bShopOpen || ShopAutoRefreshDeadline <= 0.0f)
	{
		return 0.0f;
	}

	// 用 GameState 的服务器世界时间换算 —— 引擎已经做好了时间同步，
	// 客户端不需要自己维护"服务器时间偏移"。
	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = World != nullptr ? World->GetGameState() : nullptr;
	const float CurrentServerTime = GameState != nullptr ? GameState->GetServerWorldTimeSeconds() : (World != nullptr ? World->GetTimeSeconds() : 0.0f);
	return FMath::Max(0.0f, ShopAutoRefreshDeadline - CurrentServerTime);
}

void ARoguePlayerState::Server_RequestOpenShop_Implementation()
{
	if (ARogueGameMode* RogueGameMode = GetWorld() != nullptr ? GetWorld()->GetAuthGameMode<ARogueGameMode>() : nullptr)
	{
		RogueGameMode->TryOpenShop(Cast<ARogueCharacter>(GetPawn()));
	}
}

void ARoguePlayerState::Server_RequestBuyShopOffer_Implementation(int32 OfferIndex)
{
	if (ARogueGameMode* RogueGameMode = GetWorld() != nullptr ? GetWorld()->GetAuthGameMode<ARogueGameMode>() : nullptr)
	{
		// 关键：把**请求者**的 Pawn 传进去。
		// 旧实现用 GameMode 上的 CachedCharacter，联机下会把玩家 2 的购买算到玩家 1 头上。
		RogueGameMode->TryBuyShopOffer(OfferIndex, Cast<ARogueCharacter>(GetPawn()));
	}
}

void ARoguePlayerState::Server_RequestRefreshShop_Implementation()
{
	if (ARogueGameMode* RogueGameMode = GetWorld() != nullptr ? GetWorld()->GetAuthGameMode<ARogueGameMode>() : nullptr)
	{
		RogueGameMode->TryRefreshShop(Cast<ARogueCharacter>(GetPawn()));
	}
}

void ARoguePlayerState::Server_RequestCloseShop_Implementation()
{
	if (ARogueGameMode* RogueGameMode = GetWorld() != nullptr ? GetWorld()->GetAuthGameMode<ARogueGameMode>() : nullptr)
	{
		RogueGameMode->CloseShop(Cast<ARogueCharacter>(GetPawn()));
	}
}

void ARoguePlayerState::OnRep_PendingUpgrades()
{
	// 目前无需额外处理：HUD 每帧读 GetPendingUpgrades()。
}

void ARoguePlayerState::OpenUpgradeSelection(const TArray<FRogueUpgradeOption>& InOptions)
{
	// 只允许服务端写 —— 这是权威状态
	if (!HasAuthority())
	{
		return;
	}

	PendingUpgrades = InOptions;
	bAwaitingUpgradeChoice = PendingUpgrades.Num() > 0;
}

void ARoguePlayerState::CloseUpgradeSelection()
{
	if (!HasAuthority())
	{
		return;
	}

	bAwaitingUpgradeChoice = false;
	PendingUpgrades.Reset();
}

void ARoguePlayerState::QueueUpgradeSelections(int32 Count)
{
	if (HasAuthority() && Count > 0)
	{
		QueuedUpgradeSelections += Count;
	}
}

bool ARoguePlayerState::ConsumeQueuedSelection()
{
	if (!HasAuthority() || QueuedUpgradeSelections <= 0)
	{
		return false;
	}

	--QueuedUpgradeSelections;
	return true;
}

bool ARoguePlayerState::TryConsumePendingUpgrade(int32 UpgradeIndex, FRogueUpgradeOption& OutUpgrade)
{
	if (!HasAuthority() || !bAwaitingUpgradeChoice || !PendingUpgrades.IsValidIndex(UpgradeIndex))
	{
		return false;
	}

	const ARogueCharacter* Character = GetPawn<ARogueCharacter>();
	if (Character == nullptr || !bProgressionReady)
	{
		return false;
	}
	const ERogueUpgradeType Type = PendingUpgrades[UpgradeIndex].Type;
	if (!RogueUpgradeSystemRules::IsUpgradeAllowedForProgression(Type, Character))
	{
		return false;
	}
	OutUpgrade = PendingUpgrades[UpgradeIndex];
	PendingUpgrades.Reset();
	bAwaitingUpgradeChoice = false;
	return true;
}

void ARoguePlayerState::ResetUpgradeState()
{
	if (!HasAuthority())
	{
		return;
	}

	PendingUpgrades.Reset();
	bAwaitingUpgradeChoice = false;
	QueuedUpgradeSelections = 0;
}

bool ARoguePlayerState::Server_RequestRestartRun_Validate()
{
	// 无参数，所以这里没有"参数合法性"可查。
	// 真正的准入条件（本局是否已结束）在服务端实现里由 GameMode 判定 ——
	// 此 Validate 恒为 true，不提供参数或业务过滤；准入检查依赖 GameMode 的服务端实现。
	return true;
}

void ARoguePlayerState::Server_RequestRestartRun_Implementation()
{
	if (ARogueGameMode* RogueGameMode = GetWorld() != nullptr ? GetWorld()->GetAuthGameMode<ARogueGameMode>() : nullptr)
	{
		// 传自己：服务端据此知道"谁在请求"，并决定要不要执行（本局未结束就拒绝）
		RogueGameMode->HandleRestartRequest(this);
	}
}

bool ARoguePlayerState::Server_RequestUpgradeSelection_Validate(int32 UpgradeIndex)
{
	// 只校验"索引范围合理"，真正的合法性由服务端的 TryConsumePendingUpgrade 判定
	// （它会检查是否真的处于待选择状态、索引是否在待选列表内）。
	return UpgradeIndex >= 0 && UpgradeIndex <= 8;
}

void ARoguePlayerState::Server_RequestUpgradeSelection_Implementation(int32 UpgradeIndex)
{
	// 服务端权威校验：客户端只能在自己真的有待选卡牌时消耗一次。
	// 这里必须重新取一遍，不能相信客户端的任何状态。
	FRogueUpgradeOption SelectedUpgrade;
	if (!TryConsumePendingUpgrade(UpgradeIndex, SelectedUpgrade))
	{
		return;
	}

	// 实际的施加与流程推进交给 GameMode（它手上有升级应用器、商店与关卡流程）。
	if (ARogueGameMode* RogueGameMode = GetWorld() != nullptr ? GetWorld()->GetAuthGameMode<ARogueGameMode>() : nullptr)
	{
		RogueGameMode->HandlePlayerUpgradeSelected(this, SelectedUpgrade);
	}
}

ARoguePlayerState::ARoguePlayerState()
{
	// UE5.5 的 APlayerState 默认 NetUpdateFrequency 为 1Hz；本类提高调度频率，
	// 服务于 ASC 属性及商店/升级镜像。金币、经验和等级当前在 Character 上复制。
	// 频率为调度参数，不保证固定送达间隔，关键状态另调用 ForceNetUpdate 请求更新。
	SetNetUpdateFrequency(30.0f);
	SetMinNetUpdateFrequency(10.0f);

	// ASC 要参与复制
	AbilitySystemComponent = CreateDefaultSubobject<URogueAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);

	// 项目玩家 ASC 选用 Mixed：完整 GE 主要复制给拥有者，其他连接接收精简标签/Cue。
	// 该模式不替代属性复制配置，也不单独保证技能预测正确。
	AbilitySystemComponent->ConfigureAsPlayer();

	// AttributeSet 为 UObject 子对象；本类在 PostInitializeComponents 中将其登记到
	// ASC 的复制子对象列表。玩家基础属性和武器加成都使用这一套属性集。
	AttributeSet = CreateDefaultSubobject<URogueAttributeSet>(TEXT("AttributeSet"));

	// 启用 Actor 注册子对象列表路径；本项目 ASC/AttributeSet 采用对应注册方式，
	// 这不表示所有 UE 版本和所有复制配置都只能使用这一条路径。
	bReplicateUsingRegisteredSubObjectList = true;
}

UAbilitySystemComponent* ARoguePlayerState::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void ARoguePlayerState::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// 只有服务端需要"登记要复制的子对象"。
	// 客户端侧这个 AttributeSet 实例已经由构造函数创建，等待服务端同步数值即可。
	if (HasAuthority() && AbilitySystemComponent != nullptr && AttributeSet != nullptr)
	{
		AbilitySystemComponent->AddReplicatedSubObject(AttributeSet);
	}
}

void ARoguePlayerState::BeginPlay()
{
	Super::BeginPlay();

	// 兜底：确保 ASC 知道自己的 Owner / Avatar。
	// 正常流程里 Pawn 的 PossessedBy / OnRep_PlayerState 会再调一次 RefreshAbilityActorInfo，
	// 那里才拿得到真正的 Pawn。
	RefreshAbilityActorInfo(GetPawn());

	// 注册项目的原生 GameplayCue（幂等）。
	//
	// 在本机每个 PlayerState 的 BeginPlay 尝试幂等注册，确保本机管理器能找到原生 Cue。
	// 不保证跨 Actor 的 BeginPlay 顺序，也不能据此断言任何 Cue 一定晚于此入口。
	RogueGameplayCues::EnsureNativeImpactCuesRegistered();
}

void ARoguePlayerState::RefreshAbilityActorInfo(APawn* InPawn)
{
	if (AbilitySystemComponent == nullptr)
	{
		return;
	}
	
	UE_LOG(LogRoguePlayerState, Verbose, TEXT("ActorInfo: Owner=%s Avatar=%s NetMode=%d"), *GetName(), *GetNameSafe(InPawn), GetNetMode());

	// GAS OwnerActor = PlayerState，AvatarActor = 当前 Pawn（允许为空）。
	// 网络所属连接和实际复制条件还由 Actor/ASC 的复制配置决定。
	AbilitySystemComponent->InitAbilityActorInfo(this, InPawn);
	BeginProgressionHandshake();

	// 授予默认技能。内部自带"只在服务端、只授一次"的保护，
	// 所以这里在 PossessedBy / OnRep_PlayerState 两条路径上重复调用是安全的。
	//
	// 能力集是**资产**，只在权威端解析：客户端不需要它（GrantDefaultAbilities 在客户端会直接返回），
	// 让它在客户端也走一遍 LoadSynchronous 只是白白加载一个资产。
	const URogueAbilitySet* AbilitySet = AbilitySystemComponent->IsOwnerActorAuthoritative()
		? DefaultAbilitySet.LoadSynchronous()
		: nullptr;
	AbilitySystemComponent->GrantDefaultAbilities(AbilitySet);
}

bool ARoguePlayerState::SetProgressionSnapshotForRun(const FRogueProgressionSnapshot& Snapshot)
{
	if (!HasAuthority() || bProgressionFailed || bProgressionSubmitted || bBaseAttributesInitialized)
	{
		return false;
	}
	// Reject a late RPC even when the repeating timer has not run in this frame yet.
	if (ProgressionHandshakeStarted >= 0.0 && GetWorld()
		&& GetWorld()->GetTimeSeconds() - ProgressionHandshakeStarted >= FMath::Clamp(ProgressionHandshakeTimeout, 5.0f, 120.0f))
	{
		FailProgressionHandshake();
		return false;
	}
	ProgressionSnapshot = URogueUpgradeTreeDefinition::GetDefaultDefinition()->SanitizeSnapshot(Snapshot);
	bProgressionReady = true;
	bProgressionSubmitted = true;
	GetWorldTimerManager().ClearTimer(ProgressionHandshakeTimer);
	if (ARogueCharacter* Character = GetPawn<ARogueCharacter>())
	{
		Character->NotifyProgressionReady();
	}
	ForceNetUpdate();
	UE_LOG(LogTemp, Log, TEXT("[RogueMeta] Profile ready for %s: %d purchased nodes"), *GetPlayerName(), ProgressionSnapshot.NodeRanks.Num());
	return true;
}

void ARoguePlayerState::BeginProgressionHandshake()
{
	if (!HasAuthority() || bProgressionFailed || bProgressionSubmitted || bBaseAttributesInitialized || GetWorld() == nullptr)
	{
		return;
	}
	APlayerController* Controller = Cast<APlayerController>(GetOwner());
	if (Controller != nullptr && Controller->GetLocalPlayer() != nullptr)
	{
		const URogueProgressionSubsystem* Progression = Controller->GetLocalPlayer()->GetSubsystem<URogueProgressionSubsystem>();
		if (Progression != nullptr)
		{
			SetProgressionSnapshotForRun(Progression->GetSnapshot());
			return;
		}
	}
	// Native unit worlds have no local players or network connection. They can inject
	// a profile before Possess; this fallback is deliberately not an RPC submission.
	const UGameInstance* Instance = GetWorld()->GetGameInstance();
	if (GetNetMode() == NM_Standalone && (Instance == nullptr || Instance->GetNumLocalPlayers() == 0)
		&& (Controller == nullptr || Controller->GetNetConnection() == nullptr))
	{
		bProgressionReady = true;
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	if (ProgressionHandshakeStarted < 0.0) { ProgressionHandshakeStarted = Now; }
	if (Now - ProgressionHandshakeStarted >= FMath::Clamp(ProgressionHandshakeTimeout, 5.0f, 120.0f))
	{
		FailProgressionHandshake();
		return;
	}
	if (Controller != nullptr && Controller->GetNetConnection() != nullptr)
	{
		Client_RequestProgressionSnapshot();
	}
	// PostLogin normally completes immediately. Retry covers a late owning connection
	// without replacing a real player's save with a premature empty profile.
	if (!GetWorldTimerManager().IsTimerActive(ProgressionHandshakeTimer))
	{
		GetWorldTimerManager().SetTimer(ProgressionHandshakeTimer, this,
			&ARoguePlayerState::BeginProgressionHandshake, 1.0f, true);
	}
}

void ARoguePlayerState::Client_RequestProgressionSnapshot_Implementation()
{
	SubmitLocalProgressionWhenReady();
}

void ARoguePlayerState::SubmitLocalProgressionWhenReady()
{
	APlayerController* Controller = Cast<APlayerController>(GetOwner());
	ULocalPlayer* LocalPlayer = Controller != nullptr ? Controller->GetLocalPlayer() : nullptr;
	URogueProgressionSubsystem* Progression = LocalPlayer != nullptr ? LocalPlayer->GetSubsystem<URogueProgressionSubsystem>() : nullptr;
	if (bProgressionFailed) { GetWorldTimerManager().ClearTimer(LocalProgressionRetryTimer); return; }
	if (Progression != nullptr)
	{
		Server_SubmitProgressionSnapshot(Progression->GetSnapshot());
		GetWorldTimerManager().ClearTimer(LocalProgressionRetryTimer);
		return;
	}
	if (!GetWorldTimerManager().IsTimerActive(LocalProgressionRetryTimer))
	{
		GetWorldTimerManager().SetTimer(LocalProgressionRetryTimer, this,
			&ARoguePlayerState::SubmitLocalProgressionWhenReady, 0.5f, true);
	}
}

void ARoguePlayerState::Server_SubmitProgressionSnapshot_Implementation(const FRogueProgressionSnapshot& Snapshot)
{
	// Cooperative local progression: validate the node whitelist, prerequisites and
	// caps, then freeze this run. A local save is not a proof against save editing.
	SetProgressionSnapshotForRun(Snapshot);
}

void ARoguePlayerState::OnRep_Progression()
{
	if (bProgressionReady)
	{
		if (ARogueCharacter* Character = GetPawn<ARogueCharacter>())
		{
			Character->NotifyProgressionReady();
		}
	}
}

void ARoguePlayerState::GrantBossIceBrick(const FGuid& RewardId)
{
	if (!HasAuthority() || !RewardId.IsValid() || GrantedBossRewardIds.Contains(RewardId))
	{
		return;
	}
	GrantedBossRewardIds.Add(RewardId);
	++BossIceBricksEarnedThisRun;
	ForceNetUpdate();
	const APlayerController* Controller = Cast<APlayerController>(GetOwner());
	if (Controller != nullptr && (Controller->GetLocalPlayer() != nullptr || Controller->GetNetConnection() != nullptr))
	{
		Client_GrantBossIceBrick(RewardId);
	}
}

void ARoguePlayerState::Client_GrantBossIceBrick_Implementation(const FGuid& RewardId)
{
	PendingLocalBossRewards.Add(RewardId);
	FlushLocalBossRewards();
}

void ARoguePlayerState::FlushLocalBossRewards()
{
	APlayerController* Controller = Cast<APlayerController>(GetOwner());
	ULocalPlayer* LocalPlayer = Controller != nullptr ? Controller->GetLocalPlayer() : nullptr;
	URogueProgressionSubsystem* Progression = LocalPlayer != nullptr ? LocalPlayer->GetSubsystem<URogueProgressionSubsystem>() : nullptr;
	if (Progression != nullptr)
	{
		TArray<FGuid> Committed;
		for (const FGuid& RewardId : PendingLocalBossRewards)
		{
			Progression->EnqueueBossReward(RewardId);
			Committed.Add(RewardId); // Ownership handed to the LocalPlayer, including failed saves.
		}
		for (const FGuid& RewardId : Committed) { PendingLocalBossRewards.Remove(RewardId); }
	}
	if (PendingLocalBossRewards.Num() > 0)
	{
		if (!GetWorldTimerManager().IsTimerActive(BossRewardRetryTimer))
		{
			GetWorldTimerManager().SetTimer(BossRewardRetryTimer, this,
				&ARoguePlayerState::FlushLocalBossRewards, 1.0f, true);
		}
	}
	else
	{
		GetWorldTimerManager().ClearTimer(BossRewardRetryTimer);
	}
}

void ARoguePlayerState::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	FlushLocalBossRewards(); // Handoff before the PlayerState/world is destroyed.
	GetWorldTimerManager().ClearTimer(ProgressionHandshakeTimer);
	GetWorldTimerManager().ClearTimer(LocalProgressionRetryTimer);
	GetWorldTimerManager().ClearAllTimersForObject(this);
	Super::EndPlay(EndPlayReason);
}

void ARoguePlayerState::FailProgressionHandshake()
{
	if (!HasAuthority() || bProgressionSubmitted || bProgressionReady || bProgressionFailed) { return; }
	bProgressionFailed = true;
	SetIsOnlyASpectator(true); // Excluded from wipe checks and ownership repair before disconnect.
	GetWorldTimerManager().ClearTimer(ProgressionHandshakeTimer);
	GetWorldTimerManager().ClearTimer(LocalProgressionRetryTimer);
	ForceNetUpdate();
	UE_LOG(LogRoguePlayerState, Warning, TEXT("Progression handshake timed out for %s after %.1fs"), *GetPlayerName(), ProgressionHandshakeTimeout);
	if (APlayerController* PC = Cast<APlayerController>(GetOwner()))
	{
		APawn* OldPawn = PC->GetPawn();
		PC->UnPossess();
		if (OldPawn) { OldPawn->Destroy(); }
		PC->ClientReturnToMainMenuWithTextReason(FText::FromString(TEXT("玩家升级树初始化超时，请返回主菜单后重新加入。你的本地进度没有被修改。")));
	}
}
