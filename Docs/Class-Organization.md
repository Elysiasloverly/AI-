# 类文件组织与查找指南

本次整理把 C++ 类文件按玩法职责归位，并拆开 `ARogueCharacter`、`ARogueGameMode` 的函数实现和集中声明的 UMG 基类。运行时仍是 `ai` 单模块；`aiEditor` 只在编辑器加载，用于生成 UMG 占位资产，不参与打包。数值、能力、网络权威和资产配置方式延续原有实现。

运行时对象关系、伤害与升级数据流、复制分工和池化契约见 [Project-Structure.md](Project-Structure.md)。GAS 历史迁移记录保留原有技术取舍，但旧文件路径请以本文为准。

## 目录职责

GameplayEffect 放 `AbilitySystem/GameplayEffects/`；战斗视觉表现放 `Combat/Visuals/`；参与碰撞、运动和命中的实体放 `Combat/Projectiles/`。`AbilitySystem/Cues/` 保留 GameplayCue 对表现的触发与状态联动。

`Source/ai/Public/` 放声明，`Source/ai/Private/` 放实现，业务目录相互对应。共享类型可能只有头文件，私有 helper 只在 `Private/`，拆分实现文件不要求再增加公开头。

| 目录 | 职责与常用入口 |
|---|---|
| `Framework/` | 整局调度与快照：`RogueGameMode`、`RogueGameState`；规则、配置和平衡：`RogueGameModeRules`、`RogueGameModeConfig`、`RogueGameBalanceAsset` |
| `AbilitySystem/` | GAS 基础设施：`RogueAbilitySystemComponent`、`RogueAttributeSet`、`RogueAttributeMacros`、`RogueAbilitySet`、`RogueAbilitySystemLibrary`、`RogueGameplayTags` |
| `AbilitySystem/Abilities/` | 冲刺、武器开火、敌人攻击 GameplayAbility |
| `AbilitySystem/Tasks/` | 冲刺位移、武器开火心跳 |
| `AbilitySystem/GameplayEffects/` | `RogueDamageEffect`、`RogueAttributeUpgradeEffect`、`RogueStateEffects`：伤害、升级、冷却、回复、倒地与无敌 GE |
| `AbilitySystem/Cues/` | `RogueGameplayCue_Impact`、`RogueGameplayCue_Downed`：命中及状态表现 |
| `Combat/Weapons/` | `RogueWeaponBase`、六个 `RogueWeapon_*`，配置表行及一发攻击的具体实现 |
| `Combat/Projectiles/` | `RogueProjectile`、`RogueRocketProjectile`、`RogueMortarProjectile`、`RogueEnemyProjectile`、`RogueOrbitingBlade`：有飞行/位移、碰撞与命中结算的攻击实体 |
| `Combat/Visuals/` | `RogueLaserBeam`、`RogueImpactEffect`、`RogueCombatFeedback`：光束、命中特效与伤害飘字，不负责伤害数值结算 |
| `Combat/Pooling/` | `RogueCombatPools`、`RogueCombatPoolSubsystem`：池容器、预热和获取入口 |
| `Combat/Targeting/` | `RogueEnemyTracker`、`RogueEnemyTrackerSubsystem`：注册、空间哈希、查询、高负载判据 |
| `Progression/Upgrades/` | `RogueUpgradeSystem`、`RogueUpgradeSystemRules`、`RogueUpgradeDefinitionAsset`、`RogueUpgradeRuleAsset` |
| `Progression/Shop/` | `RogueShopSystem`：报价、购买、刷新及补货运行态 |
| `Progression/` | `RogueRewardTypes.h`：成长领域共用奖励报价结构 |
| `Player/` | `RogueCharacter`、`RoguePlayerState`、`RoguePlayerController`、`RoguePlayerQuery`、视觉组件及玩家平衡资产 |
| `Enemies/` | `RogueEnemy`：统一敌人运行时、移动和攻击决策 |
| `Enemies/Data/` | `RogueEnemyArchetypes`、`RogueEnemyDataTable`、`RogueEnemyVisualResources` |
| `Enemies/Spawning/` | `RogueSpawnSubsystem`：刷怪、出生位置、波次预算 |
| `World/` | `RogueArena`、`RogueExperiencePickup`、`RogueShopTerminal`、`RogueSceneSubsystem` |
| `UI/` | `RogueHUD`：界面调度与原生绘制 |
| `UI/Widgets/` | 十个独立 UMG 基类：8 个菜单类 + 玩家状态条 + 敌人血条；共享视图数据及聚合 include |
| `Shared/` | `RogueTypes.h`：跨玩法域的枚举、复制载荷与结构体 |
| `Debug/` | `RogueSelfTest`、两个 `.SelfTest.cpp`：开关、链路诊断及攻击/压测探针 |
| `Source/aiEditor/` | 仅编辑器的工具模块：`RogueWidgetBuilder`（UMG 占位资产生成），不参与游戏打包 |

