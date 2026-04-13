import json
import pathlib

import unreal


PROJECT_ROOT = pathlib.Path(r"F:\AI_WorkSpeace\ai")
AUDIT_PATH = PROJECT_ROOT / "Tools" / "asset_reorg_audit.json"
REPORT_PATH = PROJECT_ROOT / "Tools" / "asset_reorg_cleanup_report.json"
KEEP_FOLDERS = {"GameAssets", "Collections", "Developers"}


def main() -> None:
    audit = json.loads(AUDIT_PATH.read_text(encoding="utf-8"))
    old_used_folders = audit["used_top_levels"]
    old_unused_folders = audit["unused_top_levels"]

    target_folders = sorted(
        {
            folder
            for folder in [*old_used_folders, *old_unused_folders]
            if folder not in KEEP_FOLDERS
        }
    )

    editor_asset_library = unreal.EditorAssetLibrary
    deleted: list[str] = []
    skipped: list[dict[str, str]] = []
    failed: list[str] = []

    for folder in target_folders:
        directory_path = f"/Game/{folder}"
        if not editor_asset_library.does_directory_exist(directory_path):
            skipped.append({"directory": directory_path, "reason": "missing"})
            continue

        success = editor_asset_library.delete_directory(directory_path)
        if success:
            deleted.append(directory_path)
        else:
            failed.append(directory_path)

    REPORT_PATH.write_text(
        json.dumps(
            {
                "deleted": deleted,
                "skipped": skipped,
                "failed": failed,
            },
            ensure_ascii=False,
            indent=2,
        ),
        encoding="utf-8",
    )
    unreal.log_warning(f"ASSET_REORG_CLEANUP_WRITTEN={REPORT_PATH}")


if __name__ == "__main__":
    main()
