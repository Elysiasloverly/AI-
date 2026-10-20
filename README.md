# AI

基于 **Unreal Engine 5.5** 开发的俯视角 Rogue-like 动作生存游戏，支持**联机合作**（濒死等待 / 自动复活 / 团灭结束）。

> 运行时单 C++ 模块 `ai` + 编辑器工具模块 `aiEditor` · 玩法动作与数值由 **GameplayAbilitySystem（GAS）** 驱动 · 服务端权威 · 源码按玩法职责组织

---

## 技术栈

**UE 5.5** · C++ · **GameplayAbilitySystem**（ASC / AttributeSet / GameplayEffect / GameplayAbility / GameplayCue / AbilityTask / Infinite GE Stacking / Periodic GE） · GameplayTag · **Enhanced Input** · 网络复制与预测 · DataAsset / DataTable · WorldSubsystem · Slate / UMG

---

## 技术亮点

### 1. 联机合作：GAS 原生复制与预测

- **玩家 ASC 挂 PlayerState**：角色作为 Avatar，通过接管和复制回调刷新 ActorInfo。同 Pawn 复活保留当前能力与属性；更换 Pawn 或重新连接仍需处理绑定、授予与数据恢复。
- **按需选择 GE 复制模式**：玩家 `Mixed`，敌人 `Minimal`。模式控制活动 GE 明细的接收范围，属性是否复制另由 AttributeSet 的复制声明与宿主路径决定。
- **武器开火 = LocalPredicted + 服务端权威**：本人客户端提前产生操作反馈，服务器随后确认，伤害只在服务端结算，队友通过合批多播看到表现
- **濒死默认 10 秒（`DA_PlayerBalance.Revival` 可配）→ 回出生点复活 → 默认 1.5 秒无敌**：角色的 `LifeState` 原子复制生命阶段和服务器复活截止时刻；每名濒死玩家由一个服务端定时器驱动复活，不依赖角色 Tick 或 GE 到期。濒死期间取消全部碰撞、退出怪物索敌并显示 `WBP_ResurrectionScreen`；全员濒死则取消复活并显示 `WBP_DeathScreen`。详见 [玩家生命周期](Docs/Player-LifeCycle.md)

### 2. 性能：历史压测记录与复测入口

对象池 + 空间哈希 + 高负载裁剪共同支撑，并有**可复现的压测命令**：

| 场景（199 只敌人同时在场） | 最近 10 秒平均帧时 |
|---|---|
| 敌人攻击能力**开启** | **2.95 ms/帧** |
| 敌人攻击能力**关闭**（对照组） | 2.92 ms/帧 |

历史记录的两组均值相差 0.03 ms/帧，攻击计数约 205 次/秒。

以上是旧文档记录的两组结果，本轮未重新测量。AI 决策还包含距离、目标与计时器检查，地面波能力通过逐帧 Task 推进；不能据此保证开销与“敌人数 × 帧数”无关，也不能把两组均值差直接等同于 GAS 固有成本。当前性能应在受控场景中重新测量。

### 3. 工程实践：可自检、有踩坑记录

- **五阶段无头自检**（`Rogue.SelfTest.WeaponUpgradeChain`）：验证"升级 → GAS 加成属性 → 武器生效值"跨 4 层的完整链路，双进程逐行对齐服务端与客户端的数值
- **探针机制**：接触攻击 / 地面波 / 死亡复活 / 200 敌人压测——因为**"没实现"与"没机会触发"在计数上完全一样**（这条教训踩过两次）
- **全项目唯一伤害入口**：`RogueAbilitySystem::ApplyDamageToTarget` → 元属性 `Damage` → 减伤链 → 死亡判定
- 关键职责、调用条件与实现限制写在源码注释和 [注释核验记录](Docs/Comment-Verification.md)；迁移过程与当时的日志保留在 [GAS 主架构改造方案](Docs/GAS-Main-Architecture-Plan.md)。

