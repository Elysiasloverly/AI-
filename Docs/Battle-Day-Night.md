# 白昼、黑夜与百波通关

更新：2026-10-30。

## 当前完整流程

| 阶段 | 地图 | 波次 | 推进规则 | 结束方式 |
|---|---|---|---|---|
| 第一次白天 | `/Game/Map/Battle_Day` | 1–35 | 本波生成完成，并清空敌人与 Boss | 清场后倒计时 30 秒 |
| 第一次黑夜 | `/Game/Map/Battle_Night` | 36–50 | 每 28 秒推进，不等清场 | 第 50 波计时结束后停刷；清场后倒计时 30 秒 |
| 第二次白天 | `/Game/Map/Battle_Day` | 51–85 | 本波生成完成，并清空敌人与 Boss | 清场后倒计时 30 秒 |
| 第二次黑夜 | `/Game/Map/Battle_Night` | 86–99 | 每 28 秒推进，不等清场 | 进入第 100 波立即胜利 |

100 是通关触发波数：不会生成第 100 波普通敌人或 Boss，也不要求通关后继续清场。三次切图都由服务端停刷、等待场上敌人与 Boss 清空，再统一倒计时和切图；提前清场不能提前推进黑夜普通波次。最后一段黑夜的 99 → 100 则直接结算。

## 在哪里调整

打开 `/Game/Data/Core/DA_GameBalance`，展开“08.昼夜阶段”中的 `BattleStage`：

| 编辑器字段 | 成员 | 当前值 |
|---|---|---|
| 第一次切换：白天结束波次 | `DayWaveCount` | 35 |
| 第二次切换：黑夜结束波次 | `FirstNightEndWave` | 50 |
| 第三次切换：白天结束波次 | `SecondDayEndWave` | 85 |
| 昼夜切图倒计时 | `CountdownSeconds` | 30 秒 |
| 黑夜波次时长 | `NightWaveDuration` | 28 秒 |
| 黑夜敌人攻速倍率 | `NightAttackSpeedMultiplier` | 2 |

三个结束波数必须依次递增，并且小于 100。服务器初始化时会规范异常配置：第一项 1–97，第二项大于第一项且不超过 98，第三项大于第二项且不超过 99。编辑器只提供单项范围约束；不要依赖自动纠正来表达设计意图。修改资产后开启新局生效。

例如配置 10、20、30：白天 1–10，黑夜 11–20，白天 21–30，黑夜 31–99，进入 100 通关。

白天地图和黑夜地图可在同一设置中指定。新局从白天开始；直接打开黑夜地图用于调试时，从第一次白天结束波次 +1 开始，不读取旧局的临时成长。

## 切图保留什么

`FRogueRunState.BattleStageIndex` 记录同局的第 0/1/2/3 段，随 `URogueRunTravelSubsystem` 的一次性快照一起传递。返回同一张白天地图仍接续第 51 波，不会回到第 1 波；最后一次进入黑夜仍接续第 86 波，不会回到第 36 波。

`ServerTravel` 携带 `RogueStageContinue=1` 并使用 seamless travel。`PlayerState` 显式承接每位玩家的等级、经验、金币、实际生命与属性、已装备武器、卡牌次数、待选卡、个人永久成长及已发放的 Boss 奖励记录。新世界重新建立 ASC / Avatar，再恢复一次局内成长。普通新局、失败重开和返回主菜单不继承该快照。

黑夜每次进入时重新建立阶段时钟，与整局存活时间分开。休整不增加战斗用时。黑夜敌人攻速为配置倍率，切回白天恢复 1 倍；倍率按新敌人初始化赋值，不在每次切图后累乘。它不改变移速、弹速或伤害倍率。

## 刷怪名额与边界

每波普通怪名额由 `DA_GameBalance` 的刷怪设置与增长曲线计算；Boss 另计。成功生成才消耗名额，出生点失败可重试。各类型 `MaxAlive`、`MaxSpawnPerWave` 仍生效，全场同时存活上限已移除。

白天短暂空场不能跳过尚未生成的名额。黑夜到时切换，未消耗名额不结转，旧敌人继续存在并占用类型名额。卡帧可跳过多个周期，但不会越过切图清场边界或第 100 波；不会集中补刷跳过波次的敌人。

## 提示与结算 WBP

- `/Game/UMG/Menus/InGame/WBP_NightTransition` 保留现有设计器样式：`TitleText`、`CountdownText`、`DescriptionText` 按目的地显示“暮色将至”或“曙光再临”。倒计时读取同一份服务端截止时间；设置中可分别改白天 / 黑夜倒计时格式。
- 提示不捕获输入，结束或销毁世界时收起。商店与选卡仍在更高层级；倒计时不暂停世界。
- `/Game/UMG/Menus/InGame/WBP_VictoryScreen` 为独立通关页面，显示波数、用时和全队击败数，只有“返回主菜单”一个按钮。标题、布局、配色可在 WBP 设计器编辑。
- `GameState.RunResult` 一并复制胜负原因和最终统计；HUD 收起选卡、商店、复活、失败和阶段倒计时后显示胜利。结算立即停刷、取消复活并退出战斗，不额外伪造击杀或发放 Boss 奖励。

## 维护入口

| 内容 | 文件 |
|---|---|
| 资产字段与规范化 | `Framework/RogueBattleStage.h` |
| 阶段切换、地图恢复 | `Framework/RogueGameMode.BattleStage.cpp` |
| 白天清场、黑夜时钟 | `Framework/RogueGameMode.cpp` |
| 100 波胜利与战斗清理 | `Framework/RogueGameMode.Victory.cpp` |
| 成长快照 | `Player/RoguePlayerState.StageTravel.cpp` |
| 玩家教程 | [Player-Guide.md](Player-Guide.md) |
| 教程更新流程 | [Tutorial-Maintenance.md](Tutorial-Maintenance.md) |

开发环境可用独立 QA 存档运行 `Rogue.Test.StageCycle`，在双独立进程中快速经过三次真实切图；为缩短回归，该命令把测试局的休整临时设成 1 秒，不改保存资产的 30 秒。`Rogue.Test.Victory` 让测试局跨过 99 → 100；两者不在正式玩法自动执行。
