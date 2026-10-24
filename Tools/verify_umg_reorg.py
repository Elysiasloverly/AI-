import unreal

PATHS = [
    "/Game/UMG/Menus/MainMenu/WBP_MainMenu",
    "/Game/UMG/Menus/MainMenu/WBP_Tutorial",
    "/Game/UMG/Menus/Common/WBP_SettingsMenu",
    "/Game/UMG/Menus/InGame/WBP_PauseMenu",
    "/Game/UMG/Menus/InGame/WBP_DeathScreen",
    "/Game/UMG/Menus/InGame/WBP_ResurrectionScreen",
    "/Game/UMG/Menus/Shop/WBP_Shop",
    "/Game/UMG/Menus/Shop/WBP_ShopOfferCard",
    "/Game/UMG/Menus/Upgrade/WBP_UpgradeSelection",
    "/Game/UMG/Menus/Upgrade/WBP_UpgradeCard",
    "/Game/UMG/HUD/WBP_PlayerVitals",
    "/Game/UMG/HUD/WBP_EnemyHealthBar",
    "/Game/UMG/HUD/WBP_DamageFlash",
    "/Game/UMG/Fonts/F_DroidMono_CJK",
]

for path in PATHS:
    data = unreal.EditorAssetLibrary.find_asset_data(path)
    print("LOAD", path, "exists", bool(data), "class", data.asset_class_path.asset_name if data else None)

bp = unreal.EditorAssetLibrary.load_asset("/Game/BP/Framework/BP_RogueHUD")
if bp:
    cdo = unreal.get_default_object(bp.generated_class())
    for prop in ["shop_widget_class", "upgrade_selection_widget_class", "settings_menu_widget_class",
                 "death_screen_widget_class", "revive_countdown_widget_class"]:
        try:
            print("REF", prop, "=", cdo.get_editor_property(prop))
        except Exception as exc:
            print("REF_ERR", prop, exc)

print("REDIRECTORS:")
for path in unreal.EditorAssetLibrary.list_assets("/Game/UMG", recursive=True, include_folder=False):
    data = unreal.EditorAssetLibrary.find_asset_data(path)
    if data and data.asset_class_path.asset_name == "ObjectRedirector":
        print("  ", path)
