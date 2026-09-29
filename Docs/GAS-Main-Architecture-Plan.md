# GAS 主架构改造方案与进度

> 目标：把项目从"GAS 只做数值层"改造成 **GAS 作为 Gameplay 框架**（能力驱动），
> 同时**完整保留现有数据资产**（`DT_WeaponConfig` / `DT_FRogueEnemyArchetypeRow` /
> `DT_FRogueUpgradeDefinitionRow` / `AS_*`）作为唯一数据来源。
> 每阶段的验收标准：Development + DebugGame 编译通过；双进程无头冒烟无 Fatal/Ensure；
> **数值与改造前逐项一致**（这一点比"结构好看"更重要）。

## 现状基线（改造前）

| 系统 | 现在是谁在驱动 |
|---|---|
| 玩家属性 / 伤害 / 状态标签 / 特效 | ✅ GAS（属性集 + GE + Tag + Cue） |
| 冲刺 | ✅ GAS（GameplayAbility + AbilityTask） |
| **武器开火** | ✅ GAS（Phase 2：`URogueWeaponAbility` + 逐帧心跳任务 + 冷却 GE） |
| **武器升级** | ✅ GAS（Phase 1b：改玩家 ASC 上的加成属性，武器读"基础值 + 加成"） |
| **敌人攻击** | ✅ GAS（Phase 3：AI 只决策，接触/远程/地面波三个能力执行 + 冷却 GE） |
| 对象池 / 刷怪 / 商店 / 奖励 | 保留自研（GAS 不负责这些，也不该负责） |

## 阶段与进度

### ✅ Phase 1a：武器加成属性化（基础层，已完成并编译验证）

- 新增 `URogueWeaponAttributeSet`：12 个**加成属性**（伤害/攻速/范围/弹速 + 6 类数量 + 折射 + 爆炸半径）
- 玩家 `ARoguePlayerState` 挂第二套属性集（ASC 的 `InitializeComponent()` 会自动收录 Owner 的
  所有 `UAttributeSet` 默认子对象，见 `AbilitySystemComponent_Abilities.cpp:74-84`），并登记复制
- 新增 12 个 `Upgrade.Weapon.*` 载荷标签，`GetUpgradeAttributeTag()` 现在对武器类升级也返回有效标签
- `URogueUpgradeExecution` 的映射表扩展 12 项 —— **升级 GE 现在也能改武器加成属性**
- `ARogueWeaponBase` 新增 `GetEffective*()`（基础值 + 加成）与 `GetWeaponBonusSet()`
- 顺手修掉一个结构问题：`ATTRIBUTE_ACCESSORS` 宏原来只定义在 `RogueAttributeSet.h` 里，
  第二套属性集无法使用 → 抽到 `RogueAttributeMacros.h`

### ✅ Phase 1b：武器读取"数据表基础值 + GAS 加成"（已完成，服务端数值逐项验证一致）

- 6 把武器全部改为读 `GetEffective*()`；**每把武器的换算系数原样保留**
  （火箭攻速 ×2.5/下限 0.55、迫击炮 ×2.0/下限 0.7、激光 ×2.0/下限 0.35、
   地狱塔伤害 ×0.125/跳间隔 ×0.08/范围 ×0.70、镰刀转速 ×280/半径 ×0.12）
- `ARogueCharacter::ApplyUpgrade` 的武器分支改为**与属性升级同一条 GE 路径**；
  删除 `Multicast_ApplyWeaponUpgrade`（Reliable 多播重放）、`ApplySharedWeapon*Bonus`、
  `DispatchWeaponUpgrade`、以及武器基类与 6 个子类的 `OnUpgradeApplied` / `ApplyShared*Bonus`
- 数量加成按"每把武器的数量升级类型"取（`GetCountUpgradeType()`，子类硬编码）——
  这一条是自检**抓出来的回归**：数据表那列多数行没填，最初直接读它会变成"一张弹体数量卡给 5 把武器各 +1"
