# GAS 功能面全覆盖改造方案（作品集向）

> **项目定位：作品集 / 学习项目。**
> 因此本文的判断标准**不是**"性能最优"或"工程最稳"，而是：
> **在不毁掉现有成果的前提下，把 GAS 的功能面用满，让每一处改动都能在面试里讲清"为什么用这个而不是那个"。**
>
> 本文只描述**要做什么、为什么、代价多少**。是否执行由你决定。
> 状态：**待评审，尚未动任何代码。**

---

## 0. 先纠正目标定义 —— 这一步选错会白干两周

「完全依赖 GAS」有两个截然不同的含义：

| 含义 | 内容 | 判断 |
|---|---|---|
| **A. GAS 吃掉所有代码** | 敌人 AI、刷怪、商店、对象池、UI 都变成 Ability / GE | ❌ **做不到，也不该做**（见 §4） |
| **B. GAS 功能面用满** | 原生冷却、Periodic GE、Stacking、MMC、Cost、AbilitySet、Enhanced Input、事件驱动 UI | ✅ **这才是作品集该追求的** |

技术面试看的不是你**有多少行是 GAS**，而是你**知不知道 GAS 有哪些能力、什么时候该用哪个**。

一个把 **40% 功能面用满**的项目，比一个把 **15% 功能面用满、代码量翻倍**的项目更容易过面。

**本文按 B 展开。**

---

## 1. 现状盘点：GAS 功能面覆盖率

### 1.1 已经用到的（这部分做得很好，别动）

| 功能 | 证据 |
|---|---|
| ASC 挂 PlayerState（Mixed）/ 敌人自持（Minimal） | `RoguePlayerState.cpp:272-289` |
| AttributeSet（28 属性，含元属性 `Damage` 不复制） | `RogueAttributeSet.h:46-215` |
| 减伤链 + 死亡判定在 `PostGameplayEffectExecute` | `RogueAttributeSet.cpp:176+` |
| Instant GE（伤害 / 升级） | `RogueDamageEffect` / `RogueAttributeUpgradeEffect` |
| Duration GE（6 种状态） | `RogueStateEffects.h` |
| **ExecutionCalculation** | `RogueUpgradeExecution` — 一份 GE 承载 20 种升级 |
| **GameplayCueNotify_Static** | `RogueGameplayCue_Impact` |
| **AbilityTask（含 `bTickingTask` 自定义心跳）** | 2 个：Dash / WeaponFireLoop |
| SetByCaller（`Data.Damage` / `Data.Cooldown` / `Data.Duration`） | 各 GE |
| **原生 Cooldown GE** | ⚠️ **只有冲刺 1 个**：`RogueGameplayAbility_Dash.cpp:42` |
| 预测键（显式传 `GetActivationPredictionKey()`） | `RogueWeaponAbility.cpp:216-248` |
| `RegisterGameplayTagEvent` | 2 处：`RogueCharacter.cpp:178`、`RogueEnemy.cpp:252` |
| `GetGameplayAttributeValueChangeDelegate` | ⚠️ **只有 1 处**：`RogueEnemy.cpp:249` |

### 1.2 完全没用到的高价值功能（本方案的主战场）

| 功能 | 当前状态 | 项目里本可以用在哪 |
|---|---|---|
| **Stacking**（`StackLimitCount` / `StackingType`） | ❌ 零使用 | 可叠加的升级卡 / 层数展示 |
| **Periodic GE**（`Period` / `bExecutePeriodicEffectOnApplication`） | ❌ 零使用 | 御柱持续伤害、经验球磁吸、燃烧 dot |
| **MMC**（`UGameplayModMagnitudeCalculation`） | ❌ 零使用 | 运行时按属性算加成（目前只有 ExecutionCalculation 一种算法） |
| **Cost GE**（`CostGameplayEffectClass`） | ❌ 零使用 | 技能资源消耗（耐力/弹药） |
| **原生冷却 GE** | ⚠️ 10 个能力只有 1 个用 | 武器 6 把 + 敌人 3 种攻击都是**手搓"节拍" GE** |
| **AbilitySet / 数据驱动授技** | ❌ 零使用 | 全部硬编码 `GiveAbility`（`RogueCharacter.cpp:678`、`RogueEnemy.cpp:706-718`） |
| **Enhanced Input** | ❌ 半迁移状态 | 见下 |
| **GameplayCueNotify_Actor** | ❌ 零使用 | 持续型表现（燃烧、护盾） |
| **事件驱动 UI** | ❌ 零使用 | HUD 每帧轮询属性 |

