# 蓝图目录说明

`/Game/BP` 按用途归类。蓝图资产名称保持原样；WBP 在 `/Game/UMG`，数据资产在 `/Game/Data`，地图在 `/Game/Map`，模型和材质在 `/Game/GameAssets`。

| 目录 | 内容 |
| --- | --- |
| `Framework` | 战斗 GameMode、HUD |
| `Player` | 玩家 Character 蓝图 |
| `Enemy` | 敌人本体；`AI` 放敌人控制器，`Projectiles` 放敌方弹体 |
| `Projectiles` | 玩家御札、银刃、八卦炉、极限火花、御柱和阴阳玉弹体 |
| `World` | 可拖入关卡的商店终端、正式经验球 |
| `MainMenu` | 主菜单场景中的角色展示 Actor |
| `Development/Experiments` | `NewBlueprint`、`TestPickup` 等实验内容；继续保留供查看 |
| `Development/Legacy` | 已不参与当前地图的旧 `BP_RogueArena` |

常用入口：

- 玩家外观：`/Game/BP/Player/BP_RogueCharacter`。
- 战斗规则：`/Game/BP/Framework/BP_RogueGameMode`。
- 战斗 HUD：`/Game/BP/Framework/BP_RogueHUD`。
- 关卡商店：`/Game/BP/World/BP_RogueShopTerminal`。
- 玩家弹体外观：`/Game/BP/Projectiles`。

根目录只放分类文件夹。新增蓝图按用途放入已有分类，实验蓝图放 `Development/Experiments`；正式接入后再移入功能目录。资产移动应使用 UE 内容浏览器或 AssetTools，并更新配置与脚本中的字符串路径。
