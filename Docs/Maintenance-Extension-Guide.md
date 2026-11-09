# 维护与扩展指南

更新：2026-11-09。工程入口为 `Baka.uproject`，内部模块保留 `ai` / `aiEditor` 名称。

本文用于判断“需求改在哪里、相关内容怎样同步、改完验证什么”。本轮对照当前源码、保存的数据资产和状态栏 WBP 更新；表中的保存值与 C++ 默认值分开说明。关卡世界设置、GameMode 蓝图或显式资产引用存在覆盖时，以运行时实际加载的配置为准。

## 1. 先找到修改入口

| 要修改的内容 | 首选入口 | 进一步说明 |
|---|---|---|
| 每波总量、生成速度、奖励、商店价格 | `/Game/Data/Core/DA_GameBalance` | [游戏平衡配置](Game-Balance-Configuration.md) |
| 昼夜间隔、黑夜数量和近战占比、随机地图池 | 同一资产的 `BattleStage` | [昼夜流程](Battle-Day-Night.md) |
| 玩家基础属性、冲刺、复活、武器初始加成与上限 | `/Game/Data/Player/DA_PlayerBalance` | [玩家配置](Player-Balance-Configuration.md) |
| 武器伤害、速度、范围、数量、生命周期等战斗参数 | `/Game/Data/Weapons/DT_WeaponConfig` | [武器配置](Weapon-Configuration.md) |
| 升级卡标题、介绍、单次加成 | `/Game/Data/Upgrades/DA_UpgradeDefinitions` → `DT_UpgradeDefinitions` | [升级树与卡池](UpgradeTreeGuide.md) |
| 基础卡池、武器专属卡条件 | `/Game/Data/Upgrades/DA_UpgradeRules`、`RogueUpgradeSystemRules.cpp` | [主动选卡](Manual-Upgrade-Selection.md) |
| 永久解锁、前置卡、基础属性树、冰砖调试 | `/Game/Data/Progression/DA_UpgradeTree` | [升级树](UpgradeTreeGuide.md) |
| 敌人基础行为、攻击参数、治疗、胶囊与外观档案 | `/Game/Data/Enemies/DT_EnemyArchetypes` | [敌人表](Enemy-Archetype-Table.md) |
| Boss 招式的蓄力、持续时间、施法间隔与表现 | `/Game/Data/Enemies/DT_BossSkills`、敌人 BP 的 Boss 技能组件 | [Boss 符卡](Boss-Spellcards.md)、[技能插槽](Boss-Skill-Slots.md) |
| 不规则刷怪边界、禁刷区、空气墙、地面碰撞 | `/Game/Map/Day`、`/Game/Map/Night` 中的关卡 | [地图制作](Map-Authoring.md) |
| 固定刷新点与远程怪距离 | `BP_RangedSpawnPoint`、所属刷怪区域 | [就近固定点](Fixed-Ranged-Spawn-Points.md) |
| 敌人绕障、卡住恢复与规划开销 | `RogueEnemyObstacleMovementComponent` | [绕障说明](Enemy-Obstacle-Movement.md) |
| 状态栏、升级提示、角色旁冲刺圆弧 | `/Game/UMG/HUD/WBP_PlayerVitals` | [状态栏维护](Centered-Vitals-Polish.md) |
| 武器/升级图标、叠加数字、攻击跳动 | 右下角收藏栏 WBP 与 `RogueOwnedItemsWidget` | [收藏栏](Owned-Items-HUD.md) |
| 武器、敌人、按钮音效与背景音乐 | `/Game/Data/Audio/DA_AudioConfig`、`RogueMusicSubsystem` | [音频配置](Attack-Audio-And-Music.md) |
| 游戏内教程文字 | `SourceArt/UI/tutorial_chapters.json`，同步 WBP 与 Markdown | [教程维护](Tutorial-Maintenance.md) |

## 2. 保持这些职责边界

