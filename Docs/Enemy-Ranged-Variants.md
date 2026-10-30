# 远程敌人命名与攻击方式

当前18种敌人中，16种攻击者使用远程攻击，月蝶、风镰、星灯采用低空飞行；另有两种不攻击玩家的治疗者。2026-10 已统一重命名，旧的 `Hunter` / `Charger` / `Tank` / `Shooter` / `Artillery` / `Spitter` / `Burst` / `Ringer` 不再作为类型名或资产名使用。

## 命名表

| 名字 | 定位与弹道 | 起始波次 |
|---|---|---:|
| `Rifleman` 妖精 | 基础单发 | 1 |
| `Assault` 夜雀 | 冲锋 + 散射 | 2 |
| `Juggernaut` 鬼 | 缓慢重单发 | 6 |
| `Orbiter` 人魂 | 环绕 + 单发 | 3 |
| `Marksman` 鸦天狗 | 锁定地面红圈 → 空中大型落弹 → 触地环形弹幕 | 4 |
| `Skirmisher` 妖狐 | Z 字 + 散射 | 3 |
| `Cannoneer` 山蜘蛛 | 远距离重单发 | 8 |
| `Scatterer` 唐伞 | 三发散射 | 5 |
| `ShockPillar` 地藏 | 固定环形弹幕 | 12 |
| `Sniper` 月兔 | 高速单发狙击 | 9 |
| `Shotgunner` 毒茸 | 五发宽散射 | 7 |
| `Repeater` 血蝠 | 高频单发 | 10 |
| `Ringcaster` 亡灵 | 移动环形弹幕 | 14 |
| `SkyMoth` 月蝶 | 低空环绕 + 两条交错扫射弹流 | 5 |
| `WindReaver` 风镰 | 低空环绕 + 折返风刃 | 9 |
| `StarLantern` 星灯 | 低空环绕 + 带预警和缺口的收拢星阵 | 14 |
| `AuraHealer` 青叶灵 | 绿圈内弱群体治疗，不攻击 | 10 |
| `LinkHealer` 祈愿妖精 | 绿线单体强治疗，不攻击 | 18 |

具体冷却、弹速和伤害以 `DT_EnemyArchetypes` 为准；飞行类型的资产与参数见 [飞行敌人说明](Flying-Enemies.md)。

起始波次以 `DA_GameBalance.EnemyWaveEntries` 为准；清空该数组时才回退到 `RogueGameModeRules.cpp` 的内置表。

## 资产对应

- 敌人 BP：`BP_EnemyRifleman`、`BP_EnemyAssault`、`BP_EnemyJuggernaut`、`BP_EnemyOrbiter`、`BP_EnemyMarksman`、`BP_EnemySkirmisher`、`BP_EnemyCannoneer`、`BP_EnemyScatterer`、`BP_EnemyShockPillar`、`BP_EnemySniper`、`BP_EnemyShotgunner`、`BP_EnemyRepeater`、`BP_EnemyRingcaster`、`BP_EnemySkyMoth`、`BP_EnemyWindReaver`、`BP_EnemyStarLantern`、`BP_EnemyAuraHealer`、`BP_EnemyLinkHealer`
- 弹体 BP：`BP_EnemyProjectile_Marksman`、`_Cannoneer`、`_Scatterer`、`_Repeater`、`_Ringcaster` 等
- 类型映射：`BP_RogueGameMode.EnemyClassMap`
- 表行名与 `EnemyType` 已同步为新名字；旧的 BP 重定向器已删除

## 内部参数块说明

原有 13 种地面怪物在 `FRogueEnemySpecialParams` 中采用**一怪一块**，块名与 `ERogueEnemyType` 一一对应
（`Rifleman` / `Assault` / `Juggernaut` / `Orbiter` / `Marksman` / `Skirmisher` / `Cannoneer` /
`Scatterer` / `ShockPillar` / `Sniper` / `Shotgunner` / `Repeater` / `Ringcaster`）：

- 原有类型只读取当前 `EnemyType` 对应的专属块；三种飞行类型读取顶层 `Flying` 参数；
- 旧共享块（`Charger` / `Shooter` / `Artillery` / `Spitter`）已通过 `[CoreRedirects]`
  重定向到族首怪物块，表数据由 `Tools/migrate_enemy_per_type_params.py` 重分布；
- 详见 [敌人原型表结构](Enemy-Archetype-Table.md)。

## 行为约束

- 16种攻击原型的 `Ranged.bUsesRangedAttack` 都为 `true`。
- 不授予 `URogueEnemyContactAttackAbility`，也不调用 `DecideContactAttack()`。
- `Assault` 保留 `BurstCharge` 移动，`Juggernaut` 保留 `Direct`，`Orbiter` 保留 `Orbit`，`Skirmisher` 保留 `ZigZagChase`。
- `Ringcaster` 和 `ShockPillar` 的远程攻击都使用环形弹幕；前者会移动，后者固定。固定类型还可读取BP法阵范围造成近身伤害，见 [敌人角色与提示](Enemy-Roles-And-Visibility.md)。

## 攻击距离限制已取消

`ARogueEnemy::DecideRangedAttack()` 不再检查攻击距离。只要目标存活且攻击冷却结束，敌人就会尝试开火；`IsInRangedAttackRange()` 也改为对所有远程原型返回 `true`。

数据侧也已经把这些限制字段**从结构里删除**：

- `MinimumRange`
- `MaximumRange`
- `AttackRangeMinFactor`
- Boss 覆盖的 `BossMaximumRange`

`DT_EnemyArchetypes` 的18行里已经不再有这些字段，无法再从数据表恢复距离门控。

> 普通弹体有 `PooledLifetime`（默认 30 秒）；飞行类型也统一使用弹体BP的30秒寿命。这是弹体寿命，不是攻击距离门控；如果目标远到弹体在 30 秒内飞不到，弹体仍会消失。

## 验证

```bat
UnrealEditor-Cmd.exe F:\AI_WorkSpeace\baka\Baka.uproject ^
  -ExecCmds="Automation RunTests Rogue.Gameplay.EnemyVariants+ Rogue.Gameplay.EnemyCollision+ Rogue.Gameplay.GameBalance+ Rogue.Gameplay.ShockPillar;Quit" ^
  -unattended -nopause -nullrhi -nosplash
```

`Rogue.Gameplay.EnemyVariants` 覆盖16种攻击类型；`Rogue.Gameplay.EnemyRoles` 另覆盖2种治疗类型、Boss提示与法阵伤害，核对类型、数据表行、敌人 BP、弹体类，以及蓝图默认 EnemyType 与当前类型的一致性。旧枚举兼容与地图入口见 [清理说明](Map-Entrypoints-And-Legacy-Cleanup.md)。