---

## 架构总览

`Public/` 与 `Private/` 按职责对应，WorldSubsystem 放在负责的业务目录：

| 目录 | 职责 |
|---|---|
| **Framework** | GameMode / GameState、整局流程、波次与平衡规则、运行配置 |
| **AbilitySystem** | ASC、AttributeSet、AbilitySet、标签；Abilities / Tasks / GameplayEffects / Cues |
| **Combat** | Weapons / Projectiles / Visuals / Pooling / Targeting；对象池与空间查询 |
| **Progression** | Upgrades / Shop、升级定义、抽卡规则、奖励报价与商店经济 |
| **Player** | 角色、PlayerState（**ASC 挂载点**）、控制器、视觉组件、玩家查询 |
| **Enemies** | 统一敌人运行时、Data 原型与数据表、Spawning 刷怪调度 |
| **World** | 竞技场、商店终端、经验球与场景优化 |
| **UI** | HUD、Widgets 下的 10 个 UMG 基类（8 个菜单 + 玩家状态条 + 敌人血条）及共享视图数据 |
| **Shared** | 多个玩法域共用的枚举、复制载荷与结构体 |
| **Debug** | 自检开关、升级链路和敌人压测探针 |

`ARogueCharacter` 与 `ARogueGameMode` 的函数实现已按职责拆到多个 `.cpp`；类声明、反射名、蓝图接口与运行时对象关系保留。查找具体类和新增文件的约定见 [类文件组织说明](Docs/Class-Organization.md)。

**职责划分**：GameMode 协调局内流程，并保留奖励归属、快照推送与商店操作等逻辑；规则计算和运行时系统按职责拆分。战斗伤害与客户端表现分别执行，部分表现参数仍由武器和角色共同组装。

---

## GAS 架构

注释与当前行为的核验依据、预测和伤害来源等实现边界见 [注释事实核验](Docs/Comment-Verification.md)。历史改造文档中的日志与性能数字不等于本轮已复测结论。

**数值只有一个真相源**：`URogueAttributeSet`（28 个属性）。角色上的 `MaxHealth` / `MoveSpeed` 等是不可编辑的只读缓存。

**主要玩法动作由 GameplayAbility 驱动**：

| 玩法动作 | 能力 | 持续/一次性 | 联机策略 | 谁决定"什么时候" |
|---|---|---|---|---|
| 冲刺 | `URogueGameplayAbility_Dash` | 一次性 | LocalPredicted | 玩家输入 |
| 武器自动开火 | `URogueWeaponAbility` 系列 | 持续（逐帧心跳任务） | LocalPredicted | 能力与冷却 GE；银刃持续轨道保留表现 Tick |
| 敌人接触 / 远程攻击 | `URogueEnemyAttackAbility` 两子类 | 一次性 | ServerOnly | AI 决策（距离 + 节流） |
| 地藏整圈弹幕 | `URogueEnemyRangedAttackAbility` | 一次性齐射 | ServerOnly | AI 冷却决策，一条多播同步整圈 |
| 旧地面波（兼容） | `URogueEnemyGroundWaveAttackAbility` | 持续到波结束 | ServerOnly | 地藏不再启用 |

**其余 GAS 构件**：