### 1.3 ⚠️ 头号发现：Enhanced Input 处于"半迁移"状态

我把三层都查了：

| 层 | 状态 |
|---|---|
| **配置层** | ✅ 已是 Enhanced Input：`DefaultPlayerInputClass=EnhancedPlayerInput`、`DefaultInputComponentClass=EnhancedInputComponent`（`DefaultInput.ini:79-80`） |
| **资产层** | ❌ **零资产**：`Content/` 下没有任何 `IA_*` / `IMC_*` |
| **代码层** | ❌ **全是 legacy**：`BindAction(TEXT("Dash"), IE_Pressed, ...)`（`RogueCharacter.cpp:1027-1041`），配 `DefaultInput.ini` 里 8 条 `ActionMappings` + 11 条 `AxisMappings` |

也就是说：**引擎默认给你 Enhanced Input，你用的是 UE4 时代的 FName 绑定。**
你自己的代码里也留了这条注释（`RogueCharacter.cpp:1626`）：

> `// 将来迁到 Enhanced Input 后应改为 AbilityLocalInputPressed。`

对于 **UE5.5 的作品集**，这是最显眼的减分项——因为它一眼可查、且是所有 UE 岗位的日常。

---

## 2. Track A：补齐 GAS 功能面（推荐执行）

按 **性价比降序** 排列。工时单位：**人日**（1 人日 ≈ 6 小时有效编码 + 自测）。

### A5. Enhanced Input + GAS 原生输入绑定 ⭐ 最高性价比

| 项 | 内容 |
|---|---|
| **现状** | legacy `BindAction(FName)`，零输入资产 |
| **目标** | `UInputMappingContext` + `UInputAction` 资产；`UEnhancedInputComponent::BindAction(InputAction, ...)`；冲刺改为 `AbilityLocalInputPressed`（走 GAS 标准输入路径） |
| **改动** | 新增 6-8 个 IA 资产 + 1 个 IMC；`RogueCharacter.cpp` 输入段（1027-1041）重写；`RogueGameplayAbility_Dash` 加 `InputID` 绑定；`DefaultInput.ini` 清掉 legacy 映射 |
| **收益** | 🔥 面试最直观的加分项；顺带清掉 19 条 legacy 映射 |
| **风险** | **低**——输入是独立模块，不碰数值、不碰网络 |
| **工时** | **1 - 1.5 人日** |
| **可讲点** | "为什么 GAS 的输入要经 `AbilityLocalInputPressed` 而不是直接调函数" |

### A2. Periodic GE 替换逐帧 Tick 伤害

| 项 | 内容 |
|---|---|
| **现状** | 御柱在 `WeaponTick` 里自己维护 `HellTowerDamageTickTimers` 数组手动计时 |
| **目标** | 改用 Infinite GE + `Period` + `bExecutePeriodicEffectOnApplication`，让 GAS 自己按周期结算 |
| **改动** | `RogueWeapon_HellTower.cpp`（删逐帧计时数组）、`RogueStateEffects.h` 新增周期 GE |
| **收益** | 真实减少逐帧代码；补上 Periodic GE 这个**项目零使用**的常用特性 |
| **风险** | **低**——只影响御柱一把武器，可单独回归 |
| **工时** | **1 人日** |
| **可讲点** | "哪些逻辑该交给 GE 周期、哪些必须留在 Tick" |

### A4. 补一个 MMC（GameplayModMagnitudeCalculation）

