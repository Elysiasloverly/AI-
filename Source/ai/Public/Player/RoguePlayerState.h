#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "AbilitySystemInterface.h"
#include "Shared/RogueTypes.h"
#include "Progression/Shop/RogueShopSystem.h"
#include "RoguePlayerState.generated.h"

class URogueAbilitySystemComponent;
class URogueAbilitySet;
class URogueAttributeSet;
struct FRogueUpgradeOption;

/**
 * 玩家状态：持有 ASC、属性集，以及每玩家的升级/商店运行态。
 *
 * GAS OwnerActor 为 PlayerState，AvatarActor 为当前 Pawn；ActorInfo 还会缓存控制器等信息。
 * GAS OwnerActor 不直接决定网络接收者；实际复制取决于 Actor 所属连接、相关性、
 * GE 复制模式与属性/子对象的复制配置。
 *
 * 当前十秒复活复用同一个 Pawn 和 ASC。若以后替换 Pawn，仍需刷新 ActorInfo 并处理
 * 能力/武器对旧 Avatar 的引用；断线重连或 PlayerState 重建不会自动保存技能与升级。
 */
UCLASS()
class AI_API ARoguePlayerState : public APlayerState, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	ARoguePlayerState();

	// IAbilitySystemInterface
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	URogueAbilitySystemComponent* GetRogueAbilitySystemComponent() const { return AbilitySystemComponent; }

	/** 玩家的属性集（生命/护甲/移速 + **武器加成**）。武器加成也在这套里，没有第二套集。 */
	URogueAttributeSet* GetAttributeSet() const { return AttributeSet; }

	// 注：武器加成的跨端快照**不在这里**，而在 ARogueCharacter 上。
	// GameMode 从属性集汇总角色快照，武器统一读取；与 LifeState 一样在 Character 上注册复制。
	// 详见 FRogueWeaponBonusValues 与 ARogueCharacter::GetWeaponBonusSnapshot 的注释。

	/** 由 Pawn 在 PossessedBy / OnRep_PlayerState 时调用，把 Avatar 换成当前 Pawn。
	 *  PlayerState::BeginPlay 也会尝试初始化，此时 InPawn 可能为空；不保证其他 Actor 已完成复制。 */
	void RefreshAbilityActorInfo(APawn* InPawn);

	// ===========================================================
	//  升级状态（每玩家一份 + 复制）
	// ===========================================================
	//  迁移前这些数据在 ARogueGameMode 上（单机写法）。搬到 PlayerState 有两个理由：
	//    1) 联机下每个玩家有自己的待选卡牌，不能共用一套；
	//    2) PlayerState 会被复制到拥有者客户端，而 GameMode 在客户端**根本取不到**
	//       （GetAuthGameMode 返回 nullptr）—— 这是"客户端看不到升级界面"的根因。

	/** 当前待选的升级卡（已复制到拥有者客户端） */
	const TArray<FRogueUpgradeOption>& GetPendingUpgrades() const { return PendingUpgrades; }
	bool IsAwaitingUpgradeChoice() const { return bAwaitingUpgradeChoice; }

	/** 服务端：写入本次待选卡牌并打开选择界面（会自动复制） */
	void OpenUpgradeSelection(const TArray<FRogueUpgradeOption>& InOptions);

	/** 服务端：关闭选择界面 */
	void CloseUpgradeSelection();

	void QueueUpgradeSelections(int32 Count);
	int32 GetQueuedUpgradeSelections() const { return QueuedUpgradeSelections; }

	/** 服务端：消耗一次排队名额。返回 false 表示没有待开的选择。 */
	bool ConsumeQueuedSelection();

	/** 服务端：取出第 Index 张卡（同时清空待选）。越界或当前不在选择状态时返回 false。 */
	bool TryConsumePendingUpgrade(int32 UpgradeIndex, FRogueUpgradeOption& OutUpgrade);

	void ResetUpgradeState();

	/** 客户端 → 服务端：请求选择第 Index 张升级卡。
	 *  这是联机下升级流程的入口（原来是直接调 GameMode 的方法，在客户端是空操作）。 */
	UFUNCTION(Server, Reliable, WithValidation)
	void Server_RequestUpgradeSelection(int32 UpgradeIndex);

	// ===========================================================
	//  商店（服务端权威 + 镜像给拥有者客户端）
	// ===========================================================
	//  每个 PlayerState 保存独立 ShopRuntime，GameMode 负责服务端规则推进，
	//  商品与界面状态通过本 PlayerState 的 OwnerOnly 镜像提供给所属客户端。

	/** 服务端写入商店快照（GameMode 每帧推送） */
	void ApplyShopSnapshot(bool bInOpen, const TArray<FRogueShopOffer>& InOffers, int32 InRefreshCost, float InAutoRefreshServerTimeDeadline);

	/** 商店运行态（**服务端权威、每玩家一份**）。
	 *  这是"每个客户端商店数据独立"的实现点：商品列表、刷新价格、自动刷新计时、
	 *  各类升级的已购次数都在这里，玩家之间互不影响。
	 *  客户端不直接用它 —— 客户端读复制的镜像字段（ShopOffers / bShopOpen / ...）。
	 *
	 *  仅供服务端规则代码使用；客户端没有复制的运行态，只应读取镜像。
	 *  ensureMsgf 用于诊断误用，不会阻止函数返回引用。 */
	FRogueShopSystem& GetShopRuntime()
	{
		ensureMsgf(HasAuthority(), TEXT("GetShopRuntime() 只能在服务端调用：客户端的 ShopRuntime 是空的，请改读复制的镜像字段（GetShopOffers/IsShopOpen）"));
		return ShopRuntime;
	}
	const TArray<FRogueShopOffer>& GetShopOffers() const { return ShopOffers; }
	bool IsShopOpen() const { return bShopOpen; }
	int32 GetShopRefreshCost() const { return ShopRefreshCost; }

	/** 距离自动刷新还剩多少秒。
	 *  复制的是**截止时刻**（服务端世界时间）而不是倒计时 ——
	 *  服务端每次快照以当前时间与剩余时间重算截止时刻；实际发送仍受复制调度限制。
	 *  客户端用 GameState 的服务器时间换算（引擎已做时间同步）。 */
	float GetShopSecondsUntilRefresh() const;

	// ---- 基础属性初始化（一局一次，服务端）----

	/** 基础属性是否已经灌过。
	 *
	 *  由 `ARogueCharacter::InitializeGASAttributes()` 在**第一次** possess 时置位，
	 *  并作为它自己的重入守卫。详细理由见 `bBaseAttributesInitialized` 的注释。 */
	bool IsBaseAttributesInitialized() const { return bBaseAttributesInitialized; }
	void MarkBaseAttributesInitialized() { bBaseAttributesInitialized = true; }

	// ---- 复活显示查询（转发当前 Pawn 的 LifeState 截止时刻）----

	/** 当前 Pawn 的显示用复活倒计时；零秒仍需以 IsDowned 判断是否等待服务端。 */
	float GetReviveSecondsRemaining() const;

	/** 客户端 → 服务端：商店操作请求 */
	UFUNCTION(Server, Reliable) void Server_RequestOpenShop();
	UFUNCTION(Server, Reliable) void Server_RequestBuyShopOffer(int32 OfferIndex);
	UFUNCTION(Server, Reliable) void Server_RequestRefreshShop();
	UFUNCTION(Server, Reliable) void Server_RequestCloseShop();

	/** 客户端 → 服务端：请求重开一局（死亡结算界面的"重新开始"）。
	 *
	 *  为什么必须走服务端：客户端自己 OpenLevel 只会加载出一份**单机**关卡副本，
	 *  表现为"点了重开之后队友全没了"。由服务端 ServerTravel 带全队重开，
	 *  把所有客户端一起带到新关卡。
	 *  服务端另外还会校验"本局是否已经结束"，否则一个已阵亡的玩家
	 *  就能在队友还在战斗时把整局拉走（见 ARogueGameMode::HandleRestartRequest）。 */
	UFUNCTION(Server, Reliable, WithValidation)
	void Server_RequestRestartRun();

