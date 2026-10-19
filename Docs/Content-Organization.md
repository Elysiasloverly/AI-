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
│   ├── Library/                 原 Content 外层素材包，保留包内结构
│   │   ├── StarterContent/
│   │   ├── Grid_Materials/
│   │   ├── HologramShader/
│   │   ├── Realistic_Starter_VFX_Pack_Vol2/
│   │   └── Vefects/
│   └── UI/
│       └── Textures/            原 Image/ 中的界面图片
├── BP/                          玩法蓝图
├── BPEnum/                        玩法枚举
├── Input/                       输入动作与映射
├── UMG/                         界面蓝图
├── StarterContent/Maps/          保留原样例地图及构建数据
└── Data/                        玩法配置资产（Core / Player / Enemies / Weapons / Upgrades）
```

此次移动清单包括五个外层素材包的 350 个美术资产，以及 `Image/` 中的 2 个图片/精灵资产，共 352 个。素材包目标为 `/Game/GameAssets/Library/<原包名>/`；界面图片目标为 `/Game/GameAssets/UI/Textures/`。

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
