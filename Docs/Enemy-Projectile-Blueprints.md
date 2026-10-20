# 敌人弹体蓝图

敌方弹幕现在和御札一样走蓝图类，而且**外观完全由蓝图决定**：

```text
/Game/BP/Enemy/Projectiles/
├── BP_EnemyProjectile                  占位模板（独立 BP，不再作为其它变体的父类）
├── BP_EnemyProjectile_Boss            Boss
├── BP_EnemyProjectile_Marksman       鸦天狗 / 人魂
├── BP_EnemyProjectile_Cannoneer      山蜘蛛 / 鬼
├── BP_EnemyProjectile_Scatterer      唐伞 / 妖狐
├── BP_EnemyProjectile_ShockPillar    地藏
├── BP_EnemyProjectile_Sniper         月兔
├── BP_EnemyProjectile_Shotgunner     毒茸 / 夜雀
├── BP_EnemyProjectile_Repeater       血蝠 / 妖精
└── BP_EnemyProjectile_Ringcaster     亡灵
```

这 10 个 BP 都**直接继承 C++ 的 `ARogueEnemyProjectile`**，各自持有一套 SCS 组件，互不继承。

## 为什么现在能自由删组件

C++ 类 `ARogueEnemyProjectile` **不再创建任何网格、灯光或材质组件**，只保留：

- `Collision`（USphereComponent，命中判定）
- `ProjectileMovement`（飞行）

蓝图里的视觉组件全部是 SCS 组件（Blueprint 自己创建的），因此可以在组件面板里直接删除、替换、改材质。
之前不能删的是 C++ `CreateDefaultSubobject` 创建的原生组件——蓝图只能隐藏或缩放。

当前占位模板（可自行删改）：

| BP | 组件 |
|---|---|
| `BP_EnemyProjectile` / `_Boss` / `_Marksman` / `_Cannoneer` / `_Scatterer` / `_Repeater` / `_Ringcaster` | `BodyMesh`、`CoreMesh`、`GlowMesh`、`TrailMesh`、`GlowLight` |
| `BP_EnemyProjectile_ShockPillar` | 只有 `BodyMesh`、`CoreMesh`、`GlowMesh`（整圈 48 发，去掉每发点光和拖尾） |

模板由 `Tools/create_projectile_visual_template.py` 生成；变体独立化与组件复制见
`Tools/make_projectile_variants_independent.py`，整圈裁剪见 `Tools/trim_ring_projectile_template.py`。
C++ 不会再覆盖这些组件的缩放或材质。

## 默认行为

`ARogueEnemy::GetEnemyProjectileClass()` 按以下优先级取类：

1. `bIsBoss == true` 且敌人 BP 配置了 `BossProjectileClass`；
2. 原型数据表 `DT_EnemyArchetypes` 该行的 `ProjectileClass`；
3. 敌人 BP 的 `EnemyProjectileClass`；
4. 原生 `ARogueEnemyProjectile`（只有碰撞和伤害，没有外观）。

C++ **不写死任何弹体资产路径**：没有配置就回退原生类，不会偷偷加载某个 BP。
服务端生成带伤害的真实弹体；多播时把服务端选定的类一起发给客户端，
客户端按同一个类生成零伤害表现弹体。

## 按敌人类型指定（数据表）

`Content/Data/Enemies/DT_EnemyArchetypes` 每行有「弹体蓝图类」列，当前配置：

