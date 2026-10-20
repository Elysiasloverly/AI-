# 编辑器资产目录

美术资产统一放在 `/Game/GameAssets/`（磁盘上的 `Content/GameAssets/`）。现有游戏使用的主资源目录保持原路径；原本散在 Content 外层的素材包集中到 `Library/`，保留各包内部结构和不同版本。

## 目录职责

以下列出主要目录：

```text
Content/
├── GameAssets/                  美术资产主目录
│   ├── StarterContent/          现有主资源与游戏地图
│   ├── Grid_Materials/          现有游戏引用的材质
│   ├── HologramShader/          现有游戏引用的全息材质
│   ├── Realistic_Starter_VFX_Pack_Vol2/
│   ├── Vefects/
│   ├── CharacterAssetse/        角色套装（10 套：网格、贴图与材质）
│   ├── Environment/             夜景天空等环境资源
│   ├── Map/                     场景网格与地形材质（1=森林，2=神社）
│   ├── WEapon/                  武器与弹体外观（冰锥、弹幕、冰刃、光束）
│   ├── Library/                 原 Content 外层素材包，保留包内结构
│   │   ├── StarterContent/
│   │   ├── Grid_Materials/
│   │   ├── HologramShader/
│   │   ├── Realistic_Starter_VFX_Pack_Vol2/
│   │   └── Vefects/
│   └── UI/
│       └── Textures/            原 Image/ 中的界面图片
├── BP/                          玩法蓝图
├── BPEnum/                      玩法枚举
├── Input/                       输入动作与映射
├── UMG/                         界面蓝图
├── StarterContent/Maps/         保留原样例地图及构建数据
└── Data/                        玩法配置资产
    ├── Core/                    DA_GameBalance
    ├── Player/                  DA_PlayerBalance
    ├── Enemies/                 DT_EnemyArchetypes
    ├── Weapons/                 DT_WeaponConfig
    └── Upgrades/                DA_UpgradeDefinitions、DA_UpgradeRules、DT_UpgradeDefinitions
```

## 配置资产从 DataTable 归类到 Data

原 `Content/DataTable/` 下的 7 个配置资产**没有被删除**，而是按所属系统归类到 `Content/Data/`，并统一为 `DA_`（DataAsset）/`DT_`（DataTable）命名：

| 原位置（`Content/DataTable/`） | 现在位置 | 类型 |
|---|---|---|
| `AS_RogueGameBalanceAsse` | `/Game/Data/Core/DA_GameBalance` | DataAsset |
| `AS_RoguePlayerBalanceAsset` | `/Game/Data/Player/DA_PlayerBalance` | DataAsset |
| `AS_UpgradeDefinitionAsset` | `/Game/Data/Upgrades/DA_UpgradeDefinitions` | DataAsset |
| `AS_RogueUpgradeRuleAsset` | `/Game/Data/Upgrades/DA_UpgradeRules` | DataAsset |
| `DT_FRogueEnemyArchetypeRow` | `/Game/Data/Enemies/DT_EnemyArchetypes` | DataTable |
| `DT_FRogueUpgradeDefinitionRow` | `/Game/Data/Upgrades/DT_UpgradeDefinitions` | DataTable |
| `DT_WeaponConfig` | `/Game/Data/Weapons/DT_WeaponConfig` | DataTable |

C++ 类名、数据表行结构（`FRogueEnemyArchetypeRow`、`FRogueWeaponTableRow`、`FRogueUpgradeDefinitionRow`）、行名与字段保持不变；源码默认加载路径同步为 `/Game/Data/...`。整理后 `Content/` 下不再保留独立的 `DataTable/` 目录，读取入口见 [配置资产说明](Data-Configuration.md)。

此次移动清单包括五个外层素材包的 350 个美术资产，以及 `Image/` 中的 2 个图片/精灵资产，共 352 个。素材包目标为 `/Game/GameAssets/Library/<原包名>/`；界面图片目标为 `/Game/GameAssets/UI/Textures/`。后续新增的角色套装、森林/神社场景与武器特效直接进入 `CharacterAssetse/`、`Map/`、`WEapon/`，不再经过 `Library/`。

## 引用与保留范围

主地图路径保留 `/Game/GameAssets/StarterContent/Maps/Minimal_Default`；已有 C++ 美术加载路径、默认地图配置以及 BP、BPEnum、Input、UMG 的目录保持原位置。玩法数值与配置资产现统一位于 `Data/`，详见 [配置资产说明](Data-Configuration.md)。三张原样例地图及其构建数据保留在 `StarterContent/Maps/`，不纳入美术资产移动范围。

同名资产不代表内容相同。现有 `GameAssets` 资源和原外层包作为两个版本保留，不通过这次目录整理合并或删除。玩法蓝图即使包含网格、材质或特效配置，也留在 `BP/`；界面蓝图留在 `UMG/`。

移动通过 Unreal 资产管理接口执行，并随后修复重定向器和保存引用；直接在文件管理器里移动 `.uasset` 或 `.umap` 不能代替这一流程。已检查全部资产的类型与数量、旧路径引用、移动后资源加载，以及玩法和界面蓝图加载。

## 后续维护

新美术资源放进 `GameAssets/`；整体导入的素材包放进 `GameAssets/Library/<包名>/`，界面图片放进 `GameAssets/UI/Textures/`。涉及目录变更时，同步检查蓝图、材质、地图、配置和代码中的资源引用。

`Tools/asset_reorg_*.py` 及对应 JSON 是前次整理的历史工具和记录，原有审计清单已过期，不是本次移动或删除的依据。尤其旧清理、兼容复制及移动脚本会按旧结构写入资产；不要以其中的 unused 标记判断当前资产可以删除。此次任务保留这些历史文件，不修改其代码。

## 验证结果

- 352 个资产全部移动成功，保存引用后清理所有无引用重定向器和 46 个空目录。
- 762 个非重定向资产均保留；逐类型数量一致。
- 352 个移动资产和 29 个玩法/界面蓝图，共 381 个对象成功加载。美术类型资产全部位于 GameAssets，下层保留原素材包结构。
- 无新增缺失的包引用；源代码和默认地图配置未修改。
- 示例地图及构建数据未移动，主地图仍为 /Game/GameAssets/StarterContent/Maps/Minimal_Default。
