# Q 键主动选卡

更新于 2026-11-09。

## 玩家看到的行为

- 升级和 Boss 奖励只增加待选次数，不自动弹出选卡。
- 状态栏上方的淡蓝色呼吸提示显示自己的未消费次数与 Q 按键；等级区不再重复计数。按 Q 打开，用 1 / 2 / 3 或鼠标选一张。
- 确认选择才消耗一次；还有次数时继续显示下一组。Q 或 Esc 可随时收起，稍后打开仍是同一组卡，不能靠开关界面刷卡。
- 商店和选卡都不提供免伤，世界继续运行。暂停/设置菜单的保护、冲刺无敌与复活保护各自保持原规则。
- 濒死会收起界面；仍有队友存活时保留未消费次数。切图也保留，但抵达新地图不会强制弹出。整局结束或新开一局会清空。

## 数据与调用链

`IA_Choice → Character::ToggleUpgradeSelection → HUD::HandleUpgradeSelectionKey → PlayerState::Server_ToggleUpgradeSelection → GameMode::ToggleUpgradeSelectionForPlayer`

PlayerState 是每位玩家的状态载体，队列与当前卡组仅复制给所属客户端。GameMode 在服务端生成和校验卡片；客户端不能直接修改次数或领取隐藏卡片。

| 状态 | 含义 |
|---|---|
| QueuedUpgradeSelections | 尚未生成卡组的奖励次数 |
| PendingUpgrades | 已生成且尚未消费的一组卡；收起后保留 |
| bAwaitingUpgradeChoice | 当前是否正在显示选择界面，与“有奖励可选”区分 |
| GetPendingUpgradeCount | 队列次数 + 当前卡组占用的一次奖励 |

`SuspendUpgradeSelection` 只收起界面；`CloseUpgradeSelection` 会清空当前卡组，供选择成功、重置等流程使用。重新打开保留卡片内容并更换 SelectionId，迟到或重复点击不能消费另一轮奖励。卡池为空时不会先扣掉队列次数。

HUD 把计数写入 `FRoguePlayerVitalsView.PendingUpgradeSelections`，由 WBP_PlayerVitals 的 `PendingUpgradeHintText` 在状态栏上方显示，并使用 NativeTick 做柔和呼吸。等级下方的 `PendingUpgradesText`、Q 徽标已从当前 WBP 移除；原生类中的可选旧绑定仅为兼容其他 WBP。字体、颜色、位置、呼吸周期和最低透明度都可在 WBP 调整。

## 验证位置

RogueManualUpgradeTests.cpp 覆盖累积、按需生成、暂存、防重复消费、菜单互斥、濒死保留、切图保留与计数刷新；RogueBossUpgradeTests.cpp 覆盖商店和选卡正常承伤，以及独立的暂停保护。
