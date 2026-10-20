# 最优行动方案（作品集向）

> **这份是执行版**：只列要做的事、为什么、工时、验收标准。
> 全量选项与备选方案的完整分析见 [GAS-Full-Coverage-Plan.md](./GAS-Full-Coverage-Plan.md)。
>
> **判据**：不是"GAS 覆盖率最大"，而是 **「面试收益 ÷ (人日 × 风险)」最大**。
> **总量**：约 **4.5 人日**，不新增任何玩法功能。
> 状态：**执行中** —— 第 1 批已完成并验证；第 2 批完成 1 项（原第 6 项被替换，见文末「执行记录」）。

---

## 执行记录

### ✅ 已完成

| 项 | 内容 | 验证 |
|---|---|---|
| **第 1 批 ①** | 重写 `README.md` | — |
| **第 1 批 ②** | 统一性能裁剪阈值（`RogueWeaponBase.cpp` 1 处 + `RogueHUD.cpp` 4 处） | 全树 grep 复核，残留命中只剩注释与唯一实现 |
| **第 1 批 ③** | 清理注释与文档债（共 9 处） | — |
| **第 1 批 ④** | 修正 `GAS-Main-Architecture-Plan.md` 的 Phase 5 状态与 Phase 1a 的失效描述 | — |
| **第 2 批 ⑥′** | **生命/护甲回复改为 Periodic GE**（替换原"御柱"项，理由见下） | 见下方数值验证 |
| **第 2 批 ⑦** | **倒地表现：Actor 型 GameplayCue**（`ARogueGameplayCue_Downed`） | 见下方 cue 验证 |

### 🔄 计划修正（两处，都基于实际读码后的判断）

**修正 1：原第 6 项「御柱改 Periodic GE」→ 改为「生命/护甲回复改 Periodic GE」**

读码后发现御柱**不适合** Periodic GE，原因是结构性的：

- 它的伤害**逐跳递增**（`HellTowerCurrentDamages[Index] += DamageRampPerTick`）——Periodic GE 的修饰器量级无法表达"每次比上次多"；
- 它打的是**动态外部目标**（塔自己锁定的那只敌人，状态存在武器上）——而 Periodic GE 的修饰器只能作用于**自己的 Target**（玩家）。

强行迁移只会把武器状态塞进 GE 的 ExecutionCalculation，是更差的架构。
**所以御柱保留在 Tick 里** —— 这条边界本身就是"哪些逻辑该留在 Tick"的好素材。

替代落点选 **生命/护甲回复**：它才是 Periodic GE 的教科书场景，而且一次补齐了项目里**三个零使用**的机制：

| 机制 | 迁移前（逐帧 C++） | 迁移后（GE） |
|---|---|---|
| **Period** | 每帧调用 | 每 0.25 秒结算一次 |
| **AttributeBased 修饰器量级** | `Rate * DeltaSeconds` | `Rate × Period`（每次结算重新读属性，升级自动生效） |
| **OngoingTagRequirements** | `if (HasMatchingGameplayTag(...)) return;` | 声明式标签条件（`State.Dead` / `State.RecentlyDamaged`） |
| 上限钳制 | 自己 `FMath::Min(Max, ...)` | 交给 `PreAttributeChange`（已有逻辑） |

顺带删除了逐帧函数 `HandleRecovery` / `HandleArmorRecharge`，以及因此变成死代码的 `SetCurrentArmor`。

**修正 2：原第 5 项「补一个 MMC」→ 暂缓**

逐一排查后，**这个项目里没有 MMC 的自然落点**：
- 升级量级走 SetByCaller + ExecutionCalculation（一份 GE 承载 20 种升级，必须用 Execution）；
- 减伤链刻意留在 `PostGameplayEffectExecute`（元属性模式的全部意义就在这里）；
- 武器生效值（"基础值 + 加成×本武器系数"）在武器侧算，因为**系数是每把武器自己的知识**。

为"用上 MMC"而造一个场景，产出的是**死代码**——在作品集里比"没有 MMC"更减分。
补上的是真正缺的那个修饰器计算方式：**AttributeBased**（见修正 1）。

**修正 3：原第 8 项「UI 事件化」→ 不做**

