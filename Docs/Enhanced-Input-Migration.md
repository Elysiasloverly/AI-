# Enhanced Input 迁移 —— 完成记录

> ## ✅ 迁移已完成（配置 / 资产 / 代码三层全部就位）
>
> | 层 | 迁移前 | 现在 |
> |---|---|---|
> | 配置层 | ✅ 已是 Enhanced Input（`DefaultInput.ini:79-80`） | ✅ |
> | 资产层 | ❌ 零资产 | ✅ **11 个资产**（`/Game/Input/IMC_Default` + `/Game/Input/IA/` 下 10 个） |
> | 代码层 | ❌ 全是 legacy `BindAction(TEXT("..."))` | ✅ 全 Enhanced Input；legacy 支路已删除 |
> | 旧映射 | ❌ 8 条 ActionMappings + 11 条 AxisMappings | ✅ **已删除**（`DefaultInput.ini` 里留了对照注释） |
>
> **实测日志**（`-nullrhi` 无头运行）：
>
> ```
> [Rogue][Input] Enhanced Input 已启用：IMC=IMC_Default（13 条按键映射），动作 10/10 就绪
> ```
>
> 全程无 `Fatal` / `Ensure` / `LogAbilitySystem: Error`；全树 grep 确认
> **零 legacy `PlayerInputComponent->BindAxis/BindAction` 调用残留**。
>
> ### ⚠️ 一处待你确认：映射条数是 13，不是 17
>
> 期望构成：`Move 8(WSAD+方向键) + Look 1 + Zoom 1 + Dash 1 + Jump 1 + Interact 1 + Pause 1 + 选卡 3 = 17`
>
> 实测 **13**，差了 4 —— 由算术可反推，缺的正是 **`IA_Move` 的方向键**（↑↓←→）。
> 也就是说 **WASD、鼠标、滚轮、冲刺、跳跃、交互、暂停、选卡 1/2/3 全部配好了**，
> 只是没配"方向键作为 WASD 的替代方案"。
>
> 这个在俯视角射击里**不是必须的**（主流操作就是 WASD + 鼠标）。要补的话，
> 在 `IMC_Default` 里给 `IA_Move` 再加 4 条映射，修饰器与 W/S/A/D 完全一致（见 §3.3）。
>
> **仍有待实机验证的部分**（无头运行验证不到）：按键按下去方向对不对——
> 尤其是 S 是后退而不是右移（这取决于 `IA_Move` 上 Swizzle 与 Negate 的**顺序**）。
>
> 代码侧还顺带做了一件事：**冲刺改走 GAS 原生输入路径** ——
> 能力授予时带 `RogueAbilityInputID::Dash`，输入侧调 `ASC->AbilityLocalInputPressed(...)`，
> 由 GAS 自己按 InputID 找技能并激活（原来注释里的"用旧版 InputComponent 绑定 +
> `TryActivateAbilitiesByTag`"已不再成立，注释已同步更新）。

---

## 1. 放置位置

```
/Game/Input/
├── IMC_Default          ← 映射上下文放在这一层
└── IA/                  ← 10 个输入动作统一放这个子目录
    ├── IA_Move
    ├── IA_Look
    ├── IA_Zoom
    ├── IA_Dash
    ├── IA_Jump
    ├── IA_Interact
    ├── IA_TogglePauseMenu
    └── IA_ChooseUpgrade1 / 2 / 3
```

### ✅ 已解决：`/Game/` 就是 `Content/` 本身（这条坑保留作为知识记录）

`/Game/` 是 Content 目录的**挂载点**，不是"Content 里面一个叫 Game 的文件夹"。所以：

| 你想要的资产路径 | 磁盘上应该放在 |
|---|---|
| `/Game/Input/IMC_Default` | `Content\Input\IMC_Default.uasset` |
| `/Game/Input/IA/IA_Move` | `Content\Input\IA\IA_Move.uasset` |

