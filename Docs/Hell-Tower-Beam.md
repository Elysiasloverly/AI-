# 御柱独立光束蓝图

御柱使用 `/Game/BP/Projectiles/BP_RogueHellTowerBeam`，极限火花使用 `/Game/BP/Projectiles/BP_RogueLaserBeam`。两者分别继承原生 `ARogueLaserBeam`，各自保存组件和外观配置。

## 修改御柱材质

打开 `BP_RogueHellTowerBeam`，在组件面板选择对应网格，在详情中的「材质」直接修改材质槽：

| 组件 | 用途 |
|---|---|
| BeamMesh | 主光束 |
| GlowMesh | 外层光晕；可能覆盖主光束，改变颜色时也要检查它 |
| StartRingMesh | 起点圆环 |
| EndRingMesh | 终点圆环 |

编译并保存蓝图后重新开始游戏。组件材质在光束激活、样式切换和对象池复用时均会保留，C++ 不会把它盖回原来的御柱材质。

该蓝图的「类默认值 → 外观 → 材质 → 使用组件材质」已启用。启用时，旧的极限火花/御柱材质覆盖字段会隐藏，避免出现两处材质配置互相覆盖。极限火花蓝图保留其已有材质配置模式。

## 修改御柱形状

在「类默认值 → 外观 → 御柱样式」调整主光束粗细、光晕粗细、长度倍率和圆环尺寸。光束长度会根据射击起终点计算；若需要圆环，启用「显示起点与终点圆环」，当前默认关闭。

## 武器配置与联机

`/Game/Data/Weapons/DT_WeaponConfig` 的 HellTower 行引用 `BP_RogueHellTowerBeam`，Laser 行引用 `BP_RogueLaserBeam`。对象池按实际蓝图类获取光束，客户端使用对应武器的蓝图重放表现。

御柱仍由服务器锁定目标、通过 GAS 按间隔结算递增伤害。蓝图负责表现；伤害、射程、数量和刷新间隔继续在武器表中调整。

回归入口：`Rogue.Gameplay.HellTowerBeam`，覆盖独立蓝图引用、表现重放、组件材质在激活及复用期间的保留，以及旧材质模式的空材质清理和生命周期。
