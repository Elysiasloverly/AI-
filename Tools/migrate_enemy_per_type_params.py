"""把 DT_EnemyArchetypes 的专属参数从"7 个共享块"迁到"13 个一怪一块"。

用法（项目根目录）：
    UnrealEditor-Cmd.exe F:\\AI_WorkSpeace\\ai\\Baka.uproject ^
        -run=pythonscript -script="Tools/migrate_enemy_per_type_params.py" ^
        -unattended -nopause -nullrhi -nosplash

说明：
- 前置：源码已完成一怪一块改造并编译；
- 迁移规则见 enemy_params_migration_lib.py 顶部注释；
- 改前把原 uasset 备份到 Saved/Backup/DT_EnemyArchetypes_before_per_type_params.uasset；
- 迁移后重新导出 JSON 做校验：四类"族员"行（月兔/血蝠/毒茸/亡灵）的专属块必须与源块一致；
- 重复执行安全：第二次跑时旧 key 已不存在，只会报告"无需迁移"。
"""
import json
import os
import shutil
import sys

import unreal

TOOLS_DIR = os.path.dirname(os.path.abspath(__file__))
if TOOLS_DIR not in sys.path:
    sys.path.insert(0, TOOLS_DIR)

import enemy_params_migration_lib as migration  # noqa: E402

TABLE_PATH = "/Game/Data/Enemies/DT_EnemyArchetypes"
TABLE_FILE = r"F:/AI_WorkSpeace/baka/Content/Data/Enemies/DT_EnemyArchetypes.uasset"
BACKUP_DIR = r"F:/AI_WorkSpeace/baka/Saved/Backup"
BACKUP_FILE = os.path.join(BACKUP_DIR, "DT_EnemyArchetypes_before_per_type_params.uasset")
VERIFY_DUMP = r"F:/AI_WorkSpeace/baka/Saved/DataDump/DT_EnemyArchetypes.after_migration.json"

VERIFY_RULES = {
    # EnemyType -> (目标块, 源块, 需要一致的 key；None = 整块一致)
    "Sniper": ("Sniper", "Marksman", None),
    "Repeater": ("Repeater", "Marksman", None),
    "Shotgunner": ("Shotgunner", "Scatterer", None),
    "Ringcaster": ("Ringcaster", "ShockPillar",
                   ["InitialCooldownMin", "InitialCooldownMax", "AttackCooldown", "BulletRing"]),
}


def log(message):
    unreal.log("[MigrateEnemyParams] {}".format(message))


def backup_table():
    if os.path.exists(BACKUP_FILE):
        log("backup exists: {}".format(BACKUP_FILE))
        return
    os.makedirs(BACKUP_DIR, exist_ok=True)
    shutil.copy2(TABLE_FILE, BACKUP_FILE)
    log("backup: {}".format(BACKUP_FILE))


def needs_migration(rows):
    """旧结构加载时 CoreRedirects 已经把共享块改名落位，所以不能用"是否还有旧 key"判断。
    改为检查族员行（月兔/血蝠/毒茸/亡灵）的专属块是否已经继承源块。"""
    for row in rows:
        enemy_type = str(row.get("EnemyType", "")).split("::")[-1]
        rule = VERIFY_RULES.get(enemy_type)
        if rule is None:
            continue
        target_key, source_key, keys = rule
        special = str(row.get("SpecialParams", ""))
        source = migration.block_values(special, source_key)
        target = migration.block_values(special, target_key)
        if source is None:
            continue
        if target is None:
            return True
        for key in (keys if keys is not None else source.keys()):
            if source.get(key) != target.get(key):
                return True
    return False


def main():
    table = unreal.load_asset(TABLE_PATH)
    if table is None:
        raise RuntimeError("无法加载 {} —— 检查 CoreRedirects 是否生效（旧结构与新结构之间需要属性重定向）".format(TABLE_PATH))

    rows = json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))
    log("loaded rows: {}".format(len(rows)))

    if not needs_migration(rows):
        log("族员块已与源块一致，可能已经迁移过 —— 退出")
        return

    backup_table()

    migrated_rows, report = migration.migrate_rows(rows)
    for line in report:
        log(line)

    text = json.dumps(migrated_rows, ensure_ascii=False, indent="\t")
    if not unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(table, text):
        raise RuntimeError("fill_data_table_from_json_string failed")

    unreal.EditorAssetLibrary.save_asset(TABLE_PATH, False)
    log("saved")

    # ===== 校验：重新导出，逐行核对族员块与源块一致 =====
    verify_rows = json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))
    with open(VERIFY_DUMP, "w", encoding="utf-8") as handle:
        handle.write(json.dumps(verify_rows, ensure_ascii=False, indent="\t"))

    failures = []
    for row in verify_rows:
        enemy_type = str(row.get("EnemyType", "")).split("::")[-1]
        special = row.get("SpecialParams", "")
        leftover = [key for key in ("Charger=", "Shooter=", "Artillery=", "Spitter=") if key in special]
        if leftover:
            failures.append("{}: 残留旧 key {}".format(enemy_type, leftover))

        rule = VERIFY_RULES.get(enemy_type)
        if rule is None:
            continue
        target_key, source_key, keys = rule
        source = migration.block_values(special, source_key)
        target = migration.block_values(special, target_key)
        if source is None or target is None:
            failures.append("{}: 缺少 {} 或 {} 块".format(enemy_type, target_key, source_key))
            continue
        for key in (keys if keys is not None else source.keys()):
            if source.get(key) != target.get(key):
                failures.append("{}: {}.{} 不一致 ({} != {})".format(
                    enemy_type, target_key, key, target.get(key), source.get(key)))

    if failures:
        for failure in failures:
            unreal.log_error("[MigrateEnemyParams] VERIFY FAIL: {}".format(failure))
        raise RuntimeError("迁移校验失败，详见日志")

    log("verify OK -> {}".format(VERIFY_DUMP))


main()
