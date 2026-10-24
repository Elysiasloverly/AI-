# /Game/Data 数据资产覆盖审计

审计日期：2026-10-20  
审计对象：`/Game/Data` 下的 DataAsset / DataTable，以及对应 C++ 结构里的代码默认值。

## 1. 已确认在数据资产里的内容

### `/Game/Data/Core/DA_GameBalance`（`URogueGameBalanceAsset`）

| 组 | 已进资产 | 说明 |
|---|---|---|
| `Spawn` | `MaxAlivePerWave`、`MaxAliveLimit`、`BossAliveReduction`、`BossAliveMinimumLimit`、`BatchRules`、`AccelerationPerWave`、`MaxAcceleration`、`EarlyWaves`、`MinimumInterval` | 刷怪数量、批量规则、加速、开局放缓、最短间隔 |
| `Shop` | `OfferCount`、`OfferCost`、`RefreshCost`、`AutoRefreshInterval` | 商店报价数量、购买/刷新价格、自动刷新间隔 |
| `EnemyStats` | `Health`、`MaxHealth`、`Speed`、`Damage`、`HealthGrowthStages` | 普通敌人基础值与成长 |
| `BossStats` | `Health`、`Damage`、`Speed`、`SpeedMultiplier`、`EarlyStage`、`LateStage`、`LateStageStartWave` | Boss 基础值与阶段成长 |
| `Rewards` | `EnemyExperience`、`EnemyGold`、`BossExperience`、`BossGold` | 击杀奖励 |
| 其他 | `EnemyBalanceRows`、`EnemyWaveEntries` | 敌人平衡行与波次表 |

### `/Game/Data/Player/DA_PlayerBalance`（`URoguePlayerBalanceAsset`）

| 组 | 已进资产 |
|---|---|
| `BaseStats` | 生命、护甲、移速、跳跃、重力、土狼时间、输入缓冲、Dash、回复、经验、拾取半径等 |
| `WeaponBonusCaps` | 攻击、攻速、范围、弹体/八卦炉/极限火花/阴阳玉/银刃数量、银刃尺寸、**红雾吸血上限**等 |
| `InitialWeaponBonuses` | 初始武器加成 |
| `Progression` / `SpawnState` / `Revival` | 升级、出生、复活配置 |

### `/Game/Data/Weapons/DT_WeaponConfig`

已覆盖：`WeaponClass`、`ProjectileClass`、`RocketClass`、`MortarProjectileClass`、`BeamClass`、`BladeClass`、伤害、冷却、范围、数量、弹速、爆炸半径、星尘折射、阴阳玉参数、随机弹道开关等。

### `/Game/Data/Enemies/DT_EnemyArchetypes`

已覆盖：`EnemyType`、`BaseMoveMultiplier`、`AttackAbilitySet`、`MaxAlive`、`MaxSpawnPerWave`、`ProjectileClass`、`MovementModelOverride`、`SpecialParams`、`BossOverride`。  
移动、射击、特殊攻击参数从 `SpecialParams` 读取（一怪一块，见 [敌人原型表结构](Enemy-Archetype-Table.md)）。

### 升级

| 资产 | 已覆盖 |
|---|---|
| `DA_UpgradeRules` | 武器升级规则、基础升级、最短 Dash 冷却 |
| `DT_UpgradeDefinitions` | 22 张卡的 `Type`、`Title`、`Description`、`Magnitude`；`ScytheLifesteal` 另有 `FirstMagnitude=0.20`、`MaxMagnitude=1.0` |
| `DA_UpgradeDefinitions` | 定义表引用 |

## 2. 还没搬进 `/Game/Data` 的内容

### 2.1 原 GameMode 默认结构体（已搬到 `DA_GameBalance`）

| 内容 | 现在的位置 |
|---|---|
| 对象池初始容量 | `DA_GameBalance.Pools` |
| 运行时性能阈值 | `DA_GameBalance.Performance` |
| 基础刷怪设置 | `DA_GameBalance.Spawn` 的 `BaseEnemiesPerWave` / `EnemiesPerWaveIncrement` / `EnemiesPerWaveLimit` / `SpawnRadius` / `SpawnInterval` / `BaseMaxAliveEnemies` / `EnemySpawnMinimumDistance` / `GroundTrace*` |
| Boss 节奏 | `DA_GameBalance.BossSpawn` 的 `BossWaveInterval` / `BossBonusSelections` / `BossSpawnDistance` / `BossSpawnMinimumDistance` |
| 场景优化 | `DA_GameBalance.Scene` |

`ARogueGameMode` 里仍保留 `SpawnSettings` / `BossSettings` / `PoolSettings` / `RuntimePerformanceSettings` 作为**无资产兜底**；`ConfigureRuntimeAssets()` 加载 `DA_GameBalance` 后会先用资产值覆盖它们。`Scene` 已进资产，但 `URogueSceneSubsystem::OptimizeLevelEnvironment` 当前没有调用点，属于“数据已收拢、运行时尚未接线”。

### 2.2 武器公式系数（已搬到 `DT_WeaponConfig`）

`FRogueWeaponTableRow` 新增并由各派生武器读取：

- `MinCooldown`
- `CooldownReductionScale`
- `RefractionRangeBonusScale`
- `RefractionDamageScale`
- `RocketSeekRangeBonus`
- `RocketExplosionRadiusRangeBonusScale`
- `MortarExplosionRadiusRangeBonusScale`
- `HellTowerRangeBonusScale`
- `BeamLifetimePadding`
- `SpreadStepDegrees`
- `BaseDamageBonusScale`
- `DamageRampBonusScale`
- `DamageTickIntervalReductionScale`

