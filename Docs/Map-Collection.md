# 五主题昼夜地图集合

更新：2026-11-02。当前入口仍是 `/Game/Map/MainMenu`。

> 后续修复：十张图已恢复 BP_RogueGameMode 的敌人蓝图映射，四个新场景改用独立平滑碰撞，并通过实际输入与行走检查。当前碰撞与验证说明见 [新地图玩法修复](New-Map-Gameplay-Fix.md)。


## 地图在哪里

战斗地图分为 `/Game/Map/Day` 和 `/Game/Map/Night`，每个目录五张。主菜单留在 Map 根目录。

| 主题 | Day 文件名 | Night 文件名 |
|---|---|---|
| 旧森林（Blender 细化版） | Battle_Forest_Day | Battle_Forest_Night |
| 樱庭神社 | Battle_SakuraShrine_Day | Battle_SakuraShrine_Night |
| 雾湖冰洲 | Battle_MistyIceLake_Day | Battle_MistyIceLake_Night |
| 竹林月径 | Battle_MoonBamboo_Day | Battle_MoonBamboo_Night |
| 红魔钟庭 | Battle_ScarletClock_Day | Battle_ScarletClock_Night |

新场景的模型、共用材质和纹理位于 `/Game/FourMaps`。四张导入源场景 `L_Map01` 至 `L_Map04` 保留作参考，不加入随机地图池。旧森林细化模型位于 `/Game/GameAssets/Map/ForestV3/Meshes/SM_ForestV3_Refined`；原 ForestV2 模型保留。

旧森林模型保持当前关卡里的 6 倍缩放，长边约 349.2 米。四种新模型的原始尺寸不同，因此使用等比缩放，把各自 XY 最长边统一到约 349.2 米；缩放数值不强制写成 6。水面与场景用相同倍率，避免桥岸错位。

## 怎样选图

打开 `/Game/Data/Core/DA_GameBalance` → `08.昼夜阶段` → `BattleStage`：

- **白天随机地图池**：单人新局、多人房主开局和第二段白天共同使用。
- **黑夜随机地图池**：两个黑夜阶段共同使用。
- **白天备用地图 / 黑夜备用地图**：对应池为空或全部无效时使用。

有效条目去重后等概率选择；不会因同一条目重复填写而增加权重。选择发生在服务器，客户端跟随同一次 ServerTravel。池是显式配置，不会自动扫描新放入目录的地图；以后加图，记得将其加入对应池并补入打包地图清单。

每隔 20 波先停刷、清场、倒计时 30 秒，然后全员切换昼夜。默认结束波次 20、40、60、80，进入 100 波优先胜利。白天、黑夜普通波都按清场推进；黑夜数量与生成倍率读取 DA_GameBalance，当前分别为 3 倍、2 倍；默认 70% 普通近战兵的移速和伤害翻倍，剩余类型正常生成，比例也可手动调整。攻速不额外翻倍。详见 [昼夜流程](Battle-Day-Night.md)。

## 每张图的玩法配置

每张图均有：四个分开的 PlayerStart、一处可交互的蓝图商店、一个 EnemySpawn_PlayableOutline、一个 AirWalls_OuterAndWaterHoles。旧森林昼夜图分别保留原商店的位置、旋转和缩放。 十张战斗地图中的商店均使用 BP_RogueShopTerminal，Actor 缩放统一为 (2,2,2)，新地图已按模型底部偏移校正落地高度。

- **刷怪轮廓**：闭合直线多边形，贴合庭院、冰洲和宽通道；保留胶囊半径与边缘留白。地面射线仅在地面高度带内取点，并继续检查坡度、胶囊与场景碰撞。
- **冰湖水孔**：两处内部水孔列入 ExcludedAreas，同时布置内岸空气墙。外侧水面也在闭合边界之外。
- **空气墙**：沿边界逐段生成，墙厚 65 cm、高 6000 cm，仅阻挡 Pawn；不挡相机、Visibility 射线和弹体。静止物件继续使用自己的场景碰撞。
- **复杂边界性能**：水岸有较多折点，轮廓自交校验只在 XY 点位改变后重算；移动、缩放或修改点位会重新校验。
- **灯光**：昼夜独立保存；白天保留主题色温，夜间使用主题月光、较低环境光及局部暖光。没有运行时替换整套关卡灯光。

场景列表按 Environment、Lighting、Atmosphere、Gameplay、Reference 分组。Gameplay 下分别放刷怪区域、空气墙、玩家出生点和商店。

## 以后如何修改边界

选择 EnemySpawn_PlayableOutline 的 SpawnOutline 组件拖动顶点。ExcludedAreas 是内部水孔的世界 XY 坐标。空气墙的 Loops 是另一份可编辑边界，坐标相对空气墙 Actor；当前 Actor 位于世界原点。**只修改刷怪轮廓不会自动改空气墙**，调整地图时要同步修改两者。修改后开启新局验证地面、出生点、商店与岸线。

## 旧路径兼容与打包

旧 `/Game/Map/Battle_Day`、`/Game/Map/Battle_Night` 通过 PackageRedirects 对应森林的新昼夜路径。运行时默认值和房间配置已经改用新路径。`Config/DefaultGame.ini` 的 MapsToCook 列出主菜单与十张战斗地图；地图池用软引用，选图不会提前把五张完整地图全部加载进内存。

## 验证范围

Development Editor、DebugGame Editor 均编译通过。地图池、空气墙、水孔、轮廓缓存、昼夜阶段、地面刷新与商店摆放共 24 项定向测试通过。十张图共抽样 8000 个位置，轮廓内 4782 个有效位置全部命中地面；每张图均有四个出生点，商店均位于轮廓内。

双独立进程完成三次真实随机切图，两位玩家的等级、金钱接续正确，正式存档哈希未变。该回归临时把测试局倒计时设为 1 秒；资产中的 30 秒保持不变。未执行完整发行包 Cook，也未跑完整 100 波的实机平衡体验。
