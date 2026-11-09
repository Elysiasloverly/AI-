# Baka 当前项目结构图

核对日期：2026-11-05。工程：`F:/AI_WorkSpeace/baka/Baka.uproject`，UE 5.5。

项目以 UE Gameplay Framework 组织对象与整局流程，GAS 承担能力、属性和效果；刷怪、地图、永久成长、商店、联机大厅、界面与音频有各自实现。

图中箭头表示标注的调用、数据流或持有关系，**不表示继承**。`ARogue…` 是 Actor 类，`URogue…Subsystem` 由 UE 管理生命周期；`FRogueUpgradeSystem`、`FRogueShopSystem` 是结构体，`RogueWeaponCatalog` 是描述与查询用的命名空间。

## 1. 各系统总览

```mermaid
flowchart TB
    Data["数据配置<br/>DA · DT · CurveTable · BP默认值"]
    World["关卡地图<br/>出生点 · 刷怪区域 · 空气墙 · 商店 · 灯光"]

    subgraph Authority["服务器规则与整局调度"]
        GM["RogueGameMode<br/>开局 · 波次 · 奖励 · 团灭 · 通关"]
        Spawn["RogueSpawnSubsystem<br/>本波名额 · 类型限制 · 地面落点"]
        Stage["昼夜与切图<br/>RogueBattleStage · RunTravelSubsystem"]
        Growth["升级与商店规则<br/>UpgradeSystem · UpgradeSystemRules"]
    end

    subgraph Play["角色与战斗"]
        Player["RogueCharacter<br/>输入 · 移动 · 濒死复活"]
        State["RoguePlayerState<br/>个人状态 · ASC · 属性 · 商店运行态"]
        Weapons["武器系统<br/>Catalog · WeaponBase · 八种武器"]
        Enemies["敌人系统<br/>RogueEnemy · 原型 · 绕障 · 治疗"]
        GAS["GAS<br/>Ability · Task · GameplayEffect · AttributeSet"]
        Shared["World共享服务<br/>对象池 · 空间查询 · 表现/音效服务"]
    end

    subgraph Local["本机界面、存档与连接"]
        Meta["永久成长<br/>ProgressionSubsystem · SaveGame"]
        Network["主菜单多人<br/>RoomSubsystem · Lobby类组"]
        View["RogueHUD / WBP<br/>显示状态 · 发出操作请求"]
        Music["MusicSubsystem<br/>背景音乐 · 按钮音 · 本机音量"]
    end

    GS["RogueGameState<br/>复制波次 · 昼夜 · Boss · 结算"]
    Data -->|读取配置| GM
    Data -->|武器战斗配置| Weapons
    Data -->|原型配置| Enemies
    World -->|关卡提供生成区域| Spawn
    GM -->|配置与调度| Spawn
    GM -->|协调阶段| Stage
    GM -->|调用规则| Growth
    GM -->|写入并复制| GS
    Spawn -->|获取并初始化| Enemies
    Enemies -->|死亡通知与奖励| GM
    Meta -->|提交个人本局快照| State
    Network -->|准备完成后服务器开局| GM
    State -->|提供ASC与属性访问| Player
    Player -->|创建并驱动| Weapons
    Weapons -->|调用能力/伤害入口| GAS
    GAS -->|作用于目标的ASC与属性| Enemies
    Growth -->|生成个人选项并推送| State
    Growth -->|施加升级效果| GAS
    Weapons -.->|获取实体/查询目标| Shared
    Enemies -.->|池化/登记/表现| Shared
    GS -.->|复制状态供读取| View
    State -.->|个人状态供本机读取| View
    View -->|玩家操作请求| State
    GS -.->|观察本机场景状态| Music
```

这是一张系统总览：战斗区域不代表其中对象都只存在于服务器。武器逻辑 Actor 在各端角色初始化时创建，实际伤害有权威判断；图形、音效和本地 UI 按各自表现路径执行。

## 2. 核心对象：谁创建、谁持有、谁使用