protected:
	virtual void BeginPlay() override;
	virtual void PostInitializeComponents() override;

	UFUNCTION()
	void OnRep_PendingUpgrades();

	UPROPERTY(ReplicatedUsing = OnRep_PendingUpgrades)
	TArray<FRogueUpgradeOption> PendingUpgrades;

	UPROPERTY(ReplicatedUsing = OnRep_PendingUpgrades)
	bool bAwaitingUpgradeChoice = false;

	/** 排队中的待选次数。纯服务端状态，客户端不需要知道，所以不复制。 */
	int32 QueuedUpgradeSelections = 0;

	// ---- 商店镜像（复制给拥有者）----
	UPROPERTY(Replicated) bool bShopOpen = false;
	UPROPERTY(Replicated) TArray<FRogueShopOffer> ShopOffers;
	UPROPERTY(Replicated) int32 ShopRefreshCost = 0;
	UPROPERTY(Replicated) float ShopAutoRefreshDeadline = 0.0f;

	// 复活期限不在 PlayerState 重复保存，由当前 Character.LifeState 统一复制。

	/** 商店运行态本体（服务端）。不复制 —— 客户端只收上面那组镜像。
	 *  标 UPROPERTY() 是为了让 GC 能追踪它内部的 UObject 引用。 */
	UPROPERTY()
	FRogueShopSystem ShopRuntime;

	UPROPERTY(VisibleAnywhere, Category = "Rogue|GAS")
	TObjectPtr<URogueAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, Category = "Rogue|GAS")
	TObjectPtr<URogueAttributeSet> AttributeSet;

	/** 玩家的默认能力集（**数据驱动**）。在 `BP_RoguePlayerState` 上指定。
	 *
	 *  填了它 → 玩家的初始技能完全由这个资产决定，"加一个主动技能"不用改 C++；
	 *  留空   → `GrantDefaultAbilities` 回退到硬编码的冲刺，
	 *           保证资产没配好时玩家依然能玩（而不是一个技能都没有）。
	 *
	 *  ⚠️ 授予时机不受这里影响：仍由 `RefreshAbilityActorInfo` 在
	 *     PossessedBy / OnRep_PlayerState 两条路径上触发，内部自带"只在服务端、只授一次"的保护。
	 *     详见 URogueAbilitySet 的类注释。 */
	UPROPERTY(EditDefaultsOnly, Category = "Rogue|GAS")
	TSoftObjectPtr<URogueAbilitySet> DefaultAbilitySet;

	/** 当前 PlayerState 的服务端基础属性初始化守卫，不复制。
	 *  重复 Possess/ActorInfo 刷新不应重写基值或再次补满生命。
	 *  升级由 Infinite GE 修饰器维护；此标志不负责断线重连、跨关卡或新 PlayerState 的持久化。 */
	bool bBaseAttributesInitialized = false;
};