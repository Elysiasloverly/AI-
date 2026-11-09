# 敌人纸质配饰源模型

五个原创低面数模型：莲叶护符、针形护符、蝶形纸翼、尖角扇翼、纸灯笼。单位为厘米，局部 X 是纸张厚度，Y/Z 是轮廓平面。OBJ 包含 UV 与 `Paper` / `Trim` 材质槽，没有外部贴图依赖。

`Generate_Role_Meshes.py` 仅使用 Python 标准库，在自身目录生成 OBJ 和 `Meshes.json`。重新生成后，在 UE 对相应网格执行重新导入；不会自动改蓝图的角色、模型比例或战斗数据。

导入资产位于 `/Game/GameAssets/ArtDirection/EnemyRoles/Meshes`，实例材质在同级 `Materials`。详细编辑入口见项目文档 `Docs/Art-Enemy-Motion-2026-11-03.md`。
