# 教程维护与事实核对

更新：2026-10-30。

## 玩家入口与排版

主菜单 → 教程说明 → `WBP_Tutorial`。左侧六章导航，右侧为独立可滚动的说明卡片。正文字号 22，不再把全部规则挤进一个长文本。

章节：入门与操作、波次与通关、武器与选卡、商店与成长、敌人与治疗、联机与设置。`CloseButton` 返回主菜单；`Chapter0Button` 到 `Chapter5Button` 与 `ChapterSwitcher` 由原生类负责切换。文字、圆角、间距和颜色都保存在 WBP 设计器中。

## 一份内容同步两个结果

制作源为 `SourceArt/UI/tutorial_chapters.json`；六个 `chapters` 各含 `nav`、`title`、`cards`（`heading`、`body`）。运行时不读取该文件，内容已嵌入 WBP。

修改时先核对实际玩法和资产，再修改 JSON，同步生成 `Docs/Player-Guide.md` 与 WBP。`RogueWidgetBuilder.BuildTutorialFromJson` 会重建布局，调用前备份自定义 WBP；只改少量文字可在设计器编辑对应 `ChapterXCardYBody`，并同步 JSON 和指南。旧 `UpdateTutorialText` 面向旧版单正文布局，不适用于分章页面。`BuildTutorial` 也从这份 JSON 构建，避免恢复过时文案。

## 本次核对的实际配置

| 项目 | 当前资产值 / 行为 |
|---|---|
| 三次切换波数 | `DA_GameBalance → BattleStage`：35、50、85；服务端规范为递增且均小于 100 |
| 切图流程 | 停刷 → 场上敌人与 Boss 清完 → 30 秒倒计时 → 全体切图 |
| 白天推进 | 完成本波生成且清场才进入下一波 |
| 黑夜推进 | 本波普通怪生成完毕并清空敌人与 Boss 后推进 |
| 黑夜强化 | 普通怪每波数量与生成速度 2 倍；攻速仍为 1 倍 |
| 通关 | 进入第 100 波立即结束，不生成第 100 波敌人；结算仅返回主菜单 |
| Boss 冰砖 | 每名参与玩家 3 个；与旧文案默认 1 个不同 |
| 商店 | 6 个商品、基础价 18、初始刷新价 8、自动刷新 60 秒；正文引导查看实际价格 |
| 复活 | 濒死 10 秒，恢复 50% 生命，保护 1.5 秒；全员濒死则失败 |
| 永久成长 | 武器 / 每张专属卡各 5 冰砖；属性每级 3 冰砖、最多 5 级；先花 3 冰砖解锁护甲 |
| 治疗 | 绿色圈为范围治疗、绿色线为单体治疗；绿色 + 数字为实际恢复，不超过最大生命 |

这些是保存于资产的值，不等于 C++ 默认值。用户调整平衡后需同步文字；教程不会自动跟随任意资产修改。

## 代码核对入口

| 规则 | 入口 |
|---|---|
| 三次切图与继承局内成长 | `RogueGameMode.BattleStage.cpp`、`RogueBattleStage.h`、`RoguePlayerState.StageTravel.cpp` |
| 昼夜清场推进 / 100 波胜利 | `RogueGameMode.cpp`、`RogueGameMode.Victory.cpp` |
| 胜负与结算统计复制 | `RogueGameState` 的 `FRogueRunResult`；其余局内状态仍独立复制 |
| 胜利 WBP | `/Game/UMG/Menus/InGame/WBP_VictoryScreen`，原生父类 `RogueVictoryScreenWidgetBase` |
| 个人卡池与前置 | `RogueUpgradeTreeDefinition`、`RogueUpgradeSystemRules` |
| 商店涨价、刷新 | `RogueShopSystem.cpp` |
| Boss 奖励与本地持久化 | `RogueGameMode.Progression.cpp`、`RogueProgressionSubsystem` |
| 濒死、团灭与复活 | `RogueCharacter.LifeCycle.cpp`、`RogueGameMode.Players.cpp` |
| 治疗移动、治疗数字 | `RogueEnemySupportComponent.cpp`、`RogueEnemy.HealingFeedback.cpp` |
| 联机准备与开局 | `RogueLobby`、`RogueRoomSubsystem` |

## 更新后的检查

1. 六个章节都可点击，滚动各自独立，返回主菜单可用。
2. 窄窗口检查换行、底部按钮和滚动条；保持阅读字号。
3. 用独立测试存档验证三次切图后的波数、个人成长和攻速，以及所有客户端的胜利结算。
4. 胜利不得叠出失败、复活、商店或选卡；唯一按钮退出当前对局返回主菜单。

灵符·归心新增时同步七种武器标题与第三章说明卡正文；仅更新现有 TextBlock，不重建自定义布局。