| 项 | 内容 |
|---|---|
| **现状** | 零使用。所有数值变化只有 SetByCaller 和 ExecutionCalculation 两种 |
| **目标** | 加一个 MMC，实现"运行时按属性计算量级"，例如 `伤害 = 基础 × (1 + 攻击力/100)` |
| **改动** | 纯新增 1 个类 + 1 个 GE；不动现有链路 |
| **收益** | GAS **三种量级计算方式**（SetByCaller / Execution / MMC）在项目里齐了 |
| **风险** | **极低**——纯新增 |
| **工时** | **0.5 人日** |
| **可讲点** | "MMC / ExecutionCalculation / SetByCaller 各自适合什么场景"——这是 GAS 高频面试题 |

### A7. GameplayCueNotify_Actor

| 项 | 内容 |
|---|---|
| **现状** | 只有 Static cue（命中特效，无状态） |
| **目标** | 加一个 Actor cue 做持续型表现（例如倒计时期间的护盾/燃烧环） |
| **改动** | 新增 1 个类 + 注册；挂在已有的 `State.Downed` / `State.Invulnerable` 上 |
| **收益** | 补齐 Cue 的两种形态；倒地的视觉表现本来也该有 |
| **风险** | **极低** |
| **工时** | **0.5 人日** |

> **第 1 批小计：3 - 3.5 人日**——全部低风险、互不依赖，可以任意顺序做。

---

### A8. AbilitySet（数据驱动授技）

| 项 | 内容 |
|---|---|
| **现状** | 能力全部硬编码 `GiveAbility`（`RogueCharacter.cpp:678`、`RogueEnemy.cpp:706-718`） |
| **目标** | 用 DataAsset 描述"这个角色启动时授予哪些能力 / 哪些属性 / 哪些 GE"，一次性批量授予（Lyra 的 `ULyraAbilitySet` 模式） |
| **改动** | 新增 AbilitySet 类 + 2 个资产（玩家 / 敌人）；改 `EnsureWeaponAbilities` 与 `GrantAttackAbilities` |
| **收益** | 展示 GAS **资产化最佳实践**；敌人新增攻击方式从"改代码"变成"改资产" |
| **风险** | **低**——但要小心 §1 里记录过的坑：**授予时机必须在 `InitializeEnemy()` 末尾**（放在 `ActivatePooledEnemy` 会照上一只怪的原型发技能） |
| **工时** | **1.5 人日** |
| **可讲点** | "能力应该硬编码还是数据驱动" + 那个授予时机的坑 |

### A3. GameplayEffect Stacking

| 项 | 内容 |
|---|---|
| **现状** | 升级往属性里加绝对值（`AttackPowerBonus += M`），买 5 次就是 +5M，看不出"层数" |
| **目标** | 可叠加的升级卡改用 Infinite GE + `StackLimitCount`，用 `GetStackCount()` 读层数并显示"已叠加 N 层" |
| **改动** | `RogueAttributeUpgradeEffect` / `RogueUpgradeExecution` / 抽卡与商店规则 / HUD 展示 |
| **收益** | 补上 Stacking 这个 GAS 核心特性（目前零使用）；UI 上能直观展示 |
| **风险** | **中**——这是唯一会**触碰"数值必须逐项一致"这条红线**的项。这个项目最在意的就是数值不回归 |
| **工时** | **2 人日**（含逐项数值对比验证） |
| **建议** | 单独一批做，做完立刻跑 `Rogue.SelfTest.WeaponUpgradeChain` 对比 |

### A6. UI 事件化替代 Tick 轮询

| 项 | 内容 |
|---|---|
| **现状** | `RogueHUD.cpp`（1338 行）每帧轮询属性与敌人数量；`GetGameplayAttributeValueChangeDelegate` 全项目只用过 1 处 |
| **目标** | 属性/标签变化用委托驱动，替代逐帧查询 |
| **改动** | `RogueHUD.cpp` 的血条 / 数值 / 状态提示段 |
| **收益** | 性能 + 展示 GAS **事件模型** |
| **风险** | **中**——HUD 改动面广，且这块 UI 是原生绘制（不是 UMG），改动容易漏 |
| **工时** | **1.5 - 2 人日** |

> **第 2 批小计：5 - 5.5 人日**

---

### A1. 把武器/敌人的"节拍 GE"换成原生 Cooldown GE ⚠️ 有争议