- **数值与出现规则进配置**：武器战斗值进武器表，玩家属性进玩家资产，波次与经济进游戏平衡资产。
- **外观进 BP / WBP**：模型、材质、外观缩放、拖尾、界面字体和留白在相应蓝图调整。会改变伤害或碰撞范围的参数仍属于战斗配置。
- **权威结算在服务器**：伤害、购买、抽卡消费、波次推进和切图由服务器确认。HUD 发请求并读取本地玩家状态。
- **GAS 属性是一份数值真相**：角色缓存和武器快照用于读取、复制及表现，不要再维护第二套升级结果。
- **运行状态不写回配置资产**：冷却、锁定目标、临时卡组、购买涨价和对象池状态属于本局运行态。
- **高频对象沿用池化契约**：激活时完整初始化，回池时清理旧目标、定时器、命中记录和表现。不要只关可见性。

当前目录与类落点见 [Class-Organization.md](Class-Organization.md)，系统关系见 [Project-Architecture.md](Project-Architecture.md)。不要再新建通用的 `Core/`、`Subsystems/` 目录来收纳不同玩法职责。

## 3. 当前运行规则：维护时不要恢复旧行为

### 3.1 升级由 Q 主动打开

升级与 Boss 额外选卡奖励先累计次数，不强制弹出界面。只有实际打开菜单才生成当前卡组；确认选择后消耗一次，剩余次数继续进入下一组。Q 或 Esc 可以收起，重开仍是原卡片，不能靠开关菜单免费重抽。

调用链：

```text
IA_Choice（Q）
  → Character::ToggleUpgradeSelection
  → HUD::HandleUpgradeSelectionKey
  → PlayerState::Server_ToggleUpgradeSelection
  → GameMode::ToggleUpgradeSelectionForPlayer
```

| PlayerState 状态 | 含义 |
|---|---|
| `QueuedUpgradeSelections` | 尚未生成卡组的奖励次数，仅复制给所属玩家 |
| `PendingUpgrades` | 已生成但尚未消费的一组卡，收起后保留 |
| `bAwaitingUpgradeChoice` | 当前是否正在显示选卡；不是“有奖励可选” |
| `GetPendingUpgradeCount()` | 队列次数 + 当前卡组占用的一次奖励 |

维护约定：

- 暂时收起使用 `SuspendUpgradeSelection()`；`CloseUpgradeSelection()` 会清空当前卡组，不能当作普通收起入口。
- 重开同一组卡会更新 `SelectionId`。旧点击、重复点击、隐藏卡片和过期报价仍需服务端校验。
- 空卡池不能先扣掉奖励。商店、暂停界面与选卡不能叠开。
- 濒死会收起界面；还有队友存活时保留未消费奖励。切图保留卡组和次数，抵达新地图不自动弹出；整局结束或新开一局清空。
- 只累计次数不切输入模式。输入资产和绑定见 [Enhanced-Input-Migration.md](Enhanced-Input-Migration.md)。

代码入口：`Source/ai/Private/Framework/RogueGameMode.Progression.cpp`、`Source/ai/Private/Player/RoguePlayerState.cpp`、`Source/ai/Private/UI/RogueHUD.Menus.cpp`、`Source/ai/Private/UI/RogueHUD.Input.cpp`。

### 3.2 菜单与伤害保护

伤害统一进入 `RogueAbilitySystem::ApplyDamageToTarget`，不要让弹体或新技能绕过这个入口直接改生命值。

| 状态 | 当前伤害行为 |
|---|---|
| 商店打开 | 正常承伤，世界继续运行 |
| 升级选卡打开 / 仍有未使用次数 | 正常承伤 |
| 暂停 / 设置菜单打开 | 保留该玩家独立的伤害保护；世界不暂停 |
| 正在冲刺 | `State.Dashing` 期间保护，冷却中不因此免伤 |
| 复活保护 | 按复活配置授予的无敌状态保护 |
| 濒死、淘汰或整局结束 | 按生命周期与结算状态拦截伤害 |

入口为 `Source/ai/Private/AbilitySystem/RogueAbilitySystemLibrary.cpp`。联机下必须验证房主和加入者各自的状态，不能只改本地 WBP 的显示来实现保护。

### 3.3 波次、切图和落地商店

