"""Create the main-menu map + WBP assets for the Baka! project.

Usage (project root):
  UnrealEditor-Cmd.exe Baka.uproject -run=pythonscript -script="Tools/create_main_menu.py" \
      -unattended -nopause -nullrhi -nosplash
"""
import json
import unreal

MENU_DIR = "/Game/UMG/Menus/MainMenu"
MAP_PATH = "/Game/Map/MainMenu"
RESULT = {}


def log(message):
    unreal.log("[MainMenuSetup] {}".format(message))


def ensure_dir(path):
    if not unreal.EditorAssetLibrary.does_directory_exist(path):
        unreal.EditorAssetLibrary.make_directory(path)
        log("created dir {}".format(path))


def create_or_load_wbp(name, parent_class):
    path = "{}/{}".format(MENU_DIR, name)
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        log("rebuilding existing {}".format(path))
        return unreal.EditorAssetLibrary.load_asset(path)

    factory = unreal.WidgetBlueprintFactory()
    factory.set_editor_property("parent_class", parent_class)
    widget = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        name, MENU_DIR, unreal.WidgetBlueprint, factory)
    if widget is None:
        raise RuntimeError("failed to create {}".format(path))
    log("created {}".format(path))
    return widget


def create_widgets():
    ensure_dir(MENU_DIR)

    main = create_or_load_wbp("WBP_MainMenu", unreal.RogueMainMenuWidgetBase)
    RESULT["main_build"] = bool(unreal.RogueWidgetBuilder.build_main_menu(main))
    RESULT["main_tree"] = unreal.RogueWidgetBuilder.describe_widget_tree(main)

    tutorial = create_or_load_wbp("WBP_Tutorial", unreal.RogueTutorialWidgetBase)
    RESULT["tutorial_build"] = bool(unreal.RogueWidgetBuilder.build_tutorial(tutorial))
    RESULT["tutorial_tree"] = unreal.RogueWidgetBuilder.describe_widget_tree(tutorial)

    unreal.EditorAssetLibrary.save_directory(MENU_DIR, only_if_is_dirty=True, recursive=True)


def make_map():
    if unreal.EditorAssetLibrary.does_asset_exist(MAP_PATH):
        log("map exists, reloading {}".format(MAP_PATH))
        world = unreal.EditorLoadingAndSavingUtils.load_map(MAP_PATH)
    else:
        world = None
        try:
            world = unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
            log("new_blank_map OK")
        except Exception as exc:  # noqa: BLE001
            RESULT["new_blank_map_error"] = str(exc)

        if world is None:
            try:
                created = unreal.EditorLevelLibrary.new_level(MAP_PATH)
                RESULT["new_level"] = bool(created)
                world = unreal.EditorLevelLibrary.get_editor_world()
            except Exception as exc:  # noqa: BLE001
                RESULT["new_level_error"] = str(exc)

    if world is None:
        raise RuntimeError("could not create world")

    settings = world.get_world_settings()
    try:
        settings.set_editor_property("default_game_mode", unreal.RogueMainMenuGameMode)
        RESULT["game_mode_set"] = True
    except Exception as exc:  # noqa: BLE001
        RESULT["game_mode_error"] = str(exc)

    try:
        RESULT["game_mode_readback"] = str(settings.get_editor_property("default_game_mode"))
    except Exception as exc:  # noqa: BLE001
        RESULT["game_mode_readback"] = "ERR: {}".format(exc)

    saved = unreal.EditorLoadingAndSavingUtils.save_map(world, MAP_PATH)
    RESULT["map_saved"] = bool(saved)
    RESULT["map_exists"] = bool(unreal.EditorAssetLibrary.does_asset_exist(MAP_PATH))


def main():
    try:
        create_widgets()
        make_map()
        RESULT["ok"] = True
    except Exception as exc:  # noqa: BLE001
        import traceback
        RESULT["ok"] = False
        RESULT["fatal"] = str(exc)
        RESULT["traceback"] = traceback.format_exc()

    text = json.dumps(RESULT, ensure_ascii=False, default=str)
    print("MAINMENU_SETUP " + text)
    try:
        with open(r"F:\AI_WorkSpeace\baka\Saved\DataDump\main_menu_setup.json", "w", encoding="utf-8") as handle:
            handle.write(text)
    except Exception:
        pass


main()
