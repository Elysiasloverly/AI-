"""敌人专属参数迁移的纯 Python 工具（不依赖 unreal，供离线预演与编辑器脚本共用）。

背景：
- 旧结构是 7 个"行为家族"共享块（Charger / Orbiter / Shooter / Skirmisher / Artillery /
  Spitter / ShockPillar），13 种怪物按类型读取其中一个；
- 新结构是 13 个"一怪一块"（Rifleman / Assault / Juggernaut / Orbiter / Marksman /
  Skirmisher / Cannoneer / Scatterer / ShockPillar / Sniper / Shotgunner / Repeater / Ringcaster）。

迁移规则（按 EnemyType）：
- Charger -> Assault、Shooter -> Marksman、Artillery -> Cannoneer、Spitter -> Scatterer
  （这四个是"族首怪物"，旧块数据直接落到新块）；
- Sniper / Repeater 行：把 Marksman 块整块复制到自己的块（旧结构下三族共用 Shooter 数据，
  但每行各存一份，复制的就是该行原先实际生效的值）；
- Shotgunner 行：把 Scatterer 块整块复制到自己的块；
- Ringcaster 行：把 ShockPillar 块的冷却 + BulletRing 复制到自己的块（移动段保持默认哨兵 0）；
- 其余块保持原样（Rifleman / Juggernaut 旧结构没有专属数据，新块保持默认 = 跟随基线）。

UE 结构体文本格式：`(Key=Value,Key2=(X=1.000000,Y=2.000000),...)`。
本模块只做"按 key 定位 + 平衡括号截取"的文本手术，不重排、不重格式化，
避免整个结构体被重新序列化带来的格式漂移。
"""

KEY_RENAMES = {
    "Charger": "Assault",
    "Shooter": "Marksman",
    "Artillery": "Cannoneer",
    "Spitter": "Scatterer",
}

# EnemyType -> (目标块, 源块, 拷贝哪些 key；None = 整块)
PER_ROW_COPIES = {
    "Sniper": ("Sniper", "Marksman", None),
    "Repeater": ("Repeater", "Marksman", None),
    "Shotgunner": ("Shotgunner", "Scatterer", None),
    "Ringcaster": ("Ringcaster", "ShockPillar",
                   ["InitialCooldownMin", "InitialCooldownMax", "AttackCooldown", "BulletRing"]),
}


def _scan_value(text, start):
    """从 start 开始截取一个值（结构体走平衡括号，标量走到 , 或 ) 为止）。返回 (值文本, 结束位置)。"""
    if start < len(text) and text[start] == "(":
        depth = 0
        pos = start
        while pos < len(text):
            char = text[pos]
            if char == "(":
                depth += 1
            elif char == ")":
                depth -= 1
                if depth == 0:
                    return text[start:pos + 1], pos + 1
            pos += 1
        raise ValueError("unbalanced struct value at {}".format(start))

    pos = start
    while pos < len(text) and text[pos] not in ",)":
        pos += 1
    return text[start:pos], pos


def find_block(text, key):
    """在结构体文本顶层查找 `key=`，返回 (值起始, 值结束)；找不到返回 None。"""
    depth = 0
    pos = 0
    while pos < len(text):
        char = text[pos]
        if char == "(":
            depth += 1
        elif char == ")":
            depth -= 1
        elif char == "=" and depth == 1:
            key_end = pos
            key_start = key_end - 1
            while key_start >= 0 and text[key_start] not in "(,":
                key_start -= 1
            if text[key_start + 1:key_end] == key:
                value_start = pos + 1
                _, value_end = _scan_value(text, value_start)
                return value_start, value_end
        pos += 1
    return None


def get_block(text, key):
    span = find_block(text, key)
    if span is None:
        return None
    return text[span[0]:span[1]]


def set_block(text, key, value):
    """把顶层 `key=` 的值替换成 value；不存在时在末尾追加。"""
    span = find_block(text, key)
    if span is None:
        body = text[:-1] if text.endswith(")") else text
        separator = "" if body.endswith("(") else ","
        return "{}{}{}={})".format(body, separator, key, value)
    return text[:span[0]] + value + text[span[1]:]


def copy_keys(target_text, source_text, keys):
    """按 key 把 source 的值逐个写入 target（就地文本手术）。"""
    result = target_text if target_text is not None else "()"
    for key in keys:
        source_value = get_block(source_text, key)
        if source_value is None:
            continue
        result = set_block(result, key, source_value)
    return result


def remove_block(text, key):
    """删除顶层 `key=value`（连带一个逗号），找不到时原样返回。"""
    span = find_block(text, key)
    if span is None:
        return text
    value_start, value_end = span

    # 向前找到键名起点（'=' 前、上一个逗号或左括号后）
    key_start = value_start - 1
    while key_start > 0 and text[key_start - 1] not in "(,":
        key_start -= 1
    start = key_start
    end = value_end

    if start > 0 and text[start - 1] == ",":
        start -= 1
    elif end < len(text) and text[end] == ",":
        end += 1
    return text[:start] + text[end:]


def migrate_special_params(text, enemy_type):
    """对单行的 SpecialParams 文本做迁移。返回 (新文本, 说明列表)。"""
    notes = []
    migrated = text

    for old_key, new_key in KEY_RENAMES.items():
        value = get_block(migrated, old_key)
        if value is None:
            continue
        migrated = set_block(migrated, new_key, value)
        migrated = remove_block(migrated, old_key)
        notes.append("{} -> {}".format(old_key, new_key))

    copy_rule = PER_ROW_COPIES.get(enemy_type)
    if copy_rule is not None:
        target_key, source_key, keys = copy_rule
        source_value = get_block(migrated, source_key)
        if source_value is not None:
            if keys is None:
                migrated = set_block(migrated, target_key, source_value)
                notes.append("{} = {} (整块)".format(target_key, source_key))
            else:
                target_value = get_block(migrated, target_key)
                migrated = set_block(migrated, target_key, copy_keys(target_value, source_value, keys))
                notes.append("{} <- {} ({})".format(target_key, source_key, ", ".join(keys)))

    return migrated, notes


def migrate_rows(rows):
    """对整表 JSON（export_data_table_to_json_string 的解析结果）做迁移。返回 (新 rows, 报告)。"""
    report = []
    for row in rows:
        enemy_type = str(row.get("EnemyType", "")).split("::")[-1]
        special = row.get("SpecialParams")
        if not isinstance(special, str):
            continue
        migrated, notes = migrate_special_params(special, enemy_type)
        row["SpecialParams"] = migrated
        if notes:
            report.append("{} ({}): {}".format(row.get("Name"), enemy_type, "; ".join(notes)))
    return rows, report


def block_values(text, key):
    """把某个块文本解析成 dict，用于迁移后校验（仅支持标量与一层嵌套结构体）。"""
    value = get_block(text, key)
    if value is None or not value.startswith("("):
        return None
    inner = value[1:-1]
    result = {}
    pos = 0
    while pos < len(inner):
        equals = inner.find("=", pos)
        if equals < 0:
            break
        key_start = equals
        while key_start > pos and inner[key_start - 1] not in ",":
            key_start -= 1
        field = inner[key_start:equals]
        field_value, pos = _scan_value(inner, equals + 1)
        result[field] = field_value
        if pos < len(inner) and inner[pos] == ",":
            pos += 1
    return result