- 白天和黑夜都等本波名额生成完毕，并清空敌人与 Boss 后推进；普通波间隔为 3 秒。
- 当前保存的昼夜间隔为 20 波：20 / 40 / 60 / 80 波清场后，各倒计时 30 秒，全体切入下一阶段随机地图。
- Boss 当前每 10 波出现。第 100 波仍生成完整敌人与终局 Boss，清完后胜利；不是刚进入第 100 波就结算。
- 每波清场后排队让商店从天而降。监听服优先以房主为落点参考；场上终端与每名玩家的商品运行态分开管理。
- 商店关闭后，服务器等待仍打开的个人商店全部关闭，再让终端蓄力离场；不要由单个客户端自行隐藏全场终端。
- 商店模型已在 `BP_RogueShopTerminal` 的视觉组件中缩小为原来的一半，提示锚点同步降低；交互盒和地图 Actor 缩放保留。后续改外观大小请调整模型组件，避免一起缩小交互距离。

入口为 `RogueGameMode.cpp` 的 `TickRunSystems`、`RogueGameMode.BattleStage.cpp`、`RogueGameMode.WaveShop.cpp` 和 `World/RogueShopTerminal`。切图承接见 `RoguePlayerState.StageTravel.cpp`。

### 3.4 刷怪位置与绕障

固定点是候选入口，不代表每波启用全图点位。当前优先轮换参考玩家附近最近的三个可用点，太远或受阻时回退到附近合法地面；明确禁用全部所属点位的配置仍受尊重。固定炮台类使用更近的出生圈。

出生还需通过不规则轮廓、禁刷区、地面、完整胶囊空间、敌人间距和所有存活玩家的安全距离检查。失败不消耗本波名额；不要用直接设坐标绕过这些检查。

敌人移动由服务器选择意图，再交给绕障组件。地面单位优先尝试可用 NavMesh，必要时走有预算的局部 A*；飞行单位按实际飞行胶囊绕障。刷怪区域不是导航数据。修改建筑碰撞、楼梯、窄门或地图尺寸后，需要重新检查通路。

## 4. 状态栏与冲刺圆弧怎样维护

当前外观以保存的 `WBP_PlayerVitals` 为准，设计尺寸 620×144，暗色圆角、底部居中。运行时不重建控件树。

| 控件 / 设置 | 用途与修改方式 |
|---|---|
| `VitalsPanel` / `VitalsFrame` | 整体尺寸、锚点、底部距离、底色和圆角 |
| `LevelBadge` → `LevelBadgeContent` | 等级框内的共同居中容器；标题与数字使用垂直布局，不再各自摆 Canvas 坐标 |
| `LevelCaption` / `LevelText` | “等级”标题和等级数字；在共同容器内调整字号、居中与间距 |
| `HealthText` / `HealthValueText` | 名称与当前/最大生命分开，数值由 C++ 即时更新 |
| `ArmorText` / `ArmorValueText` / `ArmorUnavailableText` | 护甲名称、数值与“暂无护甲”状态 |
| `ExperienceText` / `ExperienceValueText` / `ExperienceBar` | 底部经验读数与细条 |
| `HealthChangeText` / `ArmorChangeText` / `ExperienceChangeText` | 短暂变化数字；保留独立区域，避免压住常驻值 |
| `PendingUpgradeHintText` | 状态栏上方淡蓝色提示；有待选次数时呼吸，用完或濒死时隐藏 |
| 类默认值 → 升级提示 | 呼吸周期、最低透明度；C++ 默认 1.8 秒、0.35，可在 WBP 覆盖 |
| `PlayerDashRing` | 角色右侧圆弧的半径、弧度、粗细与冷却颜色；充满后隐藏 |
| 类默认值 → 角色旁冲刺条 | 屏幕偏移、世界高度偏移 |

等级下方的 `Q / 待选次数` 已删除。原生类保留部分可选旧绑定用于兼容自定义 WBP，不表示当前资产还应补回它们。

