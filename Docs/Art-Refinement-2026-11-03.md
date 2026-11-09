# 美术统一与精修记录

本轮统一到“Q 版绘本幻想乡”：玩家与武器优先可见，敌弹有清楚的亮芯和彩色边缘，治疗与危险预警保持不同颜色，场景细节退居背景，界面沿用浅纸、墨蓝、朱红与圆角。

## 本轮已保存的改动

| 范围 | 实际处理 |
|---|---|
| 玩家辨识度 | 保留上一阶段的 1750 cm 默认镜头、独立补光材质和朦胧白色描边；本轮不扩大玩家模型或碰撞 |
| 敌方弹幕 | 17 个弹体蓝图改用分层材质，亮芯、彩色中层、较暗边缘与低透明度光晕；保留各类弹体的网格、尺寸、运动和碰撞 |
| 灵符拖尾 | 保留蓝图里的 14 cm 宽度与 1 秒保留时间，改用低遮挡的淡青流光材质，支持曲线网格 |
| 冰弹外层 | 收敛原球形全息外壳的亮度，保留弹体本体与轮廓 |
| 激光与灼光 | 沿用已有流动纹理与起终点效果，降低外层过曝，保留亮芯 |
| 治疗范围 | 细绿色边界、少量刻度和缓慢呼吸；范围仍由原配置驱动 |
| 治疗连线 | 保留线条尺寸，改成淡绿、浅色亮芯和沿线流光 |
| 十张战斗地图 | 加入低对比绘本苔痕/霜痕投影、轻微叶片摆动；森林、樱庭与竹林使用柔和树影；夜空补月轮与稀疏微星 |
| 主菜单场景 | 灰色背景墙改为和纸舞台质感，柔化主光投影；保留角色、相机和场景布局 |
| 19 份菜单 WBP | 统一按钮正常、悬停、按下、禁用状态，修正浅色按钮上的文字对比度，细化卡片圆角边框与正文行距；不重建控件树 |

七种玩家武器的现有模型均作为当前造型保留。冰刃、火丸、冰球和符纸等已经有明确轮廓的模型不改成统一球体；本轮重点收敛它们与共用光效之间的关系。已有玩家/Boss/敌人血条动画继续使用，未为了换颜色重写反馈逻辑。

## 资源与调整入口

新增材质与贴图集中在 `/Game/GameAssets/ArtDirection/Refinement`。美术配置仍保存在蓝图与材质实例中；战斗数值留在原数据表。

| 资源 | 常用参数或使用位置 |
|---|---|
| `M_DanmakuBody` / `MI_Danmaku_*` | 敌弹本体；`Tint` 颜色、`Energy` 亮度 |
| `M_DanmakuAura` / `MI_DanmakuAura_*` | 微弱外层光晕；`Opacity` 透明度 |
| `M_OfudaSilkTrail` | `BP_RogueReturningTalisman` 的拖尾材质；`Tint`、`Opacity`，长度仍在蓝图的 Trail Lifetime 中设置 |
| `M_HealingRangeFine` | `BP_EnemyHealingCircle` 的 RangeProjection 材质；`Tint`、`Opacity`。改材质不会修改治疗半径 |
| `M_HealingLinkFine` | `BP_EnemyHealingLink` 的线条材质，尺寸仍由原组件和逻辑决定 |
| `MI_PlayerAura_*` | 玩家弹体的外层弱光 |
| `T_PaintedLichen` | 本次自制透明绘本苔痕贴图 |
| `M_PaintedGroundDetail` / `MI_GroundDetail_*` | 各主题地面点缀，`Tint`、`Opacity`；地图大纲 `Atmosphere/ArtDirection/GroundDetails` 可移动、缩放或删除 |
| `M_PaintedFoliage` | 叶片专用表面母材质，`WindAmplitudeCM` 控制世界空间微动；默认约 5–7 cm，距离远时衰减 |
| `M_SoftCanopy` | 林地主题主光的低对比树影，不修改碰撞 |
| `M_PaintedSkyLunar` | 现有昼夜天空实例的新母材质；`Night`、`Horizon`、`Zenith` |
| `M_MenuWashiStage` | MainMenu 中 MenuWall 的纸质背景 |

原有激光/灼光实例仍在 `/Game/GameAssets/Combat/BeamFX`。Core、Glow、Endpoint 分别控制亮芯、光晕和端点，调整 `Energy` 与 `Opacity` 即可。

玩家白色描边的独立设置仍在 `/Game/GameAssets/ArtDirection/PlayerFocus`，详见 [美术方向说明](Art-Direction.md)。

## 编辑习惯

- 危险预警保持红橙色并清楚显示范围；绿色专用于治疗。不要让装饰符阵、地面点缀或背景亮度接近预警。
- 地面点缀是投影，不承担碰撞或导航。增添时应投到有效地面，避免跨越水面或漂在高处。
- 世界设置继续使用 `BP_RogueGameMode`。美术编辑不要把它改回原生 GameMode。
- 弹体模型的尺寸、材质槽与外观参数在 BP 中配置。不要给普通弹体加入 Event Tick 来播放简单光效，现有材质 Time 动画即可，避免破坏集中弹幕更新。
- 改拖尾母材质后保持 `Used with Spline Meshes`，否则会回退成默认灰色材质。
- 菜单按钮和卡片仍可直接在 WBP 设计器修改。新的编辑器工具 `RefineArtPresentation` 只处理现有样式，不应当作每帧运行的逻辑。

## 验证与边界

本轮保留商店、出生点、刷怪轮廓与空气墙的变换，并在每张地图保存时对照。17 个敌弹 BP 仅替换材质，没有新增组件或 Tick。19 份 WBP 的控件名称、布局坐标、文本与事件图在处理前后逐项对照一致。

界面预览包含 16 个页面/叠加组合，使用隔离的示例数据；它不是一次真实购买或通关。战斗截图来自真实 PIE，未用生图修改实况。夜晚站桩会受到伤害，截图记录的生命与敌人数量以输出数据为准，不能据结算后的清场画面判断刷怪故障。

Development / DebugGame 编辑器工具均完成编译。与地图、UI、音效按钮绑定、弹幕碰撞和符纸有关的 26 项定向检查通过。性能数据属于 1920×1080、固定负载的快速检查，非完整实战或多人长期基准。

苔痕源图由内置 image_gen 制作，原图和提示词保存在 `ArtDirection/Source`。其余新增效果为项目内材质节点，没有引入外部素材包。
