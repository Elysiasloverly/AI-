# 敌人角色资源与重复外观替换

更新：2026-10-29。用户提供的UE5.5资源已导入现有角色素材目录：

`/Game/GameAssets/CharacterAssetse/Rebuilt/Character_01` 到 `Character_10`。

共10个静态网格角色、376个UE资产（含配套材质和贴图）。这些是无骨骼的坐姿静态模型，沿用敌人现有的扫帚和移动表现。原始文件保留在用户提供的目录。

## 敌人对应关系

原有18种敌人共用9个角色模型；保留每组中的一个原有角色，将其余9处重复外观换为新角色。现在18种敌人主体网格各不相同。

| 敌人BP | 类型 | 新资源 | 外观 | 主体高度 |
|---|---|---|---|---:|
| `BP_EnemySkyMoth` | 月蝶 | `Character_03/character_03` | 粉发红结、侧马尾与小翼 | 140cm |
| `BP_EnemyWindReaver` | 风镰 | `Character_02/character_02` | 粉发圆帽、斜袋与小刀 | 140cm |
| `BP_EnemyStarLantern` | 星灯 | `Character_10/character_10` | 浅绿长发、蓝裙与白蛇饰 | 140cm |
| `BP_EnemyAuraHealer` | 青叶灵 | `Character_08/character_08` | 浅绿短发、红格纹裙 | 144cm |
| `BP_EnemyLinkHealer` | 祈愿妖精 | `Character_06/character_06` | 绿发、紫裙、宽袖与御币 | 144cm |
| `BP_EnemySniper` | 月兔 | `Character_05/character_05` | 淡紫长发、兔耳与萝卜领带 | 144cm |
| `BP_EnemyRepeater` | 血蝠 | `Character_04/character_04` | 金发蓝裙、粉色领带 | 144cm |
| `BP_EnemyRingcaster` | 亡灵 | `Character_07/character_07` | 蓝长发、黑帽与桃饰 | 144cm |
| `BP_EnemyShotgunner` | 毒茸 | `Character_01/character_01` | 白发兽耳、红圆帽、黑红裙 | 144cm |

`Character_09`（黑长发、粉衣宽袖）已完整导入作为备用，没有为用满10个角色而替换本来独立的旧外观。旧模型资源没有删除，仍供另外9种敌人及其它资产使用。

## 在哪里继续修改

打开对应敌人BP，选择角色主体组件：原有地面怪一般是 `Cube`，治疗怪是 `HealerBody`，飞行怪是 `FlyingBody`。模型引用、材质和缩放均保存在BP组件里，C++没有新增这些角色的硬编码路径。

- 更换静态网格时，清理旧的组件材质覆盖，使用新角色网格的默认材质槽。本次9个替换组件已处理，避免旧衣服颜色覆盖新模型。
- 已按原比例适配普通/治疗怪144cm、飞行怪140cm，保留脚底高度与朝向。治疗者头顶标记位置、扫帚、法阵和其它配饰沿用原配置。
- Boss仍由基类表现组件追加1.5倍，回池恢复；无需在新角色BP里重复放大。
- 角色外观组件不参与物理碰撞；伤害判定继续由原有胶囊与法阵逻辑负责。

## 迁移和检查

这批uasset原先引用 `/Game/Rebuilt/...`。导入时先维持原包路径，再通过UE正式资源重命名迁移至最终目录，保存材质、纹理和网格的新引用；1098项依赖检查通过，最终资源不再依赖临时的 `/Game/Rebuilt` 目录，本次迁移没有留下需要保留的重定向器。

重新加载确认：10个网格可用、18种敌人主体各不相同、9处替换使用正确的默认材质，角色高度正确，其余模型和配饰比例保持一致。9个战斗数据资产的文件哈希与导入前一致，未调整本轮数值。实际地图中的运行时可见性、材质及无碰撞检查记录位于本次工作输出。

替换前的9个BP和数据资产已备份在：

`C:/Users/Elysia/Documents/Codex/2026-10-13/f-ai-workspeace-ai/work/character-import-2026-10-29/backup`。

本次为资产导入与BP组件修改，无需重新编译Development或DebugGame。
