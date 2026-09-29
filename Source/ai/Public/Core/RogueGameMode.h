#pragma once

#include "CoreMinimal.h"
#include "Core/RogueShopSystem.h"
#include "Core/RogueUpgradeSystem.h"
#include "GameFramework/GameModeBase.h"
#include "Core/RogueGameModeConfig.h"
#include "Core/RogueTypes.h"
#include "RogueGameMode.generated.h"

class ARogueCharacter;
class ARogueArena;
class ARogueEnemy;
class ARogueExperiencePickup;
class ARogueShopTerminal;
class APawn;
class URogueCombatPoolSubsystem;
class URogueEnemyTrackerSubsystem;
class URogueGameBalanceAsset;
class URogueUpgradeDefinitionAsset;
class ARoguePlayerState;
class UDataTable;
class URogueUpgradeRuleAsset;
struct FRogueCombatPoolPrewarmClasses;

UCLASS()
class AI_API ARogueGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ARogueGameMode();

	virtual void Tick(float DeltaSeconds) override;

	void HandleEnemyKilled(ARogueEnemy* Enemy, AActor* Killer);
	void StartUpgradeSelection(ARogueCharacter* Character);

	/** 服务端：某个玩家选定了升级卡（由 ARoguePlayerState::Server_RequestUpgradeSelection 调用） */
	void HandlePlayerUpgradeSelected(ARoguePlayerState* PlayerState, const FRogueUpgradeOption& SelectedUpgrade);

	/** 给指定玩家开下一轮升级选择（每玩家一份状态） */
	void OpenNextUpgradeSelectionForPlayer(ARoguePlayerState* PlayerState);

	/** 把 RunState 快照写进 GameState（服务端每帧调用，供客户端 HUD 读取） */
	void PushRunStateToGameState();

	/** 把商店快照写进各玩家的 PlayerState（服务端每帧调用，供客户端 HUD 读取与操作） */
	void PushShopStateToPlayers();

	/** 把某个玩家的商店运行态写成快照，复制给他的客户端。
	 *  打开/关闭/购买/刷新后**立即**调用一次，避免客户端要等下一帧才看到变化。 */
	void PushShopSnapshotForPlayer(ARoguePlayerState* PlayerState);

	void TryOpenShop(ARogueCharacter* Character);
	/** 关闭商店。Requester 用于只解除请求者的菜单交互输入（联机下不能只动一个人）。 */
	void CloseShop(ARogueCharacter* Requester = nullptr);
	/** 购买。Purchaser 是**请求者**——升级要加到他身上，不能用 GameMode 缓存的角色。 */
	void TryBuyShopOffer(int32 OfferIndex, ARogueCharacter* Purchaser);
	void TryRefreshShop(ARogueCharacter* Requester);
	/**
	 * 某个玩家阵亡。
	 *
	 * 联机语义：**整局只在全员阵亡时结束**，不是"谁死了就结束"。
	 * 旧实现一有人死就 `bGameOver = true`，后果是队友还活着却再也不能选升级
	 * （QueueUpgradeSelections 一开头就因为 bGameOver 早退）、商店也被强行关掉。
	 * 现在：死者自己收尾（作废他未选的卡、关他的商店、给他 UI 输入好点结算界面），
	 * 只要还有队友活着，本局继续。
	 */
	void NotifyPlayerDied(ARogueCharacter* DeadCharacter);

	/** 新玩家登录（服务端）。
	 *
	 *  为什么必须重写：实测（双进程真联机 + 调用栈取证）发现，客户端加入的瞬间引擎的登录流程会
	 *  让**已在场的 PlayerController** 先去接管新玩家的 Pawn，随后才纠正给新 PC ——
	 *  代价是那个在场 PC 自己的 Pawn 被解除控制（`Controller == nullptr`），
	 *  表现就是"主机切到固定视角 + UI 消失 + 操控不了"。
	 *  调用栈里全是 Engine/OnlineSubsystemUtils 帧、没有任何蓝图帧，所以这不是蓝图写错，
	 *  而是默认流程在本项目（Pawn 由 GameMode 指定类、PlayerState 挂 ASC）下的产物。
	 *
	 *  对策：登录结束后做一次**权威归属校正**，让每个 PC 都拿回本命 Pawn。 */
	virtual void PostLogin(APlayerController* NewPlayer) override;

	/** 本作**永不暂停世界**：从入口直接拒绝一切暂停请求。
	 *
	 *  为什么是"拒绝"而不是"事后恢复"：实测有蓝图（具体哪个未知，死亡界面里没有该节点）
	 *  在**每帧**重新暂停，事后恢复会变成"暂停→恢复"的每帧拉锯，玩家看到的仍然是停住。
	 *  而本作的设计本来就是任何时候都不暂停（暂停菜单/商店/选卡/死亡界面都不暂停），
	 *  所以入口拒绝既符合设计，也是唯一彻底的做法。
	 *  代价：控制台 `pause` 调试不再生效。 */
	virtual bool SetPause(APlayerController* PC, FCanUnpause CanUnpauseDelegate = FCanUnpause()) override;

	/** 服务端：把"没有 Pawn 的 PlayerController"与"没有控制器的本命角色"重新配对。
	 *  判据是 PlayerState 相同（引擎在 possess 时会把角色的 PlayerState 设为该 PC 的）。 */
	void RepairPlayerPawnOwnership();

	/** 客户端 → 服务端"重开一局"的落地实现（入口是 ARoguePlayerState::Server_RequestRestartRun）。
	 *  只允许在**整局已结束**时执行，否则一个已阵亡的玩家就能把还在战斗的队友连局一起拉走。 */
	void HandleRestartRequest(ARoguePlayerState* Requester);

	/** 重开是否已经被受理过。
	 *  OpenLevel/ServerTravel 是重操作：没有这个一次性闸门的话，
	 *  结算界面上的按钮被连点（或被改过的客户端反复调 RPC）会让服务端不停切图。 */
	bool bRestartRequested = false;

	/** 是否有**任意**玩家正在选升级。
	 *  取代原来读单一 UpgradeSystem 的写法 —— 联机下每个玩家有自己的选择状态。 */
	UFUNCTION(BlueprintPure)
	bool IsAwaitingUpgradeChoice() const;

	UFUNCTION(BlueprintPure)
	int32 GetCurrentWave() const { return RunState.CurrentWave; }

	UFUNCTION(BlueprintPure)
	int32 GetEnemiesDefeated() const { return RunState.EnemiesDefeated; }

	UFUNCTION(BlueprintPure)
	float GetRunTimeSeconds() const { return RunState.RunTimeSeconds; }

	// 注：待选升级卡与商店数据都**不在 GameMode 上**了。
	// 两者都是"每玩家一份"的状态，挂在 ARoguePlayerState 上并复制给拥有者客户端；
	// GameMode 只负责服务端权威逻辑与推送快照。
	// 原来这里的 GetPendingUpgrades() / IsShopOpen() / GetShopOffers() 客户端一律读不到
	// （GameMode 是服务端独占对象），HUD 请改读本地玩家的 PlayerState。

