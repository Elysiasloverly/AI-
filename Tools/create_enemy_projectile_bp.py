"""创建敌人弹体蓝图资产。

用法（项目根目录）：
    UnrealEditor-Cmd.exe F:\AI_WorkSpeace\ai\ai.uproject ^
        -run=pythonscript -script="Tools/create_enemy_projectile_bp.py" ^
        -unattended -nopause -nullrhi -nosplash

产物：/Game/BP/Enemy/Projectiles/BP_EnemyProjectile
父类：ARogueEnemyProjectile

说明：
- ARogueEnemy 的构造函数按这个固定路径查找默认弹体类，所有敌人 BP 会继承；
- 已存在时只打印路径，不覆盖你在蓝图里做的外观修改；
- 每个敌人仍可在自己的类默认值里用 EnemyProjectileClass 覆盖。
"""
import unreal

DEST = "/Game/BP/Enemy/Projectiles"
NAME = "BP_EnemyProjectile"


def log(message):
    unreal.log("[EnemyProjectile] {}".format(message))


def main():
    asset_path = "{}/{}".format(DEST, NAME)
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        log("already exists: {}".format(asset_path))
        return

    if not unreal.EditorAssetLibrary.does_directory_exist(DEST):
        unreal.EditorAssetLibrary.make_directory(DEST)

    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", unreal.RogueEnemyProjectile)
    blueprint = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        NAME, DEST, unreal.Blueprint, factory)
    if blueprint is None:
        raise RuntimeError("failed to create {}".format(asset_path))

    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    unreal.EditorAssetLibrary.save_asset(asset_path, False)
    log("created {}".format(asset_path))


main()
