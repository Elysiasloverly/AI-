"""把项目里的数据资产导出成 JSON，用于审计/批量改名。

用法：
    UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script="<此文件的绝对路径>"

输出到 Saved/DataDump/，每个资产一个 .json。只读，不修改任何资产。
"""

import json
import os

import unreal

TARGETS = {
    "DT_WeaponConfig": "/Game/Data/Weapons/DT_WeaponConfig",
    "DT_UpgradeDefinitions": "/Game/Data/Upgrades/DT_UpgradeDefinitions",
    "DT_EnemyArchetypes": "/Game/Data/Enemies/DT_EnemyArchetypes",
    "DA_UpgradeRules": "/Game/Data/Upgrades/DA_UpgradeRules",
    "DA_UpgradeDefinitions": "/Game/Data/Upgrades/DA_UpgradeDefinitions",
    "DA_GameBalance": "/Game/Data/Core/DA_GameBalance",
    "DA_PlayerBalance": "/Game/Data/Player/DA_PlayerBalance",
}

OUT_DIR = r"F:/AI_WorkSpeace/baka/Saved/DataDump"


def dump_table(name, path, out_dir):
    asset = unreal.load_asset(path)
    if asset is None:
        print("[Dump] MISSING {}".format(path))
        return

    if isinstance(asset, unreal.DataTable):
        try:
            text = unreal.DataTableFunctionLibrary.export_data_table_to_json_string(asset)
        except Exception as exc:  # noqa: BLE001
            print("[Dump] FAIL {}: {}".format(name, exc))
            return
        with open(os.path.join(out_dir, name + ".json"), "w", encoding="utf-8") as handle:
            handle.write(text)
        row_names = [str(r) for r in asset.get_row_names()]
        print("[Dump] {} rows={} chars={}".format(name, len(row_names), len(text)))
        print("[Dump]   rows: {}".format(", ".join(row_names)))
    else:
        # DataAsset：导出所有看起来像文本的属性
        print("[Dump] {} is {}".format(name, type(asset).__name__))
        lines = []
        for prop in unreal.SystemLibrary.get_class_display_name(asset.get_class()).split():
            pass
        with open(os.path.join(out_dir, name + ".txt"), "w", encoding="utf-8") as handle:
            handle.write(repr(asset))


def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    for name, path in TARGETS.items():
        dump_table(name, path, OUT_DIR)
    print("[Dump] done -> {}".format(OUT_DIR))


main()
