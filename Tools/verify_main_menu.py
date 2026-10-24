"""Verify the main-menu map and widgets after creation."""
import json
import unreal

OUT = {}


def asset(path):
    return unreal.EditorAssetLibrary.load_asset(path)


def describe(path):
    obj = asset(path)
    if obj is None:
        return {"path": path, "exists": False}
    info = {
        "path": path,
        "exists": True,
        "class": obj.get_class().get_name(),
    }
    try:
        gen = obj.generated_class()
        if gen:
            info["generated_class"] = gen.get_name()
            super_class = gen.get_super_class()
            info["super_class"] = super_class.get_name() if super_class else "<none>"
    except Exception as exc:  # noqa: BLE001
        info["generated_error"] = str(exc)
    try:
        info["tree"] = unreal.RogueWidgetBuilder.describe_widget_tree(obj)
    except Exception as exc:  # noqa: BLE001
        info["tree_error"] = str(exc)
    return info


def main():
    OUT["main_menu"] = describe("/Game/UMG/Menus/MainMenu/WBP_MainMenu")
    OUT["tutorial"] = describe("/Game/UMG/Menus/MainMenu/WBP_Tutorial")
    OUT["settings"] = describe("/Game/UMG/Menus/Common/WBP_SettingsMenu")

    try:
        OUT["game_mode_class"] = str(unreal.load_class(None, "/Script/ai.RogueMainMenuGameMode"))
        gm_cdo = unreal.get_default_object(unreal.RogueMainMenuGameMode)
        OUT["hud_class"] = str(gm_cdo.get_editor_property("hud_class"))
        OUT["default_pawn_class"] = str(gm_cdo.get_editor_property("default_pawn_class"))
    except Exception as exc:  # noqa: BLE001
        OUT["game_mode_error"] = str(exc)

    try:
        world = unreal.EditorLoadingAndSavingUtils.load_map("/Game/Map/MainMenu")
        settings = world.get_world_settings()
        OUT["map_loaded"] = world is not None
        OUT["map_game_mode"] = str(settings.get_editor_property("default_game_mode"))
    except Exception as exc:  # noqa: BLE001
        OUT["map_error"] = str(exc)

    try:
        hud_cdo = unreal.get_default_object(unreal.RogueHUD)
        for key in ["main_menu_widget_class_path", "tutorial_widget_class_path",
                    "settings_menu_widget_class_path", "single_player_map_path"]:
            OUT[key] = str(hud_cdo.get_editor_property(key))
    except Exception as exc:  # noqa: BLE001
        OUT["hud_config_error"] = str(exc)

    print("MAINMENU_VERIFY " + json.dumps(OUT, ensure_ascii=False, default=str))


main()
