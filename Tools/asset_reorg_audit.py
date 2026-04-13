import json
import pathlib
import re

import unreal


PROJECT_ROOT = pathlib.Path(r"F:\AI_WorkSpeace\ai")
OUTPUT_PATH = PROJECT_ROOT / "Tools" / "asset_reorg_audit.json"
SCAN_DIRS = [
    PROJECT_ROOT / "Source",
    PROJECT_ROOT / "Config",
]
SYSTEM_FOLDERS = {"Collections", "Developers"}
ASSET_PATTERN = re.compile(r"/Game/[A-Za-z0-9_/\.\-]+")


def normalize_package_path(raw_path: str) -> str:
    path = raw_path.strip().strip("\"'")
    last_segment = path.rsplit("/", 1)[-1]
    if "." in last_segment:
        asset_name, object_name = last_segment.split(".", 1)
        if asset_name == object_name:
            path = f"{path.rsplit('/', 1)[0]}/{asset_name}"
    return path


def collect_direct_references() -> list[str]:
    results: set[str] = set()
    for scan_dir in SCAN_DIRS:
        if not scan_dir.exists():
            continue

        for file_path in scan_dir.rglob("*"):
            if not file_path.is_file():
                continue

            try:
                text = file_path.read_text(encoding="utf-8", errors="ignore")
            except Exception:
                continue

            for match in ASSET_PATTERN.findall(text):
                results.add(normalize_package_path(match))

    return sorted(results)


def gather_used_packages(root_packages: list[str]) -> set[str]:
    asset_registry = unreal.AssetRegistryHelpers.get_asset_registry()
    dependency_options = unreal.AssetRegistryDependencyOptions()
    dependency_options.include_hard_package_references = True
    dependency_options.include_soft_package_references = True
    dependency_options.include_hard_management_references = True
    dependency_options.include_soft_management_references = True
    dependency_options.include_searchable_names = False

    used_packages: set[str] = set()
    stack = list(root_packages)

    while stack:
        package_path = stack.pop()
        if package_path in used_packages:
            continue

        used_packages.add(package_path)
        try:
            dependencies = asset_registry.get_dependencies(package_path, dependency_options) or []
        except Exception:
            dependencies = []

        for dependency in dependencies:
            dependency_path = str(dependency)
            if not dependency_path.startswith("/Game/"):
                continue
            if dependency_path not in used_packages:
                stack.append(dependency_path)

    return used_packages


def list_top_level_folders() -> list[str]:
    content_root = PROJECT_ROOT / "Content"
    if not content_root.exists():
        return []

    return sorted(path.name for path in content_root.iterdir() if path.is_dir())


def package_to_top_level(package_path: str) -> str | None:
    parts = package_path.split("/")
    if len(parts) < 3:
        return None
    return parts[2]


def main() -> None:
    direct_references = collect_direct_references()
    editor_asset_library = unreal.EditorAssetLibrary

    root_packages = [
        path for path in direct_references
        if editor_asset_library.does_asset_exist(path)
    ]

    used_packages = gather_used_packages(root_packages)
    used_top_levels = sorted(
        {
            top_level
            for top_level in (package_to_top_level(package) for package in used_packages)
            if top_level and top_level not in SYSTEM_FOLDERS
        }
    )

    all_top_levels = list_top_level_folders()
    unused_top_levels = [
        folder for folder in all_top_levels
        if folder not in used_top_levels and folder not in SYSTEM_FOLDERS
    ]

    report = {
        "direct_reference_count": len(direct_references),
        "direct_references": direct_references,
        "root_package_count": len(root_packages),
        "root_packages": root_packages,
        "used_package_count": len(used_packages),
        "used_packages": sorted(used_packages),
        "used_top_levels": used_top_levels,
        "unused_top_levels": unused_top_levels,
    }

    OUTPUT_PATH.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    unreal.log_warning(f"ASSET_REORG_AUDIT_WRITTEN={OUTPUT_PATH}")


if __name__ == "__main__":
    main()