| 敌人类型 | 弹体 |
|---|---|
| Rifleman（妖精） | `BP_EnemyProjectile_Repeater` |
| Assault（夜雀） | `BP_EnemyProjectile_Shotgunner` |
| Juggernaut（鬼） | `BP_EnemyProjectile_Cannoneer` |
| Orbiter（人魂） | `BP_EnemyProjectile_Marksman` |
| Marksman（鸦天狗） | `BP_EnemyProjectile_Marksman` |
| Skirmisher（妖狐） | `BP_EnemyProjectile_Scatterer` |
| Cannoneer（山蜘蛛） | `BP_EnemyProjectile_Cannoneer` |
| Scatterer（唐伞） | `BP_EnemyProjectile_Scatterer` |
| ShockPillar（地藏） | `BP_EnemyProjectile_ShockPillar` |
| Sniper（月兔） | `BP_EnemyProjectile_Sniper` |
| Shotgunner（毒茸） | `BP_EnemyProjectile_Shotgunner` |
| Repeater（血蝠） | `BP_EnemyProjectile_Repeater` |
| Ringcaster（亡灵） | `BP_EnemyProjectile_Ringcaster` |

批量写入用 `Tools/assign_enemy_projectiles.py`，只改 `ProjectileClass` 一列；
改前自动备份到 `Saved/Backup/DT_EnemyArchetypes_before_projectiles.uasset`。

## 在弹体 BP 里做什么

C++ 提供这些给蓝图：

| 内容 | 类型 | 用途 |
|---|---|---|
| `OnProjectileActivated(bool bBossShot, bool bSimpleVisual, float VisualScale)` | 事件 | 激活时设置材质、粒子、缩放；`VisualScale` 会给整圈弹幕传 0.4 这类值 |
| `OnProjectileDeactivated()` | 事件 | 回池前停止动画/粒子 |
| `OnProjectileCulled(bool bCulled)` | 事件 | 高负载裁剪状态变化，隐藏装饰表现 |
| `VisualScale` / `bBossShot` / `bSimpleVisual` | 只读属性 | 读取本次激活状态 |
| `ImpactEffectScale` | 可编辑 | 命中特效缩放（会再乘 `VisualScale`） |
| `PooledLifetime` | 可编辑 | 未命中时的存活时间 |
| `Collision` 组件 | 组件 | 改 `Sphere Radius` 即可改命中范围；C++ 以蓝图半径为基础，激活时乘以 `VisualScale` |

蓝图里加什么组件都可以：StaticMesh、Niagara、Cascade、PointLight、Trail 等；
C++ 不会按名字查找，也不会改它们的 Transform。

> 碰撞由 `RogueCombatCollision::DisableNonHitCollision` 在 `BeginPlay` 统一处理：
> 除 `Collision` 命中球外，所有 Primitive 都会被设成 `NoCollision` + 不模拟物理；
> 完整审计见 [弹体碰撞审计](Combat-Collision.md)。

> 占位模板没有连事件图：高负载裁剪、回池清理、按 `VisualScale` 缩放都需要你自己在
> `OnProjectileCulled` / `OnProjectileDeactivated` / `OnProjectileActivated` 里接一下；
> 不接也能显示，只是不会自动隐藏灯光/拖尾。

## 每个敌人单独换弹体（BP 覆盖）

打开敌人 BP（如 `BP_EnemyMarksman`）→ **类默认值 → Enemy|Combat**：

| 属性 | 用途 |
|---|---|
| `敌人弹体蓝图类` | 普通弹、散射、整圈弹幕使用的类 |
| `Boss 弹体蓝图类` | 该敌人作为 Boss 时优先使用的类；留空则回退普通/表配置 |

> 两个属性都是类默认值，不逐发复制。运行时不支持只改服务端。

## 对象池

`BuildCombatPoolPrewarmClasses()` 会收集敌人 BP 的 `EnemyProjectileClass` / `BossProjectileClass`
以及原型表里的 `ProjectileClass`，按类去重预热。没有配置时才回退原生类。

## 排查

- 弹体看不见：检查对应弹体 BP 里是否已经加了视觉组件（C++ 不再提供默认外观）。
- 改了 BP 但游戏里没变：确认敌人 BP/数据表引用的是这个 BP，并重启编辑器加载新 DLL。
- 客户端看不到弹体：确认客户端能加载相同资产；表现为不可靠多播，丢包会少画一发，伤害仍在服务端。
- 命中范围不对：改 `Collision` 组件的 Sphere Radius；整圈弹幕会再乘 `VisualScale`。