```mermaid
flowchart LR
    GM["GameMode<br/>仅服务器存在"]
    UE["UE框架<br/>按GameMode/地图实际Class配置生成"]
    GS["GameState<br/>整局复制状态"]
    PC["PlayerController<br/>控制角色 · 处理输入/请求"]
    PS["PlayerState<br/>每名玩家一份"]
    C["Character<br/>玩家当前Pawn"]
    ASC["玩家ASC"]
    AS["玩家AttributeSet"]
    Shop["ShopRuntime<br/>FRogueShopSystem<br/>每玩家的服务端商店运行态"]
    HUD["HUD / WBP<br/>本地显示"]
    E["Enemy<br/>每只敌人一份"]
    EASC["敌人ASC + AttributeSet"]

    GM -->|设置Class配置| UE
    UE -->|生成| GS
    UE -->|生成并关联玩家对象| PC
    UE -->|生成| PS
    UE -->|生成Pawn| C
    PC -->|Possess控制| C
    PC ---|关联| PS
    PS -->|构造创建并持有| ASC
    PS -->|构造创建并持有| AS
    ASC -.->|OwnerActor指向| PS
    ASC -.->|AvatarActor指向| C
    C -.->|通过PlayerState访问| ASC
    ASC -->|登记与使用| AS
    PS -->|持有，服务器使用| Shop
    GM -->|权威协调购买/刷新| Shop
    GM -->|更新| GS
    HUD -.->|读取| GS
    HUD -.->|读取本机玩家数据| PS
    HUD -.->|读取当前Pawn| C
    E -->|自身创建并持有| EASC
```

- 访问器返回对象不意味着创建或拥有对象；Character 使用 PlayerState 的 GAS。
- GameMode 的 Class 配置交给引擎生成对象，不应把这些关系全写成业务代码 `new`。
- `ShopRuntime` 本体不复制；其报价、开关和期限等镜像复制给拥有者。待选卡也是每玩家一份。
- PlayerState、Character、GameState 的不同状态可能先后到达客户端，不能假定同帧更新。

## 3. 一次攻击、受击与升级的数据流

```mermaid
flowchart TB
    DT["DT_WeaponConfig<br/>伤害 · 冷却 · 范围 · 弹速等"]
    Catalog["RogueWeaponCatalog<br/>稳定身份/卡片所属武器"]
    Init["Character.InitializeWeapons<br/>逐行创建武器实例"]
    GA["RogueWeaponAbility<br/>授予/激活 · 冷却"]
    Task["WeaponFireLoop Task<br/>离散开火调度"]
    Tick["Character.TickWeapons<br/>维护持续实体"]
    Weapon["派生武器<br/>索敌 · 最终公式 · 发起攻击"]
    Hit["命中/范围/射线检测<br/>弹体或武器执行"]
    Apply["RogueAbilitySystem::ApplyDamageToTarget<br/>权威伤害入口"]
    Spec["Damage GameplayEffect Spec<br/>SetByCaller伤害量"]
    Target["目标ASC → AttributeSet<br/>免伤/减伤 · 护甲 · 生命 · 死亡标签"]
    Death["Enemy延迟死亡校验<br/>当前SpawnCycleId · 生命 · 标签"]
    Reward["GameMode奖励协调<br/>经验 · 金币 · Boss冰砖 · 选卡"]
    Upgrade["个人选卡/购买通过复核<br/>Character施加升级GE"]
    Bonus["玩家AttributeSet与角色快照<br/>武器读取最新生效加成"]
    FX["表现分支<br/>命中光效 · 闪白 · 飘字 · 声音"]

    DT -->|逐行读取| Init
    Catalog -->|解析稳定身份| Init
    Init -->|生成并初始化| Weapon
    GA -->|驱动| Task
    Task -->|触发离散攻击| Weapon
    Tick -->|按武器需求维护| Weapon
    Weapon -->|检测/生成实体| Hit
    Hit -->|确认有效命中| Apply
    Apply -->|构造并施加| Spec
    Spec -->|交给目标结算| Target
    Target -->|致死时排队| Death
    Death -->|有效代次才结算| Reward
    Reward -->|生成个人候选| Upgrade
    Upgrade -->|修改属性| Bonus
    Bonus -->|下一次计算读取| Weapon
    Target -.->|属性变化/反馈事件| FX
    Weapon -.->|开火表现通知| FX
```

图中“命中”不是统一的一颗普通子弹：激光可在武器内结算，冰刀由扇形/波实体处理，不同武器共享伤害入口。表现也可能由开火事件直接触发，不必等目标扣血。

## 4. 永久成长、联机与跨地图承接

