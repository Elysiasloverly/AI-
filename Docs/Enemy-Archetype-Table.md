# 敌人原型表结构

`/Game/Data/Enemies/DT_EnemyArchetypes` 的行结构是 `FRogueEnemyArchetypeRow`。

数据链路（每只敌人初始化时走一次）：

```text
ARogueEnemy::InitializeEnemy()
└─ RogueEnemyArchetypes::BuildEnemyArchetypeFromDataTable(表, EnemyType, bIsBoss)
   ├─ 找到行 → FRogueEnemyArchetypeRow::ToArchetype(bIsBoss)
   │            先取 BuildEnemyArchetype() 的类型基线，再用行内字段逐项覆盖
   └─ 没找到行 → 回退 BuildEnemyArchetype() 硬编码基线（打 Verbose 日志）
```

行查找以行内 `EnemyType` 字段为准，不依赖 RowName（`FindRowNameForEnemyType` 逐行匹配）。

## 顶层列（11 个）

| 列 | 作用 |
|---|---|
| `EnemyType` | 敌人类型；同一类型只应有一行 |
| `BaseMoveMultiplier` | 基础移动倍率（0 表示不动） |
| `AttackAbilitySet` | 可选攻击能力集；留空时按原型标志推导授予默认接触/远程能力 |
| `MaxAlive` | 同时存活上限，0 = 不限（动态闸门，死了腾出名额） |
| `MaxSpawnPerWave` | 每波累计刷新上限，0 = 不限（静态预算，死了不补） |
| `ProjectileClass` | 该类型使用的弹体蓝图；留空回退敌人 BP / 原生类 |
| `MovementModelOverride` | 移动模型覆盖，默认「类型基线」；手动指定可强制替换（见下文） |
| `SpecialParams` | **专属参数列**：一个怪物一个子结构，只填 `EnemyType` 对应的那个 |
| `BossOverride` | 作为 Boss 时的可选覆盖参数 |
| `Visual` | 普通形态视觉：VisualKey / MeshScale / MeshLocation / MeshRotation / Capsule |
| `BossVisual` | Boss 形态视觉；独立配置缩放和胶囊，不再只靠 C++ 基线 |

## 专属参数：一怪一块

`SpecialParams` 下 13 个子结构，名字与 `ERogueEnemyType` 一一对应，互不共享：

| 怪物 | 块名 | 覆盖内容 | 字段语义 |
|---|---|---|---|
| 妖精 | `Rifleman` | 风筝移动 + 单发 | 全部哨兵（0=基线） |
| 夜雀 | `Assault` | 爆发冲刺 + 散射 | 移动直接覆盖 / 射击哨兵 |
| 鬼 | `Juggernaut` | 重型单发（移动只有顶层倍率） | 全部哨兵 |
| 人魂 | `Orbiter` | 环绕移动 + 单发 | 移动直接覆盖 / 射击哨兵 |
| 鸦天狗 | `Marksman` | 风筝移动 + 单发 | 全部直接覆盖 |
| 妖狐 | `Skirmisher` | Z 字移动 + 散射 | 移动直接覆盖 / 射击哨兵 |
| 山蜘蛛 | `Cannoneer` | 风筝移动 + 重型单发 | 全部直接覆盖 |
| 唐伞 | `Scatterer` | 风筝移动 + 散射（3 发） | 全部直接覆盖 |
| 地藏 | `ShockPillar` | 固定柱体 + 整圈弹幕 | 弹幕直接覆盖（移动被强制固定） |
| 月兔 | `Sniper` | 风筝移动 + 单发狙击 | 全部直接覆盖 |
| 毒茸 | `Shotgunner` | 风筝移动 + 散射（5 发） | 全部直接覆盖 |
| 血蝠 | `Repeater` | 风筝移动 + 单发速射 | 全部直接覆盖 |
| 亡灵 | `Ringcaster` | 风筝移动 + 整圈弹幕 | 弹幕直接覆盖 / 移动哨兵 |