`Subsystem` 表示 UE 管理生命周期的机制。找对象池去 `Combat/Pooling/`，找刷怪去 `Enemies/Spawning/`，不再集中放进通用 `Subsystems/` 目录。`Core/` 原来混放的类也已归入上述业务目录。

## 大类的函数实现

`RogueCharacter.h` 与 `RogueGameMode.h` 仍是完整类声明。以下文件的方法继续属于原类；没有为分文件新建 Actor、Component、Subsystem 或蓝图父类。

| 实现文件（相对 `Private/`） | 内容 |
|---|---|
| `Player/RogueCharacter.cpp` | 构造、BeginPlay、PossessedBy、OnRep_PlayerState、Tick、复制声明和视觉更新入口 |
| `Player/RogueCharacter.GAS.cpp` | ASC 接入、属性初始化及镜像、属性变化委托、回复 GE、升级 GE 施加 |
| `Player/RogueCharacter.Input.cpp` | Enhanced Input 绑定、移动、相机、冲刺/跳跃输入、菜单输入模式 |
| `Player/RogueCharacter.Weapons.cpp` | 武器初始化及授予、武器加成读取、开火表现合批多播 |
| `Player/RogueCharacter.Progression.cpp` | 经验、等级、金币、升级落地、商店交互和选卡请求 |
| `Player/RogueCharacter.LifeCycle.cpp` | 死亡、倒地、复活等待、恢复与无敌状态清理 |
| `Debug/RogueCharacter.SelfTest.cpp` | 武器开火及跨端数值诊断报告 |
| `Framework/RogueGameMode.cpp` | 开局配置、世界对象创建、子系统配置与预热、整局 Tick 与快照推送 |
| `Framework/RogueGameMode.Progression.cpp` | 击杀归属、金币/经验/Boss 奖励、抽卡队列、商店请求与镜像推送 |
| `Framework/RogueGameMode.Players.cpp` | 存活玩家查询、复活兜底、归属恢复、登录、暂停及重开 |
| `Debug/RogueGameMode.SelfTest.cpp` | 升级链路阶段机、接触攻击、震荡柱弹幕、死亡复活和敌人压测探针 |

读完整流程时，从主文件里的调度入口沿方法名跳转到对应实现。新增同类方法时，声明放回原类头，实现放到对应文件。改变类职责或引入新组件应作为单独的架构改动。

## UMG 类与视图数据

`UI/Widgets/` 中每个基类有自己的 `.h/.cpp`：

- `RogueShopOfferCardWidgetBase`：商店单张报价卡。
- `RogueUpgradeCardWidgetBase`：升级单张卡。
- `RoguePauseMenuWidgetBase`：暂停菜单。
- `RogueUpgradeSelectionWidgetBase`：升级选择面板。
- `RogueShopWidgetBase`：商店面板。
- `RogueSettingsMenuWidgetBase`：设置菜单。
- `RogueDeathScreenWidgetBase`：本局结束面板。
- `RogueReviveCountdownWidgetBase`：倒地复活倒计时。
- `RogueVitalsWidgetBase`：本地玩家生命 / 护甲 / 经验与左上角波数/金币，常驻视口。
- `RogueEnemyHealthBarWidgetBase`：单个敌人血条，由 HUD 池化复用。

