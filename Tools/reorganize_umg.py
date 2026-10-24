"""Reorganize /Game/UMG into HUD / Menus{Common,InGame,Shop,Upgrade,MainMenu}."""
import json
import unreal

MOVES = [
    ("/Game/UMG/Menus/WBP_SettingsMenu", "/Game/UMG/Menus/Common/WBP_SettingsMenu"),
    ("/Game/UMG/Menus/WBP_PauseMenu", "/Game/UMG/Menus/InGame/WBP_PauseMenu"),
    ("/Game/UMG/Menus/WBP_DeathScreen", "/Game/UMG/Menus/InGame/WBP_DeathScreen"),
    ("/Game/UMG/Menus/WBP_ResurrectionScreen", "/Game/UMG/Menus/InGame/WBP_ResurrectionScreen"),
    ("/Game/UMG/Menus/WBP_Shop", "/Game/UMG/Menus/Shop/WBP_Shop"),
    ("/Game/UMG/Menus/WBP_ShopOfferCard", "/Game/UMG/Menus/Shop/WBP_ShopOfferCard"),
    ("/Game/UMG/Menus/WBP_UpgradeSelection", "/Game/UMG/Menus/Upgrade/WBP_UpgradeSelection"),
    ("/Game/UMG/Menus/WBP_UpgradeCard", "/Game/UMG/Menus/Upgrade/WBP_UpgradeCard"),
    ("/Game/UMG/Menus/WBP_MainMenu", "/Game/UMG/Menus/MainMenu/WBP_MainMenu"),
    ("/Game/UMG/Menus/WBP_Tutorial", "/Game/UMG/Menus/MainMenu/WBP_Tutorial"),
]

RESULT = {"moved": [], "failed": [], "still_old": [], "bp_refs": {}}


def main():
    for old, new in MOVES:
        if unreal.EditorAssetLibrary.does_asset_exist(new):
            RESULT["moved"].append([old, new, "already"])
            continue
        ok = bool(unreal.RogueWidgetBuilder.move_asset(old, new))
        exists_new = bool(unreal.EditorAssetLibrary.does_asset_exist(new))
        exists_old = bool(unreal.EditorAssetLibrary.does_asset_exist(old))
        if ok and exists_new and not exists_old:
            RESULT["moved"].append([old, new, "ok"])
        else:
            RESULT["failed"].append([old, new, ok, exists_new, exists_old])
        if exists_old:
            RESULT["still_old"].append(old)

    unreal.EditorAssetLibrary.save_directory("/Game/UMG", only_if_is_dirty=True, recursive=True)

    # 读 BP_RogueHUD 的 WidgetClass，确认引用已跟到新路径。
    bp = unreal.EditorAssetLibrary.load_asset("/Game/BP/Framework/BP_RogueHUD")
    if bp:
        cdo = unreal.get_default_object(bp.generated_class())
        for prop in dir(cdo):
            if "widget_class" in prop.lower() or "offer_card" in prop.lower():
                try:
                    value = cdo.get_editor_property(prop)
                    if value:
                        RESULT["bp_refs"][prop] = str(value)
                except Exception:
                    pass

    print("UMG_REORG " + json.dumps(RESULT, ensure_ascii=False, default=str))


main()
