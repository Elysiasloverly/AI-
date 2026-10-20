"""把武器名 / 升级卡名与介绍改成东方 Project 风格（v2）。

与 v1 的差别：
  * 不做中间导出验证（导出会让包重新进入加载状态，保存时文件句柄未释放 -> Error 32）；
  * 保存前先 GC 释放包句柄；
  * 每次保存都记录文件 mtime，没变化就重试并再次 GC。

用法：
    UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script="<此文件绝对路径>"
"""

import json
import os
import time

import unreal

WEAPON_TABLE = "/Game/Data/Weapons/DT_WeaponConfig"
UPGRADE_TABLE = "/Game/Data/Upgrades/DT_UpgradeDefinitions"
WEAPON_DISK = r"F:/AI_WorkSpeace/ai/Content/Data/Weapons/DT_WeaponConfig.uasset"
UPGRADE_DISK = r"F:/AI_WorkSpeace/ai/Content/Data/Upgrades/DT_UpgradeDefinitions.uasset"
REPORT_PATH = r"F:/AI_WorkSpeace/ai/Saved/DataDump/rename_report2.txt"

WEAPON_NAMES = {
    "Projectile": "御札",
    "Scythe": "银刃",
    "Rocket": "八卦炉",
    "Laser": "极限火花",
    "HellTower": "御柱",
    "Mortar": "阴阳玉",
}

UPGRADE_TEXT = {
    "MaxHealth": ("蓬莱之药", "最大生命 +25，并立即回复 25 点生命。"),
    "MoveSpeed": ("天狗疾风", "移动速度 +90。"),
    "ArmorCapacity": ("结界护壁", "最大护甲 +25，并立即恢复 25 点护甲。5 秒不受伤后会快速恢复。"),
    "DashCooldown": ("隙间跳跃", "冲刺冷却 -0.5 秒，最低缩短到 0.5 秒。"),
    "AttackPower": ("灵力增幅", "御札、银刃、八卦炉、极限火花、御柱伤害全部 +8。"),
    "AttackSpeed": ("咏唱加速", "所有武器攻速同步提升。"),
    "PickupRadius": ("赛钱箱", "经验吸附半径 +180。"),
    "Recovery": ("月之药", "每秒生命恢复 +1.5。"),
    "AttackRange": ("远隔操纵", "索敌、极限火花射程和爆炸范围同步扩大。"),
    "ProjectileSpeed": ("流星弹", "御札与八卦炉飞行速度提升。"),
    "ProjectileCount": ("弹幕增殖", "御札数量 +1。"),
    "Armor": ("弹幕偏转", "减伤 +8%。"),
    "ExperienceGain": ("妖怪退治", "获得经验 +25%。"),
    "ScytheCount": ("银刃乱舞", "未持有时解锁银刃，已持有时银刃数量 +1。"),
    "RocketCount": ("八卦炉·改", "未持有时解锁八卦炉，已持有时八卦炉数量 +1。"),
    "LaserCount": ("双重魔炮", "未持有时解锁极限火花，已持有时极限火花数量 +1。"),
    "HellTowerCount": ("御柱林立", "未持有时解锁御柱，已持有时御柱数量 +1。"),
    "LaserRefraction": ("星尘折射", "极限火花命中后会继续折射到最近敌人，每跳伤害减半，折射次数 +1。"),
    "MortarCount": ("阴阳玉连投", "未持有时解锁阴阳玉，已持有时阴阳玉数量 +1。"),
    "MortarBlastRadius": ("阴阳玉·扩散", "增加阴阳玉落点的爆炸范围。"),
    "ScytheSize": ("银刃巨化", "银刃尺寸与伤害碰撞范围增加基础值的 20%，不改变环绕距离。"),
    "ScytheLifesteal": ("红雾吸血", "银刃造成伤害的 20% 回复生命；首次获得 20%，之后每级 +5%。"),
}

lines = []


def log(message):
    lines.append(str(message))


def stat_of(path):
    try:
        st = os.stat(path)
        return "{:.3f}/{}".format(st.st_mtime, st.st_size)
    except OSError as exc:
        return "ERR:{}".format(exc)


def save_with_retry(table, table_path, disk_path, attempts=6):
    """保存失败多半是包文件句柄未释放（Error 32）；GC 之后再试。"""
    tag = table_path.rsplit("/", 1)[-1]
    for index in range(attempts):
        unreal.SystemLibrary.collect_garbage()
        before = stat_of(disk_path)
        ok = unreal.EditorAssetLibrary.save_asset(table_path, False)
        after = stat_of(disk_path)
        log("  [{}] 尝试 {}: 返回={} 磁盘 {} => {}".format(tag, index + 1, ok, before, after))
        if before != after:
            log("  [{}] 落盘成功".format(tag))
            return True
        time.sleep(0.6)
    log("  [{}] 重试耗尽，仍未落盘".format(tag))
    return False


def apply_weapons():
    table = unreal.load_asset(WEAPON_TABLE)
    rows = json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))
    changed = 0
    for row in rows:
        new_name = WEAPON_NAMES.get(row["Name"])
        if new_name is not None and row["DisplayName"] != new_name:
            row["DisplayName"] = new_name
            changed += 1
    log("[武器] 需改动 {} 行".format(changed))
    unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(
        table, json.dumps(rows, ensure_ascii=False))
    return save_with_retry(table, WEAPON_TABLE, WEAPON_DISK)


def apply_upgrades():
    table = unreal.load_asset(UPGRADE_TABLE)
    rows = json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))
    changed = 0
    for row in rows:
        text = UPGRADE_TEXT.get(row["Type"])
        if text is None:
            log("[升级] 未覆盖的类型 {}".format(row["Type"]))
            continue
        title, desc = text
        if row["Title"] != title or row["Description"] != desc:
            changed += 1
        row["Title"] = title
        row["Description"] = desc
    log("[升级] 需改动 {} 行".format(changed))
    unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(
        table, json.dumps(rows, ensure_ascii=False))
    return save_with_retry(table, UPGRADE_TABLE, UPGRADE_DISK)


def main():
    try:
        ok_weapon = apply_weapons()
        ok_upgrade = apply_upgrades()
        log("[RESULT] weapon={} upgrade={}".format(ok_weapon, ok_upgrade))
    except Exception as exc:  # noqa: BLE001
        log("[EXCEPTION] {}".format(exc))
    finally:
        with open(REPORT_PATH, "w", encoding="utf-8") as handle:
            handle.write("\n".join(lines))


main()
