"""Create the editable upgrade-tree WBP assets without rebuilding the main menu.

Run with the editor closed:
  UnrealEditor-Cmd.exe Baka.uproject -run=pythonscript -script=Tools/create_upgrade_tree.py
      -unattended -nullrhi -nosound

Existing upgrade-tree widgets are kept. Add -RogueRebuildUpgradeTreeWidgets only
when intentionally replacing their authored Designer layout with these defaults.
The existing main-menu tree is never cleared; only its SkillTreeButton caption is
updated. No map is opened or saved by this script.
"""

import json
import unreal


WIDGET_DIR = "/Game/UMG/Menus/Progression"
DEFINITION_PATH = "/Game/Data/Progression/DA_UpgradeTree"
MAIN_MENU_PATH = "/Game/UMG/Menus/MainMenu/WBP_MainMenu"


def require(value, message):
    if not value:
        raise RuntimeError(message)
    return value


def create_widget(name, parent_class, build_method, rebuild):
    path = WIDGET_DIR + "/" + name
    exists = unreal.EditorAssetLibrary.does_asset_exist(path)
    if exists:
        widget = require(unreal.EditorAssetLibrary.load_asset(path), "Cannot load " + path)
        generated = require(widget.generated_class(), "Missing generated class: " + path)
        # Parentage must match the native bindings; never silently reparent user assets.
        require(isinstance(unreal.get_default_object(generated), parent_class),
                "Existing widget has the wrong native parent: " + path)
    else:
        factory = unreal.WidgetBlueprintFactory()
        factory.set_editor_property("parent_class", parent_class)
        widget = require(unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            name, WIDGET_DIR, unreal.WidgetBlueprint, factory), "Cannot create " + path)

    if not exists or rebuild:
        require(build_method(widget), "Builder failed: " + path)
    require(widget.generated_class(), "Widget failed to compile: " + path)
    return widget, not exists or rebuild


def create_definition():
    if unreal.EditorAssetLibrary.does_asset_exist(DEFINITION_PATH):
        return require(unreal.EditorAssetLibrary.load_asset(DEFINITION_PATH), "Cannot load upgrade-tree data")
    directory, name = DEFINITION_PATH.rsplit("/", 1)
    unreal.EditorAssetLibrary.make_directory(directory)
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.RogueUpgradeTreeDefinition)
    definition = require(unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        name, directory, unreal.RogueUpgradeTreeDefinition, factory), "Cannot create upgrade-tree data")
    require(unreal.RogueEditorAssetUtils.save_asset(definition), "Cannot save upgrade-tree data")
    return definition


def main():
    rebuild = "RogueRebuildUpgradeTreeWidgets" in unreal.SystemLibrary.get_command_line()
    unreal.EditorAssetLibrary.make_directory(WIDGET_DIR)
    definition = create_definition()
    node, built_node = create_widget("WBP_UpgradeTreeNode", unreal.RogueUpgradeTreeNodeWidgetBase,
                                    unreal.RogueUpgradeTreeBuilder.build_upgrade_tree_node, rebuild)
    tree, built_tree = create_widget("WBP_UpgradeTree", unreal.RogueUpgradeTreeWidgetBase,
                                    unreal.RogueUpgradeTreeBuilder.build_upgrade_tree, rebuild)
    tree_cdo = unreal.get_default_object(tree.generated_class())
    # Only initialize the class reference for a newly built page. Repeated runs retain
    # an author's later replacement node class together with the edited WBP layout.
    if built_tree:
        tree_cdo.set_editor_property("node_widget_class", node.generated_class())
    require(unreal.RogueEditorAssetUtils.save_asset(node), "Cannot save node WBP")
    require(unreal.RogueEditorAssetUtils.save_asset(tree), "Cannot save tree WBP")

    menu = require(unreal.EditorAssetLibrary.load_asset(MAIN_MENU_PATH), "Main-menu WBP not found")
    menu_before = unreal.RogueWidgetBuilder.describe_widget_tree(menu)
    require(unreal.RogueUpgradeTreeBuilder.update_main_menu_entry(menu), "SkillTreeButton text not found")
    menu_after = unreal.RogueWidgetBuilder.describe_widget_tree(menu)
    require(menu_before == menu_after, "Main-menu widget structure changed unexpectedly")
    require(unreal.RogueEditorAssetUtils.save_asset(menu), "Cannot save main-menu entry caption")
    font = unreal.EditorAssetLibrary.load_asset("/Game/UMG/Fonts/F_DroidMono_CJK")
    if font:
        require(unreal.EditorAssetLibrary.save_loaded_asset(font, only_if_is_dirty=True), "Cannot save project font")

    result = {
        "ok": True,
        "tree": tree.get_path_name(),
        "node": node.get_path_name(),
        "definition": definition.get_path_name(),
        "tree_built": built_tree,
        "node_built": built_node,
        "main_menu_structure_preserved": menu_before == menu_after,
        "tree_widgets": unreal.RogueWidgetBuilder.describe_widget_tree(tree),
        "node_widgets": unreal.RogueWidgetBuilder.describe_widget_tree(node),
        "tree_fonts": unreal.RogueWidgetBuilder.describe_text_fonts(tree),
        "node_fonts": unreal.RogueWidgetBuilder.describe_text_fonts(node),
        "node_class": str(tree_cdo.get_editor_property("node_widget_class")),
        "node_count": len(definition.get_editor_property("nodes")),
    }
    print("ROGUE_UPGRADE_TREE_CREATED " + json.dumps(result, ensure_ascii=False))


main()
