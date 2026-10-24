# HUD 状态条与敌人血条（WBP）

生命 / 护甲 / 经验条和敌人血条的外观已经从 `ARogueHUD` 的原生 `DrawRect` 拆到 UMG：

- **C++ 负责**：选可见对象、算百分比、投影屏幕坐标、对象池与脏检查；
- **WBP 负责**：颜色、字体、动画、Boss 样式；进度条与文字由 C++ 通过 `BindWidgetOptional` 直接写入；
- 两边通过两个视图结构体通信，C++ 不引用任何 Slate 组件。

## 一键生成占位 WBP

不想手建时，用编辑器脚本生成两个可直接运行的占位资产：

```bat
"C:\Program Files\Epic Games\UE_5.5\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" ^
  "F:\AI_WorkSpeace\baka\Baka.uproject" ^
  -run=pythonscript -script="F:\AI_WorkSpeace\baka\Tools\create_hud_widgets.py" ^
  -unattended -nopause -nullrhi -nosplash
```

- 产物：`/Game/UMG/HUD/WBP_PlayerVitals`、`/Game/UMG/HUD/WBP_EnemyHealthBar`；
- `WBP_PlayerVitals` 包含：左上角 `WaveText` / `GoldText`，左下角 `HealthBar` / `ArmorBar` / `ExperienceBar` 与对应的 `HealthText` / `ArmorText` / `ExperienceText`；由 C++ 的可选绑定直接更新，所以不写事件图也能动；
- 文字统一使用项目复合字体 `/Game/UMG/Fonts/F_DroidMono_CJK`（DroidSansMono 为主，中文回退 DroidSansFallback）；在 WBP 里改字体时请保留这个字体对象，否则中文可能变成方框；
- 敌人血条为 `FillBar`，由 C++ 的可选绑定直接更新；
- 重复执行会清空并重建控件树，不会重复创建资产；
- 依赖：`Baka.uproject` 已启用 `PythonScriptPlugin` 与 `EditorScriptingUtilities`（仅 Editor），并新增 `aiEditor` 编辑器模块；控件树由 `URogueWidgetBuilder` 用 C++ 建，因为 Python 读不到 `WidgetTree`（protected）；
- 生成后可直接在设计器里改颜色、字体、动画；C++ 只按名字绑定，不关心外观。

脚本以 `[HUDWidgets]` 前缀写入当次运行日志（默认 `Saved/Logs/ai.log`）；日志会滚动，历史记录可能已被清理。

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
4. 设计器里放三条 `ProgressBar`（命名 `HealthBar` / `ArmorBar` / `ExperienceBar`），放在左下角；根 Canvas Panel 会自动铺满屏幕。
5. 左上角放两个 `TextBlock`（命名 `WaveText` / `GoldText`）；三条状态条旁放 `HealthText` / `ArmorText` / `ExperienceText`。C++ 会直接更新命名绑定，不写事件图也能显示。
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
- **显示**：绿色 `(0.35, 1.0, 0.5)`，文本 `+N`（不足 1 显示 +1），生命周期 0.9s，
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

## 冲刺冷却条（2026-10-24）

`/Game/UMG/HUD/WBP_PlayerVitals` 的 `VitalsRows` 中增加 `DashCooldownRow`，位于经验条下方。

- `DashCooldownBar`：冷却开始时为空，随实际冷却恢复填满；就绪时亮蓝。
- `DashCooldownText`：显示剩余秒数或“冲刺就绪”；濒死时显示“冲刺不可用”并置灰。
- 在 WBP 设计器中调整尺寸、字体和进度条笔刷；保留上面的绑定名称即可。就绪/冷却/不可用颜色在类默认值的“Rogue → 冲刺提示”中修改。
- HUD 经 `RogueVitalsView::UpdateDashState` 读取本地角色 ASC 的冲刺能力、状态标签和当前冷却 GE 的实际剩余时间/总时长。没有额外计时器，不改动冲刺规则；联机时各客户端读取自己的预测/复制效果。
- 下一次冷却属性发生变化时，不会改变正在计时的 GE 总时长；清除冷却或更换角色后重新读取当前状态，避免旧进度残留。护甲条隐藏时，冲刺条仍然保留。

本次资产更新使用 `RogueUIThemeBuilder.add_dash_cooldown_to_vitals` 追加控件，保留现有生命、护甲、经验控件和事件图。不要为增加冲刺条重新运行早期的整树生成脚本。
