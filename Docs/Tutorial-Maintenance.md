# 教程维护与事实核对

更新：2026-11-09。

主维护入口见 [维护与扩展指南](Maintenance-Extension-Guide.md)。本页说明教程内容如何同步、哪些事实已经对照源码和保存资产核验。

## 1. 三份内容分别在哪里

| 内容 | 位置 | 作用 |
|---|---|---|
| 制作源 | `SourceArt/UI/tutorial_chapters.json` | 六章导航、标题与说明卡文字 |
| 游戏内教程 | `/Game/UMG/Menus/MainMenu/WBP_Tutorial` | 运行时实际显示的内容与样式 |
| Markdown 玩家指南 | `Docs/Player-Guide.md` | 可直接阅读的玩法说明 |

运行时不读取 JSON，也不会自动把资产数值插入教程。只改 JSON 不会改变游戏内文字；只改 WBP 会使下次制作源重建时恢复旧内容。

WBP 左侧六章导航，右侧各自滚动。`Chapter0Button`～`Chapter5Button` 与 `ChapterSwitcher` 由原生类切换；`CloseButton` 返回主菜单。普通说明卡正文名为 `ChapterXCardYBody`；敌人/Boss 图鉴使用独立控件名，见下表。

### 自定义图鉴的文字映射

| 制作源中的卡片 | 当前实际控件 |
|---|---|
| 第五章 cards[4]～[7] | EnemyBestiaryText0～3；标题为 EnemyBestiaryTitle0～3 |
| 第五章 cards[8]～[16] | BossGuideText + 技能键；标题为 BossGuideTitle + 技能键 |

九个技能键依次为 Octagram、RollingSuns、JudgmentRay、LastBloom、ScarletRain、FlameCrescents、EndlessRing、DiamondReversal、FrostPursuit。

JSON 的 `body_widget` / `heading_widget` 已记录这 13 张自定义图鉴卡的映射，供精确文字维护使用。通用 BuildTutorialFromJson 不读取这两个附加字段，重建时仍会按普通卡名创建控件，不能用它恢复当前图鉴的人工排版。读取通用卡名返回空字符串，不代表当前界面缺少该段说明。

本轮按实际控件映射核对全部 6 章、37 张卡，并把制作源中残留的旧“火丸”文字同步为已经保存的冰焰·陨星说明。未重建图鉴布局。

## 2. 更新步骤

1. 对照实际调用链判断行为，再检查关卡/BP 引用的资产。不要只读头文件默认值。
2. 改 JSON 的对应卡片，同步玩家指南。
3. 少量文案使用 `RogueWidgetBuilder.UpdateTextBlock` 精确修改已有 `ChapterXCardYBody`，保留自定义控件树、字体和间距。
4. 编译并保存目标 WBP，读取文字回查，确认三份表述一致。
5. 修改段落长度或布局时，再在实际窗口检查换行、滚动与按钮可达性。

`BuildTutorial` / `BuildTutorialFromJson` 会重建布局。仅在明确需要整页重建且已经备份时调用；`UpdateTutorialText` 面向早期单正文页面，不适用于当前分章页。日常 UI 验证入口见 [Tools/UI/README.md](../Tools/UI/README.md)。

## 3. 当前玩法事实

以下保存值在 2026-11-09 读取自 `DA_GameBalance`、`DA_PlayerBalance`；行为同时对照当前源码。用户调整资产后应重新更新，不能把这张表当作永久常量。

