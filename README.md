# AI

基于 **Unreal Engine 5.5** 开发的俯视角 Rogue-like 动作生存游戏，支持**联机合作**（倒地 / 复活）。

> **25,588 行 C++**（128 文件，单模块）· 玩法动作与数值由 **GameplayAbilitySystem（GAS）** 驱动 · 服务端权威

---

## 技术栈

**UE 5.5** · C++ · **GameplayAbilitySystem**（ASC / AttributeSet / GameplayEffect / GameplayAbility / GameplayCue / AbilityTask / ExecutionCalculation / Periodic GE） · GameplayTag · **Enhanced Input** · 网络复制与预测 · DataAsset / DataTable · WorldSubsystem · Slate / UMG

---

## 技术亮点

### 1. 联机合作：GAS 原生复制与预测

- **ASC 挂 PlayerState 而非 Pawn** —— PlayerState 生命周期覆盖整局，Pawn 死亡/重生/重连都不用重新初始化能力与属性
- **按需选择复制模式**：玩家 `Mixed`（GE 明细只发拥有者），敌人 `Minimal`（属性不复制，服务端算）
- **武器开火 = LocalPredicted + 服务端权威**：本人客户端零延迟预测，伤害只在服务端结算，队友通过合批多播看到表现
- **倒地 10 秒 → 回出生点复活 → 1.5 秒无敌**：全部由带时长的 GameplayEffect 承担，**不依赖角色 Tick**——从结构上消除了"倒下的角色不再 Tick 导致清理永不执行"这类 bug

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

项目按 **八层**组织：

| 层级 | 文件 | 行数 | 职责 |
|---|---:|---:|---|
| **Core** | 33 | 5,268 | GameMode 流程调度、波次规则、升级抽卡、商店经济、对象池、敌人注册表、空间哈希 |
| **Enemies** | 10 | 4,563 | 统一敌人运行时 + 行为原型、数据表、视觉资源、敌方弹体 |
| **Combat** | 26 | 4,500 | 6 把武器 + 弹体 / 光束 / 刀片 / 命中特效（全部走对象池） |
| **Player** | 10 | 3,797 | 角色、PlayerState（**ASC 挂载点**）、视觉组件、平衡资产 |
| **AbilitySystem** | 29 | 2,967 | **GAS 层**：属性集、GE、四类能力、能力任务、Cue |
| **UI** | 4 | 2,619 | HUD（原生绘制）+ 8 个 UMG 基类 |
| **World** | 6 | 971 | 竞技场生成、商店终端、经验球 |
| **Subsystems** | 8 | 891 | 对象池、敌人追踪、刷怪调度、场景优化 |
| **合计** | **128** | **25,588** | |

**核心设计原则**：GameMode 仅负责流程协调，具体逻辑下沉至 Rules / System / Subsystem 各层；玩法参数（伤害、冷却）与表现参数（Mesh 缩放、Glow）严格分层解耦。

---

## GAS 架构

**数值只有一个真相源**：`URogueAttributeSet`（28 个属性）。角色上的 `MaxHealth` / `MoveSpeed` 等是不可编辑的只读缓存。

**玩法动作全部是 GameplayAbility**：

| 玩法动作 | 能力 | 持续/一次性 | 联机策略 | 谁决定"什么时候" |
|---|---|---|---|---|
| 冲刺 | `URogueGameplayAbility_Dash` | 一次性 | LocalPredicted | 玩家输入 |
| 6 把武器自动开火 | `URogueWeaponAbility`（6 子类） | 持续（逐帧心跳任务） | LocalPredicted | 能力自己（冷却 GE 是唯一节奏来源） |
| 敌人接触 / 远程攻击 | `URogueEnemyAttackAbility` 两子类 | 一次性 | ServerOnly | AI 决策（距离 + 节流） |
| 敌人地面波 | `URogueEnemyAttackAbility_GroundWave` | 持续到波结束 | ServerOnly | 同上（起波后由能力推进扩散） |

**其余 GAS 构件**：

- **GameplayEffect**：伤害 1 + 升级 1 + 状态 7（死亡 / 刚受伤 / 三种冷却 / 倒地等待 / 复活无敌）
- **Periodic GE**：生命与护甲的自然回复 —— `Period`（0.25s 周期）+ `AttributeBased` 修饰器量级（每次结算重读属性，升级自动生效）+ `OngoingTagRequirements`（`State.Dead` / `State.RecentlyDamaged` 期间自动暂停，条件消失后自动恢复）
- **ExecutionCalculation**：一份 GE 承载全部 20 种升级
- **GameplayCue**：打击特效（`Static`，原生类运行时注册，不依赖 ini 路径）+ 倒地全息环（**`Actor`** 型，挂在 `URogueDownedEffect` 的 `GameplayCues` 上，由 GE 生命周期驱动 —— 不需要任何代码记得去关掉它）
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
  -ExecCmds="Rogue.SelfTest.EnemyStress 200"
```

---

## 文档

| 想知道 | 看这里 |
|---|---|
| **现在是什么结构** | [Project-Structure.md](Docs/Project-Structure.md) — 八层结构、运行时对象关系、数据流时序图、池化契约、联机分工 |
| **GAS 是怎么迁过来的** | [GAS-Main-Architecture-Plan.md](Docs/GAS-Main-Architecture-Plan.md) — 分阶段改造过程、验证数据、踩坑记录 |
| **各系统职责与协作** | [System-Architecture-Overview.md](Docs/System-Architecture-Overview.md) |
| **要加武器/卡牌/敌人** | [Maintenance-Extension-Guide.md](Docs/Maintenance-Extension-Guide.md) |
| **全部系统架构图** | [CurrentAllSystemsArchitecture.md](Docs/CurrentAllSystemsArchitecture.md) |
| **历史：自研属性系统** | [Attribute-System-Migration.md](Docs/Attribute-System-Migration.md) — ⚠️ 描述的是已删除的系统，保留用于解释"为何转向 GAS" |

---

## 演示

【自制肉鸽游戏《AI》v2版 —— 添加了商店系统，增加了游戏难度】
https://www.bilibili.com/video/BV1mbQGBkE1v
