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
| **武器开火** | ❌ 自研：`ARogueCharacter::TickWeapons` 每帧调 `ARogueWeaponBase::WeaponTick` |
| **武器升级** | ❌ 自研：改武器 Actor 上的 `Config` 结构体 + `Multicast_ApplyWeaponUpgrade` 各端重放 |
| **敌人攻击** | ❌ 自研：`ARogueEnemy::Tick` 里的接触/远程/地面波计时器 |
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

**自检工具（新增，长期保留）**：`Rogue.SelfTest.WeaponUpgradeChain 1`
在服务端逐项施加升级并打印"加成属性 + 每把武器的生效数值"，在各端打印本机读到的加成值。
无头跑法：
```
UnrealEditor.exe <uproject> <map>?listen -game -nullrhi -log
  -ExecCmds="Rogue.SelfTest.WeaponUpgradeChain 1"
```
实测输出（服务端）：伤害 20→30、主武器数量 0→1（只有它 +1）、火箭冷却 2.00→1.75（×2.5）、
迫击炮爆炸范围 2400→2500、地狱塔范围 800→870（×0.70）—— 与自研版本公式逐项一致。

### ❌ 待查根因（阻塞 Phase 2 的客户端一致性）：武器加成属性**没有复制到客户端**

**证据**（同一套属性集、同一次自检、同一份日志）：
| 端 | 生命 | 伤害加成 |
|---|---|---|
| 服务端 | 100.0 | 10.0 →（连接后改）15.0 |
| 客户端 | **100.0** | **0.0**（初始与"连接后再改一次"都没到） |

已排除：
- 属性集注册（客户端 ASC 里就一套集，且就在其中）✓
- UHT 反射标志（`AttackPowerBonus` 与 `Health` 的 `EPropertyFlags` **完全相同**：`0x0010000000000034`）✓
- 复制声明（同一份 `GetLifetimeReplicatedProps`，同样的 `DOREPLIFETIME_CONDITION(..., COND_None)`）✓
- 推送模型（`DOREPLIFETIME_CONDITION` 不设 `bIsPushBased`，不是推送模型属性）✓
- 客户端本地初始化（`InitializeGASAttributes` 在客户端从未运行：客户端日志 0 条，且它只从 `PossessedBy` 调用）✓
- 不是"第二套属性集实例"的问题（曾单独建过 `URogueWeaponAttributeSet`，回退后现象完全相同）✓

**下一步诊断（已想好，未执行）**：
1. 把客户端的自检签名里加上 `Health/MaxHealth/Armor`，第二阶段改成"**连接后既改加成、也打一次伤害**"，
   看客户端的"生命"是否跟着变 —— 一次区分"这类属性不复制"与"该属性集的所有变化都不到客户端"。
2. 在客户端打印 `ASC->GetSpawnedAttributes()[0]->GetClass()` 与 `PlayerState->GetAttributeSet()` 是否同一对象
   （排除"引擎在客户端另建了一个集实例"）。
3. 若确认是"初始同步带、后续变化不带"，检查 `PlayerState` 的 `NetUpdateFrequency`/复制条件与
   `ApplyUpgradeEffectViaGAS` 之后是否需要 `ForceNetUpdate`（商店快照就是这么做的）。

**风险与影响**：服务端结算完全正确（伤害/数量/冷却都对），所以**单机与主机体验不受影响**；
受影响的是客户端的武器表现与客户端本地预测用的数值（客户端仍按"基础值"算）。

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

### ⏳ Phase 2：武器开火改为 GameplayAbility

- 新增 `URogueWeaponAbility`：**持续激活**（`InstancingPolicy = InstancedPerActor`，
  `NetExecutionPolicy = LocalPredicted`），用 `UAbilityTask_WaitDelay` 循环开火
- 开火间隔来自 `GetEffectiveCooldown()`（Phase 1b 之后它就是"数据 + 属性"）
- 分支：服务端 → 生成真实弹体并广播；本人客户端 → 只生成本地表现弹体（保持零延迟手感）
- `ARogueCharacter::TickWeapons` 退化为"只给持续型武器（环绕刀片）跑表现"
- 升级可以顺带改成"能力层"的写法：`ActivationBlockedTags`（如冲刺中禁用某武器）

### ⏳ Phase 3：敌人攻击改为 GameplayAbility

- 接触伤害 / 远程射击 / 地面波各做成 GA，冷却用 Cooldown GE；`ARogueEnemy::Tick` 只决定"要不要打"
- **性能红线**：200+ 敌人不得退化。做法：只有进入射程才 `TryActivateAbility`，
  能力实例 `InstancedPerActor` 且冷却期间不激活；改造后用 `Tools/analyze_perf.py` 对比帧时间

### ⏳ Phase 4：状态/复活全 GE 化 + 清理

- 复活等待用 GE 时长驱动（现在由 PlayerState 上的截止时刻 + 三条路径驱动）
- 删除过渡期的双路径代码（`OnUpgradeApplied` / `ApplyShared*Bonus` / `Multicast_ApplyWeaponUpgrade`）
- 更新 `Docs/Project-Structure.md`，回归验证数据资产仍是唯一数据入口

## 不做的事（有意保留自研）

- **对象池**（`URogueCombatPoolSubsystem`）：GAS 没有对应概念，且 200+ 敌人 + 上千弹体必须复用
- **刷怪调度 / 波次 / 奖励归属**：属于 GameMode 的规则层，不是能力
- **敌人移动与索敌**：每帧的距离判断比 GA 激活便宜得多；Phase 3 只把"攻击"能力化
- **数据资产 schema**：不改列、不改行，改造只改"谁读它、怎么用它"