方案里写的现状是"HUD 每帧轮询属性"。实际读码后，这个前提**不成立**：

- HUD 读的是 `PlayerCharacter->GetCurrentHealth()` —— 那是**缓存 float 的成员读取**，不是 GAS 属性查找；
- Canvas HUD 的 `DrawHUD` **本来就是每帧调用**（这就是它的工作方式），"不每帧"没有意义；
- 敌人血条缓存的刷新**已经是按间隔的**（0.2s / 0.35s），不是每帧。

而 `SyncAttributeMirrors()` 的轮询是**项目有意识的设计**，`RogueCharacter.cpp:203-207` 写明了三条理由：

> · 属性有 14 个，逐个绑委托是纯样板；
> · 轮询读的是 FGameplayAttributeData 的属性值（直接属性读取，不是查表），成本可以忽略；
> · 只有"死亡"必须事件驱动 —— 它要在标签出现的当下被捕获，等下一帧就晚了。

把 4 个 float 赋值改成 14 条委托绑定，收益约等于零，却引入一个新的失败模式：
**漏绑一个属性 → 镜像永远停在旧值 → HUD 显示错误数值**。这是典型的"为了用特性而用特性"。

### 数值验证（无头运行实测）

`Rogue.SelfTest.WeaponUpgradeChain 1`，单进程 `-nullrhi`。周期性报告里护甲的变化：

| 报告 | 世界时间 | 护甲 | 说明 |
|---|---|---|---|
| #5 | 69.57 | 0.0 | `State.RecentlyDamaged` 抑制中 |
| #6 | 74.75 | 11.2 | 抑制解除，开始回复 |
| #7 | 75.00 | **22.5** | 距上一报告 **0.25 秒**，**+11.3** |
| #8 | 75.25 | **25.0** | 精确停在上限（钳制生效） |

- 期望每周期增量 = `ArmorRechargeRate(45) × Period(0.25)` = **11.25**，实测 **+11.3** —— 逐项吻合
- 与迁移前的 `45 × DeltaSeconds` 每秒积分**完全等价**
- 健康值保持 80.9 不变也正确（`HealthRegenPerSecond` 默认 0）
- 全程无 `Fatal` / `Ensure` / `LogAbilitySystem: Error`
- **回复 GE 挂载日志 2 条、属性初始化 2 次** → 第二次初始化跳过了重复挂载，**幂等性得到验证**

回归对照（武器生效值，与 `GAS-Main-Architecture-Plan.md` 记录的历史基线一致）：
八卦炉冷却 **1.75**、阴阳玉范围 **2500**、御柱范围 **870**、伤害 **30**。**数值红线未被触碰。**

### Actor Cue 验证（无头运行实测）

`URogueDownedEffect` 的 `GameplayCues` 上登记了 `GameplayCue.State.Downed`，
由新增的 `ARogueGameplayCue_Downed`（`AGameplayCueNotify_Actor` 子类）处理。实测日志：

```
[Rogue] 原生 GameplayCue 注册：提交 4 条，cue set 中已映射 4 条（管理器=GameplayCueManager_0）
[Rogue] 倒地表现已激活（Actor 型 GameplayCue）：目标=BP_RogueCharacter_C_0
[Rogue] 倒地表现已结束（Actor 型 GameplayCue）：目标=BP_RogueCharacter_C_0
…（6 次倒地 / 复活循环，共 12 条）
```

- **注册 4/4 映射成功** → 原生类路径对 Actor 型 cue 同样有效（与 Static 型一样不需要 .uasset）
- **`OnActive` / `OnRemove` 成对触发 6 次** → "表现挂 GE 生命周期"这条链路是通的
- 全程无 `Fatal` / `Ensure` / `LogAbilitySystem: Error`
- **不需要新建任何 .uasset**：环用 StarterContent 的 Torus，材质用角色外壳同款的全息材质，
  与 `ARogueCharacterVisualComponent` 同一套 `ConstructorHelpers` 按路径加载的做法

> ⚠️ 踩到的坑：基类名是 **`AGameplayCueNotify_Actor`**（`A` 前缀 —— 它是个 Actor），
> 不是 `UGameplayCueNotify_Actor`。写成 `U` 前缀时 UHT 报的是
> "Unable to find parent class type"，而不是"类名拼错了"，容易往错误方向排查。

