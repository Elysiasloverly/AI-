# AI

基于 **Unreal Engine 5.5** 开发的俯视角 Rogue-like 动作生存游戏。

## 技术栈

UE5.5 · C++ · Blueprint · Gameplay Framework · WorldSubsystem · DataAsset / DataTable · Slate / UMG

## 架构总览

项目按 **Core / Player / Enemies / Combat / World / UI / Subsystems** 七层架构组织，代码规模约 11,300 行 C++。

| 层级 | 职责 |
|------|------|
| **Core** | GameMode 流程调度、波次规则、升级系统、商店系统、对象池、敌人注册表、空间哈希 |
| **Player** | 玩家角色、武器调度、数值系统、视觉组件、武器配置 |
| **Enemies** | 统一敌人运行时、行为原型、视觉资源、敌方弹体 |
| **Combat** | 子弹、火箭、迫击炮、激光、镰刀、命中特效等战斗对象 |
| **World** | 竞技场生成、经验球、商店终端 |
| **UI** | HUD、升级卡牌、商店界面、暂停菜单、结算页 |
| **Subsystems** | 刷怪调度、敌人追踪、对象池管理、场景优化 |

核心设计原则：GameMode 仅负责流程协调，具体逻辑下沉至 Rules / System / Subsystem 各层；玩法参数（伤害、冷却等）与表现参数（Mesh 缩放、Glow 等）严格分层解耦。

## 核心系统

### 战斗系统

- **6 种武器族**：普通子弹、火箭、迫击炮、激光、地狱塔、镰刀
- 每种武器支持独立升级卡池，通过 DataAsset / DataTable 驱动默认数值

### 敌人系统

- **统一运行时类** + 原型模式：所有敌人共用 `ARogueEnemy`，行为差异通过原型组合 + 数据表配置驱动
- **6 种移动模型**：直追、冲锋、环绕、远程风筝、Z 字追击、固定
- **多种射击模式**：单发、散射、地面波

### 成长体系

- 升级抽卡系统（代码默认规则 + DataAsset 资产覆盖双驱动）
- 商店经济系统（同类型购买永久翻倍、手动刷新周期翻倍、自动补货重置）
- 波次动态刷怪与 Boss 战奖励

### 性能优化

- **全局对象池**：池化敌人、玩家子弹、敌方弹体、火箭、激光 Beam、镰刀、命中特效、经验球共 8 类高频对象
- **空间哈希索敌**：为自动索敌、爆炸范围判定、激光折射、HUD 血条筛选提供 O(1) 近邻查询
- **高负载特效裁剪**：高压场景自动裁减命中特效

## 文档

详细架构文档位于 `Docs/` 目录：

- [系统架构总览](Docs/System-Architecture-Overview.md) — 各系统职责划分、运行时主循环、典型时序图
- [维护与扩展指南](Docs/Maintenance-Extension-Guide.md) — 添加武器/技能卡/敌人的完整步骤与验证清单
- [全系统架构图](Docs/CurrentAllSystemsArchitecture.md) — 当前全部系统的 Mermaid 架构图集合
- [优化架构图](Docs/OptimizedArchitecture.md) — 重构后的职责边界与扩展入口

## 构建

1. 安装 Unreal Engine 5.5
2. 右键 `ai.uproject` -> Generate Visual Studio project files
3. 打开 `ai.sln`，以 Development Editor 配置编译
4. 在 UE 编辑器中打开项目

或直接双击 `ai.uproject` 让 UE 自动编译。