`DT_WeaponConfig` 已按武器行填入当前值；Laser/Rocket/Mortar 不再写死 `0.35/0.55/0.7`，而是读 `MinCooldown`。

### 2.3 敌人 `Visual`（已搬到 `DT_EnemyArchetypes`）

`FRogueEnemyArchetypeRow` 新增：

- `Visual`：`VisualKey` / `MeshScale` / `MeshLocation` / `MeshRotation` / `CapsuleRadius` / `CapsuleHalfHeight`
- `BossVisual`：Boss 形态的同一组字段

`ToArchetype()` 在行数据存在时直接使用表格视觉，不再只能依赖 `MakeXxxArchetype()` 的 C++ 基线；表格已填 13 行普通/Boss 视觉。

### 2.4 投射物/表现类的默认值

以下默认值在 C++，没有统一数据资产入口：

- 玩家弹体：`CollisionRadius=16`、`PooledLifetime=3`、`Speed=1600`、`Damage=20`
- 八卦炉：`CollisionRadius=18`、`PooledLifetime=5`、`Damage=40`
- 敌人弹体：`CollisionRadius=22`、`BaseSpeed=920`、`PooledLifetime=4`、`ImpactEffectScale=0.24`
- 环绕银刃：碰撞半径 `46`、命中冷却 `0.35`
- 阴阳玉：`MinimumLaunchSpeed=680`、落点脉冲 `4.8`

其中一部分会被 `DT_WeaponConfig` 或 BP 覆盖，但碰撞尺寸、寿命、部分表现参数没有统一来源。

### 2.5 HUD / 相机 / 输入

- 玩家状态、波数与金币已移入 `WBP_PlayerVitals`（命名绑定），Canvas 不再绘制这些文字/状态条；敌人血条兜底与伤害数字仍在 `RogueHUD.cpp`。
- 相机臂长度 `900`、相机延迟 `5`、胶囊 `42/96`、旋转速率 `720` 仍在 `RogueCharacter.cpp`。
- 输入资产目录与 `IMC_Default` / `IA_*` 名称仍是代码兜底；每个动作有 `TSoftObjectPtr` 可覆盖。
- HUD 的 8 个 WBP 路径（含主菜单、教程、设置）已在 P0 改为 `DefaultGame.ini` 的 `TSoftClassPtr` 配置。

### 2.6 P1 视觉资产（分批迁移中）

已经把 C++ 视觉组件删掉、改由 BP SCS 提供的：

- 玩家弹体：`RogueProjectile.cpp` + `BP_RogueProjectile`（Mesh/Glow/Ring/Trail/GlowLight）
- 环绕银刃：`RogueOrbitingBlade.cpp` + `BP_RogueOrbitingBlade`（Mesh + Overlay）
- 阴阳玉落点指示：`RogueMortarProjectile.cpp` + `BP_RogueMortarProjectile`（LandingIndicator）

仍由 C++ 创建/加载、尚未迁移的：

- 命中特效：`RogueImpactEffect.cpp`
- 极限火花束：`RogueLaserBeam.cpp`
- 地藏内置视觉：`RogueEnemy.cpp`
- 经验球：`RogueExperiencePickup.cpp`
- 倒地 cue：`RogueGameplayCue_Downed.cpp`

这些类的资产路径和组件创建逻辑还没有整体替换为 BP SCS；需要按 P1 后续批次处理。

## 3. 重复/冲突点（已处理）

1. **刷怪配置**：`DA_GameBalance.Spawn` / `BossSpawn` 现在是唯一配置源；GameMode 结构体只在没有资产时兜底。
2. **商店默认值**：`FRogueShopSystem::Reset()` 不再写死 `6/22/14/60`；打开商店时由 `DA_GameBalance.Shop` 的 `Configure` 写入。
3. **武器冷却/公式**：派生武器统一读 `DT_WeaponConfig` 的 `MinCooldown` 和新增公式字段，不再写死 `0.35/0.55/0.7`。
4. **敌人视觉**：`DT_EnemyArchetypes` 的 `Visual` / `BossVisual` 覆盖 C++ 基线；C++ 基线只作为缺行兜底。
5. **玩家上限**：`DA_PlayerBalance.WeaponBonusCaps` 仍是唯一业务上限；`RogueAttributeSet` 的 `1.0f` 只保留为属性自身的数学安全钳制，不是第二套策划配置。

仍属于代码/配置而非数据资产的部分：HUD 文字/颜色/布局、相机/胶囊、输入资产名、投射物碰撞/寿命默认值，以及 P1 尚未迁移的视觉类。


## 4. 建议迁移顺序

1. `DA_RuntimeConfig`：池大小、性能阈值、基础刷怪、Boss 节奏、场景优化。
2. 把 `FRogueWeaponConfig` 的派生系数补齐（`MinCooldown` 统一读取，增加 `SpreadStepDegrees`、范围/爆炸系数等）。
3. 把敌人 `Visual` 字段加入 `FRogueEnemyArchetypeRow`，或拆成 `DT_EnemyVisuals`。
4. 把投射物碰撞/寿命等默认值收进 `FRogueWeaponConfig` 或 `DA_CombatDefaults`。
5. 完成 P1 的 BP 视觉迁移后，删除 C++ 里的占位资产路径。
