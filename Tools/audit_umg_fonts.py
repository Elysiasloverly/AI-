import unreal

paths = unreal.EditorAssetLibrary.list_assets("/Game/UMG", recursive=True, include_folder=False)
bad = []
checked = 0
for path in paths:
    obj = unreal.EditorAssetLibrary.load_asset(path)
    if not isinstance(obj, unreal.WidgetBlueprint):
        continue
    checked += 1
    fonts = unreal.RogueWidgetBuilder.describe_text_fonts(obj)
    for line in fonts.splitlines():
        if "F_DroidMono_CJK" not in line:
            bad.append("{}: {}".format(path, line))

print("FONT_AUDIT checked", checked, "bad", len(bad))
for b in bad:
    print("BAD", b)
