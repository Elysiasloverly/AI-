# Boss 出生秒杀：真实 PIE 与池生命周期验证

2026-10-28。玩家报告选升级卡期间新刷 Boss，下一秒死亡。本次复现并修复两条独立路径；上一次只跑临时 Game 世界，未覆盖真实 PIE 的属性子对象状态，修复不完整。

## 一、真实 PIE 属性集成员仍为空

玩家对局日志 `player-session.log` 中，`BP_EnemySniper_C_0` 在 `UEDPIE_1_Battle_Day` 触发了属性集恢复失败的 ensure。

进一步在真正的编辑器 PIE 检查发现：`AttributeSet` 成员为空、`GetDefaultSubobjectByName("AttributeSet")` 也为空，但 **ASC 已登记的 AttributeSet 仍然存在**。UE 的默认子对象查询不仅检查名称，还检查默认子对象标志；PIE 中现有对象可能不满足该查询。上次只按默认子对象名恢复，漏掉了这一情况。

因此初始化只写了镜像，实际 GAS 生命和上限仍为 0。四类受影响蓝图为 Sniper、Shotgunner、Repeater、Ringcaster；选卡期间世界继续运行，任一真实伤害便足以致死。

修复在 `ARogueEnemy::PostInitializeComponents` 优先取 ASC 已登记的真实属性集，沿用同一个对象，不重新创建或共享属性集；客户端读取也跟随 ASC 的登记对象。

真实 PIE 对照流程：冷编辑器 MainMenu → 实际单人按钮处理入口 → Battle_Day → 打开真实选卡 WBP → 13 类实际 BP 各首次生成和回池复用 → 施加 1 点真实伤害 → 等待死亡定时器。结束 PIE，在内存重新编译基类和13个子蓝图，再执行第二轮。共 `2×13×2=52` 组，均是 `EWorldType::PIE`，不是临时 Game 测试世界。

| 对照 | 失败组数 | 四类异常 Boss 的实际生命 |
|---|---:|---|
| 修复前 | 16 / 52 | 显示 1000，GAS 0/0，1点伤害后死亡 |
| 修复后 | 0 / 52 | GAS 与显示均为1000，1点伤害后999，等待后仍存活 |

全程选卡 WBP 可见，世界未暂停且定时器持续推进。没有保存地图、蓝图或正式玩家进度。

## 二、灼光将旧生命的递增伤害带给新 Boss

`HellTowerTargets` 原来只记 Actor 指针。普通敌人和同类型 Boss 使用同一个精确类对象池；对象在两次武器采样之间死亡回池并重新生成，指针保持相同。旧校验看到“有效、存活、范围内”，便继续使用上一条命的递增伤害和残余计时。

这条路径可以在普通怪之间多次累积，最后由同一 Actor 生成 Boss 时继承。回归用例占用其他预热槽，确保实际对象池复用同一对象，使用35生命普通怪进行100次真实 GAS 击杀、延迟死亡、回池与重新取用，然后生成Boss并应用真实攻击力升级。

旧实现的测试能使884.8生命的控制样本Boss在10次0.1秒武器更新内归零。修复后，同样更新只结算新的基础伤害与本次锁定递增：伤害合计84，剩余800.8。这里的生命和伤害数值属于控制测试夹具，不是正式地图平衡配置。

现在每个锁定槽同时保存 Actor 和 `SpawnCycleId`。生成身份改变即重置伤害递增、伤害计时和光束计时，按当前升级后的基础值重新锁定。数量增删、数量归零和玩家死亡也清理对应状态。没有降低正常武器伤害或提高Boss血量。

## 三、额外防御与诊断

- 敌人下一帧死亡处理保存定时器句柄和生成身份，回池/激活时取消旧回调；执行时同时检查身份、实际生命和死亡标签。
- 统一伤害入口拒绝已回池、已死亡或实际生命尚为0的敌人，避免在新档案未写入时排入死亡结算。
- 对“出生点已有子弹、选卡保持打开”的原生及13类BP场景做了对照。普通出生碰撞路径没有在本次旧逻辑测试中复现秒杀，因此没有把它写成已证实根因。人工提前回池再激活能复现旧死亡回调杀新生命，该用例明确属于防御性场景。
- 正常Boss出生与致命伤害记录 `[BossLifecycle]`，包括生成ID、实际初始生命、致命伤害、来源和伤害类型，便于后续调查。

## 验证与边界

Development Editor 编译通过。

自动检查共23项：BossUpgrade 4、EnemySpawnLifecycle 3、HellTowerTargetLifecycle 3、ExclusiveCards 4、EnemyProjectileOverlap 3、ShockPillar 3、EnemyCollision 3。首轮22项成功，另1项原断言强制永久节点标题等于局内卡标题；已改为通过公开卡池入口验证局内卡确实读取数据表标题，保留自定义节点名，再单独重跑成功。没有改动实际数据表文案。

真实 PIE 共52组全部通过，包含两次PIE、首次生成、对象池复用、内存重编译、选卡保持打开和1点伤害后的延迟检查。未完整重演玩家当时的构筑，也未进行本次完整双进程联机回归；两条修复不能被表述为所有高伤或所有Boss故障已排除。选卡界面仍不暂停世界，正常武器攻击仍持续。

工程入口：

- `Source/ai/Private/Enemies/RogueEnemy.cpp`
- `Source/ai/Private/AbilitySystem/RogueAbilitySystemLibrary.cpp`
- `Source/ai/Private/Combat/Weapons/RogueWeapon_HellTower.cpp`
- `Source/ai/Private/Debug/Tests/RogueEnemySpawnLifecycleTests.cpp`
- `Source/ai/Private/Debug/Tests/RogueHellTowerTargetLifecycleTests.cpp`
- `Source/aiEditor/Private/RogueEditorAssetUtils.cpp`：仅编辑器验证桥，不进入打包游戏。

完整证据目录：`C:/Users/Elysia/Documents/Codex/2026-10-13/f-ai-workspeace-ai/work/boss-deep-audit-2026-10-28`。

- `player-session.log`：玩家日志原件副本。
- `red-confirmed.log`：旧逻辑的失败检查。
- `pie-baseline-complete/result.json`：修复前52组真实PIE结果。
- `green.log`、`title-check.log`：23项检查与单项重跑。
- `pie-fixed/result.json`：修复后52组真实PIE结果。
- `backup/`：修改前源码备份。