### 一条值得保留的引擎事实

迁移前 C++ 里必须写 `if (!HasAuthority()) return;` 才挡住客户端自己回甲。
改成 GE 之后这条**由引擎保证**：`GameplayEffect.cpp:4325` 要求
`Owner->IsOwnerActorAuthoritative()` 才创建周期定时器，客户端拿到的副本只负责显示复制值。

### ⏳ 未做

| 项 | 原因 |
|---|---|
| **第 1 批 ② Enhanced Input** | ✅ **已完成** —— 11 个资产就位、三层打通、旧映射与 legacy 支路已删除。实测 `IMC=IMC_Default（13 条按键映射），动作 10/10 就绪`。**唯一待办**：实机确认按键方向（尤其 S 是后退）与是否补方向键，见 [Enhanced-Input-Migration.md](./Enhanced-Input-Migration.md) |
| 第 3 批 ⑧ UI 事件化 | **调研后决定不做** —— 理由见下方「修正 3」 |

---

## 为什么是这 9 项 —— 一句话逻辑

| 批次 | 性质 | 不做会怎样 |
|---|---|---|
| 第 1 批 | **门面 + 红牌** | reviewer 前 10 分钟就扣分 |
| 第 2 批 | **三个 GAS 故事** | 少了三段能讲的面试回答 |
| 第 3 批 | 锦上添花 | 无影响 |

---

## 第 1 批 · 门面与红牌（2 人日）

### 1. 重写 README ⭐ 投入产出比最高

**现状**（`README.md`）三处硬伤：

| 问题 | 实际 |
|---|---|
| 技术栈列表里**没有 GAS** | 列的是 `Gameplay Framework` —— 那是 GameMode/PlayerState 那套，**不是** GAS，读者会被误导 |
| **七层架构里没有 `AbilitySystem`** | 实际八层，唯一漏掉的正好是 GAS 层（2,967 行 / 29 文件） |
| "约 **11,300** 行 C++" | 实际 **25,588 行** |

**要做：**
- [ ] 技术栈补 `GameplayAbilitySystem（ASC / AttributeSet / GameplayEffect / GameplayAbility / GameplayCue / AbilityTask）`
- [ ] 架构表从七层改八层，补 AbilitySystem 行
- [ ] 修正行数为 25,588（或写"约 25.6k"）
- [ ] 新增「**技术亮点**」章节，至少覆盖：
  - 联机合作（Mixed/Minimal 复制模式、倒地/复活、双进程无头验证）
  - 性能（199 敌人 / GAS 开销 0.03ms/帧 ≈1%，附压测命令）
  - 踩坑记录指引（链接到 `GAS-Main-Architecture-Plan.md`）
- [ ] 文档导航补上 `Project-Structure.md` 与 `GAS-Full-Coverage-Plan.md`

**工时**：0.5 人日　**风险**：无（纯文档）

---

### 2. Enhanced Input 迁移 ⭐ 一眼可见的红牌

**现状**：三层不一致。

| 层 | 状态 |
|---|---|
| 配置层 | ✅ 已是 Enhanced Input（`DefaultInput.ini:79-80`） |
| 资产层 | ❌ **零资产** —— `Content/` 下无任何 `IA_*` / `IMC_*` |
| 代码层 | ❌ **全 legacy** —— `BindAction(TEXT("Dash"), IE_Pressed, ...)`（`RogueCharacter.cpp:1027-1041`） |

代码里自己留了注释（`RogueCharacter.cpp:1626`）：
> `// 将来迁到 Enhanced Input 后应改为 AbilityLocalInputPressed。`

**要做：**
- [ ] 新建 `IA_*` 资产：`IA_Move`、`IA_Dash`、`IA_Interact`、`IA_Jump`、`IA_TogglePauseMenu`、`IA_ChooseUpgrade1~3`
- [ ] 新建 `IMC_Default` 映射上下文，在 `PlayerController`/`Character` 里 `AddMappingContext`
- [ ] `RogueCharacter.cpp` 输入段改用 `UEnhancedInputComponent`
- [ ] **冲刺改为 `AbilityLocalInputPressed`**（GAS 原生输入路径）
- [ ] 清理 `DefaultInput.ini` 的 8 条 `ActionMappings` + 11 条 `AxisMappings`