- 顺手修掉一个真实缺陷：`URogueAttributeUpgradeEffect` 是 Instant GE，却挂了
  `TargetTagsGameplayEffectComponent` 授予族标签 —— GAS 启动即报错且标签从未生效，已删除该组件
- **跨端一致性已验证通过**（双进程无头，同一玩家 `…-5FD9` 逐行对比）：

  | | 角色快照·伤害加成 | Projectile 生效伤害 |
  |---|---|---|
  | 服务端（落定后地面真值） | 10.00 | 30.00 |
  | 客户端（本机自己算） | **10.00** | **30.00** |

  且客户端日志在"已连上"期间**抓到了变化过程**（快照 5.00 → 10.00，比属性集更新早一帧到），
  说明走的是**变化复制**而不是只有初始同步。加成快照的载体是 `ARogueCharacter`
  （见 `FRogueWeaponBonusValues`），与服务端每帧 `PushWeaponBonusSnapshots()` 的"变化才推"配套。

**自检工具（新增，长期保留）**：`Rogue.SelfTest.WeaponUpgradeChain 1`
分 5 个阶段：①全员 5 项升级 → ②等客户端连上再全员 +5 伤害（验证**变化**复制）
→ ③等 3 秒打印服务端地面真值 → ④全员再 +5（此时必定都在线）→ ⑤再打印地面真值。
无头跑法：
```
UnrealEditor.exe <uproject> <map>?listen -game -nullrhi -log
  -ExecCmds="Rogue.SelfTest.WeaponUpgradeChain 1"
```
实测输出（服务端）：伤害 20→30、主武器数量 0→1（只有它 +1）、火箭冷却 2.00→1.75（×2.5）、
迫击炮爆炸范围 2400→2500、地狱塔范围 800→870（×0.70）—— 与自研版本公式逐项一致。

### ✅ 已结案（原"待查根因"）：不是复制 bug，是**自检本身拿两个玩家对比**

**当时的错误结论**：武器加成属性没有复制到客户端。

**真实原因**：自检只对 `Players[0]`（**主机**玩家）施加升级，而客户端打印的是**它自己**那个
玩家的属性 —— 那个玩家从来没被升级过，本来就是 0.00。两边比的根本不是同一个玩家。
`Players[0]` 在 listen server 上恒为主机，所以这个错误在每次运行里都会稳定复现，
看起来就像"属性恒为 0"。

**为什么会被骗过去**：同一份日志里，客户端的 `护甲 25→13.1→2.3→4.2→9.3`（受伤 + 回充）
是**真的在变**的 —— 那说明客户端本机的属性集复制与更新完全正常。当时把"主机被升级了 15、
客户端自己没被升级所以是 0"读成了"同一套属性集里老属性能复制、新属性不能"。

**修正**（`ARogueGameMode::RunWeaponUpgradeSelfTest`）：
1. 升级对**所有**存活玩家逐个施加（不再只对 `Players[0]`）；
2. 每一行日志都带**玩家名**，服务端/客户端同一玩家可直接对齐；
3. 每台机器额外打印本机武器算出的**生效值**（数量/伤害/冷却/范围）——这才是最终要看的数字。

**当初"已排除"的清单因此全部作废**（它们本来就不是问题）：属性集注册、UHT 反射标志、
复制条件、推送模型、客户端本地初始化、第二套属性集实例。

**仍然成立的分层结论**：跨端快照挂在 `ARogueCharacter` 上（而不是 `ARoguePlayerState`）——
它本来就是角色的战斗参数，与 `bDead` 走同一条已验证的复制通道，分层也更合理。

#### Phase 1b 迁移参考：每把武器的换算系数

⚠️ **同一个升级在不同武器上的换算系数不同**，迁移/回归时必须逐个照搬（否则数值会变）：