- **GameplayEffect**：伤害、22 种升级、死亡 / 刚受伤 / 三种冷却 / 复活无敌；濒死 GE 为 Infinite，承载由 `LifeState` 同步的标签与表现，复活时机由服务端定时器决定
- **Periodic GE**：生命与护甲的自然回复 —— `Period`（0.25s 周期）+ `AttributeBased` 修饰器量级（每次结算重读属性，升级自动生效）+ `OngoingTagRequirements`（`State.Dead` / `State.RecentlyDamaged` 期间自动暂停，条件消失后自动恢复）
- **Infinite GE Stacking**：每种升级对应一个 GE 子类，固定目标属性；`AggregateByTarget` 按目标叠层，`SetByCaller(Data.Magnitude)` 提供单次数值，层数上限为 0（不限）。最大生命 / 护甲上限变化后的补量由角色属性变化委托处理
- **AbilitySet**：玩家默认技能与敌人攻击支持 `URogueAbilitySet` 数据驱动授予，缺少配置时保留原有回退路径。武器能力由各武器的 `GetFireAbilityClass()` 提供，在角色侧幂等授予；能力授予与基础属性 / 常驻 GE 初始化分别管理
- **GameplayCue**：打击特效（`Static`，原生类运行时注册，不依赖 ini 路径）+ 濒死全息环（**`Actor`** 型，挂在 `URogueDownedEffect` 上，进入濒死时授予，复活或团灭时随 GE 移除）
- **AbilityTask**：3 个（冲刺位移、武器开火心跳、地面波心跳）
- **GameplayTag**：全部原生声明（C++ 声明，与 ini 无关）
- **GAS 原生输入路径**：冲刺能力授予时带 `InputID`，输入侧调 `AbilityLocalInputPressed`，由 GAS 自己按 ID 找技能并激活

**继续使用项目自身系统的部分**：

对象池、刷怪与波次、奖励归属、敌人移动与索敌、商店与抽卡规则、数据资产定义。它们与 GAS 协作；是否需要进一步迁移应结合职责与测量结果判断。

---

## 核心玩法系统

### 战斗

- **6 种武器族**：御札、八卦炉、阴阳玉、极限火花、御柱、银刃
- 每种武器有独立的升级换算系数（例如攻速加成在八卦炉上 ×2.5、银刃转速 +280）
- **22 种升级类型**，通过 DataAsset / DataTable 驱动默认数值

### 敌人

- **统一运行时类 + 原型模式**：所有敌人共用 `ARogueEnemy`，差异由原型组合 + 数据表配置驱动，无蓝图子类爆炸
- **13 种敌人**（妖精 / 夜雀 / 鬼 / 人魂 / 鸦天狗 / 妖狐 / 山蜘蛛 / 唐伞 / 地藏 / 月兔 / 毒茸 / 血蝠 / 亡灵）
- **6 种移动模型** + **3 种射击模式**（单发 / 散射 / 整圈弹幕）；地藏使用整圈弹幕，旧地面波代码保留兼容

### 成长

- 升级抽卡（代码默认规则 + DataAsset 资产覆盖双驱动）
- 商店经济（同类型购买永久翻倍、手动刷新周期翻倍、自动补货）
- 波次动态刷怪与 Boss 战奖励

### 濒死与复活

- `Alive → Downed`：停止移动与战斗，取消角色全部碰撞和重叠，怪物只索敌仍存活的玩家；本人显示 `WBP_ResurrectionScreen`
- 队友仍存活时，服务端等待配置的时长（默认 10 秒），在原角色上恢复碰撞、移动和生命，并移回出生位置；收到 `Alive` 后隐藏等待界面
- 全员同时处于濒死状态：整局结束，所有参与者进入 `Eliminated`，取消所有复活定时器；各玩家只显示 `WBP_DeathScreen`，直到重开

---

## 性能优化

- **全局对象池**：池化敌人、御札、敌方弹体、八卦炉 / 阴阳玉、极限火花 Beam、银刃、命中特效、经验球共 **8 类**高频对象
- **空间哈希索敌**：为自动索敌、范围攻击和 HUD 提供候选查询；遍历格子数由原点、半径与格尺寸决定，还要检查格内候选并维护/重建索引，密集分布可能退化
- **高负载特效裁剪**：按存活敌人数自动降级表现（判据唯一：`URogueEnemyTrackerSubsystem::ShouldCullCombatEffects()`）
- **复制调度**：敌人按距离设置 5 / 15 / 30 的复制候选频率；武器表现按角色合批发送不可靠 RPC。实际消息率和流量受帧率、人数、相关性与网络条件影响

