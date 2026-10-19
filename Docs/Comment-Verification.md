# 项目注释事实核验

核验日期：2026-10-15。项目版本：Unreal Engine 5.5；基线提交：`c3920e5`。

这次通读 `Source/` 下全部 163 个文件，将注释与当前项目实现、本机 UE 5.5 引擎源码相互对照。修改目标是解释职责、调用条件、状态来源与实现限制，并清理失效的旧方案、重复叙述和没有测量依据的保证。同步校正项目 README、当前结构说明与文档索引，将旧系统概览明确标为历史。

本记录的“已核验”表示能够在当前 C++ 实现或对应版本引擎源码中找到依据。它不代表二进制蓝图图表已逐节点审查，也不代表所有网络环境、性能或边界场景均已运行测试。

## 1. 关键事实纠正

### 网络角色、复制与预测

| 原说法或误导 | 当前应当如何理解 | 对照位置 |
|---|---|---|
| 客户端自己控制的 Pawn 也属于 Authority | 对正常由服务器复制的 Pawn，拥有者客户端通常是 AutonomousProxy；本地控制、Actor Authority、World NetMode 是不同概念。本地创建的非复制 Actor 可以具有本地 Authority | `ARogueCharacter::IsAuthoritySide`；UE `Actor.h`、`Pawn.cpp`；[Epic 5.5 网络角色说明](https://dev.epicgames.com/documentation/en-us/unreal-engine/actor-role-and-remote-role-in-unreal-engine?application_version=5.5) |
| APawn 默认不复制 | UE 5.5 的 APawn 构造函数设置 `bReplicates = true`；子类后续设置仍须单独检查 | UE `Engine/Private/Pawn.cpp`，`APawn::APawn` |
| ASC 放 PlayerState 就能自动保留所有重生、重连数据 | 本项目同 Pawn 复活保留当前 ASC；Pawn 变更仍需刷新 Owner/Avatar 关系，重连或 PlayerState 替换还需数据恢复逻辑 | `RoguePlayerState.cpp`、`RogueCharacter.GAS.cpp` |
| Minimal 模式会禁止属性复制 | GE 复制模式与 AttributeSet 属性复制是不同机制；属性声明、注册与宿主复制路径需要单独检查 | UE `AbilitySystemComponent`；项目 `RogueAttributeSet::GetLifetimeReplicatedProps` |
| 普通 `UPROPERTY(Replicated)` 已完成 GAS 属性通知流程 | 网络写入数值与 GAS 的 RepNotify、聚合器及变化委托不是同一件事；未配置对应通知不能假设委托已按预期执行 | `RogueAttributeSet.h/.cpp`；UE `GAMEPLAYATTRIBUTE_REPNOTIFY` |
| 一次函数内赋值就构成跨网络原子快照 | GameState 的独立属性、Character 的武器加成结构与 PlayerState 上的属性没有跨 Actor 事务保证。LifeState 的自定义 `NetSerialize` 将阶段与截止时刻作为同一载荷处理，但不保证和其它 Actor 同帧到达 | `RogueGameState`、`FRogueWeaponBonusValues`、`FRoguePlayerLifeState::NetSerialize` |
| NetUpdateFrequency 是每秒实际到达次数；无脏字段就零流量 | 它参与复制调度，实际发送与接收受帧率、相关性、网络条件等影响；字段不变不等于 Actor 完全没有其它流量 | UE `Actor.h`；`RogueGameState`、`RogueEnemy` 构造与调度代码 |
| LocalPredicted、预测键就保证零延迟和完整回滚 | 它们是预测机制的组成部分。项目 Dash 直接移动 Actor，持续武器循环复用激活预测键；不能据此宣称已经实现完整的移动重放和持续预测确认 | `RogueAbilityTask_Dash`、`RogueWeaponAbility`、`RogueAbilityTask_WeaponFireLoop` |
| 在客户端调用 Multicast 就会向服务器及其它端广播 | 服务端发起的 NetMulticast 才承担相应网络广播；客户端调用不会因此上行再分发 | `RogueCharacter` 表现路径；UE Actor RPC 调用规则 |
| 服务端表现广播意味着每个客户端一定看到完全一致的结果 | 项目的表现通路包含不可靠 RPC、本地模拟和相关性限制，时机与轨迹可能不同 | `FRogueWeaponCosmeticFire`、`RogueWeapon_Scythe`、`PlayImpactEffect` |

### GAS、效果与任务

| 原说法或误导 | 当前应当如何理解 | 对照位置 |
|---|---|---|
| `PostGameplayEffectExecute` 只处理 Instant GE，不处理周期 GE | 该回调用于效果执行后的基值修改；周期执行也可进入此路径。普通持续修饰器的聚合应用不能和周期执行混为一谈 | UE `AttributeSet.h`、`GameplayEffect.cpp`；`RogueAttributeSet` |
| `Period = 0` 表示每帧执行 | 当前引擎中它表示不启用周期执行；周期计时需要有效的正周期 | `RogueStateEffects.cpp`；UE `GameplayEffect.cpp` |
| 一个 GE 只能修改一个属性 | GE 的 Modifiers 可以包含多个修饰器；项目每种升级固定一个目标属性是项目组织方式 | `RogueAttributeUpgradeEffect.cpp`；UE `GameplayEffect.h` |
| FGameplayAbilitySpec.SourceObject 不复制 | `FGameplayAbilitySpec::SourceObject` 标为 `UPROPERTY()`，SpecDef 的同名字段才标为 `NotReplicated`。引用可复制不代表本地生成的武器 Actor 自动有网络对应实例 | UE `GameplayAbilitySpec.h`；`RogueCharacter.GAS.cpp` |
| `CommitAbility` 会重做全部激活检查 | `CommitCheck` 检查 Spec/ActorInfo、冷却与代价，不重跑完整 `CanActivateAbility` | UE `GameplayAbility.cpp`；`RogueGameplayAbility_Dash` |
| `EndTask` 会自动结束整个 Ability | 任务与能力有各自的结束流程；项目回调必须按设计调用对应结束接口 | `RogueAbilityTask_*` 与调用它们的 Ability |
| 施加冷却 GE 就自然阻止重复激活 | 当前敌人攻击未设置 `CooldownGameplayEffectClass`、未覆写 `CheckCooldown`，也未用效果句柄阻塞尝试；该 GE 记录冷却效果，实际节流依赖 AI 侧计时器 | `RogueEnemyAttackAbility`、`RogueEnemy::Tick` 与攻击决策 |
| 重复受伤一定合并成同一个效果栈 | 当前受伤效果未设合栈策略，重复施加产生独立实例；相应标签计数到最后一个实例移除后才归零 | `RogueStateEffects.cpp` |
| 属性默认值和 `SetNumericAttributeBase` 自动相加 | 后者设置基础值，不能把构造初值、初始化赋值与 GE 加法修饰器当成同一运算 | `RogueAttributeSet`、`RogueCharacter.GAS.cpp` |
| GameplayCue 只有一个可用 float，或 NormalizedMagnitude 会强制限制在 0–1 | UE 5.5 的参数包含 RawMagnitude 与 NormalizedMagnitude，序列化函数直接处理两者的 float。项目传 Scale.X 与高度比例（X 足够大时为 Z/X，否则为 1），是自身约定，独立的 Scale.Y 未编码 | UE `GameplayEffectTypes.cpp`，`FGameplayCueParameters::NetSerialize`；`RogueGameState::PlayImpactEffect` 与 Impact Cue 解码 |

### 生命周期、对象池与 C++

| 原说法或误导 | 当前应当如何理解 | 对照位置 |
|---|---|---|
| 濒死依赖 GE 到期复活，UI 跟着 GE 计时切换 | 当前由 Character 的 LifeState 与唯一服务端复活定时器决定，GE 镜像标签/表现，HUD 根据复制状态切换。倒计时为零不允许客户端自行复活 | `RogueCharacter.LifeCycle.cpp`、`RogueHUD.Widgets.cpp`、[生命周期说明](Player-LifeCycle.md) |
| 使用 TInlineAllocator\<4\> 就限定四人且永不分配堆内存 | 4 是内联容量，超过后容器可以使用后备分配器扩容 | UE `ContainerAllocationPolicies.h`，`TSizedInlineAllocator` |
| `TFunctionRef` 只允许传左值，或可长期保存任意回调 | 它是不拥有可调用对象的引用包装；可接受的参数形式应看模板约束，重点是引用的对象在使用期间必须有效 | UE `Templates/Function.h`，`TFunctionRefBase`；`RogueGameModeRules.h` |
| const 函数就是数学意义上的纯函数 | const 约束的是对象接口；方法仍可能消耗随机数、写输出参数或访问外部状态 | `RogueUpgradeSystem` |
| 标准移动复制包含 Actor 缩放 | `FRepMovement` 不包含 Scale；附着复制、组件复制是另外的路径。经验球只打开移动复制不能证明经验值驱动的大小变化也已同步 | UE `Engine/ReplicatedState.h`；`RogueExperiencePickup.cpp` |
| 对象池、MID 重建、空间哈希的性能收益可以靠代码数量直接保证 | 实现能够说明分配、复用、索引与降级策略，不能替代目标场景的内存和 CPU/GPU 测量；重复创建 MID 也不等于已证明泄漏 | `RogueCombatPools`、`RogueEnemyTracker`、`RogueEnemy`、`RogueSceneSubsystem` |
| 原生 Arena 仍在生成地板、墙和灯光 | 基线中这些代码已被注释禁用。本次移除停用代码注释，保留当前范围载体职责；关卡和蓝图外观要在资产侧检查 | `RogueArena.h/.cpp` |
| GetPlayerCharacter(0) 是整个 PIE 进程唯一玩家 | 查询以传入的 World 上下文为基础；多 World PIE 不能当作同一个全局玩家列表 | `RoguePlayerQuery`、`RogueEnemy` 相关查询 |

## 2. 发现的实现限制

以下是源码审查发现的现状，本次修改的是对应解释，没有改动玩法逻辑。它们适合后续做有测试支持的独立改进。

1. **持续预测的确认范围有限**：武器循环复用激活预测键，Dash 直接移动 Actor；应通过真实连接、延迟与丢包场景评估预测、纠正和冷却表现。
2. **敌人冷却 GE 尚未阻止激活**：未接入 `CooldownGameplayEffectClass`/`CheckCooldown`，也没有效果句柄守卫；实际尝试节流依赖 AI 计时器。
3. **非 Pawn 伤害来源的归属需复核**：伤害入口从目标 ASC 创建 Context，只有 Pawn 来源路径额外写入 Instigator；不能保证任意 AActor 来源都具有正确的 EffectCauser 和击杀归属。
4. **地面波重复激活已于 2026-10-16 修正**：`DecideGroundWaveAttack` 每次决策只尝试一次。震荡柱现改用远程 GAS 槽的 `RadialRing` 整圈弹幕，旧地面波分支仅保留兼容。
5. **武器加成存在两条复制路径**：AttributeSet 和 Character 快照独立更新；项目尚需区分网络数值到达与 GAS 属性通知是否完整。
6. **经验球大小未由 FRepMovement 承载**：活跃位、移动位姿与经验值/缩放要分开检查，后者没有因开启移动复制自动获得保证。
7. **归属修复的时间变量是函数 static**：它跨相同进程中的调用实例保留；多个 World/重开场景不宜当成每个 World 独立的计时状态。

8. **濒死环存在晚到显示限制**：`ARogueGameplayCue_Downed` 仅在 `OnActive` 中显示，未在 `WhileActive` 补显。晚加入或新相关的接收者可能只收到 `WhileActive`；静态路径已核对，本轮未新增运行复现。
9. **周期回复的基值与当前值需区分**：回复执行后钳制当前值不等于同时改写基值；长期累计与上限变化的交互需独立验证。

## 3. 历史记录与本轮验证的边界

旧 GAS 改造文档保留当时的日志、帧时与方案演变。199 敌人、2.95/2.92 ms 等数字在本轮没有重新测量；两组平均值相差 0.03 ms，不能直接证明差值全部来自 GAS。固定消息率、零延迟、零流量、必然无分配和亚毫秒开销等保证，已从当前注释和学习入口中删除或改为有条件的描述。

本轮未对 `.uasset` 蓝图图表、目标设备性能、Dedicated Server 或延迟丢包网络进行新的运行验证。已有自检的历史通过记录仍属于对应版本和场景，不能代替本轮覆盖。

## 4. 改动与检查结果

| 范围 | 审查文件 | 修改文件 | 注释调整记录 |
|---|---:|---:|---:|
| Framework / Progression / World / Shared / 模块与构建入口 | 36 | 24 | 79 |
| AbilitySystem | 33 | 31 | 146 |
| Player / UI / Debug | 46 | 25 | 140 |
| Combat / Enemies | 48 | 35 | 143 |
| 合计 | 163 | 115 | 508 |

调整记录包含事实纠正、过时解释、过度保证和表达精简；不是同等数量的玩法 bug。未发现需要修改的文件保留原样。

- 非注释内容比较：全部 163 个文件、120,263 个代码 token 与基线一致，包含字符串、数值、标识符与反射宏；宏定义形状也保持一致。
- 文件覆盖：163/163，所有变更源码文件均有审查记录；记录引用的项目/UE 源文件路径均存在。
- 改动范围：源码注释、README 与文档；未改玩法逻辑、配置或 Content 资产。
- 写回实际项目后再次逐文件核对，非注释代码与基线完全一致。
- `aiEditor Win64 Development` 编译：通过，退出码 0；UnrealHeaderTool、C++ 编译与链接均成功，总执行时间 110.99 秒。
- 文档链接：修改的 7 份 Markdown 共检查 69 个本地链接，目标均存在；`git diff --check` 通过。
- 构建提示：当前 MSVC 不是 UE 推荐版本，部分引擎头文件产生弃用 API 警告；本次构建成功，未为整理注释更换工具链或修改引擎代码。


## 5. 如何利用这次核验学习

- 读类和接口注释时先确认“谁调用、在哪一端、数据来自哪里”，再读对应实现。
- 对 UE 通则查本机 5.5 引擎定义；对本项目策略查实际调用方与状态变更路径。
- 区分“代码里存在某个机制”与“某个场景已经验证通过”。性能、预测和网络表现要有对应测量条件。
- 面试可以准确解释“做到了什么、依靠什么、仍有什么限制”，避免使用“GAS 自动解决所有联机问题”一类表述。

本机引擎源码根目录：`C:/Program Files/Epic Games/UE_5.5/Engine/Source`；GAS 插件源码位于 `Engine/Plugins/Runtime/GameplayAbilities/Source/GameplayAbilities`。核对引擎实现时应使用项目对应版本。

## 6. 源码定位

- [角色网络侧判断](../Source/ai/Private/Player/RogueCharacter.cpp)与[基础属性初始化](../Source/ai/Private/Player/RogueCharacter.GAS.cpp)
- [玩家生命周期实现](../Source/ai/Private/Player/RogueCharacter.LifeCycle.cpp)
- [GAS 属性声明](../Source/ai/Public/AbilitySystem/RogueAttributeSet.h)与[效果执行处理](../Source/ai/Private/AbilitySystem/RogueAttributeSet.cpp)
- [伤害入口与 Context 来源](../Source/ai/Private/AbilitySystem/RogueAbilitySystemLibrary.cpp)
- [武器持续能力与预测键](../Source/ai/Private/AbilitySystem/Abilities/RogueWeaponAbility.cpp)
- [敌人攻击能力](../Source/ai/Private/AbilitySystem/Abilities/RogueEnemyAttackAbility.cpp)与[AI 决策](../Source/ai/Private/Enemies/RogueEnemy.cpp)
- [濒死 GameplayCue](../Source/ai/Private/AbilitySystem/Cues/RogueGameplayCue_Downed.cpp)
- [GameState 复制与 Impact Cue 编码](../Source/ai/Private/Framework/RogueGameState.cpp)
- [普通武器快照与表现载荷](../Source/ai/Public/Shared/RogueTypes.h)
- [经验球活跃位与移动复制](../Source/ai/Private/World/RogueExperiencePickup.cpp)

