# 地图入口统一与旧枚举清理

2026-10-21，项目以用户 `Content/Map` 中的地图为准。

## 当前地图（2026-10-23 更新）

| 地图 | 用途 | 入口 |
|---|---|---|
| `/Game/Map/MainMenu` | 主菜单 | GameDefaultMap |
| `/Game/Map/Battle_Night` | 原战斗地图重命名，保留夜景 | EditorStartupMap、单人游戏、多人战斗默认目的地 |
| `/Game/Map/Battle_Day` | 复制原战斗地图的白天灯光版 | 编辑器打开或将目的地配置为此地图 |

原用户战斗地图 `/Game/Map/Minimal_Default` 重命名为 `Battle_Night`，复制为 `Battle_Day` 后只调整环境灯光。主菜单保留，打包 MapsToCook 包含以上三张地图。场景模型、刷怪范围、商店、出生点和 GameMode 配置保留。详见 [昼夜地图](Battle-Day-Night.md)。

原 `GameAssets/StarterContent/Maps` 下的另一份战斗地图、Advanced_Lighting、StarterMap 已通过 Unreal 资产接口删除；三份无引用的 BuiltData 同步清理。删除前检查了资产引用，备份存于本次 Codex 工作区，位于项目 Content 外。

后续搭建和编辑场景打开 `Content/Map/Battle_Night` 或其白天副本 `Content/Map/Battle_Day`。本文记录当时仅统一地图入口的迁移阶段。当前多人房间与永久升级树已实现；入口分别见 [多人大厅](Multiplayer-Lobby.md) 与 [升级树](UpgradeTreeGuide.md)，不再是占位按钮。

## 敌人旧类型名

DefaultEngine.ini 的 CoreRedirects 增加 ERogueEnemyType 值迁移：Hunter→Rifleman、Charger→Assault、Tank→Juggernaut、Orbit→Orbiter、Shooter→Marksman、Artillery→Cannoneer、Spitter→Scatterer、Burst→Repeater、Ringer→Ringcaster。

同时保存了十三个具体敌人蓝图，使默认 EnemyType 与其当前类型一致。基类 BP_EnemyBase 原先的 Tank 默认值迁移为 Juggernaut。未改动模型、材质和相对变换；冷加载确认旧枚举告警消失。

兼容映射仍需保留，用于读取旧版本资产；不是要求重新添加已经删除的旧蓝图或旧地图。

## 验证入口

`Tools/probe_maps.py` 现在是只读验证工具，检查 Content 中的主菜单和两张战斗地图、游戏和编辑器入口、菜单目的地及各地图 GameMode 覆盖。不保存地图，也不重建界面。

现有 `Rogue.Gameplay.EnemyVariants.RangedArchetypesAndBlueprints` 已扩充为全部十三种类型，并校验每个敌人蓝图的默认 EnemyType，防止父类旧默认值再次污染子类。

## 2026-10-21 清理时的历史验证结果

Development Editor 与 DebugGame Editor 均编译成功，两种配置各 63 项项目回归通过。冷加载未出现旧枚举警告。运行验证确认默认主菜单加载成功，并按配置的目标进入 `/Game/Map/Minimal_Default`，玩家、商店、刷怪区域及敌人正常生成。两张保留地图的文件哈希与清理前一致。