| 升级类型 | 主武器 | 火箭 | 迫击炮 | 激光 | 地狱塔 | 镰刀 |
|---|---|---|---|---|---|---|
| 伤害 | `Damage += M` | `+= M` | `+= M` | `+= M` | `BaseDamage += M×0.125`<br>`DamageRampPerTick += M×0.0625` | `Damage += M` |
| 攻速 | `Cooldown −= M`（下限 0.15） | 同左 | 同左 | 同左 | `DamageTickInterval = max(0.03, −M×0.08)` | `RotationSpeed += M×280` |
| 范围 | `Range += M` | 同左 | 同左 | 同左 | 待确认 | 待确认 |
| 弹速 | `ProjectileSpeed += M` | `RocketSpeed += M×0.75` | `MortarLaunchSpeed += M×0.5` | — | — | — |
| 数量 | `Count += M`（下限 1） | 同左 | 同左 | 同左 | 同左 | 同左 |
| 折射 | — | — | — | `RefractionCount += M`（夹 0..8） | — | — |
| 爆炸半径 | — | — | `ExplosionRadiusBonus += M` | — | — | — |

迁移顺序（每把改完即可编译验证，行为不变）：
`RogueWeapon_Projectile` → `_Rocket` → `_Mortar` → `_Laser` → `_HellTower` → `_Scythe`

每把的改法：
1. 删掉该武器的 `OnUpgradeApplied` / `ApplyShared*Bonus` override（连带头文件声明与本地加成成员）
2. 把开火代码里的 `Config.Damage/Cooldown/Range/Count/…` 换成 `GetEffective*()`；
   换算系数不同的（地狱塔伤害/攻速、镰刀转速）在武器侧写成本地 effective 访问器，读**同一个加成属性**
3. 完成后：`ARogueCharacter::ApplyUpgrade` 的武器分支改为 `ApplyUpgradeEffectViaGAS`，
   删除 `Multicast_ApplyWeaponUpgrade` 与 `DispatchWeaponUpgrade`

### ✅ Phase 2：武器开火改为 GameplayAbility（已完成，双进程验证通过）

**新增/改动**
- `URogueWeaponAbility`（`Public|Private/AbilitySystem/Abilities/`）：**持续激活**的自动开火能力
  （`InstancedPerActor` + `NetExecutionPolicy = LocalPredicted` + 资产标签 `Ability.Weapon.AutoAttack`），
  每把武器一个实例。六个子类各只有一行"我驱动哪把武器"。
- `URogueAbilityTask_WeaponFireLoop`：每帧心跳（`bTickingTask = true`）。
  GameplayAbility 没有 Tick，而开火的节奏检查**必须逐帧**（改造前 `TickWeapons` 就是每帧调一次
  `WeaponTick`）—— 换成定时器轮询会让"敌人刚进射程"时多等一帧到几十毫秒。
- `URogueWeaponCooldownEffect`：冷却 GE（`HasDuration`，时长 = SetByCaller `Data.Cooldown`，
  授予 `Cooldown.Weapon.AutoAttack`）。**它是开火节奏的唯一真相源**，武器里的 float 计时器全部删除。
- 四把事件型武器（子弹/火箭/迫击炮/激光）的 `WeaponTick` 拆成 `CollectFireTargets` + `FireAtTargets`；
  地狱塔保留自己的持续模拟（`UsesTargetedFireLoop() == false`，能力只给逐帧心跳）；
  镰刀不参与能力驱动（纯表现轨道，任何机器都要画）。
- `ARogueCharacter::EnsureWeaponAbilities`：授予（只在权威端、幂等）+ 激活（本人本地激活；
  服务端上远端玩家等本人客户端的请求 + 5 秒看门狗兜底）。
- `ARogueCharacter::TickWeapons` **只剩纯表现**（环绕刀片 + 每帧一次的表现 RPC 合并）。

**联机角色分工**（这条是 GAS 天然给的，不再手写判断）
| 机器 | 有能力的 ASC | 结果 |
|---|---|---|
| 服务端 | 每个玩家的 PlayerState | 权威模拟所有人的武器（伤害在这里结算） |
| 本人客户端 | 只有自己那份（技能列表按 `COND_OwnerOnly` 复制） | 本地预测开火，零延迟 |
| 其它客户端 | 没有别人的技能 | 只画服务端广播的那一发 |

**双进程无头验证（同一玩家逐行对比）**