圆弧由 `URogueDashRingWidget` 绘制，读取现有 GAS 冲刺冷却；仅在有实际剩余时间的充能期间显示，满条、不可用和濒死时隐藏。位置由本地玩家投影并换算 DPI，每帧跟随摄像机；它不创建体力、不参与碰撞、不接收点击。不要在它的 Canvas 位置里硬填角色坐标。

日常修改直接编辑 WBP。`RefineCompactVitals` 是明确执行才写入资产的排版工具，会覆盖其管理区域的人工样式；`UsePlayerSideDashRing` 是旧分区迁移工具，`AddPendingUpgradeCounter` 会重建已取消的等级下方计数，均不应作为日常刷新操作。

默认验证入口是 `Tools/UI/manage_ui.py`：不带重建参数时只在内存编译检查并写报告。`-RogueUIRebuild` 恢复的是基础占位布局，不会恢复当前精修样式。详见 [UI 工具说明](../Tools/UI/README.md) 与 [状态栏维护](Centered-Vitals-Polish.md)。

## 5. 添加或调整武器

当前稳定目录登记八种武器：

| 表行名 / 稳定身份 | 显示名 | 武器类后缀 |
|---|---|---|
| `Projectile` | 冰符·寒弹 | `RogueWeapon_Projectile` |
| `Scythe` | 银刃 | `RogueWeapon_Scythe` |
| `Rocket` | 冰焰·陨星 | `RogueWeapon_Rocket` |
| `Laser` | 星符·流光 | `RogueWeapon_Laser` |
| `HellTower` | 焰符·灼光 | `RogueWeapon_HellTower` |
| `Mortar` | 冰符·霜玉 | `RogueWeapon_Mortar` |
| `Talisman` | 灵符·归心 | `RogueWeapon_Talisman` |
| `FrostBlade` | 冰刀·霜月 | `RogueWeapon_FrostBlade` |

新武器按下列顺序接线：

1. **登记身份与配置**：在 `RogueWeaponCatalog` 登记稳定身份；`DT_WeaponConfig` 加行并指定 `WeaponClass`。行结构是 `FRogueWeaponTableRow`，声明在 `Source/ai/Public/Combat/Weapons/RogueWeaponBase.h`。不要依赖表行顺序或修改已发布枚举数值。
2. **实现武器与开火能力**：放入 `Combat/Weapons`；构造里设置开火能力，能力通过 `GetDrivenWeaponClass()` 指定武器类。沿用 `RunFireLoopStep()`、冷却 GE 与表现同步，不在 Character 新增另一套开火计时器。
3. **确定参数归属**：伤害、间隔、索敌、弹速、穿刺、爆炸和生效范围进入武器表；模型、材质、拖尾和外观偏移进入对应 BP。升级会改变碰撞范围时，视觉与伤害范围应使用同一有效倍率。
4. **接入升级效果**：新增枚举、定义、GE 映射和必要属性。`RogueUpgradeEffectMap::GetEffectClass()` 漏映射会出现“卡能抽到但无效果”。普通属性升级优先沿用已有 GE，不为一项加成复制完整流程。
5. **接入两层前置**：永久升级树决定个人允许的武器/卡；本局持有武器等条件由卡池规则继续过滤。冰刀解锁后开局自带，不套用一般数量卡获取方式。
6. **同步配套内容**：图标、收藏栏分类、音频事件、升级树文案、局内卡片、商店介绍和教程。数量卡与武器图标的合并规则见 [收藏栏](Owned-Items-HUD.md)。
7. **处理高频对象生命周期**：需要池化时接入预热/获取/回收；激活和回收都清掉上次命中、目标与定时器。少量常驻武器实体不必为了形式强行池化。

武器基础值与升级结果不要混写：武器读取表基础值与 GAS 加成的换算结果，不能把局内加成写回表。初始数量调试、各武器上限与外观缩放例外见 [武器配置](Weapon-Configuration.md)、[冰刀说明](Frost-Moon-Blade.md)。

## 6. 新增升级卡与永久节点

一次完整接线包含：

