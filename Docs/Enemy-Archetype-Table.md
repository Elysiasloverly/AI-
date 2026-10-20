# 敌人原型表结构

`/Game/Data/Enemies/DT_EnemyArchetypes` 的行结构是 `FRogueEnemyArchetypeRow`。整理后只保留 10 个顶层列：

| 列 | 作用 |
|---|---|
| `EnemyType` | 敌人类型；同一类型只应有一行 |
| `BaseMoveMultiplier` | 基础移动倍率（0 表示不动） |
| `AttackAbilitySet` | 可选攻击能力集；留空按原型标志推导 |
| `MaxAlive` | 同时存活上限，0 = 不限 |
| `MaxSpawnPerWave` | 每波累计刷新上限，0 = 不限 |
| `ProjectileClass` | 该类型使用的弹体蓝图；留空回退敌人 BP / 原生类 |
| `SpecialParams` | **唯一的专属参数列**，只填 `EnemyType` 对应的子结构 |
| `BossOverride` | 作为 Boss 时的可选覆盖参数 |
| `Visual` | 普通形态视觉：VisualKey / MeshScale / MeshLocation / MeshRotation / Capsule |
| `BossVisual` | Boss 形态视觉；独立配置缩放和胶囊，不再只靠 C++ 基线 |

## SpecialParams

以前是 8 个并列的顶层列（`ChargerParams`、`TankParams`、`OrbiterParams`……），
每一行都要展开一堆与当前类型无关的结构。现在合并为一列：

```text
SpecialParams（字段名沿用旧结构名，不是怪物显示名）
├── Charger       冲锋位移：爆发周期/持续/速度倍率
├── Orbiter       环绕移动：远/中/近距离与权重
├── Shooter       单发远程：移动 + 射击 + 枪口特效
├── Skirmisher    Z 字移动：追击参数
├── Artillery     重型单发：移动 + 重型射击
├── Spitter       散射远程：移动 + 散射
└── ShockPillar   环形弹幕：冷却与弹幕参数
```

当前 13 种怪物到参数块的映射：

| 怪物 | 参数块 |
|---|---|
| Rifleman / Marksman / Repeater / Sniper | `Shooter` |
| Assault / Skirmisher / Scatterer / Shotgunner | `Spitter` |
| Juggernaut / Cannoneer | `Artillery` |
| ShockPillar / Ringcaster | `ShockPillar` |
| Orbiter | `Orbiter` |

`Charger` 参数块现在主要给 `Assault` 的冲锋移动保留；旧地面波已删除。参数块名保留是为了兼容已有 JSON/资产，不再随怪物显示名一起改。

编辑时只展开当前 `EnemyType` 对应的子结构，其余保持默认即可。
`ToArchetype()` 也只读取当前类型的子结构。

> `MinimumRange` / `MaximumRange` / `AttackRangeMinFactor` 已从敌人原型表结构删除；所有远程原型都会在任何距离尝试攻击。Boss 覆盖里的 `BossMaximumRange` 也已删除。

行名现在直接使用新类型名（`Rifleman`、`Assault`、`Juggernaut`……），查找仍以 `EnemyType` 为准，不依赖行名。

## 视觉

`Visual` / `BossVisual` 现在直接在表里配置：

- `VisualKey`
- `MeshScale`
- `MeshLocation`
- `MeshRotation`
- `CapsuleRadius`
- `CapsuleHalfHeight`
- `bUseBossMaterialOverride`

`ToArchetype()` 会优先使用行里的视觉；只有行缺失或新增类型尚未填表时，才回退 `BuildEnemyArchetype()` 的基线。

## 其他整理

- 删除了 `MovementModel` 列：移动模型由 `EnemyType` 的 `BuildEnemyArchetype()` 基线决定。
  表里再填一遍只会造成“漏填就把远程怪改成直冲”的隐患。
- 删除了 `TankParams`：坦克原本只有一个 `BaseMoveMultiplier`，现在统一用顶层
  `BaseMoveMultiplier`，避免同一份速度有两个来源。原坦克行的 `0.72` 已迁到顶层。
- 删除了震荡柱的旧地面波字段（`MaxRadius` / `ExpansionDuration` / `ExpansionSpeed` /
  `HitThickness` / `DamageMultiplier` / `bJumpCanDodge`）：代码已经不再读取。
- 删除了 `FRogueEnemyGroundWaveArchetype` / `URogueEnemyGroundWaveAttackAbility` 以及
  `ARogueEnemy` 的地面波状态、复制字段和 `GroundWaveMesh` / `GroundWaveRingSegments`；
  震荡柱只用 `Ranged / RadialRing` 整圈弹幕，不再保留历史地面波分支。
- `BossOverride` 仍保留为单独一列，避免把 Boss 覆盖和普通参数混在一起。

## 迁移记录

整理前已备份：

- `Saved/Backup/DT_EnemyArchetypes_before_structure.uasset`
- `Saved/Backup/DT_EnemyArchetypes_before_structure.json`

旧列到新列的映射：

| 旧列 | 新位置 |
|---|---|
| `ChargerParams` | `SpecialParams.Charger` |
| `OrbiterParams` | `SpecialParams.Orbiter` |
| `ShooterParams` | `SpecialParams.Shooter` |
| `SkirmisherParams` | `SpecialParams.Skirmisher` |
| `ArtilleryParams` | `SpecialParams.Artillery` |
| `SpitterParams` | `SpecialParams.Spitter` |
| `ShockPillarParams` | `SpecialParams.ShockPillar` |
| `TankParams.BaseMoveMultiplier` | 顶层 `BaseMoveMultiplier` |
| `MovementModel` | 删除，改用类型基线 |

## 加新敌人类型

1. `ERogueEnemyType` 加枚举。
2. `RogueEnemyArchetypes.cpp` 的 `BuildEnemyArchetype()` 补基线（只作为缺行兜底）。
3. 如有专属参数：新增 `FRogueXxxSpecialParams`，加进 `FRogueEnemySpecialParams`；如果行为能复用，直接在 `ToArchetype()` 里 `case` 落到已有子结构。
4. `ToArchetype()` 对应 case 读取 `SpecialParams.Xxx`。
5. 表里加一行，填该类型的 `SpecialParams` 子结构，并在 `Visual` / `BossVisual` 里填视觉和胶囊。
6. `BP_RogueGameMode` 的 `EnemyClassMap` 加类型 → 敌人 BP；再把类型加进 `RogueGameModeRules.cpp` 的默认波次池（若 `DA_GameBalance.EnemyWaveEntries` 非空，以资产为准）。
