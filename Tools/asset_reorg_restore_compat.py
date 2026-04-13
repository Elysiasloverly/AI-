import json
import pathlib

import unreal


PROJECT_ROOT = pathlib.Path(r"F:\AI_WorkSpeace\ai")
AUDIT_PATH = PROJECT_ROOT / "Tools" / "asset_reorg_audit.json"
REPORT_PATH = PROJECT_ROOT / "Tools" / "asset_reorg_restore_compat_report.json"
DEST_ROOT = "/Game/GameAssets"


def main() -> None:
    audit = json.loads(AUDIT_PATH.read_text(encoding="utf-8"))
    used_packages: list[str] = sorted(audit["used_packages"])

    editor_asset_library = unreal.EditorAssetLibrary
    restored: list[dict[str, str]] = []
    skipped: list[dict[str, str]] = []
    failed: list[dict[str, str]] = []

    for original_path in used_packages:
        compat_path = f"{DEST_ROOT}/{original_path[len('/Game/'):]}"
        if editor_asset_library.does_asset_exist(original_path):
            skipped.append({"original": original_path, "reason": "already_exists"})
            continue

        if not editor_asset_library.does_asset_exist(compat_path):
            failed.append({"original": original_path, "reason": "compat_missing", "compat": compat_path})
            continue

        editor_asset_library.make_directory(original_path.rsplit("/", 1)[0])
        success = editor_asset_library.duplicate_asset(compat_path, original_path)
        if success:
            editor_asset_library.save_asset(original_path, only_if_is_dirty=False)
            restored.append({"original": original_path, "compat": compat_path})
        else:
            failed.append({"original": original_path, "reason": "duplicate_failed", "compat": compat_path})

    REPORT_PATH.write_text(
        json.dumps(
            {
                "restored_count": len(restored),
                "skipped_count": len(skipped),
                "failed_count": len(failed),
                "restored": restored,
                "skipped": skipped,
                "failed": failed,
            },
            ensure_ascii=False,
            indent=2,
        ),
        encoding="utf-8",
    )
    unreal.log_warning(f"ASSET_REORG_RESTORE_COMPAT_WRITTEN={REPORT_PATH}")


if __name__ == "__main__":
    main()
