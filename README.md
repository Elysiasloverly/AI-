# AI

基于 **Unreal Engine 5.5** 开发的俯视角 Rogue-like 动作生存游戏，支持**联机合作**（濒死等待 / 自动复活 / 团灭结束）。

> 单 C++ 模块 · 玩法动作与数值由 **GameplayAbilitySystem（GAS）** 驱动 · 服务端权威 · 源码按玩法职责组织

---

## 技术栈

**UE 5.5** · C++ · **GameplayAbilitySystem**（ASC / AttributeSet / GameplayEffect / GameplayAbility / GameplayCue / AbilityTask / Infinite GE Stacking / Periodic GE） · GameplayTag · **Enhanced Input** · 网络复制与预测 · DataAsset / DataTable · WorldSubsystem · Slate / UMG

---

## 技术亮点

### 1. 联机合作：GAS 原生复制与预测

- **ASC 挂 PlayerState 而非 Pawn** —— PlayerState 生命周期覆盖整局，Pawn 死亡/重生/重连都不用重新初始化能力与属性
- **按需选择复制模式**：玩家 `Mixed`（GE 明细只发拥有者），敌人 `Minimal`（属性不复制，服务端算）
- **武器开火 = LocalPredicted + 服务端权威**：本人客户端零延迟预测，伤害只在服务端结算，队友通过合批多播看到表现
- **濒死固定 10 秒 → 回出生点复活 → 默认 1.5 秒无敌**：角色的 `LifeState` 原子复制生命阶段和服务器复活截止时刻；每名濒死玩家由一个服务端定时器驱动复活，不依赖角色 Tick 或 GE 到期。濒死期间取消全部碰撞、退出怪物索敌并显示 `WBP_ResurrectionScreen`；全员濒死则取消复活并显示 `WBP_DeathScreen`。详见 [玩家生命周期](Docs/Player-LifeCycle.md)

### 2. 性能：200 敌人规模下的实测数据

对象池 + 空间哈希 + 高负载裁剪共同支撑，并有**可复现的压测命令**：

| 场景（199 只敌人同时在场） | 最近 10 秒平均帧时 |
|---|---|
| 敌人攻击能力**开启** | **2.95 ms/帧** |
| 敌人攻击能力**关闭**（对照组） | 2.92 ms/帧 |

→ **GAS 化的敌人攻击在 200 敌人规模下开销约 0.03 ms/帧（≈1%）**，对应约 205 次攻击/秒。

这个结果不是靠调参得来的，而是结构性保证：**AI 只做决策（一次 float 比较），能力只在实际要打时激活**——所以 GAS 的调用次数与实际攻击次数成正比，**与"敌人数量 × 帧数"无关**。

### 3. 工程实践：可自检、有踩坑记录

- **五阶段无头自检**（`Rogue.SelfTest.WeaponUpgradeChain`）：验证"升级 → GAS 加成属性 → 武器生效值"跨 4 层的完整链路，双进程逐行对齐服务端与客户端的数值
- **探针机制**：接触攻击 / 地面波 / 死亡复活 / 200 敌人压测——因为**"没实现"与"没机会触发"在计数上完全一样**（这条教训踩过两次）
- **全项目唯一伤害入口**：`RogueAbilitySystem::ApplyDamageToTarget` → 元属性 `Damage` → 减伤链 → 死亡判定
- 关键取舍与踩坑均写在代码注释里（客户端施冷却 GE 为何静默失败、敌人能力授予时机为何影响原型等），详见 [GAS 主架构改造方案](Docs/GAS-Main-Architecture-Plan.md)

---

## 架构总览

`Public/` 与 `Private/` 按职责对应，WorldSubsystem 放在负责的业务目录：