1. `Shared/RogueTypes.h` 中的升级类型。
2. `DA_UpgradeDefinitions / DT_UpgradeDefinitions` 的标题、介绍、加成值与次数规则；已有类型只改文案/数值时优先改资产。
3. `DA_UpgradeRules` 与 `RogueUpgradeSystemRules` 的局内出现规则。
4. `RogueAttributeUpgradeEffect.cpp` 的 GE 映射，以及必要的 AttributeSet 属性、上限与武器读取。
5. `DA_UpgradeTree` 的永久节点、前置节点和价格；沿用玩家自己的本地存档快照，不把房主进度复制成全队进度。
6. 对应图标、说明、商店与收藏栏展示。

不要通过每次开关选卡重抽来处理前置变化；由服务端校验当前卡、选中类型和凭据。商店购买还需校验当前报价 ID 与价格。修改定义表不会自动替换已生成的那组卡或旧报价。

详细前置、调试冰砖与重置存档的区别见 [UpgradeTreeGuide.md](UpgradeTreeGuide.md)。

## 7. 新增敌人、治疗或 Boss 招式

- 普通变体优先复用 `ARogueEnemy`，在 `ERogueEnemyType`、原型/数据表、视觉和波次权重中接线；不要复制整套受击、死亡、注册与回池逻辑。
- 移动意图在 `RogueEnemy.Movement.cpp`，绕障在 `RogueEnemyObstacleMovementComponent`。治疗者移动与施法由 Support 组件协作；固定怪和 Boss 蓄力仍保持不移动。
- 当前移动模型有 Direct、BurstCharge、Orbit、RangedKite、ZigZagChase、Stationary；射击模式还包括飞行交织、折返风刃、星阵和天坠弹，不能按早期的“三种射击模式”扩写分支。
- Boss 专属招式使用 `RogueBossSkillComponent` 的插槽与能力类，参数在 `DT_BossSkills`。关注蓄力、持续阶段、施法间隔、中断、对象池复用及剩余技能次数；不要把每次施法轮数与整局使用次数混为一谈。
- 新招式同时补图标、Boss 血条下方状态、红圈预警、地面残留淡出、教程和客户端表现；伤害仍走统一入口。
- 飞行高度、治疗范围、法阵伤害范围和 Boss 大胶囊需一起验证。模型看起来能过，不代表胶囊能通过。

细节见 [敌人原型表](Enemy-Archetype-Table.md)、[敌人目录](Enemy-Blueprint-Folders.md)、[Boss 插槽](Boss-Skill-Slots.md)、[Boss 符卡](Boss-Spellcards.md)。

## 8. 当前保存配置快照

以下为 2026-11-09 从资产读取的值，供维护文案时核对。它们不等同于源码默认值，也不替代关卡实际引用检查。

| 配置 | 保存值 |
|---|---|
| 白天/黑夜地图池 | 各 5 张 |
| 昼夜切换间隔 / 倒计时 | 20 波 / 30 秒 |
| Boss 间隔 / 额外选卡 / 每人冰砖 | 10 波 / 3 次 / 3 块 |
| 第一波普通怪 / 每波增量 / 每波总量封顶 | 45 / 5 / 0（不限）；类型限制仍单独生效 |
| 基础生成间隔 / 目标半径 / 最小玩家距离 | 0.8 秒 / 2800 cm / 1200 cm；运行时还受节奏规则和落点检查影响 |
| 黑夜数量 / 生成速度倍率 | 3 / 2 |
| 黑夜近战比例 / 近战移速 / 近战伤害倍率 | 90% / 1.8 / 2 |
| 商店商品数 / 基础价 / 刷新基础价 / 自动刷新 | 6 / 24 / 16 / 60 秒 |
| 濒死等待 / 复活生命比例 / 保护 | 10 秒 / 50% / 1.5 秒 |

全场同时存活上限已经移除，各敌人类型限制保留。`EnemiesPerWaveLimit` 限制本波累计生成总量，不是同时存活数量。

## 9. 文件落点与常用入口

