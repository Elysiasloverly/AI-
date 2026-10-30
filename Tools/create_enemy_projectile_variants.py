"""为不同敌人类型创建弹体蓝图变体。

用法（项目根目录）：
    UnrealEditor-Cmd.exe F:\AI_WorkSpeace\baka\Baka.uproject ^
        -run=pythonscript -script="Tools/create_enemy_projectile_variants.py" ^
        -unattended -nopause -nullrhi -nosplash

产物：16种攻击敌人的独立弹体，放入各敌人的同名文件夹；复制Common模板，各自直接继承原生弹体类。

已存在时只跳过，不覆盖你改过的外观。
"""
import unreal

DEST = "/Game/BP/Enemy"
BASE_PATH = DEST + "/Common/BP_EnemyProjectile"
VARIANTS = ['Rifleman', 'Assault', 'Juggernaut', 'Orbiter', 'Marksman', 'Skirmisher', 'Cannoneer', 'Scatterer', 'ShockPillar', 'Sniper', 'Shotgunner', 'Repeater', 'Ringcaster', 'SkyMoth', 'WindReaver', 'StarLantern']


def log(message):
    unreal.log("[EnemyProjectileVariants] {}".format(message))


def main():
    base_class = unreal.load_class(None, BASE_PATH + ".BP_EnemyProjectile_C")
    if base_class is None:
        raise RuntimeError("base blueprint missing: {}".format(BASE_PATH))

    for suffix in VARIANTS:
        name = "BP_EnemyProjectile_{}".format(suffix)
        folder = DEST + "/" + suffix
        asset_path = "{}/{}".format(folder, name)
        if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
            log("already exists: {}".format(asset_path))
            continue

        # Copy the standalone template so each variant owns editable components.
        blueprint = unreal.EditorAssetLibrary.duplicate_asset(BASE_PATH, asset_path)
        if blueprint is None:
            raise RuntimeError("failed to create {}".format(asset_path))

        unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
        unreal.EditorAssetLibrary.save_asset(asset_path, False)
        log("created {}".format(asset_path))


main()
