"""全项目资源审计（只读，不修改任何资产）。

用法：
    UnrealEditor-Cmd.exe F:\\AI_WorkSpeace\\ai\\Baka.uproject ^
        -run=pythonscript -script="Tools/audit_assets.py" ^
        -unattended -nopause -nullrhi -nosplash

输出：
- Saved/DataDump/asset_audit.json   完整明细（供后续处理）
- 日志里打印摘要：重定向器、无引用资产、断链依赖、重名资产、疑似旧命名资产

判定口径：
- 引用关系同时统计硬引用与软引用（软引用也算"还在用"）；
- Map 资源视为根，不参与"无引用"判定；
- Config/*.ini 里以 /Game/ 字面出现的路径额外视为"配置在用"；
- "无引用"只代表资产注册表里没人指向它，不等于可以安全删除 —— 还需要人工确认。
"""
import json
import os
import re

import unreal

PROJECT_ROOT = r"F:/AI_WorkSpeace/baka"
CONTENT_ROOT = os.path.join(PROJECT_ROOT, "Content")
OUT_JSON = os.path.join(PROJECT_ROOT, "Saved/DataDump/asset_audit.json")
CONFIG_DIR = os.path.join(PROJECT_ROOT, "Config")

# 旧命名 / 临时资产的可疑模式（只用于提示，不作为删除依据）
SUSPICIOUS_PATTERNS = [
    (r"Hunter|_Charger|Tank|Guerrilla|_Shooter|_Artillery|_Spitter|Burst|Ringer", "旧命名残留"),
    (r"Test|NewBlueprint|Copy|Backup|_old|_Old| 1$", "临时/副本资产"),
    (r"新建", "中文临时命名"),
]


def log(message):
    unreal.log("[AssetAudit] {}".format(message))


def package_to_file(package_name):
    relative = package_name
    if relative.startswith("/Game/"):
        relative = relative[len("/Game/"):]
    return os.path.join(CONTENT_ROOT, relative.replace("/", os.sep) + ".uasset")


def file_size(path):
    try:
        return os.path.getsize(path)
    except OSError:
        return 0


def build_dependency_options():
    try:
        return unreal.AssetRegistryDependencyOptions(
            include_hard_package_references=True,
            include_soft_package_references=True,
            include_searchable_names=False,
            include_soft_management_references=True,
            include_hard_management_references=True,
        )
    except Exception:  # noqa: BLE001
        return None


def load_config_paths():
    paths = set()
    pattern = re.compile(r"/Game/[A-Za-z0-9_/\-\.]+")
    for name in os.listdir(CONFIG_DIR):
        if not name.endswith(".ini"):
            continue
        try:
            with open(os.path.join(CONFIG_DIR, name), encoding="utf-8", errors="ignore") as handle:
                for match in pattern.findall(handle.read()):
                    paths.add(match.split(".")[0])
        except OSError:
            continue
    return paths


def main():
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    options = build_dependency_options()
    config_paths = load_config_paths()
    log("config 引用路径: {}".format(len(config_paths)))

    assets = registry.get_assets_by_path("/Game", recursive=True)
    log("资产总数: {}".format(len(assets)))

    entries = []
    total_bytes = 0
    for data in assets:
        package_name = str(data.package_name)
        asset_name = str(data.asset_name)
        class_name = str(data.asset_class_path.asset_name)
        size = file_size(package_to_file(package_name))
        total_bytes += size

        try:
            referencers = registry.get_referencers(data.package_name, options) if options else registry.get_referencers(data.package_name)
            referencers = sorted(str(item) for item in referencers)
        except Exception as exc:  # noqa: BLE001
            referencers = []
            log("referencers failed {}: {}".format(package_name, exc))

        try:
            dependencies = registry.get_dependencies(data.package_name, options) if options else registry.get_dependencies(data.package_name)
            dependencies = sorted(str(item) for item in dependencies)
        except Exception:  # noqa: BLE001
            dependencies = []

        entries.append({
            "package": package_name,
            "name": asset_name,
            "class": class_name,
            "bytes": size,
            "referencers": referencers,
            "dependencies": dependencies,
            "config_referenced": package_name in config_paths,
        })

    known_packages = {entry["package"] for entry in entries}

    redirectors = [e for e in entries if e["class"] in ("ObjectRedirector", "ObjectRedirectorFactory")]
    maps = [e for e in entries if e["class"] in ("World", "Level", "WorldFactory") or e["package"].startswith("/Game/Map")]
    orphan_candidates = [
        e for e in entries
        if not e["referencers"] and e not in maps and not e["config_referenced"]
    ]
    config_only = [e for e in entries if e["config_referenced"] and not e["referencers"]]
    broken = []
    for entry in entries:
        for dependency in entry["dependencies"]:
            if dependency.startswith("/Game/") and dependency not in known_packages:
                broken.append({"package": entry["package"], "missing": dependency})

    by_name = {}
    for entry in entries:
        by_name.setdefault(entry["name"], []).append(entry["package"])
    duplicates = {name: packages for name, packages in by_name.items() if len(packages) > 1}

    suspicious = []
    for entry in entries:
        for pattern, reason in SUSPICIOUS_PATTERNS:
            if re.search(pattern, entry["name"]) and entry["class"] != "ObjectRedirector":
                suspicious.append({"package": entry["package"], "reason": reason})
                break

    result = {
        "summary": {
            "asset_count": len(entries),
            "total_bytes": total_bytes,
            "redirector_count": len(redirectors),
            "orphan_candidate_count": len(orphan_candidates),
            "config_only_count": len(config_only),
            "broken_dependency_count": len(broken),
            "duplicate_name_count": len(duplicates),
            "suspicious_count": len(suspicious),
        },
        "redirectors": [{"package": e["package"], "referencers": e["referencers"]} for e in redirectors],
        "orphan_candidates": [
            {"package": e["package"], "class": e["class"], "bytes": e["bytes"]}
            for e in sorted(orphan_candidates, key=lambda item: -item["bytes"])
        ],
        "config_only": [{"package": e["package"], "class": e["class"]} for e in config_only],
        "broken_dependencies": broken,
        "duplicates": duplicates,
        "suspicious": suspicious,
        "assets": entries,
    }

    os.makedirs(os.path.dirname(OUT_JSON), exist_ok=True)
    with open(OUT_JSON, "w", encoding="utf-8") as handle:
        json.dump(result, handle, ensure_ascii=False, indent="\t")

    log("summary {}".format(json.dumps(result["summary"], ensure_ascii=False)))
    for entry in result["redirectors"]:
        log("redirector: {} (referencers={})".format(entry["package"], len(entry["referencers"])))
    for entry in result["orphan_candidates"][:40]:
        log("orphan? {} [{}] {:.1f}KB".format(entry["package"], entry["class"], entry["bytes"] / 1024.0))
    for entry in result["broken_dependencies"][:40]:
        log("broken: {} -> {}".format(entry["package"], entry["missing"]))
    for name, packages in list(result["duplicates"].items())[:20]:
        log("dup: {} -> {}".format(name, packages))
    for entry in result["suspicious"][:30]:
        log("suspicious: {} ({})".format(entry["package"], entry["reason"]))
    log("written -> {}".format(OUT_JSON))


main()