| | Projectile | Rocket | Mortar | HellTower |
|---|---|---|---|---|
| 服务端（权威那份） | 1 | 8 | 56 | 持续心跳 45890 |
| 客户端（本人预测那份） | 0 | 8 | 69 | 持续心跳 110776 |

- 击杀数持续上涨（服务端 78 / 客户端读到的复制值 89）→ 弹体真的打出去并结算了
- 起止均无 `Fatal` / `Ensure` / `LogAbilitySystem: Error`；无"未实现 FireAtTargets"；看门狗**未**触发
  （说明远端玩家的能力是客户端通过 GAS 标准路径激活的，预测没有丢）
- 两端的开火次数只有个位数差异（各端各自索敌，目标选择本来就可能不同），量级与冷却完全一致

**⚠️ 本次挖出来的最贵的一个坑（务必保留注释）**：客户端施加冷却 GE 会**静默失败**。
1. `ASC->ApplyGameplayEffectSpecToSelf(Spec)` 不带预测键 → 引擎 `HasNetworkAuthorityToApplyGameplayEffect`
   在非权威端要求"权威 **或** 有效预测键" → 客户端拿到**空句柄**；
2. 换成引擎自己的 `ApplyGameplayEffectSpecToOwner(...)`（`ApplyCooldown` 就这么写）**仍然失败** ——
   它内部用 `GetPredictionKeyForNewAction()`，那个键只在**预测窗口内**（能力激活的调用栈里）有效，
   而我们的调用发生在逐帧心跳任务里，窗口已经关闭；
3. 正确做法：显式传 `CurrentActivationInfo.GetActivationPredictionKey()`。

症状极隐蔽：冷却挂不上 → `IsWeaponCoolingDown()` 恒为假 → **客户端那把武器每帧开一枪**
（实测客户端火箭 1531 发 / 迫击炮 9486 发，而服务端只有 11/71，表现就是"伤害莫名其妙高好几倍"）。
所以代码里保留了显式报错（句柄无效就 `UE_LOG(Error)`），并且自检报告会打出
`冷却中 / 剩余 / 目标数 / 持续心跳`，让这类失效一眼可见。

### ✅ Phase 3：敌人攻击改为 GameplayAbility（已完成，三条路径逐条验证）

**新增**
- `URogueEnemyAttackAbility`（基类）+ 三个子类（`Public|Private/AbilitySystem/Abilities/`）：
  `_ContactAttack` / `_RangedAttack` / `_GroundWaveAttack`；`InstancedPerActor` + **`ServerOnly`**
  （敌人没有"本人客户端"，攻击表现走已有的复制通道；客户端上敌人**一个技能实例都没有**）。
- `URogueEnemyAttackCooldownEffect`：三种攻击共用一个冷却 GE 类（时长走 SetByCaller
  `Data.Cooldown`，各自的能力实例各有自己的 GE 实例，所以不会互相卡冷却）。
- `URogueAbilityTask_AttackTick`：逐帧心跳，**只有地面波用**（一次性攻击一个任务都不建）。

**AI 与能力的边界（这条边界就是性能红线本身）**

| 谁 | 负责 | 每帧成本 |
|---|---|---|
| `ARogueEnemy::Tick`（AI） | 移动、选目标、**决策**：距离够不够 + 决策节流到点没 | 一次 float 比较（与改造前**逐字相同**） |
| `URogueEnemyAttackAbility` | 真正结算伤害 / 生成弹体 / 推进地面波 + 冷却 GE | 只在"决定要打"时发生 |

所以 **GAS 的调用次数与"实际攻击次数"成正比，与"敌人数量 × 帧数"无关** ——
这是 200+ 敌人不退化性能的结构性保证，不是靠调参。

改造前后的逐项对应：
- `TouchPlayer`（决策+结算混在一起）→ `DecideContactAttack`（AI）+ `ExecuteContactAttack`（能力调用）
- `HandleRangedAttack`（决策）+ `FireRangedShot`（结算）→ `DecideRangedAttack`（AI）+ `ExecuteRangedAttack`（能力调用）
- `HandleGroundWaveAttack`（AI 每帧替能力推进波）→ `DecideGroundWaveAttack`（AI）+ 能力持有
  `ExecuteStartGroundWave` / `ExecuteAdvanceGroundWave`，**波扩散期间 AI 完全不参与**
