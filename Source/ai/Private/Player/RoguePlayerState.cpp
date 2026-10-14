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

	// 脏检查 + 变化才推。
	// 为什么必须做：GameMode 每帧都会调这里推快照（4 人局 60fps = 240 次/秒），
	// 而无条件赋值会深拷贝整个 ShopOffers（每个 offer 里还有两个 FString），
	// 并让 4 个复制属性每帧都"变化"。内容没变时直接返回，代价从"每帧几十次拷贝"降到"一次比较"。
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

	// 关键 UI 状态变化立刻推一次，不等下一次网络更新。
	// 本类的复制频率已被提到 30Hz（见构造函数注释），但商店"开关"这种一次性事件
	// 仍然值得立刻发包 —— 玩家点开商店却要等半秒才有反应是最容易被感知的延迟。
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
	// Validate 只做"绝不信任客户端"的那一层粗筛，业务规则统一放在权威侧。
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
	// ── 复制频率：必须显式调高 ──
	// 引擎的 APlayerState 构造函数里是 `SetNetUpdateFrequency(1)`（PlayerState.cpp:26），
	// 而本项目的 PlayerState 承载着**玩家 ASC 的全部属性**（血量/护甲/金币/经验/等级）
	// 以及商店与升级的镜像字段。沿用 1Hz 的后果是客户端最多 1 秒才看到一次变化：
	// 血条一格一格跳、买完东西半天没反应、开商店像卡住 —— 而主机（本地读取）一切正常。
	// 这里按"玩家自身状态该有的刷新率"提上来；关键事件另有 ForceNetUpdate() 兜底。
	SetNetUpdateFrequency(30.0f);
	SetMinNetUpdateFrequency(10.0f);

	// ASC 要参与复制
	AbilitySystemComponent = CreateDefaultSubobject<URogueAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);

	// 玩家侧必须 Mixed：
	// 引擎枚举注释写明 Minimal "does not work for Owned AbilitySystemComponents"，
	// 而 Mixed 的含义是"自己的完整 GE 信息 + 其他人的最小信息"，
	// 这正是联机预测需要的模式。
	AbilitySystemComponent->ConfigureAsPlayer();

	// AttributeSet 是普通 UObject（不是组件），
	// 它要被复制必须挂进 Actor 的"已注册子对象列表"（见 PostInitializeComponents）。
	// 武器加成属性也在这一套里（曾经单独建过第二套，实测值复制不到客户端，已回退）。
	AttributeSet = CreateDefaultSubobject<URogueAttributeSet>(TEXT("AttributeSet"));

	// 打开 Actor 级的"用注册子对象列表做复制"。
	// UE5 的新方式是 AddReplicatedSubObject；旧教程里那种重写
	// UAbilitySystemComponent::ReplicateSubobjects() 已经不需要了。
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
	// 为什么放在这里：PlayerState::BeginPlay 在**每一台机器**上都会跑（服务端 + 每个客户端），
	// 而且一定早于任何一次实际的特效触发。GameplayCue 的注册必须在所有会执行 cue 的机器上完成，
	// 否则那台机器上的特效就是"静默不出现"。
	//
	// 为什么不能只靠"自定义 GameplayCueManager 的钩子"：实测 DefaultGame.ini 里配的
	// GlobalGameplayCueManagerClass 会静默回退到默认管理器，钩子根本不会被调用（详见头文件说明）。
	RogueGameplayCues::EnsureNativeImpactCuesRegistered();
}

void ARoguePlayerState::RefreshAbilityActorInfo(APawn* InPawn)
{
	if (AbilitySystemComponent == nullptr)
	{
		return;
	}
	
	if (InPawn == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("InPawn is nullptr"));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("InPawn is not nullptr"));
	}
	
	
	
	// OwnerActor = this（PlayerState，负责复制）
	// AvatarActor = InPawn（负责挂载与表现，可能是 nullptr）
	AbilitySystemComponent->InitAbilityActorInfo(this, InPawn);

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