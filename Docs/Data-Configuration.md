# 玩法配置资产

配置资产统一放在 `/Game/Data/`，在内容浏览器中打开 `Data` 即可按系统查找。`DA_` 表示 DataAsset，`DT_` 表示 DataTable；C++ 类名和数据表行结构保留原名。

```text
Content/Data/
├── Core/
│   └── DA_GameBalance
├── Player/
│   └── DA_PlayerBalance
├── Enemies/
│   └── DT_EnemyArchetypes
├── Weapons/
│   └── DT_WeaponConfig
└── Upgrades/
    ├── DA_UpgradeDefinitions
    ├── DA_UpgradeRules
    └── DT_UpgradeDefinitions
```

## 从 DataTable 到 Data

原来的配置资产放在 `Content/DataTable/`。这次整理**没有删除其中的配置数据**，而是按所属系统归类到 `Data/` 并统一命名：

| 原资产（`Content/DataTable/`） | 现路径 | 类型 |
|---|---|---|
| `AS_RogueGameBalanceAsse` | `/Game/Data/Core/DA_GameBalance` | DataAsset |
| `AS_RoguePlayerBalanceAsset` | `/Game/Data/Player/DA_PlayerBalance` | DataAsset |
| `AS_UpgradeDefinitionAsset` | `/Game/Data/Upgrades/DA_UpgradeDefinitions` | DataAsset |
| `AS_RogueUpgradeRuleAsset` | `/Game/Data/Upgrades/DA_UpgradeRules` | DataAsset |
| `DT_FRogueEnemyArchetypeRow` | `/Game/Data/Enemies/DT_EnemyArchetypes` | DataTable |
| `DT_FRogueUpgradeDefinitionRow` | `/Game/Data/Upgrades/DT_UpgradeDefinitions` | DataTable |
| `DT_WeaponConfig` | `/Game/Data/Weapons/DT_WeaponConfig` | DataTable |

源码默认路径同步改为 `/Game/Data/...`；C++ 类型名、序列化字段、DataTable 行结构与行名均未改变，已有蓝图覆盖值和行数据继续生效。目录层级见 [编辑器资产目录](Content-Organization.md)。

## 修改配置时去哪里

| 想修改的内容 | 资产 | 读取入口 |
|---|---|---|
| 波次、刷怪、Boss、金币与商店平衡 | `Core/DA_GameBalance` | GameMode 加载并交给局内规则与子系统 |
| 玩家基础生命、护甲、移动、实际冲刺冷却下限；武器奖励上限 | `Player/DA_PlayerBalance` | Character 初始化 PlayerState 中的 GAS 属性 |
| 敌人原型、行为、攻击、弹体蓝图及原型限制参数 | `Enemies/DT_EnemyArchetypes` | 敌人蓝图与生成系统；行结构 `FRogueEnemyArchetypeRow`（专属参数统一在 `SpecialParams`，见 [敌人原型表结构](Enemy-Archetype-Table.md)，弹体列见 [敌人弹体蓝图](Enemy-Projectile-Blueprints.md)） |
| 武器初始数量、伤害、冷却、范围、数量上限与派生公式系数 | `Weapons/DT_WeaponConfig` | `BP_RogueCharacter.WeaponDataTable`；行结构 `FRogueWeaponTableRow` |
| 选择升级系统读取哪张定义表 | `Upgrades/DA_UpgradeDefinitions` | `UpgradeDefinitionTable` 指向 `DT_UpgradeDefinitions` |
| 升级基础池、武器来源映射和冲刺冷却卡停止出现的阈值 | `Upgrades/DA_UpgradeRules` | `RogueUpgradeSystemRules::BuildUpgradePool` |
| 升级卡名称、说明与数值 | `Upgrades/DT_UpgradeDefinitions` | 按行内 `Type` 匹配；行结构 `FRogueUpgradeDefinitionRow` |

`DA_GameBalance` 已按七个区域重新分组（敌人、刷怪、普通敌人、首领、奖励、商店、运行时），刷怪位置/Boss 节奏/池大小/性能阈值/场景优化都在资产里；具体调参位置与合并后的规则见 [DA_GameBalance 调参说明](Game-Balance-Configuration.md)。

升级定义负责“卡片是什么、加多少”，升级规则负责“哪些类型参与抽取”。当前星尘折射和阴阳玉·扩散由 `RogueUpgradeSystemRules.cpp` 根据实际生效的武器数量追加，规则资产中的五条武器来源映射无需额外补写这两行。解锁武器会立即刷新奖励快照，供连续升级的下一轮判断。

敌人原型表描述移动、攻击行为、原型生成上限和 `Visual`/`BossVisual`；生命、伤害和经验奖励由 `DA_GameBalance` 参与计算的 Profile 提供。`DA_UpgradeRules.MinimumDashCooldown` 控制冲刺卡何时出池，实际冷却数值下限由 `DA_PlayerBalance.BaseStats.DashMinCooldown` 提供。

GameMode 蓝图或关卡指定的配置优先；未指定时，代码从上述新目录加载默认资产。玩家平衡资产也支持角色蓝图覆盖。修改运行结果前，先确认当前关卡使用的 GameMode 和角色蓝图。

## 命名与维护

新配置按所属系统放入对应目录，名称描述用途，例如 `DA_UpgradeRules`；数据表名称不带 C++ 的 `F` 前缀或 `Row` 后缀。资产移动与改名使用 UE 内容浏览器或 AssetTools，再保存引用方并修复重定向器。

这次整理按系统归类并统一了 7 个配置资产的位置与名称；三张表的数据与行名、四个 DataAsset 的参数，以及已有蓝图覆盖值均保留，同步修改的只有默认加载路径和资产引用。

地藏的整圈弹幕参数见 [地藏攻击说明](Shock-Pillar-Attack.md)。玩家出生状态、成长、武器加成与复活参数见 [玩家配置说明](Player-Balance-Configuration.md)。

御柱与极限火花现在使用各自的光束蓝图；材质与粗细入口见 [御柱光束配置](Hell-Tower-Beam.md)。
