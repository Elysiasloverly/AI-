"""Apply the project composite font to every TextBlock in /Game/UMG widget blueprints."""
import json
import unreal

RESULT = {"applied": [], "skipped": [], "errors": []}


def main():
    font = unreal.RogueWidgetBuilder.get_or_create_project_font()
    RESULT["font"] = str(font) if font else None
    if font is None:
        print("FONT_APPLY " + json.dumps(RESULT, ensure_ascii=False))
        return

    unreal.EditorAssetLibrary.save_loaded_asset(font)

    paths = unreal.EditorAssetLibrary.list_assets("/Game/UMG", recursive=True, include_folder=False)
    for path in paths:
        obj = unreal.EditorAssetLibrary.load_asset(path)
        if not isinstance(obj, unreal.WidgetBlueprint):
            continue
        try:
            changed = bool(unreal.RogueWidgetBuilder.apply_project_font(obj))
            unreal.EditorAssetLibrary.save_loaded_asset(obj)
            if changed:
                RESULT["applied"].append(path)
            else:
                RESULT["skipped"].append(path)
        except Exception as exc:  # noqa: BLE001
            RESULT["errors"].append("{}: {}".format(path, exc))

    print("FONT_APPLY " + json.dumps(RESULT, ensure_ascii=False))


main()