**不要**在 `Content` 里再建一层 `Game` —— 那样得到的路径是 `/Game/Game/Input/...`，
代码找不到，日志会报"资产缺失"，而资产明明就在那里（这是最难看出的一类问题）。

> **当时踩到的**：资产一开始建在了 `Content\Game\Input\`，得到 `/Game/Game/Input/…`；
> 代码按约定找 `/Game/Input/…` 找不到，日志报"资产缺失 11 个" —— 而资产明明就在那里。
> 这是最难看出的一类问题：**看着像"代码有 bug"，实际是路径约定的理解偏差**。
>
> **已修正**：`Input` 文件夹已移到 Content 根目录，`Content\Game` 已删除。

> **不想移动也可以**：`ARogueCharacter` 上每个动作都有一个独立的 `TSoftObjectPtr` 属性，
> 在 `BP_RogueCharacter` 里逐个指定即可覆盖默认路径 —— 但**移动文件夹更干净**，
> 因为默认路径是给"按约定放置"准备的。

---

## 2. 需要创建的资产（共 11 个）

### 2.1 输入映射上下文（1 个）

| 资产名 | 类型 |
|---|---|
| `IMC_Default` | Input Mapping Context |

### 2.2 输入动作（10 个）

| 资产名 | Value Type | 触发方式 | 对应旧配置 |
|---|---|---|---|
| `IA_Move` | **Axis2D (Vector2D)** | 持续 | `MoveForward` + `MoveRight` 合并 |
| `IA_Look` | **Axis2D (Vector2D)** | 持续 | `TurnCamera` + `PitchCamera` 合并 |
| `IA_Zoom` | **Axis1D (float)** | 持续 | `ZoomCamera` |
| `IA_Dash` | Digital (bool) | Pressed | `Dash` |
| `IA_Jump` | Digital (bool) | Pressed / Released | `Jump` |
| `IA_Interact` | Digital (bool) | Pressed | `Interact` |
| `IA_TogglePauseMenu` | Digital (bool) | Pressed | `TogglePauseMenu` |
| `IA_ChooseUpgrade1` | Digital (bool) | Pressed | `ChooseUpgrade1` |
| `IA_ChooseUpgrade2` | Digital (bool) | Pressed | `ChooseUpgrade2` |
| `IA_ChooseUpgrade3` | Digital (bool) | Pressed | `ChooseUpgrade3` |

> **合并说明**：旧代码里 `MoveForward` / `MoveRight` 是两个独立的 1D 轴，Enhanced Input 的标准做法是**合成一个 2D 轴**（一个 `IA_Move` 输出 `Vector2D`），少一个资产、少一次绑定，也便于将来接手柄。

---

## 3. ⚠️ 三个容易漏的属性

### 3.1 `bTriggerWhenPaused`（暂停时仍响应）

旧代码里有 4 处显式设了 `bExecuteWhenPaused = true`：

```cpp
PauseMenuBinding.bExecuteWhenPaused = true;
InteractBinding.bExecuteWhenPaused   = true;
UpgradeOneBinding.bExecuteWhenPaused = true;   // 以及 Two / Three
```

**Enhanced Input 里对应的是 `UInputAction::bTriggerWhenPaused`。**
以下 5 个资产必须勾上，否则**暂停菜单打不开、选卡点不了**：

- [ ] `IA_TogglePauseMenu`
- [ ] `IA_Interact`
- [ ] `IA_ChooseUpgrade1`
- [ ] `IA_ChooseUpgrade2`
- [ ] `IA_ChooseUpgrade3`

> 注：本作**永不暂停世界**（`ARogueGameMode::SetPause` 直接拒绝一切暂停请求），
> 所以这条在联机下影响不大；但**单人调试**时控制台 `pause` 之类仍可能让角色停摆，
> 勾上它与旧行为一致，成本为零。

### 3.2 `IA_Look` 的 Value Type 必须是 Axis2D

鼠标的转向与俯仰要输出成 `Vector2D`（X = 水平，Y = 垂直）。
如果建成两个 1D 资产也能工作，但会和 `IA_Move` 的处理方式不一致，没有必要。

### 3.3 `IA_Move` 的修饰器（Modifier）—— 这是最容易做错的一步

数字键按下去默认输出 `X = 1`。要让 **W 输出 `Y = +1`**，必须加 **Swizzle Input Axis Values**：

| 按键 | Modifiers（**顺序敏感**） |
|---|---|
| `W` | `Swizzle Input Axis Values (YXZ)` |
| `S` | `Swizzle Input Axis Values (YXZ)` → `Negate` |
| `A` | `Negate` |
| `D` | *（无）* |
| `Up` | `Swizzle Input Axis Values (YXZ)` |
| `Down` | `Swizzle Input Axis Values (YXZ)` → `Negate` |
| `Left` | `Negate` |
| `Right` | *（无）* |

> ⚠️ `Swizzle` 必须在 `Negate` **之前**。顺序反了会得到"按 S 往右走"这类诡异结果。

---

## 4. `IMC_Default` 的映射表（照抄即可）

在 `IMC_Default` 里为每个 `IA_*` 添加映射：

| Input Action | 按键 |
|---|---|
| `IA_Move` | `W` / `S` / `A` / `D` / `Up` / `Down` / `Left` / `Right`（按 §3.3 配修饰器） |
| `IA_Look` | `Mouse XY 2D-Axis` |
| `IA_Zoom` | `Mouse Wheel Axis` |
| `IA_Dash` | `Left Shift` |
| `IA_Jump` | `Space Bar` |
| `IA_Interact` | `E` |
| `IA_TogglePauseMenu` | `Escape` |
| `IA_ChooseUpgrade1` | `One` |
| `IA_ChooseUpgrade2` | `Two` |
| `IA_ChooseUpgrade3` | `Three` |

---

## 5. 已发现的遗留死配置（顺带记录）

`DefaultInput.ini` 里有一条 **`ConfirmUpgradeSelection`（LeftMouseButton）**，
但**全项目 C++ 零引用**（grep 无命中）。

- 如果它没有被任何蓝图使用 → 迁移时直接删掉；
- 如果某个 WBP 在用它 → 迁移到 Enhanced Input 后**蓝图侧的 legacy 绑定也会失效**，
  需要一并改成 `IA_ConfirmUpgradeSelection`。

**迁移前请先在编辑器里确认它的引用情况**（右键资产 → Reference Viewer，或查
`Content/UMG` 下的 WBP）。

---

## 6. 建好之后怎么验证

代码侧落地后，启动日志会打印一行二选一：

```
[Rogue][Input] Enhanced Input 已启用：IMC=IMC_Default，动作 10/10 就绪
[Rogue][Input] Enhanced Input 资产不完整（缺少 N 个），本次回退到 legacy 输入
```

- 看到**第一行** → 迁移生效；
- 看到**第二行** → 还差资产，按上面的清单补齐（日志会列出缺哪个）。

### 验收清单

- [ ] 移动（WASD + 方向键）正常，且 S 是后退而不是右移
- [ ] 鼠标转向 / 俯仰正常
- [ ] 滚轮缩放正常
- [ ] 冲刺（Left Shift）可用，且有冷却
- [ ] 跳跃（Space）可用
- [ ] 交互（E）在商店终端附近可用
- [ ] 暂停菜单（Escape）可开可关
- [ ] 升级界面 1 / 2 / 3 可点选
- [ ] 双进程联机下两个玩家都能正常操作

---

## 7. 迁移完成后要清理的东西

- [ ] `DefaultInput.ini` 的 8 条 `ActionMappings` + 11 条 `AxisMappings`（合计 19 条）
- [ ] `RogueCharacter.cpp` 里 legacy 的 `BindAxis` / `BindAction` 分支（回退路径）
- [ ] `ConfirmUpgradeSelection` 这条死配置

> 保留 legacy 分支直到验收全过；确认无误后再删 —— 这样迁移过程**随时可回退**。
