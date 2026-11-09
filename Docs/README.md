# Docs 索引

先按需要选择当前结构与数据流文档，再查系统细节和历史取舍。C++ 已按玩法职责整理，历史文档中的 `Core/`、`Subsystems/` 或旧类文件路径请以 [Class-Organization.md](Class-Organization.md) 为准。

## 当前维护入口（2026-11-09）

- [近期修改整合](Recent-Changes-2026-11-09.md)：地图、武器与 Boss、商店、界面、美术和验证范围。
- [维护与扩展指南](Maintenance-Extension-Guide.md)：修改入口、八种武器接线、Q 主动选卡、伤害边界、波次/商店、地图与定向验证。
- [状态栏与冲刺圆弧](Centered-Vitals-Polish.md)：等级共同居中、独立数值、淡蓝升级提示、圆弧与 WBP 修改位置。
- [Q 主动选卡](Manual-Upgrade-Selection.md)：累计、收起保留、防重放、个人复制与旅行。
- [远程怪就近固定点](Fixed-Ranged-Spawn-Points.md)、[敌人绕障与卡住恢复](Enemy-Obstacle-Movement.md)。
- [教程维护](Tutorial-Maintenance.md)：源码与资产保存值核对，JSON、WBP、玩家指南同步。

| 想知道什么 | 看这里 |
|---|---|
| 当前项目各系统如何关联 | [Project-Architecture.md](Project-Architecture.md)：总览、核心对象、战斗数据流、成长与联机、生命周期和目录图 |
| 从当前代码学习 C++、UE、GAS 与联机 | [Code-Reading-Guide.md](Code-Reading-Guide.md)：阅读顺序、生命周期、调用链与交付练习 |
| 2026-11-05 可读性重构改了什么 | [Readable-Refactor.md](Readable-Refactor.md)：职责拆分、旧字段迁移、工具入口和注释约定 |
| 冰刀美术精修与新增武器接线有哪些漏项 | [Frost-Blade-Art-And-Sync-Audit.md](Frost-Blade-Art-And-Sync-Audit.md)：图标、反馈时机、受击协议、音效、商店与切图验证 |
| 冰刀·霜月如何改模型、横斩、剑波和永久解锁 | [Frost-Moon-Blade.md](Frost-Moon-Blade.md)：BP 外观、DT 战斗值、四张专属卡与联机行为 |
| 红色持续激光的锁定缩圈如何调整 | [Inferno-Lock-2026-11-04.md](Inferno-Lock-2026-11-04.md)：特效蓝图、参数、多人合并及清理 |
| 武器与升级卡图标、右下角收藏栏、数量叠加和攻击跳动怎么改 | [Owned-Items-HUD.md](Owned-Items-HUD.md)：WBP、图集、玩家归属与切图保留 |
| 经验结晶、Boss 符印、商店中文牌签与卡片动效怎么改 | [Art-Detail-2026-11-03.md](Art-Detail-2026-11-03.md)：本轮模型、标记、预警、卡片、飘字和地图微调 |
| 冰面、石坪、森林地表与昼夜光影怎么改 | [Art-Ground-Light-2026-11-03.md](Art-Ground-Light-2026-11-03.md)：地面实例参数、灯光数值、保存核对和性能快照 |
| 治疗怪、飞行怪的配饰与模型动作怎么改 | [Art-Enemy-Motion-2026-11-03.md](Art-Enemy-Motion-2026-11-03.md)：蓝图组件、动作参数、回池恢复与攻击反馈 |
| 五种武器命中与消散效果怎么改 | [Art-Weapon-Impact-2026-11-03.md](Art-Weapon-Impact-2026-11-03.md)：特效蓝图、风格预设、密度预算与受击快照同步 |
| 敌人消散、退治连锁提示和确认音怎么改 | [Art-Feedback-2026-11-03.md](Art-Feedback-2026-11-03.md)：本轮击杀反馈、WBP 与蓝图入口、联机归属、性能检查 |
| 本轮全项目美术精修改了什么、后面怎么继续改 | [Art-Refinement-2026-11-03.md](Art-Refinement-2026-11-03.md)：弹幕层次、拖尾、治疗、地图细节、主菜单与按钮状态 |
| 绘本场景、昼夜配色与自制环境/战斗特效怎么改 | [Art-Direction.md](Art-Direction.md)：统一材质、花瓣微光、符纸命中、蓝图参数和性能约束 |
| 新地图看不到怪、输入与细碎碰撞如何检查 | [New-Map-Gameplay-Fix.md](New-Map-Gameplay-Fix.md)：世界设置 GameMode、敌人蓝图映射、独立碰撞及真实游玩验证 |
| 五种主题的十张昼夜地图如何随机选、刷怪与加空气墙 | [Map-Collection.md](Map-Collection.md)：地图清单、DA_GameBalance 地图池、出生点、商店、边界和打包配置 |
| 一个类在哪里，新增类应该放哪 | [Class-Organization.md](Class-Organization.md)：目录职责、实现拆分、独立 Widget 基类与蓝图兼容约定 |
| 编辑器里的美术资产放在哪里 | [Content-Organization.md](Content-Organization.md)：美术主目录、素材包保留方式及玩法资产位置 |
| 游戏蓝图在哪、实验蓝图放哪 | [Blueprint-Organization.md](Blueprint-Organization.md)：`/Game/BP` 目录职责与常用入口 |
| 玩法配置资产放在哪、怎么改 | [Data-Configuration.md](Data-Configuration.md)：`/Game/Data/` 的 7 个资产、读取入口与 DataTable 归类 |
| 哪些配置已经进数据资产、哪些还在代码里 | [Data-Asset-Audit.md](Data-Asset-Audit.md)：`/Game/Data` 覆盖审计与待迁清单 |
| 中文名和代码里的英文名怎么对应 | [Content-Naming.md](Content-Naming.md)：武器 / 升级卡 / 敌人的显示名 ↔ 枚举 ↔ 表行键，改名入口与校验 |
| 数值怎么调 | [Game-Balance-Configuration.md](Game-Balance-Configuration.md)、[Player-Balance-Configuration.md](Player-Balance-Configuration.md)：平衡与玩家数值分组 |
| 地图、刷怪框与商店怎么摆 | [Map-Authoring.md](Map-Authoring.md)：关卡物体、刷怪区域与商店终端 |
| 简洁和纸 UI、选卡与商店布局、Boss 血条和平滑动画怎么改 | [UI-Touhou-Theme.md](UI-Touhou-Theme.md)：WBP 编辑入口、素材提示词与联机数据流 |
| 血条 / 护甲条 / 经验条 / 波数金币怎么做的 | [HUD-Vitals-And-Enemy-Bars.md](HUD-Vitals-And-Enemy-Bars.md)：玩家状态 WBP、统一字体、数据流 |
| 受伤时的屏幕泛红与镜头震动怎么调 | [Damage-Feedback.md](Damage-Feedback.md)：掉血检测、`FlashRoot` 结构、震动参数与替换方式 |
| 敌人和自己的弹体在哪里 | [Enemy-Blueprint-Folders.md](Enemy-Blueprint-Folders.md)：18种敌人逐类型目录、独立弹体和公共基类 |
| 敌方弹幕用哪个蓝图、怎么换外观 | [Enemy-Projectile-Blueprints.md](Enemy-Projectile-Blueprints.md)：`BP_EnemyProjectile`、每敌人覆盖与池化 |
| 经验球怎么换外观 | [Experience-Pickup-Blueprint.md](Experience-Pickup-Blueprint.md)：`BP_ExperiencePickup` 组件结构、BP 钩子与尺寸规则 |
| 弹体碰撞是否关闭 | [Combat-Collision.md](Combat-Collision.md)：所有弹体的命中球 / 视觉组件 / 物理设置审计 |
| 敌人原型表怎么读 | [Enemy-Archetype-Table.md](Enemy-Archetype-Table.md)：13 个顶层列、一怪一块的 `SpecialParams`、字段语义与迁移记录 |
| 鸦天狗红圈、大型落弹与触地散弹怎么改 | [Enemy-Marksman-Skyfall.md](Enemy-Marksman-Skyfall.md)：攻击流程、数据表、外观蓝图与联机同步 |
| 18 种敌人各是什么、怎么打 | [Enemy-Ranged-Variants.md](Enemy-Ranged-Variants.md)：枚举名与中文名对照、弹道、起始波次与验证 |
| 红雾吸血卡怎么配 | [Scythe-Lifesteal-Upgrade.md](Scythe-Lifesteal-Upgrade.md)：前置、首次 20%、每级 +5%、数据资产与伤害回血链 |
| 当前对象如何协作，伤害和升级如何流动 | [Project-Structure.md](Project-Structure.md)：运行时关系、网络划分、池化契约、核心数据流与自检入口 |
| 源码注释哪些说法经过核验 | [Comment-Verification.md](Comment-Verification.md)：事实纠正、源码依据、当前实现边界与检查结果 |
| 早期系统为什么这样拆分 | [System-Architecture-Overview.md](System-Architecture-Overview.md)：历史概览，正文旧行为不能当作当前实现 |
| 怎样继续加武器、升级卡、敌人和界面 | [Maintenance-Extension-Guide.md](Maintenance-Extension-Guide.md)：扩展接线与资产配置；文件落点以当前组织说明为准 |
| 从项目入口开始了解 | [项目 README](../README.md)：技术栈、玩法、GAS 架构、压测命令与演示 |
| GAS 主架构的迁移过程和验证证据 | [GAS-Main-Architecture-Plan.md](GAS-Main-Architecture-Plan.md)：分阶段改造、取舍与踩坑记录 |
| 属性从自研系统转向 GAS 的原因 | [Attribute-System-Migration.md](Attribute-System-Migration.md)：历史方案，描述的旧属性系统已被替换 |
| 输入资产和绑定配置 | [Enhanced-Input-Migration.md](Enhanced-Input-Migration.md)：输入迁移记录与配置方式 |
| 哪些资源是旧副本、哪些可以清理 | [Asset-Audit-Report.md](Asset-Audit-Report.md)：重定向器、断链、重复素材包与无用资产清单（只读审计，未删除） |
| 怎么打包、为什么打包后没输入 | [Packaging.md](Packaging.md)：BuildCookRun 命令、必须强制 Cook 的目录、C1056 处理与体积构成 |
| 如何创建、搜索、加入房间并准备开局 | [Multiplayer-Lobby.md](Multiplayer-Lobby.md)：局域网搜房、IP 连接、房主权限与 WBP 样式入口 |
| 新中文字体、主菜单和轻动效怎么改 | [UI-Typography-And-Menu.md](UI-Typography-And-Menu.md)：字体、背景、字号与交互反馈 |
| 开始菜单在哪个 map、按钮怎么改 | [Main-Menu.md](Main-Menu.md)：主菜单关卡、WBP、按钮行为、设置复用与打包注意 |
| 其它架构图与历史规划 | [CurrentAllSystemsArchitecture.md](CurrentAllSystemsArchitecture.md)、[GAS-Full-Coverage-Plan.md](GAS-Full-Coverage-Plan.md)、[Optimal-Portfolio-Plan.md](Optimal-Portfolio-Plan.md)、[OptimizedArchitecture.md](OptimizedArchitecture.md)：保留各阶段设计背景，当前行为以源码为准 |
| 战斗 HUD 新样式与材质怎么改 | [Battle-HUD-Polish-2026-11-04.md](Battle-HUD-Polish-2026-11-04.md)：状态条、Boss 条、题签与退治反馈 |