**工时**：1.2 人日　**风险**：🟢 低（输入是独立模块，不碰数值/网络）
**验收**：移动/冲刺/交互/暂停/选卡 5 类输入全部可用；PIE 单人 + 双进程各跑一次

---

### 3. 统一性能裁剪阈值 🐛 真 bug

**问题**：两套阈值同时存在，配置项失效。

```
RogueGameModeConfig.h:30            CombatEffectsCullEnemyCount = 52   ← 配置声称的阈值
RogueEnemyTrackerSubsystem.cpp:60   count >= 52                        ← ShouldCullCombatEffects()
RogueWeaponBase.cpp:226             ShouldCull() || count >= 40        ← 硬编码 40 抢先
RogueHUD.cpp:421 / 484 / 535 / 606  ShouldCull() || count >= 40        ← 硬编码 40 ×4
```

**后果**：① `CombatEffectsCullEnemyCount = 52` 对武器/HUD 路径无效（往上调没用）；② **40~52 只敌人区间里，武器已在降级表现，弹体特效还全开**。

**要做：**
- [ ] `IsHeavyCombat()` 改为 `return Tracker && Tracker->ShouldCullCombatEffects();`
- [ ] `RogueHUD.cpp` 4 处改为调用同一个判据（不要各写一遍）

**工时**：0.2 人日　**风险**：🟢 极低
**验收**：`Rogue.SelfTest.EnemyStress 200` 下裁剪行为一致，帧时无变化

---

### 4. 清理注释与文档债

**问题**：**已被证伪的结论还留在代码里当设计依据**，reviewer 会读成"这个项目有未解 bug"。

| 位置 | 问题 |
|---|---|
| `RogueAttributeSet.h:159-164` | 称"第二套属性集没复制到客户端…没定位清楚根因" —— 根因已查明是**自检拿错玩家**（`GAS-Main-Architecture-Plan.md:67-89` 已结案） |
| `GAS-Main-Architecture-Plan.md:285-289` | Phase 5 仍标 `⏳`，且待办是 Phase 4 前的旧描述 |
| `Attribute-System-Migration.md` | **整篇在描述已删除的自研系统**，第 90-96 行"遗留问题"针对不存在的代码 |
| `RogueGameMode.h:149-152` | 注释说快照走 PlayerState，实现早已改到 `ARogueCharacter` |
| `RogueGameMode.cpp:6-7` | 同一 include 写了两遍 |
| `RogueUpgradeExecution.cpp:3-4`、`RoguePlayerState.h:11-12` | 重复 include / 重复前向声明 |
| `RogueUpgradeExecution.cpp:25 / 35` | 两处都叫"（URogueAttributeSet）"，实为两个不同用途，句子自相矛盾 |

**要做：**
- [ ] 逐条修正上述 7 处
- [ ] `Attribute-System-Migration.md` 顶部加"⚠️ 本文描述的是已删除的旧系统，新架构见 `Project-Structure.md`"，或直接归档

**工时**：0.5 人日　**风险**：🟢 极低（纯注释/文档）

---

## 第 2 批 · 三个 GAS 故事（1.8 人日）

> 共同点：**每一项都能变成一段 5 分钟的面试回答**，且工作量小、风险低。

### 5. 补一个 MMC（`UGameplayModMagnitudeCalculation`）

- **现状**：❌ **零使用**。项目只有 SetByCaller 与 ExecutionCalculation 两种算法
- **要做**：新增 1 个 MMC 类 + 1 个 GE，实现"运行时按属性算量级"（例：`伤害 = 基础 × (1 + 攻击力/100)`）
- **故事**：「MMC / ExecutionCalculation / SetByCaller 分别适合什么场景」——GAS 经典面试题，你现在只答得出两种
- **工时**：0.5 人日　**风险**：🟢 极低（纯新增，不动现有链路）

### 6. 御柱改为 Periodic GE

