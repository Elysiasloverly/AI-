# Docs 索引

先按需要选择当前结构与数据流文档，再查系统细节和历史取舍。C++ 已按玩法职责整理，历史文档中的 `Core/`、`Subsystems/` 或旧类文件路径请以 [Class-Organization.md](Class-Organization.md) 为准。

| 想知道什么 | 看这里 |
|---|---|
| 一个类在哪里，新增类应该放哪 | [Class-Organization.md](Class-Organization.md)：目录职责、实现拆分、独立 Widget 基类与蓝图兼容约定 |
| 编辑器里的美术资产放在哪里 | [Content-Organization.md](Content-Organization.md)：美术主目录、素材包保留方式及玩法资产位置 |
| 玩法配置资产放在哪、怎么改 | [Data-Configuration.md](Data-Configuration.md)：`/Game/Data/` 的 7 个资产、读取入口与 DataTable 归类 |
| 哪些配置已经进数据资产、哪些还在代码里 | [Data-Asset-Audit.md](Data-Asset-Audit.md)：`/Game/Data` 覆盖审计与待迁清单 |
| 中文名和代码里的英文名怎么对应 | [Content-Naming.md](Content-Naming.md)：武器 / 升级卡 / 敌人的显示名 ↔ 枚举 ↔ 表行键，改名入口与校验 |
| 数值怎么调 | [Game-Balance-Configuration.md](Game-Balance-Configuration.md)、[Player-Balance-Configuration.md](Player-Balance-Configuration.md)：平衡与玩家数值分组 |
| 地图、刷怪框与商店怎么摆 | [Map-Authoring.md](Map-Authoring.md)：关卡物体、刷怪区域与商店终端 |
| 血条 / 护甲条 / 经验条 / 波数金币怎么做的 | [HUD-Vitals-And-Enemy-Bars.md](HUD-Vitals-And-Enemy-Bars.md)：玩家状态 WBP、CJK 字体、数据流 |
| 受伤时的屏幕泛红与镜头震动怎么调 | [Damage-Feedback.md](Damage-Feedback.md)：掉血检测、`FlashRoot` 结构、震动参数与替换方式 |
| 敌方弹幕用哪个蓝图、怎么换外观 | [Enemy-Projectile-Blueprints.md](Enemy-Projectile-Blueprints.md)：`BP_EnemyProjectile`、每敌人覆盖与池化 |
| 经验球怎么换外观 | [Experience-Pickup-Blueprint.md](Experience-Pickup-Blueprint.md)：`BP_ExperiencePickup` 组件结构、BP 钩子与尺寸规则 |
| 弹体碰撞是否关闭 | [Combat-Collision.md](Combat-Collision.md)：所有弹体的命中球 / 视觉组件 / 物理设置审计 |
| 敌人原型表怎么读 | [Enemy-Archetype-Table.md](Enemy-Archetype-Table.md)：11 个顶层列、一怪一块的 `SpecialParams`、字段语义与迁移记录 |
| 13 种敌人各是什么、怎么打 | [Enemy-Ranged-Variants.md](Enemy-Ranged-Variants.md)：枚举名与中文名对照、弹道、起始波次与验证 |
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
| 其它架构图与历史规划 | [CurrentAllSystemsArchitecture.md](CurrentAllSystemsArchitecture.md)、[GAS-Full-Coverage-Plan.md](GAS-Full-Coverage-Plan.md)、[Optimal-Portfolio-Plan.md](Optimal-Portfolio-Plan.md)、[OptimizedArchitecture.md](OptimizedArchitecture.md)：保留各阶段设计背景，当前行为以源码为准 |

推荐先读 `Class-Organization.md` 找文件，再读 `Project-Structure.md` 理解运行时关系。维护时结合扩展指南理解接线，避免把新逻辑继续堆回 GameMode、Character 或 HUD。

除明确标注为“历史”的文档外，当前结构、路径与接口以仓库源码为准（2026-10 核对）；历史文档只用于解释当时的取舍。

当前升级效果使用 22 种 Infinite GE 子类按目标叠层，能力授予支持 AbilitySet；旧计划中的单一升级 ExecutionCalculation 和硬编码授予描述属于迁移历史。文档保留历史上下文，不表示这些旧实现仍存在。

## 死亡与复活

当前实现与验证结果见 [玩家生命周期说明](Player-LifeCycle.md)。默认十秒（`DA_PlayerBalance.Revival` 可配）的濒死复活、团灭终态和界面切换以该文为准。

怪物出生高度、地面射线和胶囊避障配置见 [怪物落地刷新说明](Enemy-Ground-Spawning.md)。

地图场景、可编辑刷怪框及蓝图商店摆放见 [地图摆放说明](Map-Authoring.md)。

玩家漂浮组件参数与当前神社夜景调整入口见 [玩家漂浮与神社夜景](Player-Floating-And-Night-Lighting.md)。