- 冷却：`ContactTimer` / `RangedAttackTimer` / `GroundWaveAttackTimer` 保留为 **AI 的决策节流**；
  能力侧另有一份冷却 GE（GAS 用它挡住重复激活，也让"这只怪在冷却中"变成可查询状态）。
  计时器只在**激活成功**后重置 —— 万一能力被挡住，AI 下一帧会再试，而不是白等一个冷却。

**⚠️ 本次挖出来的两个坑（都表现为"计数恒为 0"，看起来像功能没实现）**
1. **授予时机**：`URogueSpawnSubsystem::SpawnEnemy` 的调用顺序是
   `ActivatePooledEnemy()` → `InitializeEnemy()`，而 `CurrentArchetype` 是后者才赋值的。
   最初把 `GrantAttackAbilities()` 放在 `ActivatePooledEnemy` 里 → 照着**上一只怪**的原型发技能
   （首次生成时是默认原型）。症状是"远程怪不远程、震荡柱不起波"，而且没有任何报错。
   → 现在授予放在 `InitializeEnemy()` 末尾（那之前 `ResetForPool()` 已经清过技能表）。
2. **验证方法本身**：无头测试里武器会在敌人靠近前把它打死，于是接触/地面波两条路径
   **根本不会被触发**。只统计"执行次数"的话，"没实现"和"没机会触发"完全一样（实测两者都是 0）。
   → 自检加了两个探针（在自检全部完成之后才跑，避免干扰数值对比）：
   `ProbeContactAttack`（把最近的敌人改硬并瞬移到玩家身上）与
   `ProbeGroundWaveAttack`（直接在玩家附近刷一只震荡柱）。
   探针必须要**先把敌人改硬**：普通怪在六把武器面前活不过一次决策 + 起波初始冷却（实测踩了两次）。

**验证（Development + DebugGame 编译通过；双进程无头）**

| 攻击 | AI 决策尝试 | 能力执行 | 备注 |
|---|---|---|---|
| 接触 | 1 | 1 | 探针触发（`HasTouching=1`、能力=已授予、蓝图未重写行为 Tick） |
| 远程 | 4 | 4 | 自然发生（远程原型进入射程） |
| 地面波 | 1 | 1 | 探针触发（震荡柱起波并扩散到结束） |

无 `Fatal` / `Ensure` / `LogAbilitySystem: Error`；帧时在波次 1→4 之间稳定在 ~1.0 ms
（无头 nullrhi 下主要由游戏线程逻辑决定）。自检报告同时打印**尝试**与**执行**两个计数 ——
两者必须一起看，这正是上面第 2 条坑的教训。

**性能这条线要说清楚（避免过度声称）**
- **已论证**：AI 每帧只做一次 float 比较（与改造前逐字相同），GAS 只在"决定要打"时被调用 ——
  所以每帧成本与敌人数量线性且系数不变，GAS 成本与实际攻击次数成正比（报告里的"尝试"计数即证据）。
- **未实测到 200+**：当前无头自检里武器清怪速度快于刷怪，整局场上只有 2–4 只敌人
  （跑满 5 分钟也只到第 4 波）。所以"200+ 敌人帧时"这条**还没有真实数据**，
  要测它需要专门的压测模式（一次性刷 N 只怪 + 对比帧时），属于后续可补的验证项。

### ✅ Phase 4：倒地等待 / 复活无敌改为状态 GE（已完成，双进程验证通过）

**新增两个带时长的状态 GE**（都在 `RogueStateEffects.h`，时长走 SetByCaller `Data.Duration`）
- `URogueDownedEffect` → 授予 `State.Downed`，时长 = 复活等待秒数。
  **它是"还剩多久复活"的唯一真相源。**
- `URogueInvulnerableEffect` → 授予 `State.Invulnerable`，时长 = 复活无敌秒数。

