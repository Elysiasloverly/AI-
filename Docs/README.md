# Docs 索引

先按需要选择当前结构与数据流文档，再查系统细节和历史取舍。C++ 已按玩法职责整理，历史文档中的 `Core/`、`Subsystems/` 或旧类文件路径请以 [Class-Organization.md](Class-Organization.md) 为准。

| 想知道什么 | 看这里 |
|---|---|
| 一个类在哪里，新增类应该放哪 | [Class-Organization.md](Class-Organization.md)：目录职责、实现拆分、独立 Widget 基类与蓝图兼容约定 |
| 编辑器里的美术资产放在哪里 | [Content-Organization.md](Content-Organization.md)：美术主目录、素材包保留方式及玩法资产位置 |
| 当前对象如何协作，伤害和升级如何流动 | [Project-Structure.md](Project-Structure.md)：运行时关系、网络划分、池化契约、核心数据流与自检入口 |
| 各系统为什么这样分工 | [System-Architecture-Overview.md](System-Architecture-Overview.md)：游戏流程与系统协作；旧目录示例结合当前组织说明阅读 |
| 怎样继续加武器、升级卡、敌人和界面 | [Maintenance-Extension-Guide.md](Maintenance-Extension-Guide.md)：扩展接线与资产配置；文件落点以当前组织说明为准 |
| 从项目入口开始了解 | [项目 README](../README.md)：技术栈、玩法、GAS 架构、压测命令与演示 |
| GAS 主架构的迁移过程和验证证据 | [GAS-Main-Architecture-Plan.md](GAS-Main-Architecture-Plan.md)：分阶段改造、取舍与踩坑记录 |
| 属性从自研系统转向 GAS 的原因 | [Attribute-System-Migration.md](Attribute-System-Migration.md)：历史方案，描述的旧属性系统已被替换 |
| 输入资产和回退路径 | [Enhanced-Input-Migration.md](Enhanced-Input-Migration.md)：输入迁移记录与配置方式 |
| 其它架构图与历史规划 | [CurrentAllSystemsArchitecture.md](CurrentAllSystemsArchitecture.md)、[GAS-Full-Coverage-Plan.md](GAS-Full-Coverage-Plan.md)、[Optimal-Portfolio-Plan.md](Optimal-Portfolio-Plan.md)、[OptimizedArchitecture.md](OptimizedArchitecture.md)：保留各阶段设计背景，当前行为以源码为准 |

推荐先读 `Class-Organization.md` 找文件，再读 `Project-Structure.md` 理解运行时关系。维护时结合扩展指南理解接线，避免把新逻辑继续堆回 GameMode、Character 或 HUD。

当前升级效果使用 20 种 Infinite GE 子类按目标叠层，能力授予支持 AbilitySet；旧计划中的单一升级 ExecutionCalculation 和硬编码授予描述属于迁移历史。文档保留历史上下文，不表示这些旧实现仍存在。

## 死亡与复活

当前实现与验证结果见 [玩家生命周期说明](Player-LifeCycle.md)。固定十秒濒死复活、团灭终态和界面切换以该文为准。
