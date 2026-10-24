"""创建 Boss 专用弹体蓝图。

用法（项目根目录）：
    UnrealEditor-Cmd.exe F:\AI_WorkSpeace\baka\Baka.uproject ^
        -run=pythonscript -script="Tools/create_boss_projectile_bp.py" ^
        -unattended -nopause -nullrhi -nosplash

产物：/Game/BP/Enemy/Projectiles/BP_EnemyProjectile_Boss
父类：BP_EnemyProjectile（继承普通弹体外观，再单独改 Boss 样式）

ARogueEnemy 构造函数按固定路径查找它；只有 bIsBoss 为真时使用，
缺失时自动回退普通弹体。
"""
import unreal

DEST = "/Game/BP/Enemy/Projectiles"
NAME = "BP_EnemyProjectile_Boss"
BASE_PATH = DEST + "/BP_EnemyProjectile"


def log(message):
    unreal.log("[BossProjectile] {}".format(message))


def main():
    asset_path = "{}/{}".format(DEST, NAME)
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        log("already exists: {}".format(asset_path))
        return

    base_class = unreal.load_class(None, BASE_PATH + ".BP_EnemyProjectile_C")
    if base_class is None:
        raise RuntimeError("base blueprint missing: {}".format(BASE_PATH))

    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", base_class)
    blueprint = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        NAME, DEST, unreal.Blueprint, factory)
    if blueprint is None:
        raise RuntimeError("failed to create {}".format(asset_path))

    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    unreal.EditorAssetLibrary.save_asset(asset_path, False)
    log("created {}".format(asset_path))


main()
