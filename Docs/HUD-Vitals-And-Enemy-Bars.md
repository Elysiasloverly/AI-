# HUD 状态条与敌人血条（WBP）

更新：2026-11-09。当前玩家状态栏尺寸、等级居中、上方升级提示与角色旁冲刺圆弧见 [状态栏维护](Centered-Vitals-Polish.md)。[2026-11-04 战斗 HUD 精修](Battle-HUD-Polish-2026-11-04.md) 保留当时的样式记录。

生命 / 护甲 / 经验条和敌人血条的外观已经从 `ARogueHUD` 的原生 `DrawRect` 拆到 UMG：

- **C++ 负责**：选可见对象、算百分比、投影屏幕坐标、对象池与脏检查；
- **WBP 负责**：颜色、字体、动画、Boss 样式；进度条与文字由 C++ 通过 `BindWidgetOptional` 直接写入；
- 玩法结果通过视图结构体传给界面；多数控件使用 UMG 绑定，角色旁冲刺圆弧由原生 `URogueDashRingWidget` / Slate 矢量绘制。

## 当前日常维护入口

直接编辑保存的 `/Game/UMG/HUD/WBP_PlayerVitals`。状态栏底部居中、设计尺寸 620×144；冲刺冷却在角色右侧，待选升级提示在状态栏上方。等级框只有标题与数字。

验证使用 `Tools/UI/manage_ui.py` 的默认模式：只在内存中编译检查并写入报告，不保存资产。命令与显式重建的区别见 [UI 工具说明](../Tools/UI/README.md)。

| 绑定 | 当前用途 |
|---|---|
| LevelBadgeContent / LevelCaption / LevelText | 框内共同居中的等级标题与数字 |
| HealthText / HealthValueText、ArmorText / ArmorValueText | 属性名称与读数分开；数值由 ScaleBox 约束显示范围 |
| ArmorUnavailableText | 无护甲时的淡灰说明 |
| ExperienceText / ExperienceValueText / ExperienceBar | 底部经验行 |
| PendingUpgradeHintText | 淡蓝色待选次数 / Q 提示；不再使用等级下方重复计数 |
| PlayerDashRing | 角色旁圆弧，读取现有 GAS 冷却并投影跟随 |

`Tools/create_hud_widgets.py`、原生 `BuildPlayerVitals` 生成的是早期占位布局，会覆盖人工样式。当前排版制作入口为 `RefineCompactVitals`；它也只供明确重排并备份后执行，不应在运行时或每次打开编辑器时调用。

## 类与数据

| 类型 | 作用 | 文件 |
|---|---|---|
| `URogueVitalsWidgetBase` | 本地玩家生命 / 护甲 / 经验、左上角波数/金币 | `Public/UI/Widgets/RogueVitalsWidgetBase.h` |
| `URogueEnemyHealthBarWidgetBase` | 单个敌人血条（池化复用） | `Public/UI/Widgets/RogueEnemyHealthBarWidgetBase.h` |
| `FRoguePlayerVitalsView` | 生命/护甲/经验、波数、金币的视图数据 | `Public/UI/Widgets/RogueHUDViewData.h` |
| `FRogueEnemyHealthBarView` | 单个敌人血条视图数据 | 同上 |

数据流：

```text
HUD::Tick / DrawHUD
  → RefreshMenuWidgets()
      → UpdateVitalsWidget(本地角色)
          → URogueVitalsWidgetBase::UpdateVitals(View)   // 值没变就不触发蓝图
              → OnVitalsUpdated(View)                    // 蓝图实现
      → UpdateEnemyHealthBarWidgets()
          → RefreshEnemyHealthBarCache()                 // 0.05s / 0.10s 刷新
          → 池中取 widget，逐个 UpdateBar(View)
              → OnBarUpdated(View)                       // 蓝图实现
```

## 手建 WBP（可选）

### 1. 玩家状态条

1. Content Browser 里建 `/Game/UMG/HUD/`（已存在则跳过）。
2. 右键 → **User Interface → Widget Blueprint**，命名 `WBP_PlayerVitals`。
3. 打开后 **Class Settings → Parent Class** 选 `RogueVitalsWidgetBase`。
4. 设计器里放三条 `ProgressBar`（命名 `HealthBar` / `ArmorBar` / `ExperienceBar`），放在底部居中的状态区；根 Canvas Panel 铺满屏幕，角色旁圆弧位于同一全屏层。
5. 左上角放 `WaveText` / `GoldText`。属性名使用 `HealthText` / `ArmorText` / `ExperienceText`，另加对应 `HealthValueText` / `ArmorValueText` / `ExperienceValueText` 显示数值；未添加可选数值绑定的旧 WBP 仍会输出完整一行。等级标题和数字放在同一个居中容器。
6. 事件图：右键 → **Add Event → OnVitalsUpdated**，可追加动画/样式；数据字段包括 `HealthPercent` / `ArmorPercent` / `ExperiencePercent`、`CurrentHealth` / `MaxHealth`、`CurrentArmor` / `MaxArmor`、`Experience` / `ExperienceToNextLevel`、`CurrentWave`、`Money`。
7. 保存。HUD 会默认加载 `/Game/UMG/HUD/WBP_PlayerVitals`；也可以在建好后到 `BP_RogueHUD` 的 **VitalsWidgetClass** 手动指定。

### 2. 敌人血条