`ToArchetype()` 只读取当前 `EnemyType` 对应的块，填错块不会生效。

### 两种字段语义

- **直接覆盖**：字段值无条件覆盖类型基线，默认值 = 该怪物的非 Boss 基线。
  常见于从旧共享块迁过来的字段（移动权重、单发/散射参数），行存在时以表为准。
- **哨兵（0 = 跟随基线）**：`0` / 不填 = 保持类型基线（含 Boss 基线差分），`> 0` 才覆盖。
  用于新增的射击段与全新块（妖精/鬼/亡灵的移动段），老资产不填即保持原行为。
  带负数的权重类字段用"非零"判断，可以填负值（近距离排斥）。

射击模式（单发/散射/整圈）、移动模型**不在专属块里**：由 `EnemyType` 的 C++ 基线决定，
避免"漏填就把远程怪改成直冲"。需要换模型时用 `MovementModelOverride` 列（见下）。

### 距离类字段 ≠ 攻击范围

块里的 `PreferredRange`（偏好距离）、`FarDistance`（远距离）、`CloseDistance`（近距离）
和各类 `Weight` **只参与走位计算**（`ARogueEnemy::GetMovementDirection`），
决定"想站多远、什么时候追、什么时候后撤绕圈"，**不参与攻击判定**：

- 攻击距离门控已整体删除：`DecideRangedAttack()` 只检查冷却，
  `IsInRangedAttackRange()` 对有远程攻击的原型恒返回 `true`；
- `MinimumRange` / `MaximumRange` / `AttackRangeMinFactor` 已从结构里移除，没有字段能恢复门控；
- 把 `FarDistance` / `CloseDistance` 调小只会让怪贴得更近，不会让它"打不到"；
- 运行时结构里的 `FRogueEnemyRangedArchetype::PreferredRange`（只写不读的历史镜像）已删除，
  距离数值只保留 `Movement` 一份，避免两处概念混淆。

震荡柱/亡灵的 `BulletRing`（`FRogueEnemyBulletRingSettings`）导入时会经 `Sanitized()`
钳制到安全范围（弹数 8–128、弹速 100–3000 等），蓝图/配置导入越界值不会直接进运行时。

## 移动模型覆盖

`MovementModelOverride` 默认「类型基线」，可选：直线追击 / 爆发冲刺 / 环绕运动 /
远程风筝 / Z字形追击 / 固定不动。

- 在 `ToArchetype()` 最后应用，**显式选择优先于类型基线**（包括地藏的强制固定不动）。
- 换模型不会自动换参数：新模型会沿用该行的现有参数运行（比如把风筝怪改成环绕，
  环绕用的半径权重仍来自风筝参数），效果需要实测后再调参。

## BossOverride

对**除地藏/亡灵以外的所有远程类型**生效（环形弹幕两族不读 Boss 覆盖，
弹幕参数在各专属块里单独调）。`bEnableBossOverride` 打开后：

- **float 字段**：`> 0` 才覆盖，否则保留基础值（0 即"未设置"的哨兵）；
- **bool 字段**（`bBossLargeVisual` / `bBossSpawnMuzzleEffect`）：bool 没有"未设置"哨兵，
  取**叠加语义**——Boss 覆盖只能把表现**打开**，不能关掉基础配置里已打开的表现；
- `BossSpreadAngleStep` 只对散射类（夜雀/妖狐/唐伞/毒茸）有意义。

## 视觉

`Visual` / `BossVisual` 直接在表里配置：`VisualKey`、`MeshScale`、`MeshLocation`、
`MeshRotation`、`CapsuleRadius`、`CapsuleHalfHeight`、`bUseBossMaterialOverride`。

- C++ 侧 `ARogueEnemy` 初始化末尾调用 `OnApplyVisualStyle()`（BlueprintNativeEvent，
  原生默认空实现），外观由各敌人 BP 重写该事件、读取行内视觉字段来应用。