protected:
	virtual void BeginPlay() override;

private:
	void SpawnRuntimeWorldActors();
	void ConfigureRuntimeAssets();
	void ConfigureRuntimeSubsystems();
	void PrewarmCombatPools();
	void BuildCombatPoolPrewarmClasses(FRogueCombatPoolPrewarmClasses& OutClasses) const;

	/**
	 * 刷怪的参照玩家：**随机挑一个活着的玩家**。
	 *
	 * 为什么不能用 GetCachedCharacter()（它基本就是玩家 0 / 主机）：
	 *   1) 所有怪都围着主机刷，队伍分开走时队友那边一个怪都不来；
	 *   2) 主机一死，SpawnEnemy 的 IsDead() 闸门会让**整局停止刷怪**，
	 *      而队友可能还活着在打 —— 这是联机下最刺眼的 bug 之一。
	 * 随机取一个活着的人，既修掉这两点，又让怪的来向自然分散。
	 */
	ARogueCharacter* GetSpawnReferenceCharacter() const;

	/** 收集当前所有活着的玩家角色（用 TInlineAllocator<4> 避免堆分配：联机上限 4 人）。 */
	void CollectAlivePlayers(TArray<ARogueCharacter*, TInlineAllocator<4>>& OutPlayers) const;

	/**
	 * 开发自检：验证「升级 → GAS 加成属性 → 武器生效值」这条链是否真的通。
	 *
	 * 为什么需要它：GAS 改造把"升级改武器配置"换成了"升级改玩家 ASC 上的加成属性，
	 * 武器读 基础值+加成"。这条链跨了 4 层（升级 GE → ExecutionCalculation → 属性集 → 武器），
	 * 任何一层接错都只表现为"卡牌抽到了但没效果"，靠手玩很难定位。
	 * 这个自检在服务端跑一次：逐项施加升级、打印加成属性与每把武器的生效数值，
	 * 用日志证明数值按预期变化（也顺带证明了加成是加在**正确的武器**上的）。
	 *
	 * 开关（默认关）：`Rogue.SelfTest.WeaponUpgradeChain 1`
	 * 例：无头验证
	 *   UnrealEditor.exe <uproject> <map>?listen -game -nullrhi
	 *     -ExecCmds="Rogue.SelfTest.WeaponUpgradeChain 1" -log
	 */
	void RunWeaponUpgradeSelfTest();

	/** 自检阶段：0=未跑，1=第一阶段已跑（等客户端连接），2=全部跑完。
	 *  第二阶段刻意在"客户端连上之后"再改一次属性，用来区分
	 *  "属性从来没复制" 与 "只有初始同步没带、后续变化能到"。 */
	int32 SelfTestPhase = 0;

	/** 第一阶段完成时的世界时间（第二阶段要等客户端连上且再过 20 秒） */
	float SelfTestPhaseOneTime = 0.0f;

	/** 倒地等待 → 复活的兜底驱动（服务端每帧，且**不受"本局已结束"影响**）。
	 *
	 *  为什么 GameMode 也要管：复活本来只靠"角色 Tick 轮询截止时刻 + 角色身上的定时器"，
	 *  而实测这两条都没生效 —— 日志里 "进入倒地等待" 有、"已复活" 一次都没有，
	 *  观感就是倒地者永远躺着。倒下的角色恰恰是最容易不再 Tick 的对象，
	 *  所以由 GameMode（服务端唯一确定在跑的东西）来兜底。
	 *  三条路都调用同一个 Revive()，而 Revive() 自带 `bDead` 早退，重复调用安全。 */
	void TickReviveQueue();

	/** 离给定位置最近的存活玩家；一个都没有时返回 nullptr。 */
	ARogueCharacter* FindNearestAlivePlayer(const FVector& Location) const;

	void AdvanceShopAutoRefresh(float DeltaSeconds, bool bWorldPaused);
	void TickRunSystems(float DeltaSeconds);

	/**
	 * 击杀奖励（金币）归谁。
	 *
	 * 解析顺序：
	 *   1) Killer 本身就是玩家；
	 *   2) Killer 是弹体/刀片/塔 → 顺着 Owner / Instigator 找回玩家；
	 *   3) 都解析不出来 → **离现场最近的存活玩家**。
	 *
	 * 第 3 条原来是回退到 CachedCharacter（联机下就是玩家 0 / 主机），
	 * 于是玩家 2 打死的怪，赏金可能进了主机口袋 —— 联机里最容易被察觉的那种"抢人头"。
	 * 单机时两种回退结果完全一致（场上只有一个人）。
	 */
	ARogueCharacter* ResolveRewardCharacter(AActor* Killer, const AActor* RewardOrigin) const;
	void AwardEnemyCurrency(const ARogueEnemy* Enemy, ARogueCharacter* RewardCharacter);
	void SpawnExperienceReward(ARogueEnemy* Enemy, URogueEnemyTrackerSubsystem* Tracker, URogueCombatPoolSubsystem* Pools);
	void HandleBossKilledReward();
	void QueueUpgradeSelections(int32 Count, ARogueCharacter* Character);

	/** 默认敌人类（未配置子类映射时的回退类） */
	UPROPERTY(EditDefaultsOnly, Category = "Spawning")
	TSubclassOf<ARogueEnemy> DefaultEnemyClass;

	/** 按敌人类型配置蓝图子类（在编辑器中为每种类型指定对应的蓝图子类） */
	UPROPERTY(EditDefaultsOnly, Category = "Spawning")
	TMap<ERogueEnemyType, TSubclassOf<ARogueEnemy>> EnemyClassMap;

	UPROPERTY(EditDefaultsOnly, Category = "Spawning")
	TSubclassOf<ARogueExperiencePickup> ExperiencePickupClass;

	UPROPERTY(EditDefaultsOnly, Category = "Arena")
	TSubclassOf<ARogueArena> ArenaClass;

	UPROPERTY(EditDefaultsOnly, Category = "Arena")
	TSubclassOf<ARogueShopTerminal> ShopTerminalClass;

	UPROPERTY(EditDefaultsOnly, Category = "Config")
	FRogueSceneOptimizationSettings SceneOptimizationSettings;

	UPROPERTY(EditDefaultsOnly, Category = "Config")
	FRogueRuntimePerformanceSettings RuntimePerformanceSettings;

	UPROPERTY(EditDefaultsOnly, Category = "Config")
	FRoguePoolSettings PoolSettings;

	UPROPERTY(EditDefaultsOnly, Category = "Config")
	FRogueSpawnSettings SpawnSettings;

	UPROPERTY(EditDefaultsOnly, Category = "Config")
	FRogueBossSettings BossSettings;

	UPROPERTY(EditDefaultsOnly, Category = "Config")
	TSoftObjectPtr<URogueGameBalanceAsset> GameBalanceAsset;

	UPROPERTY(EditDefaultsOnly, Category = "Config")
	TSoftObjectPtr<URogueUpgradeDefinitionAsset> UpgradeDefinitionAsset;

	UPROPERTY(EditDefaultsOnly, Category = "Config")
	TSoftObjectPtr<URogueUpgradeRuleAsset> UpgradeRuleAsset;

	/** 敌人原型表（行结构 `FRogueEnemyArchetypeRow`）——"每波刷新上限"从这里读。
	 *  留空则用 `/Game/DataTable/DT_FRogueEnemyArchetypeRow`。 */
	UPROPERTY(EditDefaultsOnly, Category = "Config")
	TSoftObjectPtr<UDataTable> EnemyArchetypeTable;

	UPROPERTY(VisibleInstanceOnly, Category = "Run")
	FRogueRunState RunState;

	UPROPERTY()
	FRogueUpgradeSystem UpgradeSystem;

	// 注：不再有共享的 FRogueShopSystem 成员 —— 商店运行态已下放到各玩家的 PlayerState。

	UPROPERTY()
	TObjectPtr<URogueGameBalanceAsset> LoadedGameBalanceAsset;

	UPROPERTY()
	TObjectPtr<UDataTable> LoadedEnemyArchetypeTable;

	TWeakObjectPtr<ARogueCharacter> CachedCharacter;
	TWeakObjectPtr<ARogueArena> SpawnedArena;
	TWeakObjectPtr<ARogueShopTerminal> SpawnedShopTerminal;
};