| 目录 | 职责 |
|---|---|
| **Framework** | GameMode / GameState、整局流程、波次与平衡规则、运行配置 |
| **AbilitySystem** | ASC、AttributeSet、AbilitySet、标签；Abilities / Tasks / Effects / Cues |
| **Combat** | Weapons / Projectiles / Effects / Pooling / Targeting；对象池与空间查询 |
| **Progression** | Upgrades / Shop、升级定义、抽卡规则、奖励报价与商店经济 |
| **Player** | 角色、PlayerState（**ASC 挂载点**）、控制器、视觉组件、玩家查询 |
| **Enemies** | 统一敌人运行时、Data 原型与数据表、Spawning 刷怪调度 |
| **World** | 竞技场、商店终端、经验球与场景优化 |
| **UI** | HUD、Widgets 下的 8 个 UMG 基类及共享视图数据 |
| **Shared** | 多个玩法域共用的枚举、复制载荷与结构体 |
| **Debug** | 自检开关、升级链路和敌人压测探针 |

`ARogueCharacter` 与 `ARogueGameMode` 的函数实现已按职责拆到多个 `.cpp`；类声明、反射名、蓝图接口与运行时对象关系保留。查找具体类和新增文件的约定见 [类文件组织说明](Docs/Class-Organization.md)。

**核心设计原则**：GameMode 仅负责流程协调，具体逻辑下沉至 Rules / System / Subsystem 各层；玩法参数（伤害、冷却）与表现参数（Mesh 缩放、Glow）严格分层解耦。

---

## GAS 架构

**数值只有一个真相源**：`URogueAttributeSet`（28 个属性）。角色上的 `MaxHealth` / `MoveSpeed` 等是不可编辑的只读缓存。

**主要玩法动作由 GameplayAbility 驱动**：

| 玩法动作 | 能力 | 持续/一次性 | 联机策略 | 谁决定"什么时候" |
|---|---|---|---|---|
| 冲刺 | `URogueGameplayAbility_Dash` | 一次性 | LocalPredicted | 玩家输入 |
| 武器自动开火 | `URogueWeaponAbility` 系列 | 持续（逐帧心跳任务） | LocalPredicted | 能力与冷却 GE；镰刀持续轨道保留表现 Tick |
| 敌人接触 / 远程攻击 | `URogueEnemyAttackAbility` 两子类 | 一次性 | ServerOnly | AI 决策（距离 + 节流） |
| 敌人地面波 | `URogueEnemyAttackAbility_GroundWave` | 持续到波结束 | ServerOnly | 同上（起波后由能力推进扩散） |

**其余 GAS 构件**：

- **GameplayEffect**：伤害、20 种升级、死亡 / 刚受伤 / 三种冷却 / 复活无敌；濒死 GE 为 Infinite，承载由 `LifeState` 同步的标签与表现，复活时机由服务端定时器决定
- **Periodic GE**：生命与护甲的自然回复 —— `Period`（0.25s 周期）+ `AttributeBased` 修饰器量级（每次结算重读属性，升级自动生效）+ `OngoingTagRequirements`（`State.Dead` / `State.RecentlyDamaged` 期间自动暂停，条件消失后自动恢复）
- **Infinite GE Stacking**：每种升级对应一个 GE 子类，固定目标属性；`AggregateByTarget` 按目标叠层，`SetByCaller(Data.Magnitude)` 提供单次数值，层数上限为 0（不限）。最大生命 / 护甲上限变化后的补量由角色属性变化委托处理
- **AbilitySet**：玩家默认技能与敌人攻击支持 `URogueAbilitySet` 数据驱动授予，缺少配置时保留原有回退路径。武器能力由各武器的 `GetFireAbilityClass()` 提供，在角色侧幂等授予；能力授予与基础属性 / 常驻 GE 初始化分别管理
- **GameplayCue**：打击特效（`Static`，原生类运行时注册，不依赖 ini 路径）+ 濒死全息环（**`Actor`** 型，挂在 `URogueDownedEffect` 上，进入濒死时授予，复活或团灭时随 GE 移除）
- **AbilityTask**：3 个（冲刺位移、武器开火心跳、地面波心跳）
- **GameplayTag**：全部原生声明（C++ 声明，与 ini 无关）
- **GAS 原生输入路径**：冲刺能力授予时带 `InputID`，输入侧调 `AbilityLocalInputPressed`，由 GAS 自己按 ID 找技能并激活

