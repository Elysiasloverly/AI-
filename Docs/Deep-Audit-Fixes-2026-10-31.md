# 深度检查问题修复 · 2026-10-31

本轮修复审计报告中的五个缺陷，并维护了受旧预期影响的测试。使用 UE 5.5 的 Development Editor 配置编译，保留用户原有数值、外观和本地升级树进度。

## 修复内容

| 问题 | 现在的行为 |
| --- | --- |
| 重复点击消耗下一轮选卡 | 每轮有独立 `SelectionId`。请求携带界面实际显示的轮次；服务端拒绝旧轮次。本机提交后锁定该轮，同样文字的新一轮也会重新刷新并恢复操作。 |
| 失败后子弹继续击杀和奖励 | 胜利、失败共用战斗收尾。结束后清退敌人、武器、弹体、持续攻击及经验球；伤害入口和击杀奖励入口也校验权威结束状态，迟到回调不能增加伤害、击杀数或冰砖。 |
| 商店刷新后旧卡位误购 | 每个商品有独立 `QuoteId`，购买请求携带报价编号及显示价格。刷新后旧报价、价格不符和重复购买都不能扣钱。有效的新报价照常购买。 |
| 武器表重排后参数错配 | 七行武器显式绑定各自的 `WeaponClass`，补正旧表前五行留作默认值的 `CountUpgradeType`。历史空类表按稳定行 ID / 类型回退，取消按行位置匹配。 |
| 归心符低速无法归位 | 返回过程补偿玩家和环绕目标点的移动，再按设定速度缩短相对距离。增加可调的“返回超时归位”，默认 8 秒，异常情况下可恢复后续攻击。 |

卡牌编号与完整内容、商品编号与完整报价使用原子网络序列化，减少新旧字段交错产生的短暂错误显示。卡片点击和键盘选卡使用同一条提交路径；键盘也读取 HUD 最后呈现的轮次。

## 后续配置位置

- 武器类：`/Game/Data/Weapons/DT_WeaponConfig` 各行的 `WeaponClass`。增加或更换武器时在对应行设置类，重排数据行不会改变绑定。
- 归心符返回保障：`/Game/BP/Projectiles/BP_RogueReturningTalisman` → 类默认值 → 灵符 / 飞行 → **返回超时归位**。
- 本次没有调整武器伤害、初始数量、冷却、材质、弹体外观或用户的编辑器初始数量调试开关。
- 当前保存的归心符蓝图仍为环绕半径 70 cm、高度偏移 35 cm、角速度 75°/s、出击 / 返回速度 1500 / 1900 cm/s、尾迹约 0.7 秒。

## 验证

- Development Editor 编译通过。
- 完整自动化运行 231 项，230 项通过。最后一项是旧测试误把“激光来源规则”写成必须有一条显式折射卡规则；实际折射卡由来源规则动态加入。修正该断言后，前置规则组 7 项定向复测全部通过。合并每项最新结果为 **231 / 231 通过**。
- 八项审计回归全部通过，包含旧请求拒绝、新请求成功、失败后迟到伤害和奖励回调、真实旧格式武器表乱序，以及归心符低速返回。
- 双独立进程使用 `PktLag=150`、`PktLagVariance=25` 验证复制后的新轮次、重复旧选卡拒绝、刷新后旧报价拒绝及新报价购买；详细日志与核对结果见同目录证据文件。
- 已读回保存后的武器表，确认七种武器类与类型一一对应；与修改前核对，除类绑定与类型元数据外其他字段一致。
- 正式存档及备份另行核对 SHA-256；验证结果见 `save-verification.json`。

## 测试维护

测试不再要求用户的护盾模型必须为空，不再写死旧的 48 发弹幕、0.45 秒尾迹和 9 条升级规则。数值逻辑测试明确设置临时武器数量；奖励幂等测试使用独立的非单位奖励配置；凹形刷怪轮廓测试明确给予 30 只预算，以免被默认 24 只预算提前中止。

双进程探针只通过开发命令 `Rogue.Test.AuditRequests` 显式启动，普通游戏不会运行。测试使用独立 QA 存档。测试脚本的中间启动与退出问题已与业务缺陷分开记录，最终结果以最新通过日志为准。

## 代码入口

- [选卡消费与本机提交锁](F:/AI_WorkSpeace/baka/Source/ai/Private/Player/RoguePlayerState.cpp)
- [商店报价校验](F:/AI_WorkSpeace/baka/Source/ai/Private/Progression/Shop/RogueShopSystem.cpp)
- [统一战斗收尾](F:/AI_WorkSpeace/baka/Source/ai/Private/Framework/RogueGameMode.Victory.cpp)
- [伤害结束状态检查](F:/AI_WorkSpeace/baka/Source/ai/Private/AbilitySystem/RogueAbilitySystemLibrary.cpp)
- [武器绑定](F:/AI_WorkSpeace/baka/Source/ai/Private/Player/RogueCharacter.Weapons.cpp)
- [归心符返回](F:/AI_WorkSpeace/baka/Source/ai/Private/Combat/Projectiles/RogueReturningTalisman.cpp)
- [审计回归及双进程探针](F:/AI_WorkSpeace/baka/Source/ai/Private/Debug/Tests/RogueDeepAuditReproTests.cpp)

本轮验证涵盖上述路径，未执行完整 100 波联机通关或打包版本画面检查。
