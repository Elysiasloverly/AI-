"""整圈弹幕是 48 发/圈，去掉每发的点光和拖尾，避免模板默认拖性能。

用法（项目根目录）：
    UnrealEditor-Cmd.exe F:\AI_WorkSpeace\ai\ai.uproject ^
        -run=pythonscript -script="Tools/trim_ring_projectile_template.py" ^
        -unattended -nopause -nullrhi -nosplash
"""
import unreal

BLUEPRINT = "/Game/BP/Enemy/Projectiles/BP_EnemyProjectile_ShockPillar"
REMOVE = ["GlowLight", "TrailMesh"]


def log(message):
    unreal.log("[TrimRingProjectile] {}".format(message))


def main():
    bp = unreal.EditorAssetLibrary.load_asset(BLUEPRINT)
    if bp is None:
        raise RuntimeError("missing blueprint {}".format(BLUEPRINT))

    for name in REMOVE:
        removed = unreal.RogueProjectileVisualBuilder.remove_component(bp, name)
        log("{} remove {} -> {}".format(BLUEPRINT, name, removed))

    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    unreal.EditorAssetLibrary.save_asset(BLUEPRINT, False)
    log("final components:\n{}".format(unreal.RogueProjectileVisualBuilder.describe_components(bp)))


main()