九个菜单视图 USTRUCT 集中在 `RogueMenuViewData.h`；HUD 状态条的 `FRoguePlayerVitalsView` / `FRogueEnemyHealthBarView` 在 `RogueHUDViewData.h`。`RogueMenuWidgetBases.h` 保留为菜单聚合头，统一包含菜单视图数据和八个菜单基类；两个 HUD 状态条类单独 include，不参与菜单聚合。

C++ 仍通过 `Update*View` 提供数据，蓝图通过原有 `On*Updated` 等事件更新外观。拆头文件不会新增界面状态转换或改变 HUD 的调度方式。

## 反射与资产兼容约定

- 模块仍叫 `ai`；UCLASS、USTRUCT、UENUM 名称、继承关系和字段名称保留，原来的 `/Script/ai.*` 类型身份保持。
- BlueprintCallable / BlueprintPure、BlueprintImplementableEvent、RPC 及事件参数签名保留；复制条件、默认子对象名与默认属性值延续原实现。
- 前次 C++ 类文件整理没有搬 `Content/` 资产、改 DataTable 行类型、换蓝图父类或引入类重定向。后续编辑器美术目录整理见 [Content-Organization.md](Content-Organization.md)。
- 源码 include 使用当前路径。每个反射头包含自己的 `*.generated.h`，放在最后一个 include；聚合头本身不需要生成头。
- 拆开的 `.cpp` 独立包含所需的头，同一方法只保留一份实现，避免依赖 Unity Build 偶然提供的声明。

目录可用于代码导航；蓝图兼容性和运行效果仍应以 UE 编译、资产加载及游戏验证为准。

## 新功能放哪里

| 新需求 | 落点与接线 |
|---|---|
| 新武器 | `Combat/Weapons/` 加武器；开火能力放 `AbilitySystem/Abilities/`；配置接 `DT_WeaponConfig`，池化及预热接 `Combat/Pooling/` |
| 新弹体或短命战斗表现 | `Combat/Projectiles/`（攻击实体）或 `Combat/Visuals/`（表现），伤害由 GAS 结算，遵守激活/回收契约 |
| 新玩家主动技能 | `AbilitySystem/Abilities/` 加能力，逐帧过程放 `Tasks/`；通过 `RogueAbilitySet` 配授予，输入接玩家输入实现 |
| 新状态或升级 GE | `AbilitySystem/GameplayEffects/`；标签放 `RogueGameplayTags`。仅固定属性映射的一组升级子类可继续放现有升级 GE 文件 |
| 新升级卡 | 枚举放 `Shared/RogueTypes.h`；定义和规则放 `Progression/Upgrades/`；属性映射接升级 GE；落地在 `RogueCharacter.Progression.cpp` |
| 新敌人或刷怪规则 | 原型及数据表放 `Enemies/Data/`，调度放 `Enemies/Spawning/`，攻击能力放 GAS 的 `Abilities/` |
| 新场景交互或掉落物 | `World/`，结算调用商店/成长系统 |
| 新界面 | `UI/Widgets/` 加独立基类，共享数据放 `RogueMenuViewData.h`；由 HUD 创建及推数据，再接 WBP |
| 新调试开关或探针 | `Debug/`，默认关闭，通过现有入口调度 |

先按玩法职责放文件，不要只因名称带 `System`、`Library`、`Asset` 或 `Subsystem` 就另建通用目录。多个领域真正共用的类型才放 `Shared/`。`RogueGameModeRules` 与 `RogueGameModeConfig` 仍包含跨领域规则及配置，保留它们是本次整理的范围选择。

## 验证入口

自检与压测实现已归 `Debug/`，开关名字保持：

```text
-ExecCmds="Rogue.SelfTest.WeaponUpgradeChain 1,Rogue.SelfTest.EnemyStress 200"
```

`EnemyStress` 在 `RunWeaponUpgradeSelfTest()` 内触发，必须同时开启 `WeaponUpgradeChain`。攻击能力对照组追加 `,Rogue.SelfTest.DisableEnemyAttackAbilities 1`。压测可单进程运行；验证同名玩家在两端的属性、快照及武器值，需要服务端和客户端两份进程。
