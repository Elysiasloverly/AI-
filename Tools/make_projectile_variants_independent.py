"""把敌人弹体变体改为直接继承 C++ 类，并各自生成可删除的占位组件。

用法（项目根目录）：
    UnrealEditor-Cmd.exe F:\AI_WorkSpeace\ai\ai.uproject ^
        -run=pythonscript -script="Tools/make_projectile_variants_independent.py" ^
        -unattended -nopause -nullrhi -nosplash

原因：如果变体继承 BP_EnemyProjectile，父蓝图里的 SCS 组件在子蓝图里只能隐藏、不能删除。
改为直接继承 ARogueEnemyProjectile 后，每个变体都有自己的一套 Body/Core/Glow/Trail/GlowLight，
可以单独删除/替换。
"""
import unreal

DEST = "/Game/BP/Enemy/Projectiles"
VARIANTS = [
    "BP_EnemyProjectile_Boss",
    "BP_EnemyProjectile_Shooter",
    "BP_EnemyProjectile_Artillery",
    "BP_EnemyProjectile_Spitter",
    "BP_EnemyProjectile_ShockPillar",
]


def log(message):
    unreal.log("[ProjectileVariantsIndependent] {}".format(message))


def main():
    for name in VARIANTS:
        path = "{}/{}".format(DEST, name)
        bp = unreal.EditorAssetLibrary.load_asset(path)
        if bp is None:
            log("missing: {}".format(path))
            continue

        reparented = unreal.BlueprintEditorLibrary.reparent_blueprint(bp, unreal.RogueEnemyProjectile)
        log("{} reparent -> {}".format(name, reparented))

        built = unreal.RogueProjectileVisualBuilder.build_template(bp)
        log("{} template -> {}".format(name, built))

        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        unreal.EditorAssetLibrary.save_asset(path, False)
        log("{} components:\n{}".format(name, unreal.RogueProjectileVisualBuilder.describe_components(bp)))


main()