1. 同样新建 Widget Blueprint，命名 `WBP_EnemyHealthBar`，父类选 `RogueEnemyHealthBarWidgetBase`。
2. 设计一个约 **70×12** 的条（背景 + 填充 + 可选边框）；根控件尺寸就是屏幕上的血条尺寸。
3. 若想改尺寸：打开 WBP 的 **Class Defaults → Rogue|EnemyBar → 血条屏幕尺寸**，不要在设计器里靠缩放硬凑。
4. 事件图：**OnBarUpdated**，接：
   - `View.HealthPercent` → 填充条 `SetPercent`
   - `View.bBoss` → 换色 / 显示「首领」文字 / 加宽
5. 保存。默认路径 `/Game/UMG/HUD/WBP_EnemyHealthBar`，或在 `BP_RogueHUD` 的 **EnemyHealthBarWidgetClass** 指定。

## 行为与限制

- **池化**：敌人血条最多同时显示 22 个普通怪（高负载降到 12）+ 全部 Boss；widget 数量按需增长，不会随敌人总数增长。
- **刷新**：血条位置每帧更新；比例来自 `GetHealthPercent()`，非有限值按 0 处理。
- **过渡兜底**：玩家状态不再走 Canvas；`VitalsWidgetClass` 为空时不会显示玩家状态条，必须配置 `WBP_PlayerVitals`。敌人血条 WBP 为空时仍保留 Canvas 兜底。
- **Canvas 玩家状态已移除**：左上角原生命/护甲/经验/击败/等级/伤害/攻速/武器数量/冲刺/FPS/操作提示/商店提示不再由 `ARogueHUD::DrawHUD` 绘制；Canvas 只保留敌人血条兜底与伤害数字。
- **点击**：状态条设置为 `HitTestInvisible`，不拦截鼠标。
- **性能**：Tick 与 DrawHUD 各触发一次刷新（同帧最多两轮），每轮对每个可见血条调用一次 `SetPositionInViewport`，会触发一次布局。当前上限约 22 个，实测前不要假设它一定便宜；若以后要压到更低，可改成"单个全屏 layer + CanvasPanel 子项"的写法。

## 回血飘字

玩家生命上涨时，头顶会弹出绿色 `+N`，与敌人伤害数字共用同一条 Canvas 绘制管线
（`FRogueDamageNumber` 数组、`ARogueHUD::DrawDamageNumbers`，只是颜色与文本不同）。

- **入口**：`ARogueCharacter::SyncAttributeMirrors()` 每帧比对 `CurrentHealth`，
  上涨时调用 `ShowHealingNumber(Delta)`；持续回复、吸血、上限提升补血都走这一条路径。
- **节流**：0.35s 窗口内累计成一条（`PendingHealingNumber`），窗口到期由 Tick 补弹；
  和敌人伤害数字一样，避免多段回复刷屏。
- **不弹的情况**：首次属性同步（出生/进场）、`0 → 满血` 的整段补满（复活/重连初始化）。
- **显示**：绿色 `(0.35, 1.0, 0.5)`，文本 `+N`（不足 1 保留小数），生命周期 0.9s，
  位置在胶囊顶部再上移 34cm，带 ±16cm 随机偏移。
- **联机**：`SyncAttributeMirrors` 在各端都会跑，客户端据复制后的生命变化弹字；
  专用服务器没有本地 HUD，`RogueCombatFeedback::AddHealingNumber` 会自然跳过。

## 验证清单

- [ ] 受伤/回血时血条与 `HealthText` 变化；没有护甲上限（`MaxArmor <= 0`）时护甲条/`ArmorText` 隐藏（上限 > 0、当前为 0 时显示 0% 条）。
- [ ] 回血时玩家头顶弹出绿色 `+N`，持续回复在 0.35s 窗口内合并成一条，出生/复活补满不弹。
- [ ] 升级后经验条从 0 重新增长，`ExperienceText` 变化。
- [ ] 左上角 `WaveText` 随波次变化，`GoldText` 随金币变化。
- [ ] 敌人受伤时头顶血条变化，死亡后消失。
- [ ] Boss 血条使用 Boss 样式，普通怪用普通样式。
- [ ] 200 敌人压测时血条数量不超过上限，帧时无明显退化。
- [ ] 双人模式下每个窗口只显示自己玩家的状态条。

## 冲刺冷却圆弧（2026-11-09）

当前 `/Game/UMG/HUD/WBP_PlayerVitals` 使用角色旁的 `PlayerDashRing`，旧 `DashCooldownRow` 留作自定义 WBP 兼容绑定，在当前布局中收起。

- HUD 经 `RogueVitalsView::UpdateDashState` 读取本地角色 ASC 的冲刺能力、状态标签和冷却 GE 的实际剩余时间与总时长。圆弧不维护另一套计时器。
- 仅在可用、未濒死且有实际剩余冷却时间时显示，随充能填满；充满立即隐藏。只有阻挡标签而没有实际冷却计时，也不会留下空圆弧。
- 显示更新和每帧投影共用同一充能条件，避免定位逻辑重新显示满条。没有本地角色或投影失败时收起。
- 在 WBP 设计器的 `PlayerDashRing` 调半径、弧度、粗细与冷却颜色，在类默认值调屏幕与世界高度偏移；详见 [状态栏维护](Centered-Vitals-Polish.md)。
- 清除冷却或更换角色后重新读取状态，护甲是否解锁不影响冲刺显示。

不要重新运行旧整树生成脚本来改动圆弧，以免覆盖已保存的生命、护甲、等级和经验布局。