推荐先读 `Class-Organization.md` 找文件，再读 `Project-Structure.md` 理解运行时关系。维护时结合扩展指南理解接线，避免把新逻辑继续堆回 GameMode、Character 或 HUD。

维护入口与以上近期专题已更新至 2026-11-09；其余带日期文档保留对应阶段的核对范围，不能把旧尺寸、默认值或测试结论直接当作当前配置。当前结构与行为以源码和实际加载资产为准。

当前升级效果使用各类型对应的 Infinite GE 子类按目标叠层，能力授予支持 AbilitySet；旧计划中的单一升级 ExecutionCalculation 和硬编码授予描述属于迁移历史。文档保留历史上下文，不表示这些旧实现仍存在。

## 死亡与复活

当前实现与验证结果见 [玩家生命周期说明](Player-LifeCycle.md)。默认十秒（`DA_PlayerBalance.Revival` 可配）的濒死复活、团灭终态和界面切换以该文为准。

怪物出生高度、地面射线和胶囊避障配置见 [怪物落地刷新说明](Enemy-Ground-Spawning.md)。

地图场景、可编辑刷怪框及蓝图商店摆放见 [地图摆放说明](Map-Authoring.md)。

玩家漂浮组件参数与当前神社夜景调整入口见 [玩家漂浮与神社夜景](Player-Floating-And-Night-Lighting.md)。

