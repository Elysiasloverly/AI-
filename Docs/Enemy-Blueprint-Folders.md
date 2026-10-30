# 敌人蓝图目录

更新：2026-10-30。每种敌人本体、自己的弹体和专属效果统一放入 `/Game/BP/Enemy/<类型>`。

| 中文名 | 子文件夹 | 敌人本体 | 弹体 / 专属效果 |
|---|---|---|---|
| 妖精 | `Rifleman` | `BP_EnemyRifleman` | `BP_EnemyProjectile_Rifleman` |
| 夜雀 | `Assault` | `BP_EnemyAssault` | `BP_EnemyProjectile_Assault` |
| 鬼 | `Juggernaut` | `BP_EnemyJuggernaut` | `BP_EnemyProjectile_Juggernaut` |
| 人魂 | `Orbiter` | `BP_EnemyOrbiter` | `BP_EnemyProjectile_Orbiter` |
| 鸦天狗 | `Marksman` | `BP_EnemyMarksman` | `BP_EnemyProjectile_Marksman、BP_EnemySkyfall_Marksman` |
| 妖狐 | `Skirmisher` | `BP_EnemySkirmisher` | `BP_EnemyProjectile_Skirmisher` |
| 山蜘蛛 | `Cannoneer` | `BP_EnemyCannoneer` | `BP_EnemyProjectile_Cannoneer` |
| 唐伞 | `Scatterer` | `BP_EnemyScatterer` | `BP_EnemyProjectile_Scatterer` |
| 地藏 | `ShockPillar` | `BP_EnemyShockPillar` | `BP_EnemyProjectile_ShockPillar` |
| 月兔 | `Sniper` | `BP_EnemySniper` | `BP_EnemyProjectile_Sniper` |
| 毒茸 | `Shotgunner` | `BP_EnemyShotgunner` | `BP_EnemyProjectile_Shotgunner` |
| 血蝠 | `Repeater` | `BP_EnemyRepeater` | `BP_EnemyProjectile_Repeater` |
| 亡灵 | `Ringcaster` | `BP_EnemyRingcaster` | `BP_EnemyProjectile_Ringcaster` |
| 月蝶 | `SkyMoth` | `BP_EnemySkyMoth` | `BP_EnemyProjectile_SkyMoth` |
| 风镰 | `WindReaver` | `BP_EnemyWindReaver` | `BP_EnemyProjectile_WindReaver` |
| 星灯 | `StarLantern` | `BP_EnemyStarLantern` | `BP_EnemyProjectile_StarLantern` |
| 青叶灵 | `AuraHealer` | `BP_EnemyAuraHealer` | `BP_EnemyHealingCircle` |
| 祈愿妖精 | `LinkHealer` | `BP_EnemyLinkHealer` | `BP_EnemyHealingLink` |

## 公共内容

- `/Game/BP/Enemy/Common`：`BP_EnemyBase`、`BP_EnemyProjectile`、`BP_EnemyProjectile_Boss`、`BP_EnemyBossMarker`、`BP_EnemyFlyingMarker`。
- `/Game/BP/Enemy/AI`：`BP_AIController`。

原来的 `Projectiles` 集中目录已清理。修改受击材质等继承配置仍在 `Common/BP_EnemyBase` 中设置，子类继续继承。

## 原来共用弹体的五种敌人

妖精、夜雀、鬼、人魂、妖狐现在各有一份独立弹体。它们从原先实际使用的弹体复制，保留模型、材质、缩放、碰撞与寿命配置。`DT_EnemyArchetypes.ProjectileClass` 和各自敌人BP的回退槽已接到自己的弹体；之后改某个弹体不会同时改到另一种敌人。

关卡、数据表和蓝图引用由UE迁移并保存。源码中的动态路径通过 `RogueEnemyAssetPaths` 统一生成，旧包名在 `DefaultEngine.ini` 中保留兼容重定向；资产实际引用均使用新路径。
