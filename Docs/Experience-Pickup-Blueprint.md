# 经验球蓝图

经验球的**外观完全由蓝图决定**，C++ 只负责拾取、池化、复制和磁吸位移。

```text
/Game/BP/World/
└── BP_ExperiencePickup      父类 ARogueExperiencePickup
```

蓝图类通过 `Config/DefaultGame.ini` 接入（不写死路径，改配置即可换 BP）：

```ini
[/Script/ai.RogueGameMode]
ExperiencePickupClass=/Game/BP/World/BP_ExperiencePickup.BP_ExperiencePickup_C
```

留空或加载失败会回退原生 `ARogueExperiencePickup`：它只有碰撞体，**画面上看不见球**，捡得到但没反馈。
`ARogueGameMode::PrewarmCombatPools()` 在空配置时会打一条 Warning，方便排查"经验条涨了但没有球"。

## 为什么现在能自由删组件

C++ 类不再创建任何网格、灯光或材质，也不再写死任何资产路径，只保留：

- `Collision`（USphereComponent，拾取判定与磁吸抵达判定）
- 经验值、活跃位复制，池化与磁吸位移

因此蓝图里的组件全是 SCS 组件，可以在组件面板里直接删除、替换、改材质。

生成的默认结构（可任意删改）：

```text
Collision（C++ 根组件，球半径 40）
└── VisualRoot            视觉挂点：悬浮动画作用在它身上
    ├── BodyMesh          主体球（Shape_Sphere，0.24 倍）
    ├── GlowMesh          外层辉光球（Galaxy 材质，0.46 倍）
    └── GlowLight         点光源（默认关闭）
Floating                  漂浮组件，目标为 VisualRoot（幅度 10cm / 周期 3.14s）
```

由 `Tools/create_experience_pickup_bp.py` 生成，已存在时只打印路径、不覆盖。
手写也行：新建蓝图 → 父类选 `RogueExperiencePickup` → 自己加网格和灯光。

> `Collision` 是 C++ 根组件，**不能删**。它是拾取判定和磁吸抵达判定的依据（半径 40）。
> 想让拾取范围更大就改它的 Sphere Radius，或者把子组件整体缩放，不要指望放大器件。

## 在经验球 BP 里做什么

C++ 提供的接口：

| 内容 | 类型 | 用途 |
|---|---|---|
| `OnPickupActivated(int32 InExperienceValue)` | 事件 | 激活时按经验值换材质参数、颜色、粒子强度。服务端与客户端都会收到 |
| `OnPickupMerged(int32 NewExperienceValue)` | 事件 | 与附近的球合并后触发，可播一次合并脉冲 |
| `OnPickupDeactivated()` | 事件 | 回池时触发（被捡走 / 超时 / 整局结束）。Actor 已隐藏，但位置仍在，可从这里生成"被吸走"特效 |
| `GetExperienceValue()` | 只读 | 当前经验值，用于分档外观 |
| `ShouldSimplifyVisuals()` | 只读 | 高负载裁剪开关，为 true 时应关掉拖尾、粒子、多灯 |
| `AttractionSpeed` | 可编辑 | 磁吸飞行速度（默认 900 cm/s） |
| `PickupLifetime` | 可编辑 | 存活时间（默认 20s，合并会重置） |
| `AutoMagnetDelay` | 可编辑 | 忽略拾取半径、直接吸向最近玩家的等待时间（默认 6s） |

常见改法：

- **换球的外观**：选中 `BodyMesh` / `GlowMesh`，换 Static Mesh 或 Override Materials。C++ 不会覆盖它们。
- **要发光照亮地面**：把 `GlowLight` 的 `Visible` 打开，调强度、半径和颜色。
- **要拖尾/粒子**：在 `VisualRoot` 下加 Niagara / Cascade 组件，默认会自动显示；想在高负载下自动关掉，就在 `OnPickupActivated` 和 BP 的 Tick 里读 `ShouldSimplifyVisuals()`。
- **要自转**：加一个 `RotatingMovementComponent`，或者在 BP 的 `Event Tick` 里转 `VisualRoot`。
- **不要悬浮**：删掉 `Floating` 组件，或把它的 `启用漂浮` 勾掉。

> 默认漂浮靠 `URogueFloatingComponent`（项目里玩家也在用的那个），它只改 `VisualRoot` 的相对位置，
> 不动根组件，因此不影响拾取判定和磁吸位移。

## 尺寸与网络

经验值越高球越大：`1 + 经验值 × 0.015`，上限 2.4 倍，作用在 **Actor 缩放**上，
BP 的子组件按相对缩放跟随，不需要自己再乘一遍。

- `ExperienceValue` 现在单独复制（`DOREPLIFETIME_CONDITION`），客户端在 `OnRep_ExperienceValue` 里
  重新应用缩放 —— 之前只有 `FRepMovement`，它不带 Scale，所以队友看到的球大小是错的。
- 尺寸变化是**跳变**，没有插值动画；需要平滑就自己在 BP 里对 `VisualRoot` 做 Lerp。
- 磁吸位移只在服务端算，客户端跟随复制。不要在客户端的 Tick 里改 Actor 位置，会和复制打架。

## 对象池

经验球是 8 类池化对象之一（默认预热 64 个）。契约不变：

- 由 `URogueCombatPoolSubsystem::AcquireExperiencePickup()` 取出，`ActivatePooledPickup()` 激活；
- 被拾取 / 超时 / 整局结束时 `DeactivateToPool()` 归池，**不要自己调 `Destroy`**；
- 回池时 `SetActorHiddenInGame(true)`、`SetActorTickEnabled(false)`、缩放归一；
  BP 的 `Event Tick` 挂在同一个 Actor Tick 上，会一起停掉。

## 排查

- **球看不见**：检查 `DefaultGame.ini` 的 `ExperiencePickupClass` 是否指向 BP；BP 里是否有视觉组件。
- **球大小不对**：确认服务器和客户端加载的是同一个 BP；`ExperienceValue` 已复制，客户端的缩放由 `OnRep_ExperienceValue` 驱动。
- **球不悬浮**：`Floating` 组件的 `漂浮目标` 要指向 `VisualRoot`；改过组件名后需要重新选一次。
- **改了 BP 但游戏里没变**：确认 `ExperiencePickupClass` 没指向别的 BP，并重启编辑器加载新 DLL。
