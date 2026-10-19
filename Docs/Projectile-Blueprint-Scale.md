# 弹体模型缩放

除激光和地狱塔外，弹体模型的基础尺寸使用蓝图组件设置。

打开 `Content/BP/Projectile` 中对应蓝图，在组件面板选择模型组件，修改「变换 → 缩放」，然后编译并保存。

| 蓝图 | 模型缩放规则 |
|---|---|
| BP_RogueProjectile | 保留 Mesh、GlowMesh、RingMesh、TrailMesh 的组件缩放；飞行、高负载切换和池复用不再重写大小。 |
| BP_RogueRocketProjectile | 模型、粒子等蓝图组件继续使用自身缩放，C++ 不改弹体模型大小。 |
| BP_RogueMortarProjectile | 飞行模型使用蓝图缩放；地面落点圆环仍跟随爆炸范围，属于伤害范围提示。 |
| BP_RogueOrbitingBlade | 未升级时使用蓝图 Mesh 的基础尺寸；镰刀大小升级在该尺寸上乘算，并同步放大伤害碰撞球。回池后恢复基础尺寸。 |
| BP_RogueLaserBeam / BP_RogueHellTowerBeam | 保留光束长度、粗细和样式的现有规则。 |

敌方弹体也保留各模型组件的缩放，普通、Boss、密集弹幕和高负载表现不再用固定网格大小覆盖它。敌方弹体的碰撞半径、灯光和命中特效仍按原配置计算；敌人表的 ProjectileScale 不再改变模型网格大小。

普通弹体和敌方弹体原 VisualConfig/VisualState 中的固定网格缩放与大小脉冲字段保留序列化兼容，但已从编辑面板隐藏。请直接在蓝图组件的缩放中设置尺寸。此次修改没有重存任何现有蓝图。

回归入口：`Rogue.Gameplay.ProjectileScale`。