---

## 快速开始

无头自检（验证跨端数值一致性，需要两份进程）：

```bash
# 服务端
UnrealEditor.exe ai.uproject /Game/GameAssets/StarterContent/Maps/Minimal_Default?listen -game -nullrhi -log \
  -ExecCmds="Rogue.SelfTest.WeaponUpgradeChain 1"

# 性能压测（200 敌人）
UnrealEditor.exe ai.uproject <map> -game -nullrhi -log \
  -ExecCmds="Rogue.SelfTest.WeaponUpgradeChain 1,Rogue.SelfTest.EnemyStress 200"
```

压测探针在升级链路自检入口内运行，必须同时开启 `WeaponUpgradeChain`。攻击能力对照组在相同命令中追加 `,Rogue.SelfTest.DisableEnemyAttackAbilities 1`；跨端数值验证还需启动客户端连接服务端。

---

## 文档

| 想知道 | 看这里 |
|---|---|
| **类文件在哪里 / 新类放哪里** | [Class-Organization.md](Docs/Class-Organization.md) — 目录职责、文件索引、实现拆分与蓝图兼容约定 |
| **美术资产与玩法配置放在哪** | [Content-Organization.md](Docs/Content-Organization.md)、[Data-Configuration.md](Docs/Data-Configuration.md) — 美术主目录与 `/Game/Data/` 的 7 个配置资产 |
| **现在是什么结构** | [Project-Structure.md](Docs/Project-Structure.md) — 当前目录、运行时对象关系、数据流时序图、池化契约、联机分工 |
| **濒死 / 复活 / 团灭如何同步** | [Player-LifeCycle.md](Docs/Player-LifeCycle.md) — 生命周期真值、幽灵碰撞、默认 10 秒定时器（资产可配）、两个界面与验证用例 |
| **GAS 是怎么迁过来的** | [GAS-Main-Architecture-Plan.md](Docs/GAS-Main-Architecture-Plan.md) — 分阶段改造过程、验证数据、踩坑记录 |
| **各系统职责与协作** | [System-Architecture-Overview.md](Docs/System-Architecture-Overview.md) |
| **要加武器/卡牌/敌人** | [Maintenance-Extension-Guide.md](Docs/Maintenance-Extension-Guide.md) |
| **中文名和代码英文名怎么对应** | [Content-Naming.md](Docs/Content-Naming.md) — 显示名 ↔ 枚举 ↔ 表行键，改名要动的 6 个位置 |
| **血条 / 护甲条 / 经验条怎么改** | [HUD-Vitals-And-Enemy-Bars.md](Docs/HUD-Vitals-And-Enemy-Bars.md) — WBP 基类、生成脚本与数据流 |
| **受伤泛红与镜头震动怎么调** | [Damage-Feedback.md](Docs/Damage-Feedback.md) — 掉血检测、`FlashRoot` 结构、震动参数与替换方式 |
| **敌方弹幕怎么换外观** | [Enemy-Projectile-Blueprints.md](Docs/Enemy-Projectile-Blueprints.md) — `BP_EnemyProjectile`、每敌人覆盖与池化 |
| **经验球怎么换外观** | [Experience-Pickup-Blueprint.md](Docs/Experience-Pickup-Blueprint.md) — `BP_ExperiencePickup`、BP 钩子与尺寸规则 |
| **全部系统架构图** | [CurrentAllSystemsArchitecture.md](Docs/CurrentAllSystemsArchitecture.md) |
| **历史：自研属性系统** | [Attribute-System-Migration.md](Docs/Attribute-System-Migration.md) — ⚠️ 描述的是已删除的系统，保留用于解释"为何转向 GAS" |

---

## 演示

【自制肉鸽游戏《AI》v2版 —— 添加了商店系统，增加了游戏难度】
https://www.bilibili.com/video/BV1mbQGBkE1v
