#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "AbilitySystemInterface.h"
#include "Core/RogueTypes.h"
#include "Core/RogueShopSystem.h"
#include "RoguePlayerState.generated.h"

class URogueAbilitySystemComponent;
class URogueAttributeSet;
struct FRogueUpgradeOption;

/**
 * 玩家状态（联机版）。
 *
 * **为什么 ASC 挂在 PlayerState 而不是 Pawn** —— 这是 GAS 联机架构里最关键的一条：
 *   · PlayerState 的生命周期覆盖整场对局，Pawn 死了/重生/重连会换新的；
 *     ASC 挂 Pawn 的话每次重生都要重新 InitAbilityActorInfo、重新给技能、重算属性，
 *     而且旧 Pawn 的 GE 会随 Pawn 一起没了（升级全部丢失）。
 *   · 服务端与客户端都要能从 PlayerState 拿到 ASC —— 客户端拿到的是复制代理，
 *     属性同步与 GameplayCue 都靠它。
 *   · 这也是 UE 官方模板（Lyra）的做法：OwnerActor = PlayerState，AvatarActor = Pawn。
 *
 * 注意 Owner 与 Avatar 的区别：
 *   `InitAbilityActorInfo(OwnerActor, AvatarActor)`
 *   Owner 负责**复制**（谁的连接收到这些信息），Avatar 负责**挂载与表现**（技能从谁身上发出）。
 *   两者分开后，"玩家换了个 Pawn"这件事对属性与技能毫无影响。
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
	// 这份数据本来就是"角色的战斗参数"，挂在角色上分层更合理，也和 bDead 一样走角色的复制通道。
	// 详见 FRogueWeaponBonusValues 与 ARogueCharacter::GetWeaponBonusSnapshot 的注释。

	/** 由 Pawn 在 PossessedBy / OnRep_PlayerState 时调用，把 Avatar 换成当前 Pawn。
	 *  服务端在 PossessedBy 调用，客户端在 OnRep_PlayerState（以及 Pawn 复制到达时）调用。 */
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
	//  与升级同样的理由：GameMode 在客户端拿不到，所以"商店开着没、有哪些商品、多少钱"
	//  必须复制到 PlayerState 上，客户端 HUD 才能画出来。
	//
	//  注意当前商店仍是**全局一个**（不是每玩家一份），所以各玩家的镜像内容相同；
	//  但"谁买的"按请求者区分 —— 修掉了原来用 `CachedCharacter` 导致
	//  "玩家 2 买东西、加到玩家 1 身上"的错误。

	/** 服务端写入商店快照（GameMode 每帧推送） */
	void ApplyShopSnapshot(bool bInOpen, const TArray<FRogueShopOffer>& InOffers, int32 InRefreshCost, float InAutoRefreshServerTimeDeadline);

	/** 商店运行态（**服务端权威、每玩家一份**）。
	 *  这是"每个客户端商店数据独立"的实现点：商品列表、刷新价格、自动刷新计时、
	 *  各类升级的已购次数都在这里，玩家之间互不影响。
	 *  客户端不直接用它 —— 客户端读复制的镜像字段（ShopOffers / bShopOpen / ...）。
	 *
	 *  ⚠️ 只在服务端调。客户端的这份 FRogueShopSystem 是**空的**（不参与复制），
	 *  误用不会崩也不会报错，只会静默读写一份空数据（表现为"商店永远没商品"），
	 *  所以这里用 ensureMsgf 把误用喊出来，而不是让它变成一个查不出原因的怪现象。 */
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
	 *  倒计时每帧都在变，会把复制频率打满；截止时刻只在刷新时变化。
	 *  客户端用 GameState 的服务器时间换算（引擎已做时间同步）。 */
	float GetShopSecondsUntilRefresh() const;

	// ---- 基础属性初始化（一局一次，服务端）----

	/** 基础属性是否已经灌过。
	 *
	 *  由 `ARogueCharacter::InitializeGASAttributes()` 在**第一次** possess 时置位，
	 *  并作为它自己的重入守卫。详细理由见 `bBaseAttributesInitialized` 的注释。 */
	bool IsBaseAttributesInitialized() const { return bBaseAttributesInitialized; }
	void MarkBaseAttributesInitialized() { bBaseAttributesInitialized = true; }

	// ---- 复活倒计时（由 ASC 上的 State.Downed GE 承担，HUD 直接读它的剩余时长）----

	/** 距离复活还剩多少秒；<=0 表示当前没有待复活。
	 *
	 *  迁移前这里复制了一个"复活截止时刻"，现在**没有**这样的复制字段了：
	 *  真相源是玩家 ASC 上那个带时长的 `URogueDownedEffect`（State.Downed），
	 *  带时长的 GE 本来就会同步给本人，所以 HUD 读的就是服务端那一份的剩余时间。
	 *  "有没有排过复活"由 ARogueCharacter::IsRevivePending() 回答（两者语义不同：
	 *  团灭取消等待时 GE 被移除，但角色仍然是倒下的）。 */
	float GetReviveSecondsRemaining() const;

	/** 客户端 → 服务端：商店操作请求 */
	UFUNCTION(Server, Reliable) void Server_RequestOpenShop();
	UFUNCTION(Server, Reliable) void Server_RequestBuyShopOffer(int32 OfferIndex);
	UFUNCTION(Server, Reliable) void Server_RequestRefreshShop();
	UFUNCTION(Server, Reliable) void Server_RequestCloseShop();

	/** 客户端 → 服务端：请求重开一局（死亡结算界面的"重新开始"）。
	 *
	 *  为什么必须走服务端：客户端自己 OpenLevel 只会加载出一份**单机**关卡副本，
	 *  表现为"点了重开之后队友全没了"。由服务端 OpenLevel 才会触发 server travel，
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

	/** 复活截止时刻（GameState 服务器世界时间；0 = 无待复活）。复制给拥有者，
	 *  客户端据此自己算"还有几秒复活"，服务端不需要每帧推倒计时。 */
	// 已删除：ReviveServerTime（迁移到 State.Downed GE 的剩余时长，见 GetReviveSecondsRemaining）。

	/** 商店运行态本体（服务端）。不复制 —— 客户端只收上面那组镜像。
	 *  标 UPROPERTY() 是为了让 GC 能追踪它内部的 UObject 引用。 */
	UPROPERTY()
	FRogueShopSystem ShopRuntime;

	UPROPERTY(VisibleAnywhere, Category = "Rogue|GAS")
	TObjectPtr<URogueAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, Category = "Rogue|GAS")
	TObjectPtr<URogueAttributeSet> AttributeSet;

	/** 基础属性是否已灌过（**服务端**，一局一次）。见 IsBaseAttributesInitialized()。
	 *
	 *  ⚠️ 为什么需要这个守卫：`InitializeGASAttributes()` 用 `SetNumericAttributeBase` 写的是
	 *  属性的 **BaseValue**，而升级（Instant GE）改的**也是** BaseValue —— 再灌一次就会把玩家
	 *  已经拿到的属性升级（生命/护甲上限/移速/拾取范围/回复/经验/冲刺冷却/减伤）**全部覆盖回初始值**。
	 *
	 *  这条路径**真实可达**，不是理论风险：GameMode 的归属自愈
	 *  （`ARogueGameMode::RepairPlayerPawnOwnership`，每 0.5 秒检查一次）会为了纠正 Pawn 归属
	 *  而调 `PlayerController->Possess()`，进而再次触发 `PossessedBy()`；而项目文档记载过
	 *  归属自愈在**客户端加入的瞬间实测真的会触发**。
	 *
	 *  放在 PlayerState 而不是 Pawn 上：ASC 与属性集都挂在这里 —— 它才是"这一局这个玩家的数值"
	 *  的生命周期载体，Pawn 被重新 possess 不该重灌。
	 *  不复制：纯服务端状态，客户端从不灌基础值（客户端等属性复制过来）。 */
	bool bBaseAttributesInitialized = false;
};