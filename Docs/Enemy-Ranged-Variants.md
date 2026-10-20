# 远程敌人命名与攻击方式

当前 13 种敌人全部使用远程攻击。2026-10 已统一重命名，旧的 `Hunter` / `Charger` / `Tank` / `Shooter` / `Artillery` / `Spitter` / `Burst` / `Ringer` 不再作为类型名或资产名使用。

## 命名表

| 新名字 | 定位 | 弹道 | 起始波次 |
|---|---|---|---:|
| `Rifleman` 妖精 | 基础单发 | `Single`，950 cm/s，1.25s 冷却 | 1 |
| `Assault` 夜雀 | 冲锋 + 散射 | `Spread3`，850 cm/s，1.7s 冷却 | 2 |
| `Juggernaut` 鬼 | 缓慢重单发 | `Single`，680 cm/s，2.6s 冷却 | 4 |
| `Orbiter` 人魂 | 环绕 + 单发 | `Single`，1000 cm/s，1.4s 冷却 | 3 |
| `Marksman` 鸦天狗 | 标准单发 | `Single`，980 cm/s，1.18s 冷却 | 3 |
| `Skirmisher` 妖狐 | Z 字 + 散射 | `Spread3`，900 cm/s，1.3s 冷却 | 2 |
| `Cannoneer` 山蜘蛛 | 远距离重单发 | `Single`，860 cm/s，1.85s 冷却 | 5 |
| `Scatterer` 唐伞 | 三发散射 | `Spread3`，980 cm/s，0.70s 冷却 | 4 |
| `ShockPillar` 地藏 | 固定环形弹幕 | `RadialRing`，48 发，720 cm/s | 7 |
| `Sniper` 月兔 | 超远距离单发 | `Single`，1700 cm/s，2.1s 冷却 | 6 |
| `Shotgunner` 毒茸 | 五发宽散射 | `Spread3`，5 发，860 cm/s | 5 |
| `Repeater` 血蝠 | 高频单发 | `Single`，1180 cm/s，0.48s 冷却 | 7 |
| `Ringcaster` 亡灵 | 移动环形弹幕 | `RadialRing`，16 发，760 cm/s | 9 |

起始波次以 `DA_GameBalance.EnemyWaveEntries` 为准；清空该数组时才回退到 `RogueGameModeRules.cpp` 的内置表。

## 资产对应

- 敌人 BP：`BP_EnemyRifleman`、`BP_EnemyAssault`、`BP_EnemyJuggernaut`、`BP_EnemyOrbiter`、`BP_EnemyMarksman`、`BP_EnemySkirmisher`、`BP_EnemyCannoneer`、`BP_EnemyScatterer`、`BP_EnemyShockPillar`、`BP_EnemySniper`、`BP_EnemyShotgunner`、`BP_EnemyRepeater`、`BP_EnemyRingcaster`
- 弹体 BP：`BP_EnemyProjectile_Marksman`、`_Cannoneer`、`_Scatterer`、`_Repeater`、`_Ringcaster` 等
- 类型映射：`BP_RogueGameMode.EnemyClassMap`
- 表行名与 `EnemyType` 已同步为新名字；旧的 BP 重定向器已删除

## 内部参数块说明

`FRogueEnemySpecialParams` 现在是**一怪一块**，块名与 `ERogueEnemyType` 一一对应
（`Rifleman` / `Assault` / `Juggernaut` / `Orbiter` / `Marksman` / `Skirmisher` / `Cannoneer` /
`Scatterer` / `ShockPillar` / `Sniper` / `Shotgunner` / `Repeater` / `Ringcaster`）：

- `ToArchetype()` 只读取当前 `EnemyType` 对应的块，互不共享；
- 旧共享块（`Charger` / `Shooter` / `Artillery` / `Spitter`）已通过 `[CoreRedirects]`
  重定向到族首怪物块，表数据由 `Tools/migrate_enemy_per_type_params.py` 重分布；
- 详见 [敌人原型表结构](Enemy-Archetype-Table.md)。

## 行为约束

- 所有 13 种原型的 `Ranged.bUsesRangedAttack` 都为 `true`。
- 不授予 `URogueEnemyContactAttackAbility`，也不调用 `DecideContactAttack()`。
- `Assault` 保留 `BurstCharge` 移动，`Juggernaut` 保留 `Direct`，`Orbiter` 保留 `Orbit`，`Skirmisher` 保留 `ZigZagChase`。
- `Ringcaster` 和 `ShockPillar` 都只放环形弹幕；前者会移动，后者固定。

## 攻击距离限制已取消

`ARogueEnemy::DecideRangedAttack()` 不再检查攻击距离。只要目标存活且攻击冷却结束，敌人就会尝试开火；`IsInRangedAttackRange()` 也改为对所有远程原型返回 `true`。

数据侧也已经把这些限制字段**从结构里删除**：

- `MinimumRange`
- `MaximumRange`
- `AttackRangeMinFactor`
- Boss 覆盖的 `BossMaximumRange`

`DT_EnemyArchetypes` 的 13 行里已经不再有这些字段，无法再从数据表恢复距离门控。

> 弹体本身仍有 `PooledLifetime`（默认 4 秒）的存活时间。这是弹体寿命，不是攻击距离门控；如果目标远到弹体在 4 秒内飞不到，弹体仍会消失。

## 验证

```bat
UnrealEditor-Cmd.exe F:\AI_WorkSpeace\ai\ai.uproject ^
  -ExecCmds="Automation RunTests Rogue.Gameplay.EnemyVariants+ Rogue.Gameplay.EnemyCollision+ Rogue.Gameplay.GameBalance+ Rogue.Gameplay.ShockPillar;Quit" ^
  -unattended -nopause -nullrhi -nosplash
```

`Rogue.Gameplay.EnemyVariants` 会核对新类型、数据表行、敌人 BP、弹体类和 `BP_RogueGameMode.EnemyClassMap` 的映射。