```mermaid
flowchart TB
    Save["本地SaveGame<br/>余额 · 节点等级"]
    Meta["RogueProgressionSubsystem<br/>LocalPlayer生命周期"]
    Tree["DA_UpgradeTree<br/>前置 · 费用 · 属性收益"]
    TreeUI["WBP_UpgradeTree / Node<br/>永久解锁操作"]
    PS["本玩家的服务器PlayerState<br/>清洗并冻结本局成长快照"]
    Filter["升级/商店候选过滤<br/>永久权限 + 本局持有 + 前置"]
    Room["RogueRoomSubsystem<br/>GameInstance生命周期<br/>LAN搜房 · IP连接 · 会话"]
    Lobby["Lobby GameMode/State/PC/PS<br/>名单与各自准备状态"]
    Start["房主请求开始<br/>服务器复核全部准备"]
    Battle["战斗GameMode<br/>全员进入服务器所选地图"]
    Travel["RunTravelSubsystem<br/>GameInstance生命周期<br/>仅承接整局RunState"]
    PlayerTravel["PlayerState.StageTravel<br/>CopyProperties / StageGrowth<br/>承接各玩家本局成长"]
    Next["下一张Day/Night地图<br/>重建世界对象并恢复本局成长"]

    Save <-->|本机读写| Meta
    Tree -->|解锁规则| Meta
    TreeUI -->|购买请求| Meta
    Meta -->|本玩家开局提交| PS
    PS -->|按各自快照判断| Filter
    Room -->|创建/加入| Lobby
    Lobby -->|房主发起| Start
    Start -->|服务器切图| Battle
    Battle -->|每位玩家独立握手| PS
    Battle -->|同局昼夜切图前保存| Travel
    Travel -->|恢复波数与整局状态| Next
    PS -->|捕获当前角色成长| PlayerTravel
    PlayerTravel -->|新世界初始化后恢复一次| Next
    Battle -.->|Boss奖励RPC返回本机落盘| Meta
```

本地永久存档、整局切图快照、各玩家局内成长是三层数据。RunTravelSubsystem 只保存整局 RunState；各玩家已结算属性与生命流程由 PlayerState.StageTravel 和 Character 的捕获/恢复路径承接，不把旧世界里的 GE 对象直接搬到新世界。

## 5. 生命周期与模块边界

| 生命周期/范围 | 主要对象 | 关系 |
| --- | --- | --- |
| 每个本地玩家 | `RogueProgressionSubsystem` | 本机该玩家的永久成长和 SaveGame |
| 每个 GameInstance | `RogueRoomSubsystem`、`RogueMusicSubsystem`、`RogueRunTravelSubsystem` | 会话、本地音乐、整局RunState换图承接 |
| 每个 World | `RogueSpawnSubsystem`、`RogueCombatPoolSubsystem`、`RogueEnemyTrackerSubsystem`、`RogueCombatAudioSubsystem` | 刷怪、复用、空间查询、本地战斗声音；随世界重建 |
| 每名玩家 | `RoguePlayerState`、`RoguePlayerController`、`RogueCharacter` | 状态与GAS、控制、当前角色；三者生命周期不同 |
| 每个本地视图 | `RogueHUD` 和 WBP 实例 | 读取状态并显示，转交操作请求 |
| 每只敌人/每个池实体 | `RogueEnemy`、弹体/效果 | Actor可复用；每次出池应清理旧生命周期状态 |
| Runtime模块 | `Source/ai` | 实际游戏；大多数功能目录仍在同一个模块里 |
| 开发宏 | `ai/Private/Debug` | 测试/探针；Shipping保留必要空接口 |
| Editor模块 | `Source/aiEditor` | 资产生成、验证、预览；不属于正式游戏模块 |
| 编辑器脚本 | `Tools` | 调用Editor工具，保存或检查资产；不在运行时自动执行 |

`GameInstance` 本身没有跨网络自动同步的含义。每台机器的音乐/房间服务各自存在；共享玩法状态通过 Actor 的复制/RPC 传播。

## 6. 当前代码与资产目录

