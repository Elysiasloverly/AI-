"""给敌人弹体基础蓝图添加一套可删除的占位视觉组件。

用法（项目根目录）：
    UnrealEditor-Cmd.exe F:\AI_WorkSpeace\ai\ai.uproject ^
        -run=pythonscript -script="Tools/create_projectile_visual_template.py" ^
        -unattended -nopause -nullrhi -nosplash

产物：在 BP_EnemyProjectile 里新增 BodyMesh / CoreMesh / GlowMesh / TrailMesh / GlowLight。
这些是蓝图 SCS 组件，可以在组件面板里直接删除或替换。
已存在同名组件时不会重复添加。
"""
import unreal

BLUEPRINT = "/Game/BP/Enemy/Projectiles/BP_EnemyProjectile"


def log(message):
    unreal.log("[ProjectileVisualTemplate] {}".format(message))


def main():
    bp = unreal.EditorAssetLibrary.load_asset(BLUEPRINT)
    if bp is None:
        raise RuntimeError("missing blueprint {}".format(BLUEPRINT))

    if not unreal.RogueProjectileVisualBuilder.build_template(bp):
        raise RuntimeError("build_template failed")

    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    unreal.EditorAssetLibrary.save_asset(BLUEPRINT, False)

    log("template built: {}".format(BLUEPRINT))
    log(unreal.RogueProjectileVisualBuilder.describe_components(bp))


main()
