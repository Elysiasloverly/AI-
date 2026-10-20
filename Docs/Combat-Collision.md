# 弹体碰撞审计

结论：项目里所有弹体的**物理模拟关闭**，视觉组件**不参与碰撞**；只有命中用的球体保留 **QueryOnly** 查询，用来判定命中/挡墙。

## 各弹体设置

| 类 | 命中球 | 视觉/其它组件 | 物理模拟 |
|---|---|---|---|
| `ARogueProjectile`（御札） | `QueryOnly`；Pawn Overlap、WorldStatic Block、WorldDynamic Ignore | `Mesh` / `GlowMesh` / `RingMesh` / `TrailMesh` 全部 `NoCollision` | 无 |
| `ARogueRocketProjectile`（八卦炉） | `QueryOnly`；Pawn Overlap、WorldStatic Block | C++ 不建网格；蓝图追加的组件在 BeginPlay 被关闭 | 无 |
| `ARogueMortarProjectile`（阴阳玉） | 继承八卦炉 | `LandingIndicatorMesh` `NoCollision` | 无 |
| `ARogueEnemyProjectile`（敌人弹体） | `QueryOnly`；Pawn Overlap、WorldStatic Block | 模板网格 `NoCollision`；蓝图追加的组件在 BeginPlay 被关闭 | 无 |
| `ARogueOrbitingBlade`（银刃） | `QueryOnly`；Pawn Overlap | `Mesh` `NoCollision` | 无 |
| `ARogueLaserBeam`（光束） | 无命中球 | `BeamMesh` / `GlowMesh` / 圆环全部 `NoCollision` | 无 |
| `ARogueImpactEffect`（命中特效） | 无命中球 | `EffectMesh` `NoCollision` | 无 |

全项目没有 `SetSimulatePhysics(true)`，也没有 `QueryAndPhysics` / `PhysicsCollision` 的弹体。

## 运行期兜底

`Source/ai/Public/Combat/RogueCombatCollision.h` 提供：

```cpp
RogueCombatCollision::DisableNonHitCollision(Actor, HitCollision);
```

每个弹体/表现 Actor 在 `BeginPlay` 调用它：

- 除 `HitCollision` 外，所有 `UPrimitiveComponent` 都：
  - `SetSimulatePhysics(false)`
  - `SetCollisionEnabled(NoCollision)`
  - `SetGenerateOverlapEvents(false)`
- `HitCollision` 强制 `QueryOnly`，即使蓝图把它改成物理碰撞也会被拉回查询模式。

这样即使以后在弹体 BP 里追加网格、粒子碰撞体，也不会阻挡角色、互相推挤或参与物理。

## 注意

`QueryOnly` **不是物理碰撞**，但命中球仍会：
- 与 `Pawn` **Overlap**（判定命中玩家/敌人）
- 与 `WorldStatic` **Block**（撞墙后停下/消失）

如果希望御札穿墙，需要单独把 `WorldStatic` 响应改成 `Ignore`；那会改变玩法命中规则，不建议直接全局改。
