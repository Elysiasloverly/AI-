"""生成受伤泛红占位 WBP：WBP_DamageFlash。

用法（项目根目录）：
    UnrealEditor-Cmd.exe F:\\AI_WorkSpeace\\ai\\Baka.uproject ^
        -run=pythonscript -script="Tools/create_damage_flash_widget.py" ^
        -unattended -nopause -nullrhi -nosplash

说明：
- 资产建在 /Game/UMG/HUD/WBP_DamageFlash，父类 URogueDamageFlashWidgetBase；
- 控件树由 aiEditor 模块的 URogueWidgetBuilder 用 C++ 建（Python 读不到 protected 的 WidgetTree）；
- 全屏容器的名字是 FlashRoot，与 C++ 的可选绑定一致，所以不写事件图也有闪烁与淡出；
- 重复执行会重建控件树，不会重复创建资产 —— 你在 WBP 里改的配色会被覆盖，
  所以做外观优化时请把本脚本的调用去掉，别在改完之后再跑一次。
"""
import unreal

DEST = "/Game/UMG/HUD"
ASSET_NAME = "WBP_DamageFlash"


def log(message):
    unreal.log("[DamageFlash] {}".format(message))


def ensure_destination():
    if not unreal.EditorAssetLibrary.does_directory_exist(DEST):
        unreal.EditorAssetLibrary.make_directory(DEST)
        log("created {}".format(DEST))


def create_or_load():
    asset_path = "{}/{}".format(DEST, ASSET_NAME)
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        log("rebuilding existing {}".format(asset_path))
        return unreal.EditorAssetLibrary.load_asset(asset_path)

    factory = unreal.WidgetBlueprintFactory()
    factory.set_editor_property("parent_class", unreal.RogueDamageFlashWidgetBase)
    widget = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        ASSET_NAME, DEST, unreal.WidgetBlueprint, factory)
    if widget is None:
        raise RuntimeError("failed to create {}".format(asset_path))
    log("created {}".format(asset_path))
    return widget


def main():
    ensure_destination()

    widget = create_or_load()
    if not unreal.RogueWidgetBuilder.build_damage_flash(widget):
        raise RuntimeError("build_damage_flash returned False")

    unreal.BlueprintEditorLibrary.compile_blueprint(widget)
    unreal.EditorAssetLibrary.save_asset("{}/{}".format(DEST, ASSET_NAME), False)
    log("saved {}/{}".format(DEST, ASSET_NAME))
    log(unreal.RogueWidgetBuilder.describe_widget_tree(widget))


main()