```text
Baka.uproject
├─ Source/
│  ├─ ai/                           Runtime玩法模块
│  │  ├─ Public/                    对外声明
│  │  └─ Private/                   实现，按相同业务目录归类
│  │     ├─ Framework/              GameMode / GameState / 规则 / 昼夜切图
│  │     ├─ Player/                 Character / PlayerState / Controller / 外观组件
│  │     ├─ AbilitySystem/          ASC / AttributeSet / Ability / Task / GE / Cue
│  │     ├─ Combat/
│  │     │  ├─ Weapons/             Catalog / 武器基类 / 八种派生武器
│  │     │  ├─ Projectiles/         飞行、碰撞与命中实体
│  │     │  ├─ Visuals/             光束、锁定圈、命中/击杀反馈
│  │     │  ├─ Pooling/             对象池
│  │     │  └─ Targeting/           敌人登记与空间查询
│  │     ├─ Enemies/
│  │     │  ├─ Data/               原型与数据表转换
│  │     │  ├─ Spawning/           生成预算、落点与导航预算
│  │     │  ├─ Components/         绕障、治疗、表现等专用组件
│  │     │  └─ Visuals/            敌人专用表现实体
│  │     ├─ Progression/            Meta / Upgrades / Shop
│  │     ├─ Networking/             房间与大厅
│  │     ├─ UI/                     HUD / Widgets / 展示数据
│  │     ├─ Audio/                  音乐与战斗音效
│  │     ├─ World/                  刷怪区域、空气墙、商店、经验拾取
│  │     ├─ Shared/                 共用枚举与载荷
│  │     └─ Debug/                  自检、探针、Tests
│  └─ aiEditor/                     独立Editor模块
│     └─ Private/                   UI/Authoring / UI/LegacyThemes / AssetSetup / Preview / Support
├─ Content/                         UE中的 /Game
│  ├─ Map/                          MainMenu + Day/Night各五张战斗图
│  ├─ Data/                         DA / DT / 成长曲线
│  ├─ BP/                           Framework / Player / Enemy / Projectiles / World等
│  ├─ UMG/                          WBP / 字体 / 界面素材
│  └─ GameAssets/                   当前美术资产目录之一
├─ Config/                          默认配置；实际地图/BP/资产可覆盖相关默认值
├─ Docs/                            系统说明与阅读指南
└─ Tools/                           编辑器脚本和验证入口
```

GameAssets 不是承诺全部历史素材均已迁入的唯一物理目录；查具体资源以 Content Browser 和资产引用为准。

## 7. 大文件拆分后的阅读位置

| 同一个类 | 实现文件 | 读什么 |
| --- | --- | --- |
| Character | 主文件、`.Input`、`.GAS`、`.Weapons`、`.Progression`、`.LifeCycle` | 生命周期与调度、输入、GAS、武器、成长、濒死复活 |
| GameMode | 主文件、`.Players`、`.Progression`、`.BattleStage`、`.Victory` | 调度、玩家/重开、奖励/购买、昼夜承接、通关 |
| Enemy | 主文件、`.LifeCycle`、`.Combat`、`.Movement`、`.Flying`、`.Skyfall`、`.HitFeedback`、`.AttackAudio` | 调度、池生命周期、攻击、移动、飞行、落地攻击、受击、音效 |
| HUD | 主文件、`.MainMenu`、`.Menus`、`.Settings`、`.CombatFeedback`、`.KillFeedback` | 调度、主菜单、局内菜单、设置、血条/飘字、击杀反馈 |

分文件不会新建一个同名系统对象；`RogueWeaponCatalog` 与规则函数也不应被理解成额外“全局管理器”。

核对入口：[GameMode](F:/AI_WorkSpeace/baka/Source/ai/Public/Framework/RogueGameMode.h)、[PlayerState](F:/AI_WorkSpeace/baka/Source/ai/Public/Player/RoguePlayerState.h)、[Character](F:/AI_WorkSpeace/baka/Source/ai/Public/Player/RogueCharacter.h)、[Enemy](F:/AI_WorkSpeace/baka/Source/ai/Public/Enemies/RogueEnemy.h)、[HUD](F:/AI_WorkSpeace/baka/Source/ai/Public/UI/RogueHUD.h)、[武器Catalog](F:/AI_WorkSpeace/baka/Source/ai/Public/Combat/Weapons/RogueWeaponCatalog.h)。

配套：[代码阅读指南](F:/AI_WorkSpeace/baka/Docs/Code-Reading-Guide.md)、[源码目录说明](F:/AI_WorkSpeace/baka/Docs/Class-Organization.md)。
