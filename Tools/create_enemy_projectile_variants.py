"""为不同敌人类型创建弹体蓝图变体。

用法（项目根目录）：
    UnrealEditor-Cmd.exe F:\AI_WorkSpeace\ai\ai.uproject ^
        -run=pythonscript -script="Tools/create_enemy_projectile_variants.py" ^
        -unattended -nopause -nullrhi -nosplash

产物（父类均为 BP_EnemyProjectile）：
    BP_EnemyProjectile_Shooter
    BP_EnemyProjectile_Artillery
    BP_EnemyProjectile_Spitter
    BP_EnemyProjectile_ShockPillar

已存在时只跳过，不覆盖你改过的外观。
"""
import unreal

DEST = "/Game/BP/Enemy/Projectiles"
BASE_PATH = DEST + "/BP_EnemyProjectile"
VARIANTS = ["Shooter", "Artillery", "Spitter", "ShockPillar"]


def log(message):
    unreal.log("[EnemyProjectileVariants] {}".format(message))


def main():
    base_class = unreal.load_class(None, BASE_PATH + ".BP_EnemyProjectile_C")
    if base_class is None:
        raise RuntimeError("base blueprint missing: {}".format(BASE_PATH))

    for suffix in VARIANTS:
        name = "BP_EnemyProjectile_{}".format(suffix)
        asset_path = "{}/{}".format(DEST, name)
        if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
            log("already exists: {}".format(asset_path))
            continue

        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", base_class)
        blueprint = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            name, DEST, unreal.Blueprint, factory)
        if blueprint is None:
            raise RuntimeError("failed to create {}".format(asset_path))

        unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
        unreal.EditorAssetLibrary.save_asset(asset_path, False)
        log("created {}".format(asset_path))


main()
