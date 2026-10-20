#pragma once

#include "CoreMinimal.h"
#include "Progression/Shop/RogueShopSystem.h"
#include "Progression/Upgrades/RogueUpgradeSystem.h"
#include "GameFramework/GameModeBase.h"
#include "Framework/RogueGameModeConfig.h"
#include "Shared/RogueTypes.h"
#include "RogueGameMode.generated.h"

class ARogueCharacter;
class ARogueEnemySpawnRegion;
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
class UPrimitiveComponent;
class URogueUpgradeRuleAsset;
struct FRogueCombatPoolPrewarmClasses;

/** 服务端局内调度：刷怪、奖励、玩家商店/选卡协调、团灭与重开；共享显示数据交给 GameState。 */
UCLASS(Config = Game)
class AI_API ARogueGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ARogueGameMode();

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;

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

	/** 操作后立即将该玩家商店运行态写入 PlayerState 镜像；客户端显示仍需等待复制。 */
	void PushShopSnapshotForPlayer(ARoguePlayerState* PlayerState);

	void TryOpenShop(ARogueCharacter* Character);
	/** 关闭请求者的商店运行态并推送镜像；本机 HUD 结合全部菜单状态推导输入模式。 */
	void CloseShop(ARogueCharacter* Requester = nullptr);
	/** 购买。Purchaser 是**请求者**——升级要加到他身上，不能用 GameMode 缓存的角色。 */
	void TryBuyShopOffer(int32 OfferIndex, ARogueCharacter* Purchaser);
	void TryRefreshShop(ARogueCharacter* Requester);
	/** 收起濒死玩家的商店与待选卡，并检查团灭；尚有存活队友时继续本局。
	 *  等待复活界面由本机 HUD 根据 LifeState 推导，团灭后才显示结算。 */
	void NotifyPlayerDied(ARogueCharacter* DeadCharacter);
	/** 统一团灭判定；致死处理已排队的玩家也不算存活。 */
	bool EvaluateTeamWipe();
	bool IsGameOver() const { return RunState.bGameOver; }

	/** 只读查询：某类型是否配置了专属敌人蓝图（测试/调试用）。 */
	bool HasEnemyClassForType(ERogueEnemyType Type) const;

	/** 登录完成后检查 Pawn 归属；已结束的局中加入者保持出局。 */
	virtual void PostLogin(APlayerController* NewPlayer) override;

	/** 拒绝经 GameMode::SetPause 入口提出的暂停请求，使菜单、商店和选卡保持实时玩法。
	 *  Tick 中另有恢复世界暂停的兜底；这两个入口不保证世界不会被其他机制冻结。 */
	virtual bool SetPause(APlayerController* PC, FCanUnpause CanUnpauseDelegate = FCanUnpause()) override;

	/** 为无 Pawn 的控制器重新认领现有角色：先尝试记录的失控 Pawn，再按 PlayerState 查找无控制器角色。
	 *  首选记录路径只检查有效性与控制器，不额外比较 PlayerState。 */
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
	friend struct FRoguePlayerLifeCycleTestAccess;
	/** 双进程生命周期自检；开启时接管本局调度，避免刷怪干扰真实死亡/计时/复制。 */
	bool RunPlayerLifeCycleSelfTest();
	int32 LifeCycleSelfTestPhase = 0;
	double LifeCycleSelfTestStartTime = -1.0;
	double LifeCycleSelfTestPhaseTime = 0.0;
	double LifeCycleSelfTestDeathTime = 0.0;
	TArray<TWeakObjectPtr<ARogueCharacter>> LifeCycleSelfTestPlayers;
	TArray<TWeakObjectPtr<APlayerController>> LifeCycleSelfTestControllers;
	TMap<TWeakObjectPtr<UPrimitiveComponent>, uint8> LifeCycleSelfTestCollisionSnapshot;
	bool bLifeCycleSelfTestActorCollision = true;

	void FindLevelGameplayActors();
	void ConfigureRuntimeAssets();
	void ConfigureRuntimeSubsystems();
	void PrewarmCombatPools();
	void BuildCombatPoolPrewarmClasses(FRogueCombatPoolPrewarmClasses& OutClasses) const;

	/** 从存活玩家中随机选择刷怪参照；无人存活或尚无角色时返回 nullptr。
	 *  不固定使用玩家 0，避免所有刷新点只围绕主机。 */
	ARogueCharacter* GetSpawnReferenceCharacter() const;

	/** 收集存活角色；内联容量为 4，超过时容器仍可扩容，不构成联机人数限制。 */
	void CollectAlivePlayers(TArray<ARogueCharacter*, TInlineAllocator<4>>& OutPlayers) const;
	/** 将各玩家 AttributeSet 的 13 项加成写入其 Character 快照，由武器统一读取。
	 *  属性本身也参与复制；这层提供角色侧输入接口与近似相等脏检查，不证明 ASC 属性未同步。
	 *  快照与 AttributeSet 属于不同复制路径，客户端不能假设二者同帧到达。 */
	void PushWeaponBonusSnapshots();

	/** 开发自检：向当前参与玩家施加升级，按玩家名记录属性与武器生效值。
	 *  入口：Rogue.SelfTest.WeaponUpgradeChain 1；具体等待阶段见 SelfTest.cpp。
	 *  比较两端时必须对应同一玩家；固定等待时间不构成网络已同步的保证。 */
	void RunWeaponUpgradeSelfTest();

	/** 将敌人放到目标附近，给接触攻击创造可重复的触发条件；用于开发自检。 */
	void ProbeContactAttack(ARogueCharacter* Target);

	/** 在目标附近生成震荡柱，减少随机刷怪对震荡柱弹幕覆盖的影响。 */
	void ProbeShockPillarBarrage(ARogueCharacter* Target);

	/** 三个攻击路径探针是否已经跑过（自检完成时只跑一次）。 */
	bool bSelfTestProbesDone = false;

	/** 压测敌人是否已经刷过（与阶段机无关：压测用单进程跑，不需要等客户端）。 */
	bool bSelfTestStressSpawned = false;

	/** 对目标施加致命伤害以观察濒死/复活；团灭和真实连接另由生命周期自检覆盖。 */
	void ProbeDeathAndRevive(ARogueCharacter* Target);

	/** 自检辅助：刷 N 只"很硬、伤害 0"的敌人在玩家周围，用来量 200+ 敌人时的帧时（见 RogueSelfTest.h）。 */
	void ProbeEnemyStress(ARogueCharacter* Target, int32 Count);

	/** 武器升级自检阶段（0未启动，1–4执行中，5完成）；时间点与日志以实现为准。 */
	int32 SelfTestPhase = 0;

	/** 当前阶段开始时的世界时间（用于阶段之间的等待） */
	float SelfTestPhaseTime = 0.0f;


	/** 离给定位置最近的存活玩家；一个都没有时返回 nullptr。 */
	ARogueCharacter* FindNearestAlivePlayer(const FVector& Location) const;

	void AdvanceShopAutoRefresh(float DeltaSeconds, bool bWorldPaused);
	void TickRunSystems(float DeltaSeconds);

	/** 奖励归属依次尝试 Killer、本层 Owner、本层 Instigator；不递归遍历拥有链。
	 *  无法解析时回退到奖励位置最近的存活玩家；直接解析出的玩家不在这里再过滤存活状态。 */
	ARogueCharacter* ResolveRewardCharacter(AActor* Killer, const AActor* RewardOrigin) const;
	void AwardEnemyCurrency(const ARogueEnemy* Enemy, ARogueCharacter* RewardCharacter);
	void SpawnExperienceReward(ARogueEnemy* Enemy, URogueEnemyTrackerSubsystem* Tracker, URogueCombatPoolSubsystem* Pools);
	void HandleBossKilledReward();
	void QueueUpgradeSelections(int32 Count, ARogueCharacter* Character);

	/** 玩家 Pawn 蓝图；默认从 DefaultGame.ini 读取，C++ 不再写死资产路径。 */
	UPROPERTY(EditDefaultsOnly, Config, Category = "Config")
	TSoftClassPtr<ARogueCharacter> PlayerPawnClass;

	/** 默认敌人类（未配置子类映射时的回退类） */
	UPROPERTY(EditDefaultsOnly, Category = "Spawning")
	TSubclassOf<ARogueEnemy> DefaultEnemyClass;

	/** 按敌人类型配置蓝图子类（在编辑器中为每种类型指定对应的蓝图子类） */
	UPROPERTY(EditDefaultsOnly, Category = "Spawning")
	TMap<ERogueEnemyType, TSubclassOf<ARogueEnemy>> EnemyClassMap;

	/** 经验球蓝图类。外观全部在 BP 里配置，C++ 原生类只有碰撞与拾取逻辑。
	 *  路径从 DefaultGame.ini 读取，留空时回退原生类（此时画面上看不到经验球）。 */
	UPROPERTY(EditDefaultsOnly, Config, Category = "Spawning")
	TSubclassOf<ARogueExperiencePickup> ExperiencePickupClass;

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
	 *  留空则用 `/Game/Data/Enemies/DT_EnemyArchetypes`。 */
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
	/** 关卡中唯一的刷怪区域；缺少或重复时停止自动刷怪。 */
	TWeakObjectPtr<ARogueEnemySpawnRegion> LevelSpawnRegion;
};