| 项 | 内容 |
|---|---|
| **现状** | 10 个能力里只有冲刺用了 `CooldownGameplayEffectClass`；武器 6 把 + 敌人 3 种攻击全是手搓 HasDuration GE + 标签查询 |
| **你的理由** | 代码注释写得很清楚：**持续激活的能力用不上 GAS 内置冷却**，所以冷却"退化为节拍" |
| **要改的话** | 得把武器从"持续能力 + 逐帧心跳"改成"每发激活一次能力"，才能走原生冷却 |
| **收益** | 架构上更标准；面试时"为什么不用原生冷却"这个问题不再需要解释 |
| **风险** | **高**。① 会撞上那个**最贵的坑**——客户端施冷却 GE 必须显式传 `CurrentActivationInfo.GetActivationPredictionKey()`，否则静默失败、客户端每帧开一枪；② 逐帧心跳改成一发一次激活，**索敌频率与手感都会变** |
| **工时** | **2 - 3 人日**（含双进程无头验证） |
| **我的建议** | ⚠️ **先不做**。你现在的设计是**正确地**绕开了 GAS 的一个真实限制。面试时讲清"我为什么没用原生冷却"比改掉它更能体现理解深度。如果你仍想做，建议**只在敌人攻击上试点**（三种攻击本来就是一次性的，改造成本最低）。 |

### A9. 加 Cost GE（能力消耗）

| 项 | 内容 |
|---|---|
| **现状** | 零使用。冲刺/攻击都无消耗 |
| **目标** | 加一个资源属性（耐力/弹药）+ Cost GE |
| **注意** | ⚠️ **这会改变玩法**，不是纯技术改动 |
| **工时** | **0.5 - 1 人日** |
| **建议** | 只有在你想扩充玩法时才做；纯为"用上 Cost GE"而加一个资源条，反而显得刻意 |

> **第 3 批小计：2.5 - 4 人日（可选）**

---

## 3. 前置清理（建议在任何改造之前做）

这些是本次分析中已确认的问题，**改动小、风险低，且做完能让后续改造更干净**：

| 项 | 位置 | 工时 |
|---|---|---|
| 🐛 统一性能裁剪阈值（5 处硬编码 40 vs 配置 52） | `RogueWeaponBase.cpp:226` + `RogueHUD.cpp:421/484/535/606` | 0.2 人日 |
| 🧹 清掉已证伪的注释（"加成不复制"已结案） | `RogueAttributeSet.h:159-164` | 0.1 人日 |
| 🧹 删重复 include / 重复前向声明 | `RogueGameMode.cpp:6-7`、`RogueUpgradeExecution.cpp:3-4`、`RoguePlayerState.h:11-12` | 0.1 人日 |
| 📄 修正 Phase 5 标记与旧描述 | `GAS-Main-Architecture-Plan.md:285-289` | 0.2 人日 |
| 📄 重写 `Attribute-System-Migration.md`（整篇在描述已删除的代码） | 该文档 | 0.3 人日 |

> **小计：约 1 人日**

---

## 4. Track B：架构纯度改造（**不推荐**）

如果你的目标是"GAS 吃掉所有代码"，这是唯一的路。我列出代价，供你判断：

| 项 | 目标 | 工时 | 风险 |
|---|---|---|---|
| 敌人移动与索敌 → 持续 GA + 逐帧 Task | 200 敌人 = 200 个 GA 实例每帧跑（当前是一次 float 比较） | 3 人日 + 1 人日性能验证 | 🔴 **可能击穿 2.95ms/帧这条线**，而这是项目**唯一有实测数据**的性能成果 |
| 刷怪 / 波次 → GA | 语义上就是错的：波次是**流程**，不是"谁的能力" | 2 人日 | 🟡 中 |
| 经验球磁吸 → GA | 无收益 | 0.5 人日 | 🟢 低 |

### 为什么我不建议