**删掉的旧机制（这才是本阶段的重点）**
| 迁移前 | 迁移后 |
|---|---|
| `ARoguePlayerState::ReviveServerTime`（复制给拥有者的**截止时刻**） | 带时长的 GE 自己会同步给本人，**复制字段直接删除** |
| HUD 用"截止时刻 − 当前服务器时间"自己换算 | `GetDownedSecondsRemaining(ASC)` 读 GE 剩余时长（实现只有一份） |
| 复活无敌 = loose tag + 角色上的 float 截止时刻 + Tick 比对时间 | 带时长的 GE，到期由 GAS 移除 |
| `TickReviveInvulnerability` / `ClearReviveInvulnerability`（loose tag + float 截止时刻 + Tick 比对） | `RemoveInvulnerableEffect()`（按授予标签移除） |

**为什么"移动驱动的清理"必须改掉**：迁移前的无敌到期依赖角色 Tick 在跑，
而这个项目**已经踩过两次**"倒下的角色不再 Tick → 清理逻辑永远不执行"的坑
（复活轮询、客户端自检都被 `if (bDead) return;` 挡掉过）。改成带时长的 GE 之后，
到期由 GAS 负责，与角色是否 Tick 无关。

**保留的三个复活驱动，但判据收成一个闸门**：角色 Tick、`GameMode::TickReviveQueue()`、
角色上的定时器 —— 三者都调 `ARogueCharacter::TryReviveIfExpired()`，判据是
"倒地等待 GE 剩余 <= 0"。保留三个驱动是因为本项目踩过"某个驱动不触发 → 人永远躺着"的坑；
但**真相源只有一个**（GE），所以不会再出现"三个驱动各用一套判据"的问题。

`State.Dead` 与 `State.Downed` 刻意分开：前者 = 已经倒下（伤害闸门、技能阻塞、表现都看它），
后者 = 正在等待复活（只用于倒计时与触发复活）。

**验证（双进程无头）**
- 新增探针 `ProbeDeathAndRevive`（对目标玩家施加致命伤害）——
  原因同 Phase 3：无头自检里武器太强、敌人贴不上来，整局 0 次倒地，不主动打死一个就验证不到。
- 服务端日志：`进入倒地等待：10.0 秒后回出生点复活（等待状态 = GE RogueDownedEffect）`
  → `已复活（回到出生点，生命 50%，1.5 秒无敌；无敌 GE=已挂 当前带 State.Invulnerable=1）` ✓
- 倒计时读数：服务端 两个玩家分别 `复活剩余=4.88 / 4.00`；
  **客户端 本人 `复活剩余=5.27`** ✓ —— 带时长的 GE 确实同步给了本人（HUD 读的就是它）。
  别人那一份在客户端读 0，这是 `Mixed` 模式的固有行为（GE 明细只发拥有者），
  与迁移前 `ReviveServerTime` 的 `COND_OwnerOnly` 完全一致，且 HUD 只用本人那一份。
- 无 `Fatal` / `Ensure` / `LogAbilitySystem: Error`。

### ⏳ Phase 5：清理旧路径 + 更新结构文档 + 回归验证

- 复活等待用 GE 时长驱动（现在由 PlayerState 上的截止时刻 + 三条路径驱动）
- 删除过渡期的双路径代码（`OnUpgradeApplied` / `ApplyShared*Bonus` / `Multicast_ApplyWeaponUpgrade`）
- 更新 `Docs/Project-Structure.md`，回归验证数据资产仍是唯一数据入口

## 不做的事（有意保留自研）

- **对象池**（`URogueCombatPoolSubsystem`）：GAS 没有对应概念，且 200+ 敌人 + 上千弹体必须复用
- **刷怪调度 / 波次 / 奖励归属**：属于 GameMode 的规则层，不是能力
- **敌人移动与索敌**：每帧的距离判断比 GA 激活便宜得多；Phase 3 只把"攻击"能力化
- **数据资产 schema**：不改列、不改行，改造只改"谁读它、怎么用它"
