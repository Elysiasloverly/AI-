"""把不同敌人类型的弹体蓝图写进 DT_EnemyArchetypes。

用法（项目根目录）：
    UnrealEditor-Cmd.exe F:\AI_WorkSpeace\baka\Baka.uproject ^
        -run=pythonscript -script="Tools/assign_enemy_projectiles.py" ^
        -unattended -nopause -nullrhi -nosplash

说明：
- 只改 `ProjectileClass` 这一列，其余行数据通过导出/回填 JSON 保留；
- 改前会把原 uasset 备份到 Saved/Backup/；
- 重复执行安全（已是目标值时不写入）。
"""
import json
import os
import shutil

import unreal

TABLE_PATH = "/Game/Data/Enemies/DT_EnemyArchetypes"
TABLE_FILE = r"F:/AI_WorkSpeace/baka/Content/Data/Enemies/DT_EnemyArchetypes.uasset"
BACKUP_DIR = r"F:/AI_WorkSpeace/baka/Saved/Backup"

# 敌人类型 -> 弹体蓝图资产路径（None 表示保持空，走敌人 BP/原生回退）
PROJECTILE_BY_TYPE = {
    "Rifleman": "/Game/BP/Enemy/Rifleman/BP_EnemyProjectile_Rifleman.BP_EnemyProjectile_Rifleman_C",
    "Assault": "/Game/BP/Enemy/Assault/BP_EnemyProjectile_Assault.BP_EnemyProjectile_Assault_C",
    "Juggernaut": "/Game/BP/Enemy/Juggernaut/BP_EnemyProjectile_Juggernaut.BP_EnemyProjectile_Juggernaut_C",
    "Orbiter": "/Game/BP/Enemy/Orbiter/BP_EnemyProjectile_Orbiter.BP_EnemyProjectile_Orbiter_C",
    "Marksman": "/Game/BP/Enemy/Marksman/BP_EnemyProjectile_Marksman.BP_EnemyProjectile_Marksman_C",
    "Skirmisher": "/Game/BP/Enemy/Skirmisher/BP_EnemyProjectile_Skirmisher.BP_EnemyProjectile_Skirmisher_C",
    "Cannoneer": "/Game/BP/Enemy/Cannoneer/BP_EnemyProjectile_Cannoneer.BP_EnemyProjectile_Cannoneer_C",
    "Scatterer": "/Game/BP/Enemy/Scatterer/BP_EnemyProjectile_Scatterer.BP_EnemyProjectile_Scatterer_C",
    "ShockPillar": "/Game/BP/Enemy/ShockPillar/BP_EnemyProjectile_ShockPillar.BP_EnemyProjectile_ShockPillar_C",
    "Sniper": "/Game/BP/Enemy/Sniper/BP_EnemyProjectile_Sniper.BP_EnemyProjectile_Sniper_C",
    "Shotgunner": "/Game/BP/Enemy/Shotgunner/BP_EnemyProjectile_Shotgunner.BP_EnemyProjectile_Shotgunner_C",
    "Repeater": "/Game/BP/Enemy/Repeater/BP_EnemyProjectile_Repeater.BP_EnemyProjectile_Repeater_C",
    "Ringcaster": "/Game/BP/Enemy/Ringcaster/BP_EnemyProjectile_Ringcaster.BP_EnemyProjectile_Ringcaster_C",
    "SkyMoth": "/Game/BP/Enemy/SkyMoth/BP_EnemyProjectile_SkyMoth.BP_EnemyProjectile_SkyMoth_C",
    "WindReaver": "/Game/BP/Enemy/WindReaver/BP_EnemyProjectile_WindReaver.BP_EnemyProjectile_WindReaver_C",
    "StarLantern": "/Game/BP/Enemy/StarLantern/BP_EnemyProjectile_StarLantern.BP_EnemyProjectile_StarLantern_C"
}


def log(message):
    unreal.log("[AssignEnemyProjectiles] {}".format(message))


def backup_table():
    os.makedirs(BACKUP_DIR, exist_ok=True)
    backup_path = os.path.join(BACKUP_DIR, "DT_EnemyArchetypes_before_projectiles.uasset")
    if not os.path.exists(backup_path):
        shutil.copy2(TABLE_FILE, backup_path)
        log("backup: {}".format(backup_path))


def main():
    table = unreal.load_asset(TABLE_PATH)
    if table is None:
        raise RuntimeError("missing table {}".format(TABLE_PATH))

    rows = json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))
    log("rows: {}".format(len(rows)))

    changed = 0
    for row in rows:
        enemy_type = row.get("EnemyType")
        target = PROJECTILE_BY_TYPE.get(enemy_type)
        if target is None:
            continue
        if row.get("ProjectileClass") != target:
            row["ProjectileClass"] = target
            changed += 1
            log("{} ({}) -> {}".format(row.get("Name"), enemy_type, target))

    if changed == 0:
        log("nothing to change")
        return

    backup_table()
    text = json.dumps(rows, ensure_ascii=False, indent="\t")
    if not unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(table, text):
        raise RuntimeError("fill_data_table_from_json_string failed")

    unreal.EditorAssetLibrary.save_asset(TABLE_PATH, False)
    log("saved, changed {} rows".format(changed))

    verify_rows = json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))
    for row in verify_rows:
        log("verify {} ({}) -> {}".format(row.get("Name"), row.get("EnemyType"), row.get("ProjectileClass")))


main()