**有意保留自研的部分**（GAS 没有对应概念，或收益为负）：

对象池（200+ 敌人 + 上千弹体必须复用）、刷怪调度与波次、奖励归属、敌人移动与索敌（每帧距离判断比 GA 激活便宜得多）、商店与抽卡规则、数据资产 schema。

---

## 核心玩法系统

### 战斗

- **6 种武器族**：普通子弹、火箭、迫击炮、激光、地狱塔、镰刀
- 每种武器有独立的升级换算系数（例如攻速加成在火箭上 ×2.5、镰刀转速 +280）
- **20 种升级类型**，通过 DataAsset / DataTable 驱动默认数值

### 敌人

- **统一运行时类 + 原型模式**：所有敌人共用 `ARogueEnemy`，差异由原型组合 + 数据表配置驱动，无蓝图子类爆炸
- **9 种敌人**（猎手 / 冲锋者 / 坦克 / 环绕者 / 射手 / 游击者 / 炮兵 / 喷射者 / 震荡柱）
- **6 种移动模型** + **3 种射击模式**（单发 / 散射 / 地面波）

### 成长

- 升级抽卡（代码默认规则 + DataAsset 资产覆盖双驱动）
- 商店经济（同类型购买永久翻倍、手动刷新周期翻倍、自动补货）
- 波次动态刷怪与 Boss 战奖励

### 濒死与复活

- `Alive → Downed`：停止移动与战斗，取消角色全部碰撞和重叠，怪物只索敌仍存活的玩家；本人显示 `WBP_ResurrectionScreen`
- 队友仍存活时，服务端等待固定 10 秒，在原角色上恢复碰撞、移动和生命，并移回出生位置；收到 `Alive` 后隐藏等待界面
- 全员同时处于濒死状态：整局结束，所有参与者进入 `Eliminated`，取消所有复活定时器；各玩家只显示 `WBP_DeathScreen`，直到重开

---

## 性能优化

- **全局对象池**：池化敌人、玩家子弹、敌方弹体、火箭 / 迫击炮、激光 Beam、镰刀、命中特效、经验球共 **8 类**高频对象
- **空间哈希索敌**：为自动索敌、爆炸范围判定、激光折射、HUD 血条筛选提供近邻查询（1600 半径只需扫 25 格，而非全表）
- **高负载特效裁剪**：按存活敌人数自动降级表现（判据唯一：`URogueEnemyTrackerSubsystem::ShouldCullCombatEffects()`）
- **复制预算**：敌人复制频率按距离分档 5 / 15 / 30 Hz；武器表现每帧合批成一条不可靠 RPC（稳态约 240 条/秒，而非每发一条）

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
| **现在是什么结构** | [Project-Structure.md](Docs/Project-Structure.md) — 当前目录、运行时对象关系、数据流时序图、池化契约、联机分工 |
| **濒死 / 复活 / 团灭如何同步** | [Player-LifeCycle.md](Docs/Player-LifeCycle.md) — 生命周期真值、幽灵碰撞、10 秒定时器、两个界面与验证用例 |
| **GAS 是怎么迁过来的** | [GAS-Main-Architecture-Plan.md](Docs/GAS-Main-Architecture-Plan.md) — 分阶段改造过程、验证数据、踩坑记录 |
| **各系统职责与协作** | [System-Architecture-Overview.md](Docs/System-Architecture-Overview.md) |
| **要加武器/卡牌/敌人** | [Maintenance-Extension-Guide.md](Docs/Maintenance-Extension-Guide.md) |
| **全部系统架构图** | [CurrentAllSystemsArchitecture.md](Docs/CurrentAllSystemsArchitecture.md) |
| **历史：自研属性系统** | [Attribute-System-Migration.md](Docs/Attribute-System-Migration.md) — ⚠️ 描述的是已删除的系统，保留用于解释"为何转向 GAS" |

---

## 演示

【自制肉鸽游戏《AI》v2版 —— 添加了商店系统，增加了游戏难度】
https://www.bilibili.com/video/BV1mbQGBkE1v