| 目录 | 职责 |
|---|---|
| `Framework` | GameMode / GameState、波次、昼夜、胜负、奖励归属与商店调度 |
| `AbilitySystem` | ASC、AttributeSet、能力、Task、GE、Tag 与 Cue |
| `Combat` | Weapons、Projectiles、Visuals、Pooling、Targeting |
| `Progression` | Meta 永久成长、Upgrades 局内卡池、Shop 经济运行态 |
| `Player` | Character 拆分实现、PlayerState、输入、生命周期与玩家表现 |
| `Enemies` | 敌人运行时、Data、Components、Spawning |
| `World` | 商店终端、经验球、刷怪区域、固定点、场景与边界 |
| `Networking` | 局域网/IP、房间与准备、旅行与失败恢复 |
| `Audio` | 音效/BGM 配置与音乐子系统 |
| `UI` | HUD 拆分实现、WBP 基类、视图数据与圆弧控件 |
| `Source/aiEditor` | 编辑器制作、迁移、保存验证与临时预览，不进入游戏运行模块 |

`RogueHUD.CombatFeedback.cpp` 组装玩家 View；`RogueVitalsWidgetBase.cpp` 更新文本、血条与提示，`RogueDashRingWidget.cpp` 绘制圆弧。各文件都位于 `Source/ai/Private/UI` 对应目录。

## 10. 改完怎样验证

| 改动类型 | 合适的验证 |
|---|---|
| Markdown、说明文字 | 核对源码/资产、相对链接、JSON/WBP/玩家指南的一致性；无需重编 C++ |
| WBP 样式与间距 | 备份 → 编译该 WBP → 保存 → 实际窗口检查；无护甲、三位等级、大数值和小窗口都要看 |
| 数据资产数值 | 确认关卡/BP 引用与覆盖，检查新局及下一次生成/抽卡的实际值 |
| C++ 实现或反射接口 | 编译正在使用的配置，执行相关定向测试；模块或资产被编辑器占用时，先保存再关闭后更新 |
| 联机状态/输入/旅行 | 分别验证房主和加入者，确认自己的界面、奖励、伤害和状态恢复，避免只验证房主 |
| 性能改动 | 固定场景、负载、配置和采样窗口，记录前后数据；无头测试与寻路到达率不能代替真实战斗帧耗时 |

Development Editor 用于日常编辑；DebugGame Editor 用于相应调试启动。只有需要交付对应配置时才补编另一套，不默认每次都编两套。

相关测试筛选前缀：`Rogue.UI.ManualUpgrade`、`Rogue.Gameplay.ManualUpgrade`、`Rogue.UI.VitalsFeedback`、`Rogue.UI.DashHUD`、`Rogue.Gameplay.PlayerMenus`。改变哪条规则就先验证相应集合；已通过且没有新增影响时，不重复跑整个项目。

## 11. 常见故障先查什么

| 现象 | 先检查 |
|---|---|
| 升级没弹出 | 当前为主动选卡；看上方次数并按 Q，再查 IA_Choice 与拥有者 PlayerState |
| 收起后奖励没了 | 是否误用 CloseUpgradeSelection / ResetUpgradeState；是否确实新开一局或团灭 |
| 切图回来强制弹出选卡 | 是否把有奖励误当作界面打开；检查 StageTravel 的 bAwaitingUpgradeChoice |
| 商店/选卡又免伤了 | 统一伤害入口是否错误加入菜单条件；同时检查独立暂停保护与无敌标签 |
| 等级文字偏移 | LevelBadgeContent 的水平/垂直居中、标题/数字槽位及父容器，不只看字体 Justification |
| 冲刺圆弧偏离角色 | 本地拥有者、投影/DPI、屏幕偏移；不要额外给它动画位移或固定 Canvas 位置 |
| 改样式后又变回占位 | 是否运行了初始化/重建脚本，或未关闭的编辑器把内存旧资产再次保存 |
| 远程怪散在全图 | 固定点所属区域、距离筛选、就近回退与禁用规则 |
| 敌人卡建筑或穿进地形 | 出生检查、真实胶囊、场景碰撞、NavMesh/局部路线与实际移动入口；不要只扩大到达距离 |
| 卡有文案却无效果 | GE 映射、Magnitude、属性上限、武器读取与当前使用的定义资产 |
