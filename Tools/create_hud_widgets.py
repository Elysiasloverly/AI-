"""生成 HUD 占位 WBP：玩家状态条 + 敌人血条。

用法（项目根目录）：
    UnrealEditor-Cmd.exe F:\AI_WorkSpeace\ai\ai.uproject ^
        -run=pythonscript -script="Tools/create_hud_widgets.py" ^
        -unattended -nopause -nullrhi -nosplash

说明：
- 资产建在 /Game/UMG/HUD/WBP_PlayerVitals 与 /Game/UMG/HUD/WBP_EnemyHealthBar；
- 控件树由 aiEditor 模块的 URogueWidgetBuilder 用 C++ 建（Python 读不到 protected 的 WidgetTree）；
- 三个 ProgressBar 命名为 HealthBar / ArmorBar / ExperienceBar，敌人血条的叫 FillBar，
  与 C++ 的可选绑定一致，所以不写事件图也能跑；
- 重复执行会重建已有资产的控件树，不会重复创建资产。
"""
import unreal

DEST = "/Game/UMG/HUD"


def log(message):
    unreal.log("[HUDWidgets] {}".format(message))


def ensure_destination():
    if not unreal.EditorAssetLibrary.does_directory_exist(DEST):
        unreal.EditorAssetLibrary.make_directory(DEST)
        log("created {}".format(DEST))


def create_or_load(name, parent_class):
    asset_path = "{}/{}".format(DEST, name)
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        log("rebuilding existing {}".format(asset_path))
        return unreal.EditorAssetLibrary.load_asset(asset_path)

    factory = unreal.WidgetBlueprintFactory()
    factory.set_editor_property("parent_class", parent_class)
    widget = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        name, DEST, unreal.WidgetBlueprint, factory)
    if widget is None:
        raise RuntimeError("failed to create {}".format(asset_path))
    log("created {}".format(asset_path))
    return widget


def build(name, parent_class, builder):
    widget = create_or_load(name, parent_class)
    if not builder(widget):
        raise RuntimeError("builder returned False for {}".format(name))

    unreal.BlueprintEditorLibrary.compile_blueprint(widget)
    asset_path = "{}/{}".format(DEST, name)
    unreal.EditorAssetLibrary.save_asset(asset_path, False)
    log("saved {}".format(asset_path))
    log(unreal.RogueWidgetBuilder.describe_widget_tree(widget))


def main():
    ensure_destination()

    build("WBP_PlayerVitals", unreal.RogueVitalsWidgetBase,
          unreal.RogueWidgetBuilder.build_player_vitals)
    build("WBP_EnemyHealthBar", unreal.RogueEnemyHealthBarWidgetBase,
          unreal.RogueWidgetBuilder.build_enemy_health_bar)

    log("done: 2 widget blueprints ready under {}".format(DEST))


main()