| 项目 | 当前规则 / 保存值 |
|---|---|
| 主动选卡 | 升级与 Boss 额外奖励累计次数，按 IA_Choice 对应的 Q 打开；1 / 2 / 3 或鼠标确认 |
| 收起选卡 | Q / Esc 收起，同组卡片和未消费次数保留，不免费重抽 |
| 待选提示 | 状态栏上方淡蓝色呼吸文字；等级框只保留标题与数字，不再显示下方 Q/待选行 |
| 玩家状态与冲刺 | 状态栏底部居中；冲刺冷却在角色右侧圆弧显示，填满就绪 |
| 菜单与伤害 | 商店、选卡正常承伤；暂停/设置的保护、冲刺保护和复活保护独立保留，世界不暂停 |
| 普通波推进 | 白天与黑夜都要求本波名额生成完且清场，波间休整 3 秒 |
| 昼夜切图 | 20 / 40 / 60 / 80 波清场后，倒计时 30 秒，全体进入对应昼夜随机地图；各池 5 张 |
| 通关 | 第 100 波仍完整战斗；生成完并清掉终局 Boss 和场上敌人后结算，结算仅返回主菜单 |
| 普通怪总量 | 第一波 45，每波增加 5，每波累计总量封顶 0（不限）；类型限制保留 |
| 黑夜强化 | 数量倍率 3、生成倍率 2；近战占比 90%，近战移速 1.8 倍、伤害 2 倍；Boss 另计 |
| Boss | 间隔 10 波，额外选卡 3 次，每名参与玩家获 3 冰砖 |
| 商店 | 6 张商品，基础价 24、刷新基础价 16、自动刷新 60 秒；实际价仍受购买/刷新累计影响 |
| 商店终端 | 波次清场后落到玩家附近；监听服优先房主，个人商店全部关闭后蓄力离场 |
| 复活 | 濒死 10 秒、恢复 50% 生命、保护 1.5 秒；全员濒死失败 |
| 永久成长 | 武器与专属卡解锁、属性树按 DA_UpgradeTree 配置；实际个人卡池仍检查本局前置 |
| 治疗 | 绿色圈范围治疗、绿色线单体治疗；绿色数字显示实际回复，不超过最大生命 |

旧说明中的“进入第 100 波立即通关”“固定 70% 近战、移速 2 倍”“商店基础价 18、刷新价 8”不能用于解释当前保存配置。

## 4. 事实核对入口

| 规则 | 源码入口 |
|---|---|
| 生成完成、清场、3 秒波间休整、最终通关时机 | `Framework/RogueGameMode.cpp` 的 TickRunSystems、`RogueGameMode.Victory.cpp` |
| 昼夜切图与本局成长承接 | `RogueGameMode.BattleStage.cpp`、`RogueBattleStage.h`、`Player/RoguePlayerState.StageTravel.cpp` |
| Q 开关、消费与保存卡组 | `RogueGameMode.Progression.cpp`、`Player/RoguePlayerState.cpp`、`UI/RogueHUD.Menus.cpp` |
| 菜单伤害保护边界 | `AbilitySystem/RogueAbilitySystemLibrary.cpp` |
| 提示文字、等级居中、圆弧 | `RogueVitalsWidgetBase.cpp`、`RogueDashRingWidget.cpp`、保存的 WBP_PlayerVitals |
| 商店报价与终端移动 | `Progression/Shop/RogueShopSystem.cpp`、`RogueGameMode.WaveShop.cpp`、`World/RogueShopTerminal.cpp` |
| Boss 奖励与本地持久化 | `RogueGameMode.Progression.cpp`、`RogueProgressionSubsystem` |
| 个人卡池与前置 | `RogueUpgradeTreeDefinition`、`RogueUpgradeSystemRules` |
| 濒死、团灭与复活 | `RogueCharacter.LifeCycle.cpp`、`RogueGameMode.Players.cpp` |
| 治疗与绿色数字 | `RogueEnemySupportComponent.cpp`、`RogueEnemy.HealingFeedback.cpp` |
| 联机准备与开局 | `Networking/RogueLobby`、`RogueRoomSubsystem` |

特别注意：`CompleteRunIfVictoryReached()` 的条件不能脱离调用位置解读。当前只在 `IsWaveComplete` 确认本波完成后调用，所以它表示打完第 100 波，而不是进入第 100 波。

## 5. 按改动范围验证

- 纯 Markdown：查来源、链接和表述，不需要重编 C++。
- 仅改已有 TextBlock：核对改前/改后文字、编译目标蓝图、保存；没有必要重建全部 UI。
- 字体/布局：检查六章导航、各自滚动、返回按钮、小窗口换行。
- 行为变化：使用隔离测试存档，验证房主和加入者各自的选卡、承伤、旅行、胜负与奖励；结果页不能叠出商店、选卡或复活界面。
- 数值变化：记录读取日期、资产路径和实际引用；资产值不等于 C++ 默认值。

本轮维护使用源码核对与资产读取，并不代表重新完成百波游玩或性能压测。各功能的运行验证入口和既有证据分别记录在对应专题文档中。