- `VisualKey` 在 C++ 里只写不读（基线赋值 + 测试断言），真正的消费方是蓝图。
- **行存在时视觉整块以表为准**：行里 `Visual` 保持结构默认值也会盖掉基线视觉。
- 基线里"非地藏 Boss 缩放 ×1.75、胶囊 62/116、Boss 材质"只在**整行缺失**兜底时生效；
  行存在时 Boss 尺寸完全由 `BossVisual` 决定。

## 结构迁移记录

### 一怪一块（本版）

旧结构是 7 个"行为家族"共享块，13 种怪物按类型复用；现拆为一怪一块：

| 旧块 | 新落点 |
|---|---|
| `Charger`（冲锋者） | `Assault`（夜雀） |
| `Shooter`（射手） | `Marksman`（鸦天狗），并复制到 `Sniper` / `Repeater` |
| `Artillery`（炮兵） | `Cannoneer`（山蜘蛛） |
| `Spitter`（喷射者） | `Scatterer`（唐伞），并复制到 `Shotgunner` |
| `Orbiter`（环绕者） | `Orbiter`（人魂，原名不变） |
| `Skirmisher`（游击者） | `Skirmisher`（妖狐，原名不变） |
| `ShockPillar`（震荡柱） | `ShockPillar`（地藏），冷却/弹幕复制到 `Ringcaster` |

- 旧结构名通过 `Config/DefaultEngine.ini` 的 `[CoreRedirects]` 重定向，旧资产可正常加载；
- 表数据由 `Tools/migrate_enemy_per_type_params.py` 重分布（迁移库 `enemy_params_migration_lib.py`），
  迁移前自动备份到 `Saved/Backup/DT_EnemyArchetypes_before_per_type_params.uasset`；
- 迁移后月兔/血蝠/毒茸/亡灵四行会各自继承原先实际生效的参数，行为不变。

### 更早的整理（保留记录）

整理前已备份：`Saved/Backup/DT_EnemyArchetypes_before_structure.uasset` / `.json`。

- 8 个并列的专属列合并为单列 `SpecialParams`；
- `TankParams.BaseMoveMultiplier` 迁到顶层 `BaseMoveMultiplier`；
- 删除 `MovementModel` 列（改由类型基线决定，本版以 `MovementModelOverride` 可选恢复）；
- 删除 `MinimumRange` / `MaximumRange` / `AttackRangeMinFactor`（所有远程原型任何距离都会攻击）；
- 删除地藏旧地面波字段与 `FRogueEnemyGroundWaveArchetype`，统一用 `RadialRing` 弹幕。

## 加新敌人类型

1. `ERogueEnemyType` 加枚举（`Shared/RogueTypes.h`），同时在 `ERogueEnemyVisualKey` 加视觉键。
2. `RogueEnemyArchetypes.cpp` 的 `BuildEnemyArchetype()` 补基线（移动模型、射击参数、默认视觉），
   作为缺行兜底。
3. 在 `RogueEnemyDataTable.h` 新增 `FRogueXxxSpecialParams`，加进 `FRogueEnemySpecialParams`
   （字段语义建议沿用哨兵，老资产不填即保持基线）。
4. `ToArchetype()` 增加对应 `case`，读取 `SpecialParams.Xxx`，需要 Boss 覆盖就调 `ApplyBossOverride`。
5. 表里加一行：填 `EnemyType`、对应 `SpecialParams` 子结构、`Visual` / `BossVisual`
   （行存在后视觉以表为准，不填会拿到结构默认值）。
6. `BP_RogueGameMode` 的 `EnemyClassMap` 加类型 → 敌人 BP；BP 里重写 `OnApplyVisualStyle`
   应用外观。再把类型加进 `RogueGameModeRules.cpp` 的默认波次池
   （若 `DA_GameBalance.EnemyWaveEntries` 非空，以资产为准）。