1. **数学上到不了"完全"**。对象池、空间哈希、经济规则、关卡生成、UMG 布局——GAS **没有对应概念**，改不了。乐观估计做完 Track B，GAS 占比从 **11.6%** 只升到 **~20%**。
2. **会毁掉最亮的那块**。`Docs/Project-Structure.md` §11 记的 200 敌人压测（GAS 开销 0.03ms/帧 ≈1%）是这个项目**最有说服力的一条工程证据**。Track B 的第一个动作就会威胁它。
3. **调试成本**。GAS 调用栈深、GE 数据驱动。现在能一眼看懂的 `HandleEnemyKilled`，会变成"某个 GE 的某个 Execution 改了某个属性"。
4. **面试反效果**。面试官问"你为什么把敌人移动做成能力"，正确答案是"不该做"。**知道边界在哪，比假装没有边界更加分。**

**Epic 自家的 Lyra 也没有全盘 GAS 化**——AI 用 BehaviorTree/StateTree，UI 用 CommonUI，流程用自研 Experience 系统。

---

## 5. 执行顺序与里程碑建议

| 批次 | 内容 | 工时 | 风险 | 收益 |
|---|---|---:|---|---|
| **第 0 批** | 前置清理（§3） | **1 人日** | 🟢 极低 | 代码与文档一致，后续改造更干净 |
| **第 1 批** | A5 Enhanced Input + A2 Periodic GE + A4 MMC + A7 Actor Cue | **3 - 3.5 人日** | 🟢 低 | 🔥 **面试最直观**：输入标准化 + 三个零使用的功能补齐 |
| **第 2 批** | A8 AbilitySet + A3 Stacking + A6 UI 事件化 | **5 - 5.5 人日** | 🟡 中 | GAS 资产化 + 核心特性完整度 |
| **第 3 批** | A1 原生冷却（建议只试点敌人）+ A9 Cost GE | **2.5 - 4 人日** | 🔴 高 | 有争议，可跳过 |
| ~~Track B~~ | ~~AI/刷怪能力化~~ | ~~6.5 人日~~ | 🔴 高 | ❌ 不推荐 |

**推荐路径：第 0 批 → 第 1 批 → 第 2 批，共约 9 - 10 人日。**
做完后 GAS 功能面覆盖率从当前约 **40% → 90%+**，而**架构骨架一行不用改**。

---

## 6. 每批次做完后应该能回答的问题

改造的价值最终体现在**你能不能讲清楚**。做完后你应该能回答：

**第 1 批之后：**
- GAS 的输入为什么要走 `AbilityLocalInputPressed`，直接调函数不行吗？
- 哪些逻辑适合交给 Periodic GE，哪些必须留在 Tick？边界在哪？
- MMC / ExecutionCalculation / SetByCaller 分别适合什么场景？

**第 2 批之后：**
- 能力应该硬编码授予还是数据驱动？各有什么代价？
- GE Stacking 的层数语义与"往属性里加值"有什么本质区别？
- 为什么 UI 要订阅属性变化，而不是每帧读？

**始终能回答（现在就已经做到了）：**
- 为什么伤害必须有唯一入口？
- 为什么判服务端要用 `GetNetMode() != NM_Client` 而不是 `HasAuthority()`？
- 为什么客户端施冷却 GE 会静默失败？（这条坑很多人没踩过，讲出来很加分）
- **为什么敌人 AI 不该做成 GameplayAbility？**（这条最能体现判断力）

---

## 7. 明确不做的事

- ❌ 不改数据资产 schema（`DT_*` / `AS_*` 的行与列保持不变）
- ❌ 不动对象池、空间哈希、刷怪调度、经济规则
- ❌ 不动已完成的 GAS 链路（属性集、伤害入口、状态 GE、预测键处理）
- ❌ 不为了"用上某个特性"而发明玩法

---

## 附：本文的数据来源

- 全量行数统计：`Source/ai` 128 文件 / 25,588 行；GAS 层（`Public|Private/AbilitySystem`）29 文件 / 2,967 行 = **11.6%**
- GAS 功能面：对上述源码全树 grep（`StackLimitCount` / `Period` / `GameplayModMagnitudeCalculation` / `CostGameplayEffectClass` / `AbilitySet` / `AbilityLocalInputPressed` 等关键词逐一核对）
- 输入三态：`Config/DefaultInput.ini` + `Content/` 资产扫描 + `RogueCharacter.cpp:1018-1041`
- 性能红线：`Docs/Project-Structure.md` §11、`Docs/GAS-Main-Architecture-Plan.md` Phase 3