- **现状**：`RogueWeapon_HellTower.cpp` 自己维护 `HellTowerDamageTickTimers` 数组手动计时
- **要做**：改用 Infinite GE + `Period` + `bExecutePeriodicEffectOnApplication`，删掉逐帧计时数组
- **故事**：「哪些逻辑该交给 GE 周期、哪些必须留在 Tick」——**边界判断力比会用 API 更值钱**
- **工时**：1.0 人日　**风险**：🟢 低（只影响一把武器，可单独回归）
- **验收**：御柱伤害频率与总量与改造前逐项一致

### 7. 补一个 GameplayCueNotify_Actor

- **现状**：❌ 只有 Static cue
- **要做**：新增 1 个 Actor cue，挂在 `State.Downed` / `State.Invulnerable` 上（倒地表现本来也该有）
- **故事**：补齐 Cue 的两种形态；"表现层为什么要和逻辑解耦"
- **工时**：0.3 人日　**风险**：🟢 极低

---

## 第 3 批 · 有余力再做（1 人日）

### 8. UI 事件化（只做最能讲的两处）

- **现状**：`RogueHUD.cpp`（1338 行）每帧轮询属性与敌人数；`GetGameplayAttributeValueChangeDelegate` 全项目**只用过 1 处**
- **要做**：**只改「血条」与「数值显示」两处**为委托驱动，不必推翻整个 HUD
- **故事**：「为什么 UI 要订阅属性变化，而不是每帧读」
- **工时**：1.0 人日　**风险**：🟡 中（HUD 原生绘制，改动易漏）
- **验收**：受击/回血/升级时数值刷新无延迟、无闪烁

---

## 明确不做（附理由）

| 不做 | 理由 |
|---|---|
| ❌ **Stacking**（2 人日） | 唯一会**触碰"数值必须逐项一致"红线**的项，而这项目最在意的就是数值不回归。风险 > 收益 |
| ❌ AbilitySet（1.5 人日） | 收益只是"更规范"，讲不出强故事 |
| ❌ Cost GE | 会改玩法。为"用上 Cost GE"而加个资源条显得刻意 |
| ❌ **原生冷却重构**（2-3 人日） | 🔴 现有设计是**正确地绕开了 GAS 的真实限制**（持续能力用不上内置冷却）。改掉还得重踩预测键的坑 |
| ❌ **Track B：AI / 刷怪能力化**（6.5 人日） | 🔴 会威胁**唯一有实测数据**的性能成果（199 敌人 2.95ms/帧）。且面试时"为什么不做"比"做了"更加分 |
| ❌ 不动 | 数据资产 schema（`DT_*` / `AS_*`）、对象池、空间哈希、经济规则 |

---

## 执行顺序与回归验证

```
① README 重写           ── 独立，先做，立刻见效
② 阈值统一 + 注释债      ── 独立，纯清理，零风险
   ↓
③ Enhanced Input        ── 独立模块，不碰数值/网络
   ↓
④ MMC（纯新增）          ── 零风险，先拿一个故事
⑤ Periodic GE（御柱）   ── 只影响一把武器
⑥ Actor Cue             ── 纯表现
   ↓
⑦ UI 事件化（选做）
```

⚠️ **①②③ 做完后必须重跑一次双进程无头验证**（`Rogue.SelfTest.WeaponUpgradeChain 1` + `-nullrhi`）——
这是项目最硬的证据链，别省。

---

## 完成后的面试问答清单

**现在就答得出（已具备）：**
- 为什么伤害必须有唯一入口？
- 为什么判服务端用 `GetNetMode() != NM_Client` 而不是 `HasAuthority()`？
- 客户端施冷却 GE 为什么会静默失败？
- **为什么敌人 AI 不该做成 GameplayAbility？**（最能体现判断力）

**第 2 批之后新增：**
- GAS 的输入为什么要走 `AbilityLocalInputPressed`？
- 哪些逻辑交给 Periodic GE，哪些必须留在 Tick？
- MMC / ExecutionCalculation / SetByCaller 怎么选？

**这 7 个问题覆盖了 GAS 从入门到架构判断的完整梯度。**

---

## 一句话总结

> **4.5 人日，不新增任何功能，只做「补门面 + 修红牌 + 补三个故事」。**
> 做完后项目从"一个 GAS 用得很好的 UE 项目"，变成"**一个能讲清楚的 GAS 项目**"——
> 差别全在 reviewer 能不能看见。