地图入口与旧枚举清理见 [Map-Entrypoints-And-Legacy-Cleanup.md](Map-Entrypoints-And-Legacy-Cleanup.md)：当前只保留 Map 文件夹下的主菜单和战斗地图。

## 三种低空敌人

[月蝶、风镰、星灯](Flying-Enemies.md)：交错扫射、单次折返风刃、带预警缺口的收拢星阵；包含蓝图、刷怪波次、参数和验证范围。

[飞行怪物数值调整（2026-10-29）](Flying-Enemy-Balance-2026-10-29.md)：三种新怪物的曲线倍率、普通/Boss伤害预算、前后对照和实况验证。

## 昼夜流程与玩家教程（2026-10-30）

- [白昼、黑夜与百波通关](Battle-Day-Night.md)：每 20 波交替昼夜，20 / 40 / 60 / 80 四次切图，打完第 100 波的终局 Boss 与敌人后通关。
- [神社调查手帖](Player-Guide.md)：与 WBP 教程同步的六章玩家指南。
- [教程维护说明](Tutorial-Maintenance.md)：事实核对、WBP 结构与更新方式。

- [幸存者战斗节奏：第一版](Survivor-Combat-Feel.md)：追击杂兵、前期进攻保底、武器与经验成长。

- [灵符·归心：环绕、穿刺、返航与升级配置](Returning-Talisman-Weapon.md)

- [开火闪光、法术涟漪与敌人命中碎光](Combat-Accents.md)
